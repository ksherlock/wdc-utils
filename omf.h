#ifndef __omf_h__
#define __omf_h__

#include <stdint.h>
#include <vector>
#include <string>

namespace omf {

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

		std::string loadname;
		std::string segname;

		std::vector<uint8_t> data;
		std::vector<interseg> intersegs;
		std::vector<reloc> relocs;
	};


}

#endif
