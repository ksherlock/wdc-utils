/*
 * WDC to OMF Linker.
 *
 *
 */

#include <sysexits.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <assert.h>
#include <stdio.h>

#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <utility>
#include <numeric>
#include <iterator>

#include "obj816.h"
#include "expression.h"
#include "omf.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif


struct {
	bool _v = false;
	bool _C = false;
	bool _X = false;
	bool _S = false;
	std::string _o;

	unsigned _errors = 0;
} flags;

enum class endian {
	little = __ORDER_LITTLE_ENDIAN__,
	big = __ORDER_BIG_ENDIAN__,
	native = __BYTE_ORDER__
};


template<class T>
void swap_if(T &t, std::false_type) {}

void swap_if(uint8_t &, std::true_type) {}

void swap_if(uint16_t &value, std::true_type) {
	value = __builtin_bswap16(value);
}

void swap_if(uint32_t &value, std::true_type) {
	value = __builtin_bswap32(value);
}

void swap_if(uint64_t &value, std::true_type) {
	value = __builtin_bswap64(value);
}

template<class T>
void le_to_host(T &value) {
	swap_if(value, std::integral_constant<bool, endian::native == endian::big>{});
}


#pragma pack(push, 1)
struct Header {
	uint32_t magic;				/* magic number for detection */
	uint16_t version;			/* version number of object format */
	uint8_t filetype;			/* file type, object or library */
};

#pragma pack(pop)


struct section {
	std::string name;
	uint8_t flags = 0;
	uint32_t org = 0;
	uint32_t size = 0;

	unsigned number = -1;
	std::vector<uint8_t> data;
	std::vector<expression> expressions;

	unsigned end_symbol = 0; // auto-generated _END_{name} symbol.
};

struct symbol {
	std::string name;
	uint8_t type = 0;
	uint8_t flags = 0;
	uint32_t offset = 0;
	int section = -1;
};

template<class T>
uint8_t read_8(T &iter) {
	uint8_t tmp = *iter;
	++iter;
	return tmp;
}

template<class T>
uint16_t read_16(T &iter) {
	uint16_t tmp = 0;

	tmp |= *iter << 0;
	++iter;
	tmp |= *iter << 8;
	++iter;
	return tmp;
}

template<class T>
uint32_t read_32(T &iter) {
	uint32_t tmp = 0;

	tmp |= *iter << 0;
	++iter;
	tmp |= *iter << 8;
	++iter;
	tmp |= *iter << 16;
	++iter;
	tmp |= *iter << 24;
	++iter;


	return tmp;
}

template<class T>
std::string read_cstring(T &iter) {
	std::string s;
	for(;;) {
		uint8_t c = *iter;
		++iter;
		if (!c) break;
		s.push_back(c);
	}
	return s;
}


template<class T>
std::string read_pstring(T &iter) {
	std::string s;
	unsigned  size = *iter;
	++iter;
	s.reserve(size);
	while (size--) {
		uint8_t c = *iter;
		++iter;
		s.push_back(c);
	}
	return s;
}


std::vector<section> read_sections(const std::vector<uint8_t> &section_data) {

	std::vector<section> sections;
	auto iter = section_data.begin();
	while (iter != section_data.end()) {

		section s;

		s.number = read_8(iter);
		s.flags = read_8(iter);
		s.size = read_32(iter);
		s.org = read_32(iter);

		if (!(s.flags & SEC_NONAME)) s.name = read_cstring(iter);

		sections.emplace_back(std::move(s));
	}
	return sections;
}


std::vector<symbol> read_symbols(const std::vector<uint8_t> &symbol_data) {

	std::vector<symbol> symbols;

	auto iter = symbol_data.begin();
	while (iter != symbol_data.end()) {
		symbol s;
		s.type = read_8(iter);
		s.flags = read_8(iter);
		s.section = read_8(iter);
		s.offset = s.type == S_UND ? 0 : read_32(iter);
		s.name = read_cstring(iter);


		symbols.emplace_back(std::move(s));
	}

	return symbols;
}


std::unordered_map<std::string, int> section_map;
std::vector<section> sections;

std::unordered_map<std::string, int> symbol_map;
std::vector<symbol> symbols;

std::unordered_set<std::string> undefined_symbols;





/*
 * replace undefined symbols (if possible) and simplify expressions.
 *
 */
void simplify() {
	for (auto &s : sections) {
		for (auto &e : s.expressions) {
			bool delta = false;

			// first check for undefined symbols.
			if (e.undefined) {
				e.undefined = false;
				for (auto &t : e.stack) {

					if (t.tag == OP_SYM) {
						const auto &ss = symbols[t.section];
						switch(ss.type & 0x0f) {
							case S_UND:
								e.undefined = true;
								break;
							case S_REL:
								t = expr{OP_LOC, ss.offset, (uint32_t)ss.section};
								delta = true;
								break;
							case S_ABS:
								t = expr{OP_VAL, (uint32_t)ss.offset};
								delta = true;
								break;
						}
					}
				}
			}
			if (e.stack.size() > 1) simplify_expression(e);
		}
	}
}



/*
 * read and process all sections...
 * if section > 5, remap based on name.
 *
 */

void one_module(const std::vector<uint8_t> &data, const std::vector<uint8_t> &section_data, const std::vector<uint8_t> &symbol_data) {

	std::array<int, 256> remap_section;

	int current_section = SECT_CODE;
	std::vector<uint8_t> *data_ptr = &sections[current_section].data;

	std::fill(remap_section.begin(), remap_section.end(), -1);
	remap_section[SECT_PAGE0] = SECT_PAGE0;
	remap_section[SECT_CODE] = SECT_CODE;
	remap_section[SECT_KDATA] = SECT_KDATA;
	remap_section[SECT_DATA] = SECT_DATA;
	remap_section[SECT_UDATA] = SECT_UDATA;

	std::vector<section> local_sections = read_sections(section_data);
	std::vector<symbol> local_symbols = read_symbols(symbol_data);

	// convert local sections to global 
	for (auto &s : local_sections) {
		if (s.number <= SECT_UDATA) {
			sections[s.number].size += s.size; // for page0 / udata sections.
			continue;
		}

		// todo -- should install section name as global symbol?

		auto iter = section_map.find(s.name);
		if (iter == section_map.end()) {
			int virtual_section = sections.size();
			remap_section[s.number] = virtual_section;
			s.number = virtual_section;
			sections.emplace_back(s);
			symbol_map.emplace(s.name, virtual_section);
		} else {
			auto &ss = sections[iter->second];
			assert(ss.flags == s.flags); // check org????
			remap_section[s.number] = iter->second;
			s.number = iter->second;

			// update size (for ref-only sections)
			ss.size += s.size;
		}
	}

	// convert local symbols to global.
	for (auto &s : local_symbols) {
		if (s.type == S_UND) {
			auto iter = symbol_map.find(s.name);
			if (iter == symbol_map.end()) {
				s.section = symbols.size();
				symbol_map.emplace(s.name, s.section);
				undefined_symbols.emplace(s.name);
			}
			else {
				// already exists... 
				const auto &ss = symbols[iter->second];
				if (ss.type != S_UND) s = ss;
			}
			continue;
		}

		// remap and fudge the offset.
		if ((s.type & 0x0f) == S_REL) {
			int virtual_section = remap_section[s.section];
			assert(virtual_section != -1);
			s.section = virtual_section;
			s.offset += sections[virtual_section].data.size();
		} else {
			s.section = -1;
		}

		constexpr const unsigned mask = SF_GBL | SF_DEF;
		if ((s.flags & mask) == mask) {

			auto iter = symbol_map.find(s.name);

			if (iter == symbol_map.end()) {
				unsigned tmp = symbols.size();
				symbol_map.emplace(s.name, tmp);
				symbols.emplace_back(s);
			} else {
				auto &ss = symbols[iter->second];

				// if it was undefined, define it!
				if (ss.type == S_UND) {
					ss = s;
					undefined_symbols.erase(s.name);
				}
				else {
					// ok if symbols are identical..
					if (ss.type != s.type || ss.flags != s.flags || ss.section != s.section || ss.offset != s.offset) {
						warnx("Duplicate label %s", s.name.c_str());
						flags._errors++;
					} 
				}
			}

		}

	}


	auto iter = data.begin();
	for(;;) {
		uint8_t op = read_8(iter);
		if (op == REC_END) return;

		if (op < 0xf0) {
			data_ptr->insert(data_ptr->end(), iter, iter + op);
			iter += op;
			continue;
		}

		switch(op) {
			case REC_SPACE: {
				uint16_t count = read_16(iter);
				data_ptr->insert(data_ptr->end(), count, 0);
				break;
			}

			case REC_SECT: {
				/* switch sections */
				uint8_t s = read_8(iter);
				current_section = remap_section[s];
				assert(current_section > 0 && current_section < sections.size());

				data_ptr = &sections[current_section].data;
				break;
			}

			case REC_ORG: {
				assert(!"ORG not supported.");
				break;
			}

			case REC_RELEXP:
			case REC_EXPR: {

				expression e;
				e.relative = op == REC_RELEXP;

				e.offset =  data_ptr->size();
				e.size = read_8(iter);

				data_ptr->insert(data_ptr->end(), e.size, 0);

				/**/
				for(;;) {
					op = read_8(iter);
					if (op == OP_END) break;

					switch(op) {
						case OP_VAL: {
							uint32_t offset = read_32(iter);
							e.stack.emplace_back(op, offset);
							break;
						}
						case OP_SYM: {
							uint16_t symbol = read_16(iter);
							assert(symbol < local_symbols.size());
							auto &s = local_symbols[symbol];
							switch (s.type & 0x0f) {
								case S_UND:
									// S_UND indicates it's still undefined globally.
									e.stack.emplace_back(OP_SYM, 0, s.section); /* section is actually a symbol number */
									e.undefined = true;
									break;
								case S_REL:
									e.stack.emplace_back(OP_LOC, s.offset, s.section);
									break;

								case S_ABS:
									e.stack.emplace_back(OP_VAL, s.offset);
									break;

								default:
									assert(!"unsupported symbol flags.");
							}
							break;
						}
						case OP_LOC: {
							uint8_t section = read_8(iter);
							uint32_t offset = read_32(iter);
							int real_section = remap_section[section];
							assert(real_section >= 0);
							e.stack.emplace_back(op, offset, real_section);
							break;
						}
						// operations..
						//unary
						case OP_NOT:
						case OP_NEG:
						case OP_FLP:
						// binary
						case OP_EXP:
						case OP_MUL:
						case OP_DIV:
						case OP_MOD:
						case OP_SHR:
						case OP_SHL:
						case OP_ADD:
						case OP_SUB:
						case OP_AND:
						case OP_OR:
						case OP_XOR:
						case OP_EQ:
						case OP_GT:
						case OP_LT:
						case OP_UGT:
						case OP_ULT:
							e.stack.emplace_back(op);
							break;
						default:
							assert(!"unsupported expression opcode.");
					}
				}
				sections[current_section].expressions.emplace_back(std::move(e));
				break;
			}


			case REC_LINE: break;
			case REC_DEBUG: {
				uint16_t size = read_16(iter);
				iter += size;
				break;		
			}

		}
	}
}


/*
 * n.b -- UDATA and PAGE0 are ref only, therefore no data is generated.
 * as a special case for UDATA (but not PAGE0) have a flag so it will be 0-filled and generate data?
 *
 */
void init() {

	sections.resize(5);

	sections[SECT_PAGE0].number = SECT_PAGE0;
	sections[SECT_PAGE0].flags = SEC_DATA | SEC_NONAME | SEC_DIRECT | SEC_REF_ONLY;
	sections[SECT_PAGE0].name = "page0";

	sections[SECT_CODE].number = SECT_CODE;
	sections[SECT_CODE].flags = SEC_NONAME;
	sections[SECT_CODE].name = "code"; 

	sections[SECT_KDATA].number = SECT_KDATA;
	sections[SECT_KDATA].flags = SEC_DATA | SEC_NONAME;
	sections[SECT_KDATA].name = "kdata"; 

	sections[SECT_DATA].number = SECT_DATA;
	sections[SECT_DATA].flags = SEC_DATA | SEC_NONAME;
	sections[SECT_DATA].name = "data"; 

	sections[SECT_UDATA].number = SECT_UDATA;
	sections[SECT_UDATA].flags = SEC_DATA | SEC_NONAME | SEC_REF_ONLY;
	sections[SECT_UDATA].name = "udata"; 

	/*
	 * For each section, [the linker] creates three symbols, 
	 * _ROM_BEG_secname, _BEG_secname and _END_secname, which
	 * correspond to the rom location and the execution beginning
	 * and end of the section. These will be used more in the next
	 * two sections of code.
	 */

	// n.b - only for pre-defined sections [?], skip the _ROM_BEG_* symbols...

	static std::string names[] = {
			"_BEG_PAGE0", "_END_PAGE0",
			"_BEG_CODE", "_END_CODE",
			"_BEG_KDATA", "_END_KDATA",
			"_BEG_DATA", "_END_DATA",
			"_BEG_UDATA", "_END_UDATA",
	};

	for (int i = 0; i < 5; ++i) {

		// begin is 0.
		symbol s;
		s.name = names[i * 2];
		s.section = i;
		s.type = S_REL;
		s.flags = SF_DEF | SF_GBL;
		symbol_map.emplace(s.name, i * 2);
		symbols.emplace_back(s);

		// end is undefined...
		s.name = names[i * 2 + 1];
		s.section = i * 2 + 1; // symbol number.
		s.type = S_UND;
		s.flags = 0;

		symbol_map.emplace(s.name, i * 2 + 1);
		symbols.emplace_back(s);

	}

}


void generate_end() {

/*
	const std::string names[] = {
			"_END_PAGE0",
			"_END_CODE",
			"_END_KDATA"
			"_END_DATA"
			"_END_UDATA"	
	};
*/
	for (int i = 0; i < 5; ++i) {
		symbol s;
		s.section = i;
		s.type = S_REL;
		s.flags = SF_DEF | SF_GBL;
		s.offset = sections[i].size; // data.size() doesn't word w/ ref_only

		symbols[i * 2 + 1] = s;
	}
}


std::vector<omf::segment> omf_segments;


template<class T>
void append(std::vector<T> &to, std::vector<T> &from) {
	to.insert(to.end(),
		std::make_move_iterator(from.begin()),
		std::make_move_iterator(from.end())
	);
}


template<class T>
void append(std::vector<T> &to, const std::vector<T> &from) {
	to.insert(to.end(),
		from.begin(),
		from.end()
	);
}

template<class T>
void append(std::vector<T> &to, unsigned count, const T& value) {
	to.insert(to.end(),
		count,
		value
	);
}

/*
 * convert a wdc expression to an omf reloc/interseg record.
 *
 */
void to_omf(const expression &e, omf::segment &seg) {
	if (e.stack.empty()) return; //?

	if (e.stack.size() == 1 && e.stack[0].tag == OP_VAL) {
		uint32_t value = e.stack[0].value;

		for(int i = 0; i < e.size; ++i, value >>= 8)
			seg.data[e.offset + i] = value & 0xff;
		return;
	}

	if (e.stack.size() == 1 && e.stack[0].tag == OP_LOC) {
		auto &loc = e.stack[0];

		uint32_t value = loc.value;

		if (loc.section == 0) {
			warnx("Unable to relocate (invalid segment).");
			flags._errors++;
			return;
		}
		if (loc.section == seg.segnum) {
			omf::reloc r;
			r.size = e.size;
			r.offset = e.offset;
			r.value = value;

			// also store value in data
			for (int i = 0; i < e.size; ++i, value >>= 8)
				seg.data[e.offset + i] = value & 0xff;

			seg.relocs.emplace_back(r);
		} else {
			omf::interseg r;
			r.size = e.size;
			r.offset = e.offset;
			r.segment = loc.section;
			r.segment_offset = loc.value;

			seg.intersegs.emplace_back(r);
		}
		return;
	}

	if (e.stack.size() == 3
		&& e.stack[0].tag == OP_LOC
		&& e.stack[1].tag == OP_VAL
		&& (e.stack[2].tag == OP_SHL || e.stack[2].tag == OP_SHR)) {

		auto &loc = e.stack[0];
		auto &shift = e.stack[1];
		auto &op = e.stack[2];



		if (shift.value > 24) {
			warnx("shift %d", shift.value);
			for(int i = 0; i < e.size; ++i)
				seg.data[e.offset +i] = 0;
			return;
		}


		if (loc.section == 0) {
			warnx("Unable to relocate expression (invalid segment).");
			flags._errors++;
			return;
		}

		uint32_t value = loc.value;
		uint8_t shift_value = shift.value;
		if (op.tag == OP_SHR) {
			value >>= shift_value;
			shift_value = -shift_value;
		} else {
			value <<= shift_value;
		}

		if (loc.section == seg.segnum) {
			omf::reloc r;
			r.size = e.size;
			r.offset = e.offset;
			r.value = loc.value;
			r.shift = shift_value;

			// also store value in data
			for (int i = 0; i < e.size; ++i, value >>= 8)
				seg.data[e.offset + i] = value & 0xff;

			seg.relocs.emplace_back(r);
		} else {
			omf::interseg r;
			r.size = e.size;
			r.offset = e.offset;
			r.segment = loc.section;
			r.segment_offset = loc.value;
			r.shift = shift_value;

			seg.intersegs.emplace_back(r);
		}
		return;
	}

	warnx("Relocation expression too complex.");
	flags._errors++;
	return;

}


void build_omf_segments() {


	std::vector< std::pair<unsigned, uint32_t> > remap;

	remap.resize(sections.size());


#if 0
	if (!flags._X) {
		// create an expressload segments.
		// (should verify it's expressable later...)
		omf::segment seg;
		seg.segnum = omf_segments.size()+ 1;
		seg.kind = 0x8001; // dynamic data segment
		seg.segname = "~ExpressLoad";

		omf_segments.emplace_back(std::move(seg));

	}
#endif


	// if data + code can fit in one bank, merge them
	// otherwise, merge all data sections and 1 omf segment
	// per code section.

	// code is next segment...
	unsigned code_segment = 0;
	unsigned data_segment = 0;
	{
		omf_segments.emplace_back();
		auto &seg = omf_segments.back();

		code_segment = data_segment = seg.segnum = omf_segments.size();
		seg.kind = 0x0000; // static code segment.

		auto &s = sections[SECT_CODE];

		remap[s.number] = std::make_pair(code_segment, 0);
		append(seg.data, s.data);
		s.data.clear();
	}
	

	uint32_t total_code_size = 0;
	uint32_t total_data_size = 0;
	for (const auto &s : sections) {
		if (s.flags & SEC_REF_ONLY) continue;
		if (s.flags & SEC_DATA) {
			total_data_size += s.size;
		} else {
			total_code_size += s.size;
		}
	}

	// add in UDATA
	total_data_size += sections[SECT_UDATA].size;

	if (total_data_size + sections[SECT_CODE].size > 0xffff) {

		omf_segments.emplace_back();

		auto &seg = omf_segments.back();
		data_segment = seg.segnum = omf_segments.size();
		seg.kind = 0x0001; // static data segment.
	}

	//omf::segment &code_seg = omf_segments[code_segment-1];
	omf::segment &data_seg = omf_segments[data_segment-1];


	// KDATA, DATA, UDATA, other segment order.
	for (auto &s : sections) {
		if (s.flags & SEC_REF_ONLY) continue;
		if ((s.flags & SEC_DATA) == 0) continue;

		remap[s.number] = std::make_pair(data_segment, data_seg.data.size());

		append(data_seg.data, s.data);
		s.data.clear();
	}

	// add in UDATA
	{
		auto &s = sections[SECT_UDATA];
		remap[s.number] = std::make_pair(data_segment, data_seg.data.size());
		append(data_seg.data, s.size, (uint8_t)0);
	}

	// for all other sections, create a new segment.
	for (auto &s : sections) {
		if (s.flags & SEC_REF_ONLY) continue;
		if (s.flags & SEC_DATA) continue;
		if (s.number == SECT_CODE) continue;

		omf::segment seg;
		seg.segnum = omf_segments.size() + 1;
		seg.kind = 0x0000; // static code.
		seg.data = std::move(s.data);
		seg.segname = s.name;
		s.data.clear();

		remap[s.number] = std::make_pair(seg.segnum, 0);

		omf_segments.emplace_back(std::move(seg));
	}


	// add a stack segment at the end
	if (flags._S) {
		auto &s = sections[SECT_PAGE0];

		// create stack/dp segment.
		uint32_t size = s.size;
		if (size) {
			// ????
			size = (size + 255) & ~255;

			omf::segment seg;
			seg.segnum = omf_segments.size() + 1;
			seg.kind = 0x12; // static dp/stack segment.
			seg.data.resize(size, 0);
			seg.loadname = "~Stack";
			omf_segments.emplace_back(std::move(seg));

			// remap SECT_PAGE0...
			remap[s.number] = std::make_pair(seg.segnum, 0);

		} else {
			warnx("page0 is 0 sized. Stack/dp segment not created.");
		}
	}

	// now adjust all the expressions, simplify, and convert to reloc records.
	for (auto &s :sections) {
		for (auto &e : s.expressions) {

			for (auto &t : e.stack) {
				if (t.tag == OP_LOC) {
					const auto &x = remap[t.section];
					t.section = x.first;
					t.value += x.second;
				}
			}
			simplify_expression(e);
			to_omf(e, omf_segments[s.number-1]);
		}
	}

	// and we're done...

}




bool one_file(const std::string &name) {

	int fd = open(name.c_str(), O_RDONLY | O_BINARY);
	if (fd < 0) {
		warn("Unable to open %s", name.c_str());
		return false;
	}
	bool rv = false;

	Header h;
	ssize_t ok;

	ok = read(fd, &h, sizeof(h));
	if (ok != sizeof(h)) {
		warnx("Invalid object file: %s", name.c_str());
		close(fd);
		return false;
	}

	le_to_host(h.magic);
	le_to_host(h.version);
	le_to_host(h.filetype);

	if (h.magic != MOD_MAGIC || h.version != MOD_VERSION || h.filetype < MOD_OBJECT || h.filetype > MOD_LIBRARY) {
		warnx("Invalid object file: %s", name.c_str());
		close(fd);
		return false;
	}

	if (h.filetype == MOD_LIBRARY) {
		warnx("%s is a library", name.c_str());
		close(fd);
		// todo -- add to library list...
		return true;
	}

	//
	rv = true;
	lseek(fd, 0, SEEK_SET);
	for(;;) {
		Mod_head h;

		ok = read(fd, &h, sizeof(h));
		if (ok == 0) break; // eof.

		rv = false;
		if (ok < sizeof(h)) {
			warnx("Invalid object file: %s", name.c_str());
			break;
		}

		le_to_host(h.h_magic);
		le_to_host(h.h_version);
		le_to_host(h.h_filtyp);
		le_to_host(h.h_namlen);
		le_to_host(h.h_recsize);
		le_to_host(h.h_secsize);
		le_to_host(h.h_symsize);
		le_to_host(h.h_optsize);
		le_to_host(h.h_tot_secs);
		le_to_host(h.h_num_secs);
		le_to_host(h.h_num_syms);

		assert(h.h_magic == MOD_MAGIC);
		assert(h.h_version == 1);
		assert(h.h_filtyp == 1);


		std::string module_name;
		{
			// now read the name (h_namlen includes 0 terminator.)
			std::vector<char> tmp;
			tmp.resize(h.h_namlen);
			ok = read(fd, tmp.data(), h.h_namlen);
			if (ok != h.h_namlen) {
				warnx("Invalid object file: %s", name.c_str());
				break;
			}
			module_name.assign(tmp.data());
		}

		std::vector<uint8_t> record_data;
		std::vector<uint8_t> symbol_data;
		std::vector<uint8_t> section_data;

		record_data.resize(h.h_recsize);
		ok = read(fd, record_data.data(), h.h_recsize);
		if (ok != h.h_recsize) {
			warnx("Truncated object file: %s", name.c_str());
			break;
		}

		section_data.resize(h.h_secsize);
		ok = read(fd, section_data.data(), h.h_secsize);
		if (ok != h.h_secsize) {
			warnx("Truncated object file: %s", name.c_str());
			break;
		}

		symbol_data.resize(h.h_symsize);
		ok = read(fd, symbol_data.data(), h.h_symsize);
		if (ok != h.h_symsize)  {
			warnx("Truncated object file: %s", name.c_str());
			break;
		}

		if (flags._v) {
			printf("Processing %s:%s\n", name.c_str(), module_name.c_str());
		}

		// should probably pass in name and module....
		one_module(record_data, section_data, symbol_data);
		rv = true;
	}


	close(fd);
	return rv;
}

void help() {
	exit(0);
}

void usage() {
	exit(EX_USAGE);
}

int main(int argc, char **argv) {

	std::vector<std::string> _l;
	std::vector<std::string> _L;


	int c;
	while ((c = getopt(argc, argv, "vCXL:l:o:")) != -1) {
		switch(c) {
			case 'v': flags._v = true; break;
			case 'X': flags._X = true; break;
			case 'C': flags._C = true; break;
			case 'o': flags._o = optarg; break;
			case 'l': _l.emplace_back(optarg); break;
			case 'L': _L.emplace_back(optarg); break;
			case 'h': help(); break;
			case ':':
			case '?':
			default:
				usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) usage();

	init();

	for (int i = 0 ; i < argc; ++i) {
		if (!one_file(argv[i])) flags._errors++;
	}

	//
	simplify();

	// for each undefined, try to find it in a library...
	// ... except for the _BEG / _END symbols!

	for (const auto & s : undefined_symbols) {
		printf("%s\n", s.c_str());
	}

	generate_end();


	if (flags._v) {
		for (const auto &s : sections) {
			//if (s.flags & SEC_REF_ONLY) continue;
			printf("section %3d %-20s $%04x $%04x\n",
				s.number, s.name.c_str(), (uint32_t)s.data.size(), s.size);
		}
		fputs("\n", stdout);
	}

	build_omf_segments();

	if (flags._v) {
		for (const auto &s : omf_segments) {
			printf("segment %3d %-20s $%04x\n",
				s.segnum, s.segname.c_str(), (uint32_t)s.data.size());

			for (auto &r : s.relocs) {
				printf("  %02x %02x %06x %06x\n",
					r.size, r.shift, r.offset, r.value);
			}
			for (auto &r : s.intersegs) {
				printf("  %02x %02x %06x %02x %04x %06x\n",
					r.size, r.shift, r.offset, r.file, r.segment, r.segment_offset);
			}
		}
	}

	if (flags._o.empty()) flags._o = "out.omf";
	void save_omf(std::vector<omf::segment> &segments, bool expressload, const std::string &path);

	save_omf(omf_segments, !flags._X, flags._o);


}

