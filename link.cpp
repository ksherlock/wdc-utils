/*
 * WDC to OMF Linker.
 *
 *
 */

#include <sysexits.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <assert.h>
#include <stdio.h>

#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <array>

#include "obj816.h"
#include "expression.h"




struct section {
	std::string name;
	uint8_t flags = 0;
	uint32_t org = 0;
	uint32_t size = 0;

	unsigned number = -1;
	std::vector<uint8_t> data;
	std::vector<expression> expressions;
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
		if (s.number <= SECT_UDATA) continue;

		auto iter = section_map.find(s.name);
		if (iter == section_map.end()) {
			int virtual_section = sections.size();
			remap_section[s.number] = virtual_section;
			s.number = virtual_section;
			sections.emplace_back(s);
			symbol_map.emplace(s.name, virtual_section);
		} else {
			const auto &ss = sections[iter->second];
			assert(ss.flags == s.flags); // check org????
			remap_section[s.number] = iter->second;
			s.number = iter->second;
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
					assert(ss.type == ss.type && ss.flags == s.flags && ss.section == s.section && ss.offset == s.offset);
				}
			}

		}

	}


	auto iter = data.begin();
	for(;;) {
		uint8_t op = read_8(iter);
		if (op == REC_END) return;

		++iter;
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
			"_BEG_KDATA", "_END_KDATA"
			"_BEG_DATA", "_END_DATA"
			"_BEG_UDATA", "_END_UDATA"
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





void help() {
	exit(0);
}

void usage() {
	exit(EX_USAGE);
}

int main(int argc, char **argv) {

	std::string _o;
	bool _C = false;
	bool _X = false;

	std::vector<std::string> _l;
	std::vector<std::string> _L;


	int c;
	while ((c = getopt(argc, argv, "CXL:l:o:")) != -1) {
		switch(c) {
			case 'X': _X = true; break;
			case 'C': _C = true; break;
			case 'o': _o = optarg; break;
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

}

