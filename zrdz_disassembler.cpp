#include "zrdz_disassembler.h"

#include <err.h>
#include <stdio.h>

#include <algorithm>

static constexpr const int equ_type = (ST_EQU << 4) | S_ABS;

zrdz_disassembler::zrdz_disassembler(std::vector<section> &&sections, std::vector<symbol> &&symbols) : 
	disassembler(wdc), _symbols(std::move(symbols))
{

	// do not sort _symbols ... order matters for lookup by entry number.
#if 0
	// sort by section / address.
	std::sort(_symbols.begin(), _symbols.end(), [](const symbol &a, const symbol &b){
		if (a.section < b.section) return true;
		if (a.section == b.section && a.offset < b.offset) return true;
		return false;
	});
#endif

	for (auto &s : sections) {
		if (s.number >= _sections.size()) _sections.resize(s.number + 1);
		_sections[s.number].valid = true;
		_sections[s.number].pc = s.org;
		_sections[s.number] = std::move(s);
	}

	for (auto &s : _symbols) {

		if (s.type == S_UND) continue;
		if (s.flags & SF_VAR) continue;

		if (s.section >= _sections.size()) {
			warnx("invalid section %d for symbol %s", s.section, s.name.c_str());
			continue;
		}
		auto &e = _sections[s.section];
		if (!e.valid) {
			warnx("invalid section %d for symbol %s", s.section, s.name.c_str());
			continue;
		}
		if (s.type == equ_type) continue;
		e.symbols.emplace_back(s);
	}

	// sort labels...
	for (auto &e : _sections) {
		if (e.symbols.empty()) continue;
		std::sort(e.symbols.begin(), e.symbols.end(), [](const symbol &a, const symbol &b) {
			return a.offset > b.offset;
		});
	}

	if (_sections.size() < 5) _sections.resize(5);
	_sections[SECT_PAGE0].name = "page0";
	_sections[SECT_CODE].name = "code";
	_sections[SECT_KDATA].name = "kdata";
	_sections[SECT_DATA].name = "data";
	_sections[SECT_UDATA].name = "udata";

#if 0
	// now place equates at the end...
	for (auto &s : _symbols) {
		if (s.type != equ_type) continue;
		auto &sec = _sections[s.section];
		sec.symbols.emplace_back(s);
	}	
#endif

}


zrdz_disassembler::~zrdz_disassembler() {

}


void zrdz_disassembler::front_matter(const std::string &module) {
	emit("", "module", module);
	putchar('\n');
	print_externs();
	print_variables();

	// reference-only sections.
	for (auto &e : _sections) {
		if (!e.valid) continue;

		if ((e.flags & SEC_REF_ONLY) == 0) continue;
		if (e.size == 0 && e.symbols.empty()) continue;

		print_section(e);
		print_globals(e.number);
		print_equs(e.number);
		e.processed = true;

		if (e.org) emit("", ".org", to_x(e.org, 4, '$'));
		uint32_t pc = e.org;


		for (; !e.symbols.empty(); e.symbols.pop_back()) {
			auto &s = e.symbols.back();

			if (s.offset > pc) {
				emit("","ds", std::to_string(s.offset - pc));
				pc = s.offset;
			}
			emit(s.name);
		}

		if (pc < e.size)
			emit("","ds",std::to_string(e.size - pc));

		emit("", "ends");
		putchar('\n');
	}

	set_section(1);
	/*
	emit("","code");
	print_globals(1);
	print_equs(1);
	_sections[1].processed = true;
	*/

}

void zrdz_disassembler::back_matter(unsigned flags) {

	flush();
	_section = -1;


	// todo -- print remaining symbols.
	// todo -- print any empty sections?

	emit("", "ends");
	putchar('\n');

	for (auto &e : _sections) {
		if (e.processed) continue;
		if (!e.valid) continue;

		print_section(e);
		print_globals(e.number);
		print_equs(e.number);
		emit("","ends");
		e.processed = true;
		// ...check for symbols?
	}


	if (flags & 0x01) {
		fputs("; sections\n", stdout);
		for (const auto &e : _sections) {
			if (!e.valid) continue;
			printf("; %-20s %02x %02x %04x %04x\n",
				e.name.c_str(), e.number, e.flags, e.size, e.org);
		}

		fputs(";\n", stdout);

		fputs("; symbols\n", stdout);
		for (const auto &s : _symbols) {
			printf("; %-20s %02x %02x %02x %08x\n",
				s.name.c_str(), s.type, s.flags, s.section, s.offset);
		}
		putchar('\n');
	}


	emit("", "endmod");
	putchar('\n');
}

void zrdz_disassembler::print_externs() {

	std::vector<std::string> tmp;

	for (auto &s : _symbols) {
		if (s.type == S_UND) tmp.push_back(s.name);
	}

	if (tmp.empty()) return;
	std::sort(tmp.begin(), tmp.end());
	for (const auto &s : tmp) emit("", "extern", s);
	putchar('\n');

}


// for sorting symbols by name.
bool operator<(const symbol &a, const symbol &b) {
	return a.name < b.name;
}



void zrdz_disassembler::print_variables() {

	std::vector<symbol> tmp;

	for (auto &s : _symbols) {
		if (s.flags & SF_VAR) tmp.push_back(s);
	}

	if (tmp.empty()) return;
	std::sort(tmp.begin(), tmp.end());

	for (const auto &s : tmp) {
		if (s.flags & SF_GBL) emit("", "public", s.name);
		emit(s.name, "var", to_x(s.offset, 4, '$'));
	}

	putchar('\n');

}

void zrdz_disassembler::print_globals(int section) {

	std::vector<std::string> tmp;

	for (auto &s : _symbols) {
		if (s.section == section && s.flags & SF_GBL) tmp.push_back(s.name);
	}

	if (tmp.empty()) return;
	std::sort(tmp.begin(), tmp.end());
	for (const auto &s : tmp) emit("", "public", s);
	putchar('\n');
}

void zrdz_disassembler::print_equs(int section) {

	std::vector<symbol> tmp;

	std::copy_if(_symbols.begin(), _symbols.end(), std::back_inserter(tmp),
		[=](const symbol &s) { return s.section == section && s.type == equ_type; }
	);

	if (tmp.empty()) return;
	std::sort(tmp.begin(), tmp.end());

	for (const auto &s : tmp) emit(s.name, "gequ", to_x(s.offset, 4, '$'));
	putchar('\n');
}

void zrdz_disassembler::print_section(const entry &e) {
	std::string attr;
	bool comma = false;


	if (e.processed || e.number < 5) {
		emit("", e.name);
		return;
	}

	if (e.flags & SEC_OFFSET) {
		if (comma) attr += ", ";
		attr += "SEC_OFFSET $";
		attr += to_x(e.org, 4);
		comma = true;
	}
	if (e.flags & SEC_INDIRECT) {
		if (comma) attr += ", ";
		attr += "SEC_INDIRECT $";
		attr += to_x(e.org, 4);
		comma = true;
	}
#define _(x) if (e.flags & x) { if (comma) attr += ", "; attr += #x; comma = true; }
	_(SEC_STACKED)
	_(SEC_REF_ONLY)
	_(SEC_CONST)
	_(SEC_DIRECT)
	_(SEC_NONAME)
	_(SEC_DATA)
#undef _

	emit(e.name, "section", attr);
	putchar('\n');
}

void zrdz_disassembler::set_section(int section) {
	if (section == _section) return;
	if (section >= _sections.size()) {
		warnx("Invalid section %d", section);
		return;
	}
	auto &e = _sections[section];
	if (section >5 && !e.valid) {
		warnx("Invalid section %d", section);
		return;		
	}

	if (_section >= 0) {
		flush();
		_sections[_section].pc = pc();
		emit("", "ends");
		putchar('\n');
	}

	print_section(e);

	if (!e.processed) {
		print_globals(section);
		print_equs(section);
		e.processed = true;
	}

	_section = section;
	set_pc(e.pc);
	set_code((e.flags & SEC_DATA) == 0);
	recalc_next_label();
}


int32_t zrdz_disassembler::next_label(int32_t pc) {
	if (_section < 0) return -1;

	auto &symbols = _sections[_section].symbols;
	if (pc >= 0) {

		for(; !symbols.empty(); symbols.pop_back()) {
			auto &s = symbols.back();
			if (s.offset > pc) return s.offset;
			if (s.offset == pc) emit(s.name);
			else {
				warnx("Unable to place symbol %s at offset $%04x in section %d",
					s.name.c_str(), s.offset, _section);
			}
		}

	}
	if (symbols.empty()) return -1;
	return symbols.back().offset;
}


std::string zrdz_disassembler::location_name(unsigned section, uint32_t offset) const {

	if (section >= _sections.size()) {
		warnx("Invalid section %d", section);
		return "$";
	}
	auto &e = _sections[section];
	if (!e.valid) {
		warnx("Invalid section %d", section);
		return "$";
	}

	// todo -- need to verify relative/absolute value

	auto iter = std::find_if(_symbols.begin(), _symbols.end(), [=](const symbol &s){
		if (s.type == equ_type) return false;
		if (s.type == S_UND) return false;
		return s.section == section && s.offset == offset;
	});

	if (iter != _symbols.end()) return iter->name;

	//fallback to section name + offset
	std::string tmp = e.name;
	if (tmp.empty()) tmp = "section" + std::to_string(section);
	if (offset) tmp += "+$" + to_x(offset, 4);
	return tmp;
}

std::string zrdz_disassembler::symbol_name(unsigned entry) const {
	if (entry >= _symbols.size()) {
		warnx("Invalid symbol %d", entry);
		return "$";
	}
	return _symbols[entry].name;
}

std::string zrdz_disassembler::section_name(unsigned section) const {

	std::string defname = std::string("section") + std::to_string(section);

	if (section >= _sections.size()) {
		warnx("Invalid section %d", section);
		return defname;
	}
	auto &e = _sections[section];
	if (!e.valid) {
		warnx("Invalid section %d", section);
		return defname;
	}

	return e.name;
}

