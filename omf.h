#ifndef __omf_h__
#define __omf_h__

#include <stdint.h>
#include <vector>
#include <string>


namespace omf {

	enum opcode : uint8_t {
		END = 0x00,
		// 0x01-0xdf CONST
		ALIGN = 0xe0,
		ORG = 0xe1,
		RELOC = 0xe2,
		INTERSEG = 0xe3,
		USING = 0xe4,
		STRONG = 0xe5,
		GLOBAL = 0xe6,
		GEQU = 0xe7,
		MEM = 0xe8,
		EXPR = 0xeb,
		ZEXPR = 0xec,
		BEXPR = 0xed,
		RELEXPR = 0xee,
		LOCAL = 0xef,
		EQU = 0xf0,
		DS = 0xf1,
		LCONST = 0xf2,
		LEXPR = 0xf3,
		ENTRY = 0xf4,
		cRELOC = 0xf5,
		cINTERSEG = 0xf6,
		SUPER = 0xf7,
	};

	struct reloc {
		uint8_t size = 0;
		uint8_t shift = 0;
		uint32_t offset = 0;
		uint32_t value = 0;

		constexpr bool can_compress() const {
			return offset <= 0xffff && value <= 0xffff; 
		}
	};


	struct interseg {

		uint8_t size = 0;
		uint8_t shift = 0;
		uint32_t offset = 0;
		uint16_t file = 1;
		uint16_t segment = 0;
		uint32_t segment_offset = 0;

		constexpr bool can_compress() const {
			return file == 1 && segment <= 255 && offset <= 0xffff && segment_offset <= 0xffff; 
		}	
	};

	struct segment {

		uint16_t segnum = 0;
		uint16_t kind = 0;
		uint32_t alignment = 0;
		uint32_t reserved_space = 0;
		uint32_t org = 0;

		std::string loadname;
		std::string segname;

		std::vector<uint8_t> data;
		std::vector<interseg> intersegs;
		std::vector<reloc> relocs;
	};


}

enum {
	// flags
	OMF_V1 = 1,
	OMF_V2 = 0,
	OMF_NO_SUPER = 2,
	OMF_NO_COMPRESS = 4,
	OMF_NO_EXPRESS = 8

};

void save_omf(const std::string &path, std::vector<omf::segment> &segments, unsigned flags);


#endif
