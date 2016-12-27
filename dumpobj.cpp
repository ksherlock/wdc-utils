#include <string>
#include <err.h>
#include <unistd.h>
#include <sysexits.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>

#include <type_traits>
#include <vector>
#include <algorithm>
#include <iterator>

#include "obj816.h"
#include "disassembler.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

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
uint16_t read_32(T &iter) {
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
	uint8_t type;
	uint8_t flags;
	uint8_t section;
	uint32_t offset;
};


std::vector<symbol> read_symbols(const std::vector<uint8_t> &symbol_data) {

	std::vector<symbol> symbols;

	auto iter = symbol_data.begin();
	while (iter != symbol_data.end()) {
		uint8_t type = read_8(iter);
		uint8_t flags = read_8(iter);
		uint8_t section = read_8(iter);
		uint32_t offset = type == S_UND ? 0 : read_32(iter);
		std::string name = read_cstring(iter);


		symbols.emplace_back(symbol{name, type, flags, section, offset});
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
	return symbol{"", S_UND, 0, 0xff, 0};
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

bool dump_obj(const char *name, int fd)
{
	static const char *sections[] = { "PAGE0", "CODE", "KDATA", "DATA", "UDATA" };
	static const char *types[] = { "S_UND", "S_ABS", "S_REL", "S_EXP", "S_REG", "S_FREG" };

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


	printf("name: %s\n", oname.data());
	printf("record size    : $%04x\n", h.h_recsize);
	printf("section size   : $%04x\n", h.h_secsize);
	printf("symbol size    : $%04x\n", h.h_symsize);
	printf("option size    : $%04x\n", h.h_optsize);
	printf("number sections: $%04x\n", h.h_num_secs);
	printf("number symbols : $%04x\n", h.h_num_syms);

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

	unsigned section = 1; // default section = CODE.
	std::vector<symbol> labels = labels_for_section(symbols, section);
	//uint32_t label_pc = labels.empty() ? 0xffff : labels.back().offset;

	uint8_t op = REC_END;

	disassembler d;

	d.set_pc(0);
	d.set_code(true);

	auto iter = data.begin();
	while (iter != data.end()) {

		place_labels(labels, d.pc());

		op = read_8(iter);
		if (op == 0) break;
		if (op < 0xf0) {
			auto end = iter + op;
			while (iter != end) {
				d(*iter++);
				place_labels(labels, d.pc());
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

								symbol s = find_symbol(symbols, section, offset);
								if (s.type) {
									stack.push_back(s.name);
								} else {

									if (section < sizeof(sections) / sizeof(sections[0]))
										snprintf(buffer, sizeof(buffer), "%s+$%04x", sections[section], offset);
									else
										snprintf(buffer, sizeof(buffer), "section %02x+$%04x", section, offset);
									stack.push_back(buffer);
								}
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
									".NOT.", "-", "/" // ?
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
					//printf("\t;DEBUG\n");

					for (unsigned i = 0; i < size; ++i) {
						uint8_t op = read_8(iter);
						switch(op) {
							case D_LONGA_ON:
								d.set_m(true);
								printf("\tlonga\ton\n");
								break;
							case D_LONGA_OFF:
								d.set_m(false);
								printf("\tlonga\toff\n");
								break;
							case D_LONGI_ON:
								d.set_x(true);
								printf("\tlongi\ton\n");
								break;
							case D_LONGI_OFF:
								d.set_x(false);
								printf("\tlongi\toff\n");
								break;
							case D_C_FILE: {
								std::string s = read_cstring(iter);
								uint16_t line = read_16(iter);
								i += 2;
								i += s.length() + 1;
								printf("\t.file\t\"%s\", %d\n", s.c_str(), line);
								break;
							}
							case D_C_LINE: {
								uint16_t line = read_16(iter);
								printf("\t.line\t%d\n", line);
								i += 2;
								break;
							}
							case D_C_BLOCK: {
								uint16_t block = read_16(iter);
								printf("\t.block\t%d\n", block);
								i += 2;
								break;
							}
							case D_C_FUNC: {
								uint16_t arg = read_16(iter);
								printf("\t.function\t%d\n", arg);
								i += 2;
								break;								
							}
							/*
							case D_C_SYM: {
								break;
							}
							*/

							default:
								errx(EX_DATAERR, "%s: unknown debug opcode %02x (%d)", name, op, op);
								break;

						}
					}
				}
				break;

			case REC_SECT: {
				d.flush();
				d.set_pc(0);
				uint8_t sec = read_8(iter);
				printf("\t.sect\t%d\n", sec);
				if (sec != section) {
					section = sec;
					labels = labels_for_section(symbols, section);
					d.set_code(section == 1 || section > 4);
				}
				break;
			}

			case REC_ORG: {
				d.flush();
				uint32_t org = read_32(iter);
				printf("\t.org\t$%04x\n", org);
				d.set_pc(org);
				break;
			}

			case REC_SPACE: {
				d.flush();
				uint16_t count = read_16(iter);
				printf("\tds\t$%04x\n", count);
				d.set_pc(d.pc() + count);
				break;
			}

			case REC_LINE: {
				d.flush();
				uint16_t line = read_16(iter);
				printf("\t.line\t%d\n", line);
				break;
			}
			default:
				d.flush();
				errx(EX_DATAERR, "%s: unknown opcode %02x", name, op);
		}
	}

	place_labels(labels, d.pc());


	for(auto &label : labels) {
		warnx("Unable to place label %s (offset $%04x)", label.name.c_str(), label.offset);
	}



	if (iter != data.end() || op != REC_END) errx(EX_DATAERR, "%s records ended early", name);


	// section info

	printf("\nSections\n");

	iter = section_data.begin();
	while (iter != section_data.end()) {


		uint8_t number = read_8(iter);
		uint8_t flags = read_8(iter);
		uint32_t size = read_32(iter);
		uint32_t org = read_32(iter);

		printf("section %d\n", number);
		printf("flags: %02x ", flags);

#undef _
#define _(x) if (flags & x) fputs(#x " ", stdout)
		_(SEC_OFFSET);
		_(SEC_INDIRECT);
		_(SEC_STACKED);
		_(SEC_REF_ONLY);
		_(SEC_CONST);
		_(SEC_DIRECT);
		_(SEC_NONAME);
		fputs("\n", stdout);

		// todo -- section name -- cstring?
		//assert(flags & SEC_NONAME);
		std::string s;
		if (!(flags & SEC_NONAME)) s = read_cstring(iter);
	}


	// symbol info
#if 0
	printf("\nSymbols\n");

	iter = symbol_data.begin();
	while (iter != symbol_data.end()) {
		uint8_t type = read_8(iter);
		uint8_t flags = read_8(iter);
		uint8_t section = read_8(iter);
		uint32_t offset = type == S_UND ? 0 : read_32(iter);
		std::string name = read_cstring(iter);

		printf("name : %s\n", name.c_str());
		printf("type : $%02x %s\n", type, type < sizeof(types) / sizeof(types[0]) ? types[type] : "");
		printf("flags: $%02x ", flags);
#undef _
#define _(x) if (flags & x) fputs(#x " ", stdout)
		_(SF_GBL);
		_(SF_DEF);
		_(SF_REF);
		_(SF_VAR);
		_(SF_PG0);
		_(SF_TMP);
		_(SF_DEF2);
		_(SF_LIB);
		fputs("\n", stdout);
		printf("section: %02x %s\n", section,
			section < sizeof(sections) / sizeof(sections[0]) ? sections[section] : "");
		if (type != S_UND)
			printf("offset: %04x\n", offset);

	}
#endif
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
	while ((c = getopt(argc, argv, "")) != -1) {

	}

	argv += optind;
	argc -= optind;

	if (argc == 0) usage();

	for (int i = 0; i < argc; ++i) {
		dump(argv[i]);
	}

	return 0;
}