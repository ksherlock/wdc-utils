#include <string>
#include <err.h>
#include <unistd.h>
#include <sysexits.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <type_traits>
#include <vector>
#include <algorithm>
#include <iterator>

#include "obj816.h"
#include "disassembler.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif


/* not documented */
#ifndef SEC_DATA
#define SEC_DATA 0x80
#endif


struct {
	bool _S = false;
	bool _g = false;
} flags;


enum class endian {
	little = __ORDER_LITTLE_ENDIAN__,
	big = __ORDER_BIG_ENDIAN__,
	native = __BYTE_ORDER__
};



//extern unsigned init_flags(bool longM, bool longX);
//void dump(const std::vector<uint8_t> &data, unsigned &pc);
//extern void disasm(const std::vector<uint8_t> &data, unsigned &flags, unsigned &pc);



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

void usage() {
	exit(EX_USAGE);
}

#pragma pack(push, 1)
struct Header {
	uint32_t h_magic;				/* magic number for detection */
	uint16_t h_version;			/* version number of object format */
	uint8_t h_filtyp;				/* file type, object or library */
};

#pragma pack(pop)




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


struct symbol {
	std::string name;
	uint8_t type = S_UND;
	uint8_t flags = 0;
	uint8_t section = 0xff;
	uint32_t offset = 0;
};

struct section {
	std::string name;
	uint8_t number = 0;
	uint8_t flags = 0;
	uint32_t size = 0;
	uint32_t org = 0;

	// for disassembly tracking...
	uint32_t pc = 0;
	std::vector<symbol> symbols;
};

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

		if (sections.size() < s.number) sections.resize(s.number);
		if (sections.size() == s.number) sections.emplace_back(std::move(s));
		else sections[s.number] = std::move(s);
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

std::vector<symbol> labels_for_section(const std::vector<symbol> &symbols, unsigned section) {
	std::vector<symbol> out;
	std::copy_if(symbols.begin(), symbols.end(), std::back_inserter(out), 
		[section](const symbol &s) { return s.section == section  && s.type != S_UND; }
	);

	std::sort(out.begin(), out.end(), [](const symbol &a, const symbol &b){
		return a.offset > b.offset;
	});

	return out;
}

symbol find_symbol(const std::vector<symbol> &symbols, unsigned section, unsigned offset) {
	auto iter = std::find_if(symbols.begin(), symbols.end(), [section, offset](const symbol &s){
		return s.section == section && s.offset == offset && s.type != S_UND;
	});
	if (iter != symbols.end()) return *iter;
	return symbol{};
}


void place_labels(std::vector<symbol> &labels, uint32_t pc) {
	while (!labels.empty()) {
		auto &label = labels.back();
		if (label.offset > pc) return;
		if (label.offset == pc) {
			printf("%s:\n", label.name.c_str());
		} else {
			warnx("Unable to place label %s (offset $%04x)", label.name.c_str(), label.offset);
		}
		labels.pop_back();
	}
}

void emit(const std::string &label) {
	fputs(label.c_str(), stdout);
	//fputc(':', stdout); // special case.
	fputc('\n', stdout);
}

void emit(const std::string &label, const std::string &opcode) {
	fputs(label.c_str(), stdout);

	int column = label.length();

	if (!opcode.empty()) {
		do {
			putc(' ', stdout);
			column++;
		} while (column < 20);
		fputs(opcode.c_str(), stdout);
	}
	fputc('\n', stdout);
}



void emit(const std::string &label, const std::string &opcode, const std::string &operand) {
	fputs(label.c_str(), stdout);
	int column = label.length();

	if (!opcode.empty()) {

		do {
			putc(' ', stdout);
			column++;
		} while (column < 20);
		fputs(opcode.c_str(), stdout);
		column += opcode.length();
	}

	if (!operand.empty()) {
		do {
			putc(' ', stdout);
			column++;
		} while (column < 30);
		fputs(operand.c_str(), stdout);
		column += operand.length();
	}
	fputc('\n', stdout);
	return;
}

static std::string to_x(uint32_t x, unsigned bytes, char prefix = 0) {
	std::string s;
	char buffer[16];
	if (prefix) s.push_back(prefix);

	if (x >= 0xff && bytes < 4) bytes = 4;
	if (x >= 0xffff && bytes < 6) bytes = 6;
	if (x >= 0xffffff && bytes < 8) bytes = 8;

	memset(buffer, '0', sizeof(buffer));
	int i = 16;
	while (x) {
		buffer[--i] = "0123456789abcdef"[x & 0x0f];
		x >>= 4;
	}

	s.append(buffer + 16 - bytes, buffer + 16);

	return s;
}

bool dump_obj(const char *name, int fd)
{
	static const char *kSections[] = { "PAGE0", "CODE", "KDATA", "DATA", "UDATA" };
	static const char *kTypes[] = { "S_UND", "S_ABS", "S_REL", "S_EXP", "S_REG", "S_FREG" };

	Mod_head h;
	ssize_t ok;

	ok = read(fd, &h, sizeof(h));
	if (ok == 0) return false;

	if (ok != sizeof(h))
		errx(EX_DATAERR, "%s is not an object file", name);


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

	// now read the name (h_namlen includes 0 terminator.)
	std::vector<char> oname;
	oname.resize(h.h_namlen);
	ok = read(fd, oname.data(), h.h_namlen);
	if (ok != h.h_namlen) errx(EX_DATAERR, "%s", name);


	/*
	printf("name: %s\n", oname.data());
	printf("record size    : $%04x\n", h.h_recsize);
	printf("section size   : $%04x\n", h.h_secsize);
	printf("symbol size    : $%04x\n", h.h_symsize);
	printf("option size    : $%04x\n", h.h_optsize);
	printf("number sections: $%04x\n", h.h_num_secs);
	printf("number symbols : $%04x\n", h.h_num_syms);
	*/

	// records [until record_eof]

	std::vector<uint8_t> data;
	data.resize(h.h_recsize);
	ok = read(fd, data.data(), h.h_recsize);
	if (ok != h.h_recsize) errx(EX_DATAERR, "%s records truncated", name);


	std::vector<uint8_t> section_data;
	section_data.resize(h.h_secsize);
	ok = read(fd, section_data.data(), h.h_secsize);
	if (ok != h.h_secsize) errx(EX_DATAERR, "%s sections truncated", name);


	std::vector<uint8_t> symbol_data;
	symbol_data.resize(h.h_symsize);
	ok = read(fd, symbol_data.data(), h.h_symsize);
	if (ok != h.h_symsize) errx(EX_DATAERR, "%s symbols truncated", name);

	std::vector<symbol> symbols = read_symbols(symbol_data);
	std::vector<section> sections = read_sections(section_data);

	disassembler d;

	uint8_t op = REC_END;
	unsigned section = SECT_CODE; // default section = CODE
	unsigned line = 0;
	std::string file;

	if (sections.size() < 5) sections.resize(5);
	for (int i = 0; i < 5; ++i) 
		if (sections[i].name.empty()) sections[i].name = kSections[i];


	/* custom sections... */
	if (sections.size() > 5) {

		printf("\n");
		for (auto iter = sections.begin() + 5; iter != sections.end(); ++iter) {
			std::string attr;

			bool comma = false;
			int flags = iter->flags;
			if (flags & SEC_OFFSET) {
				if (comma) attr += ", ";
				attr += "SEC_OFFSET $";
				attr += to_x(iter->org, 4);
				comma = true;
			}
			if (flags & SEC_INDIRECT) {
				if (comma) attr += ", ";
				attr += "SEC_INDIRECT $";
				attr += to_x(iter->org, 4);
				comma = true;
			}
#define _(x) if (flags & x) { if (comma) attr += ", "; attr += #x; comma = true; }
			_(SEC_STACKED)
			_(SEC_REF_ONLY)
			_(SEC_CONST)
			_(SEC_DIRECT)
			_(SEC_NONAME)
			_(SEC_DATA)
#undef _

			emit(iter->name, "section", attr);
		}
		printf("\n");
	}


	emit("", "MODULE", std::string(oname.data()));
	printf("\n");

	d.set_pc(0);
	d.set_code(true);


	bool newline = false;

	for (const auto &s : symbols) {
		if (s.type != S_UND) continue;
		emit("", "extern", s.name);
		newline = true;
	}

	for (const auto &s : symbols) {
		if (s.type == S_UND) continue;
		if (s.type == S_ABS) continue; // ? equ/gequ
		// only if s.type == S_REL?
		sections[s.section].symbols.push_back(s);

		if (s.flags & SF_GBL) {
			emit("", "public", s.name);
			newline = true;
		}
	}
	if (newline) printf("\n");

	for (auto &section : sections) {
		std::sort(section.symbols.begin(), section.symbols.end(), [](const symbol &a, const symbol &b){
			return a.offset > b.offset;
		});
	}


	// print out any reference-only sections and symbols.
	for (auto &section : sections) {
		if ((section.flags & SEC_REF_ONLY) == 0) continue;
		if (section.size == 0) continue;

		emit("", section.name);

		if (section.org) emit("",".org", to_x(section.org,4,'$'));
		uint32_t pc = section.org;
		
		auto &symbols = section.symbols;
		while (!symbols.empty()) {
			auto &s = symbols.back();

			if (s.offset > pc) {
				emit("","ds", std::to_string(s.offset - pc));
				pc = s.offset;
			}
			emit(s.name);
			symbols.pop_back();
		}

		if (pc < section.size)
			emit("","ds",std::to_string(section.size - pc));

		emit("","ends");
		printf("\n");
	}

	//
	// print equates.
	// doesn the section matter?
	newline = false;
	for (const auto &s : symbols) {
		if (s.type == (ST_EQU << 4) + S_ABS) {
			emit(s.name,"gequ", to_x(s.offset, 4, '$'));
			newline = true;
		}
	}
	if (newline) printf("\n");


	d.set_label_callback([&section, &sections](int32_t offset) -> int32_t {
		auto &symbols = sections[section].symbols;

		if (offset >= 0) {

			while(!symbols.empty()) {
				auto &s = symbols.back();
				if (s.offset > offset) return s.offset;
				if (s.offset == offset) {
					emit(s.name);
				} else {
					std::string tmp = "; ";
					tmp += s.name;
					tmp += " = $";
					tmp += to_x(s.offset, 4);
					emit("", tmp);
				}
				symbols.pop_back();
				continue;
			}
		}
		if (symbols.empty()) return -1;
		return symbols.back().offset;
	});

	//std::vector<symbol> labels = labels_for_section(symbols, section);


	emit("", "CODE", "; section 1");

	auto iter = data.begin();
	while (iter != data.end()) {

		//place_labels(labels, d.pc());

		op = read_8(iter);
		if (op == 0) break;
		if (op < 0xf0) {
			auto end = iter + op;
			while (iter != end) {
				d(*iter++);
				//place_labels(labels, d.pc());
			}
			continue;
		}

		switch(op) {

			case REC_RELEXP:
			case REC_EXPR:
				{

					// todo -- pass the relative flag to ()
					// so it can verify it's appropriate for the opcode.

					// todo -- move all this stuff to a separate function.
					uint8_t size = read_8(iter);

					char buffer[32];


					std::vector<std::string> stack;

					// todo -- need to keep operation for precedence?
					// this ignores all precedence...

					for(;;) {
						uint8_t op = read_8(iter);
						if (op == OP_END) break;
						switch (op) {
							case OP_LOC: {
								uint8_t section = read_8(iter);
								uint32_t offset = read_32(iter);

								std::string name;

								symbol s = find_symbol(symbols, section, offset);
								if (s.type) {
									name = s.name;
								} else {

									if (section < sizeof(kSections) / sizeof(kSections[0]))
										name = kSections[section];
									else {
										snprintf(buffer, sizeof(buffer), "section%d", section);
										name = buffer;
									}
								}
								if (offset) {
									snprintf(buffer, sizeof(buffer), "+$%04x", offset);
									name += buffer;
								}
								stack.push_back(name);

								break;
							}

							case OP_VAL:
								snprintf(buffer, sizeof(buffer), "$%04x", read_32(iter));
								stack.push_back(buffer);
								break;

							case OP_SYM: {
								uint16_t symbol = read_16(iter);
								if (symbol < symbols.size()) stack.push_back(symbols[symbol].name);
								else {
									snprintf(buffer, sizeof(buffer), "symbol $%02x", symbol);
									stack.push_back(buffer);
								}
								break;
							}

							// unary operatos
							case OP_NOT:
							case OP_NEG:
							case OP_FLP: {
								static const char *ops[] = {
									".NOT.", "-", "\\"
								};

								if (stack.empty()) errx(EX_DATAERR, "%s : stack underflow error", name);
								std::string a = std::move(stack.back()); stack.pop_back();
								std::string b(ops[op-10]);
								stack.emplace_back(b + a);
								break;
							}

							// binary operators
							case OP_SHR: 
							case OP_SHL:
							case OP_ADD: 
							case OP_SUB: {
								static const char *ops[] = {
									"**", "*", "/", ".MOD.", ">>", "<<", "+", "-", "&", "|", "^", "=", ">", "<"

								};
								if (stack.size() < 2) errx(EX_DATAERR, "%s : stack underflow error", name);
								std::string a = std::move(stack.back()); stack.pop_back();
								std::string b = std::move(stack.back()); stack.pop_back();
								stack.emplace_back(b + ops[op-20] + a);
								break;
							}
							default:
								errx(EX_DATAERR, "%s: unknown expression opcode %02x", name, op);

						}
					}
					if (stack.size() != 1) errx(EX_DATAERR, "%s stack overflow error.", name);
					d(stack.front(), size);
				}
				break;

			case REC_DEBUG:
				{
					static const char *debugs[] = {
						"D_C_FILE",
						"D_C_LINE",
						"D_C_SYM",
						"D_C_STAG",
						"D_C_ETAG",
						"D_C_UTAG",
						"D_C_MEMBER",
						"D_C_EOS",
						"D_C_FUNC",
						"D_C_ENDFUNC",
						"D_C_BLOCK",
						"D_C_ENDBLOCK",
						"D_LONGA_ON",
						"D_LONGA_OFF",
						"D_LONGI_ON",
						"D_LONGI_OFF",
					};

					d.flush();
					uint16_t size = read_16(iter);

					auto end = iter + size;
					while (iter < end) {
						uint8_t op = read_8(iter);
						switch(op) {
							case D_LONGA_ON:
								d.set_m(true);
								emit("", "longa", "on");
								break;
							case D_LONGA_OFF:
								d.set_m(false);
								emit("", "longa", "off");
								break;
							case D_LONGI_ON:
								d.set_x(true);
								emit("", "longi", "on");
								break;
							case D_LONGI_OFF:
								d.set_x(false);
								emit("", "longi", "off");
								break;
							case D_C_FILE: {
								file = read_cstring(iter);
								line = read_16(iter);
								std::string tmp = file + ", " + std::to_string(line);
								emit("", ".file", tmp);
								break;
							}
							case D_C_LINE: {
								line = read_16(iter);
								emit("",".line", std::to_string(line));
								break;
							}
							case D_C_BLOCK: {
								uint16_t block = read_16(iter);
								emit("",".block", std::to_string(line));
								break;
							}
							case D_C_ENDBLOCK: {
								uint16_t line = read_16(iter);
								emit("",".endblock", std::to_string(line));
								break;
							}
							case D_C_FUNC: {
								uint16_t arg = read_16(iter);
								emit("",".function", std::to_string(line));
								break;								
							}
							case D_C_ENDFUNC: {
								uint16_t line = read_16(iter);
								uint16_t local_offset = read_16(iter);
								uint16_t arg_offset = read_16(iter);
								std::string tmp;
								tmp = std::to_string(line) + ", "
									+ std::to_string(local_offset) + ", "
									+ std::to_string(arg_offset);
								emit("",".endfunc", tmp);
								break;
							}

							// etag? reserved for enums but not actually used?
							case D_C_STAG:
							case D_C_ETAG:
							case D_C_UTAG: {
								const char *kOpNames[] = { ".stag", ".etag", ".utag" };
								const char *opname = kOpNames[op - D_C_STAG];

								std::string name = read_cstring(iter);
								uint16_t size = read_16(iter);
								uint16_t tag = read_16(iter);

								std::string tmp;
								tmp = name + ", " + std::to_string(size) + ", " + std::to_string(tag);
								emit("", opname, tmp);
								break;
							}
							case D_C_EOS: {
								emit("", ".eos");
								break; 
							}

							case D_C_MEMBER:
							case D_C_SYM: {
								// warning - i don't fully understand this one..
								std::string name = read_cstring(iter);
								uint8_t version = read_8(iter); //???
								uint32_t value;
								if (version == 0) value = read_16(iter); // symbol
								if (version == 1) value = read_32(iter); // numeric value.
								assert(version == 0 || version == 1);
								uint32_t type = read_32(iter);
								uint8_t klass = read_8(iter);
								uint16_t size = read_16(iter);


								const char *opname = ".sym";
								if (op == D_C_MEMBER) opname = ".member";


								std::string attr;

								if (version == 0) {
									std::string svalue;
									svalue = symbols[value].name;

									attr = name + ", " + svalue;
								}

								if (version == 1) {
									attr = name + ", " + std::to_string(value);
								}

								attr += ", " + std::to_string(type);
								attr += ", " + std::to_string(klass);
								attr += ", " + std::to_string(size);


								/*
								 * type bits 1 ... 5 are T_xxxx
								 * then 3 bits of DT_xxx (repeatedly)
								 *
								 * eg, char ** = (DT_PTR << 11) + (DT_PTR << 8) + T_CHAR
								 */
								int t = type & 0x1f;
								if ((t == T_STRUCT) || (t == T_UNION)) {
									uint16_t tag = read_16(iter);
									attr += ", " + std::to_string(tag);
								}

								// need to do it until t == 0 for
								// multidimensional arrays.
								for ( t = type >> 5; t; t >>= 3) {
									if ((t & 0x07) == DT_ARY) {
										uint16_t dim = read_16(iter);
										attr += ", " + std::to_string(dim);
									}
								}


								emit("", opname, attr);

								break;
							}

							default:
								errx(EX_DATAERR, "%s: unknown debug opcode %02x (%d)", name, op, op);
								break;

						}
					}
				}
				break;

			case REC_SECT: {
				d.flush();
				uint8_t sec = read_8(iter);
				//printf("\t.sect\t%d\n", sec);
				if (sec != section) {

					if (sec >= sections.size()) {
						warnx("Undefined section %d", sec);
					}

					const auto &s = sections[sec];
					emit("", "ends", std::string("; end section ") + std::to_string(section));
					printf("\n");
					emit("", s.name, std::string("; section ") + std::to_string(sec));

					sections[section].pc = d.pc();
					section = sec;
					d.set_pc(s.pc);
					d.recalc_next_label();
					d.set_code((s.flags & SEC_DATA) == 0);
				}
				break;
			}

			case REC_ORG: {
				d.flush();
				uint32_t org = read_32(iter);
				emit("", ".org", to_x(org, 4, '$'));
				d.set_pc(org);
				break;
			}

			case REC_SPACE: {
				d.flush();
				uint16_t count = read_16(iter);
				// todo -- need to coordinate with label printer/disassembler.
				emit("", "ds", to_x(count, 4, '$'));
				d.set_pc(d.pc() + count);
				break;
			}

			case REC_LINE: {
				// bump line counter, no argument.
				d.flush();
				++line;
				break;
			}
			default:
				d.flush();
				errx(EX_DATAERR, "%s: unknown opcode %02x", name, op);
		}
	}
	// dump any unfinished business.
	d.flush();

	//place_labels(labels, d.pc());
	/*
	for(auto &label : labels) {
		warnx("Unable to place label %s (offset $%04x)", label.name.c_str(), label.offset);
	}
	*/
	emit("", "ends");
	printf("\n");
	emit("", "endmod");
	printf("\n");

	if (iter != data.end() || op != REC_END) errx(EX_DATAERR, "%s records ended early", name);


	if (flags._S) {
		printf("; symbols\n");
		for (auto &s : symbols) {
			printf("; %-20s %02x %02x %02x %08x\n",
				s.name.c_str(), s.type, s.flags, s.section, s.offset);
		}
	}


	return true;
}



void dump_lib(const char *name, int fd)
{
	Lib_head h;
	ssize_t ok;

	ok = read(fd, &h, sizeof(h));
	if (ok != sizeof(h))
		errx(EX_DATAERR, "%s is not an object file", name);


	le_to_host(h.l_magic);
	le_to_host(h.l_version);
	le_to_host(h.l_filtyp);
	le_to_host(h.l_modstart);
	le_to_host(h.l_numsyms);
	le_to_host(h.l_symsize);
	le_to_host(h.l_numfiles);

	assert(h.l_magic == MOD_MAGIC);
	assert(h.l_version == 1);
	assert(h.l_filtyp == 2);

	printf("modstart      : $%04x\n", h.l_modstart);
	printf("number symbols: $%04x\n", h.l_numsyms);
	printf("number files  : $%04x\n", h.l_numfiles);

	printf("\n");
	std::vector<uint8_t> data;
	long count = h.l_modstart - sizeof(h);
	if (count < 0) errx(EX_DATAERR, "%s", name);
	data.reserve(count);
	ok = read(fd, data.data(), count);
	if (ok != count) errx(EX_DATAERR, "%s truncated", name);


	// files
	auto iter = data.begin();
	for (int i = 0; i < h.l_numfiles; ++i) {
		uint16_t file_number = read_16(iter);
		std::string s = read_pstring(iter);
		printf("$%02x %s\n", file_number, s.c_str());
	}
	printf("\n");

	// symbols
	auto name_iter = iter + h.l_numsyms * 8;
	for (int i = 0; i < h.l_numsyms; ++i) {
		uint16_t name_offset = read_16(iter);
		uint16_t file_number = read_16(iter);
		uint32_t offset = read_32(iter);
		std::string name = read_pstring(name_iter);

		printf("symbol       : $%04x %s\n", i, name.c_str());
		//printf("name offset: %02x\n", name_offset);
		printf("file_number  : $%02x\n", file_number);
		printf("module offset: $%04x\n", offset); 
	}
	printf("\n");

}

void dump(const char *name) {
	Header h;
	int fd;
	ssize_t ok;

	fd = open(name, O_RDONLY | O_BINARY);
	if (fd < 0) err(EX_NOINPUT, "Unable to open %s", name);


	ok = read(fd, &h, sizeof(h));
	if (ok != sizeof(h))
		errx(EX_DATAERR, "%s is not an object file", name);

	le_to_host(h.h_magic);
	le_to_host(h.h_version);
	le_to_host(h.h_filtyp);

	if (h.h_magic != MOD_MAGIC || h.h_version != 1 || h.h_filtyp > 2)
		errx(EX_DATAERR, "%s is not an object file", name);

	lseek(fd, 0, SEEK_SET);
	if (h.h_filtyp == 2) dump_lib(name, fd);

	// files may contain multiple modules.
	while (dump_obj(name, fd)) /* ... */;

	close(fd);
}

int main(int argc, char **argv) {

	int c;
	while ((c = getopt(argc, argv, "Sg")) != -1) {
			switch(c) {
				case 'S': flags._S = true; break;
				case 'g': flags._g = true; break;
				default: exit(EX_USAGE); break;
			}
	}

	argv += optind;
	argc -= optind;

	if (argc == 0) usage();

	for (int i = 0; i < argc; ++i) {
		dump(argv[i]);
	}

	return 0;
}
