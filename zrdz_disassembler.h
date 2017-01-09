#ifndef __zrdz_disassembler_h__
#define __zrdz_disassembler_h__

#include <string>
#include <vector>
#include <stdint.h>
#include "disassembler.h"
#include "obj816.h"

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
};


class zrdz_disassembler final : public disassembler {
	
public:

	zrdz_disassembler(std::vector<section> &&, std::vector<symbol> &&);
	virtual ~zrdz_disassembler();

	//int section() const { return _section; }
	void set_section(int);

	void front_matter(const std::string &module_name);
	void back_matter(unsigned flags);

	std::string location_name(unsigned section, uint32_t offset) const;
	std::string symbol_name(unsigned entry) const;
	std::string section_name(unsigned entry) const;

protected:

	virtual int32_t next_label(int32_t pc) override;

private:


	struct entry : public section {
		using section::section;
		using section::operator=;

		bool processed = false;
		bool valid = false;
		std::vector<symbol> symbols;
		uint32_t pc = 0;
	};

	std::vector<symbol> _symbols;
	std::vector<entry> _sections;

	int _section = -1;


	void print_section(const entry &e);
	void print_externs();
	void print_variables();
	void print_globals(int section);
	void print_equs(int section);


};

#endif
