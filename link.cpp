/*
 * WDC to OMF Linker.
 *
 *
 */

#include <sysexits.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include <string>
#include <vector>
#include <algorithm>

#include "obj816.h"
 #include "expression.h"




struct section {
	std::string name;
	uint8_t flags = 0;
	uint32_t org = 0;

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
						if (ss.type == S_UND) e.undefined = true;
						else {
							switch(ss.flags & 0x0f) {
								case SF_REL:
									t = expr{OP_LOC, ss.offset, ss.section};
									delta = true;
									break;
								case SF_ABS:
									t = expr{OP_VAL, ss.offset};
									delta = true;
									break;
							}
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

	int current_section = CODE;
	section *section_ptr = &sections[current_section];

	std::fill(remap_section.begin(), remap_section.end(), -1);
	remap_section[PAGE0] = PAGE0;
	remap_section[CODE] = CODE;
	remap_section[KDATA] = KDATA;
	remap_section[DATA] = DATA;
	remap_section[UDATA] = UDATA;

	std::vector<section> local_sections = read_sections(section_data);
	std::vector<symbol> local_symbols = read_symbols(symbol_data);

	// convert local sections to global 
	for (auto &s : local_sections) {
		if (s.number <= UDATA) continue;

		auto iter = section_map.find(s.name);
		if (iter == section_map.end()) {
			int virtual_section = sections.size();
			remap_section[s.number] = virtual_section;
			s.number = virtual_section;
			sections.emplace_back(s);
			symbol_map.insert(s.name, virtual_section);
		} else {
			const auto &&s = sections[iter->second];
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
				symbol_map.insert(s.name, s.section);
				undefined_symbols.insert(s.name);
			}
			else {
				// already exists... 
				const auto &ss = symbols[iter->second];
				if (ss.type != S_UND) s = ss;
			}
			continue;
		}

		// remap and fudge the offset.
		if (s.type & 0x0f == S_REL) {
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
				symbol_map.insert(s.name, tmp);
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
					assert(ss.type == ss.type && ss.flags == s.flags && ss.section == s.section && && ss.offset == s.offset);
				}
			}

		}

	}


	auto iter = data.begin();
	for(;;) {
		uint8_t *op = read_8(iter);
		if (op == REC_END) return;

		++iter;
		if (op < 0xf0) {
			section->data.append(iter, iter + op);
			iter += op;
			continue;
		}

		switch(op) {
			case REC_SPACE: {
				uint16_t count = read_16(iter);
				section->data.append(count, 0);
				break;
			}

			case REC_SECT: {
				/* switch sections */
				uint8_t s = read_8(iter);
				current_section = remap_section[s];
				assert(current_section > 0 && current_section < sections.size());

				section_ptr = &sections[current_section];
				break;
			}

			case REC_ORG: {
				assert(!"ORG not supported.");
				break;
			}

			case REC_RELEXPR:
			case REC_EXPR: {

				expression e;
				e.relative = op == REC_RELEXPR;

				e.offset = section_ptr->data.size();
				e.size = read_8(iter);
				section_ptr->data.append(e.size, 0);

				/**/
				for(;;) {
					op = read_8(iter);
					if (op == OP_END) break;

					switch(op) {
						case OP_VAL: {
							uint32_t offset = read_32(iter);
							e.stack.emplace_back(op, value);
							break;
						}
						case OP_SYM: {
							uint16_t symbol = read_16(iter);
							assert(symbol < local_symbols.size());
							auto &s = local_symbols[symbol];
							if (s.type == S_UND) {
								// S_UND indicates it's still undefined globally.
								e.stack.emplace_back(OP_SYM, 0, s.section); /* section is actually a symbol number */
								e.undefined = true;
							} else {
								switch (s.flags & 0x0f) {
									case SF_REL:
										e.stack.emplace_back(OP_REL, s.offset, s.section);
										break;
									case SF_ABS:
										e.stack.emplace_back(OP_VAL, s.offset);
										break;
									default:
										assert(!"unsupported symbol flags.");
								}
							}
							break;
						}
						case OP_LOC: {
							uint8_t section = read_8(iter);
							uint32_t offset = read_32(iter);
							int real_section = remap[section];
							assert(real_section >= 0);
							e.stack.emplace_back(op, offset, real_section);
							break;
						}
						case O
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



struct section {
	std::vector<uint8_t> data;
	uint32_t offset; 
}