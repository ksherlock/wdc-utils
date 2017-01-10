#include "omf.h"

#include <vector>
#include <string>
#include <algorithm>

#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <sysexits.h>

#pragma pack(push, 1)
struct omf_header {
	uint32_t bytecount = 0;
	uint32_t reserved_space = 0;
	uint32_t length = 0;
	uint8_t unused1 = 0;
	uint8_t lablen = 0;
	uint8_t numlen = 4;
	uint8_t version = 2;
	uint32_t banksize = 0;
	uint16_t kind = 0;
	uint16_t unused2 = 0;
	uint32_t org = 0;
	uint32_t alignment = 0;
	uint8_t numsex = 0;
	uint8_t unused3 = 0;
	uint16_t segnum = 0;
	uint32_t entry = 0;
	uint16_t dispname = 0;
	uint16_t dispdata = 0;
};

#pragma pack(pop)

void push(std::vector<uint8_t> &v, uint8_t x) {
	v.push_back(x);
}

void push(std::vector<uint8_t> &v, uint16_t x) {
	v.push_back(x & 0xff);
	x >>= 8;
	v.push_back(x & 0xff);
}

void push(std::vector<uint8_t> &v, uint32_t x) {
	v.push_back(x & 0xff);
	x >>= 8;
	v.push_back(x & 0xff);
	x >>= 8;
	v.push_back(x & 0xff);
	x >>= 8;
	v.push_back(x & 0xff);
}

void push(std::vector<uint8_t> &v, const std::string &s) {
	uint8_t count = std::min((int)s.size(), 255);
	push(v, count);
	v.insert(v.end(), s.begin(), s.end());
}


void save_omf(std::vector<omf::segment> &segments, bool expressload, const std::string &path) {

	if (expressload) {
		for (auto &s : segments) {
			s.segnum++;
			for (auto &r : s.intersegs) r.segment++;
		}
	}

	int fd;
	fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) {
		err(EX_CANTCREAT, "Unable to open %s", path.c_str());
	}


	for (auto &s : segments) {
		omf_header h;
		h.length = s.data.size();
		h.kind = s.kind;
		h.banksize = s.data.size() > 0xffff ? 0x0000 : 0x010000;
		h.segnum = s.segnum;

		std::vector<uint8_t> data;

		// push segname and load name onto data.
		data.insert(data.end(), 10, ' ');
		push(data, s.segname);

		h.dispname = sizeof(omf_header);
		h.dispdata = sizeof(omf_header) + data.size();

		//lconst record
		push(data, (uint8_t)0xf2);
		push(data, (uint32_t)h.length);
		data.insert(data.end(), s.data.begin(), s.data.end());

		// should interseg/reloc records be sorted?
		// todo -- compress into super records.
		for (const auto &r : s.relocs) {
			if (r.can_compress()) {
				push(data, (uint8_t)0xf5);
				push(data, (uint8_t)r.size);
				push(data, (uint8_t)r.shift);
				push(data, (uint16_t)r.offset);
				push(data, (uint16_t)r.value);
			} else {
				push(data, (uint8_t)0xe5);
				push(data, (uint8_t)r.size);
				push(data, (uint8_t)r.shift);
				push(data, (uint32_t)r.offset);
				push(data, (uint32_t)r.value);
			}
		}

		for (const auto &r : s.intersegs) {
			if (r.can_compress()) {
				push(data, (uint8_t)0xf6);
				push(data, (uint8_t)r.size);
				push(data, (uint8_t)r.shift);
				push(data, (uint16_t)r.offset);
				push(data, (uint16_t)r.segment);			
				push(data, (uint16_t)r.segment_offset);			
			} else {
				push(data, (uint8_t)0xe3);
				push(data, (uint8_t)r.size);
				push(data, (uint8_t)r.shift);
				push(data, (uint32_t)r.offset);
				push(data, (uint16_t)r.file);
				push(data, (uint32_t)r.segment);			
				push(data, (uint32_t)r.segment_offset);		
			}
		}

		// end-of-record
		push(data, (uint8_t)0x00);

		h.bytecount = data.size() + sizeof(omf_header);

		write(fd, &h, sizeof(h));
		write(fd, data.data(), data.size());
	}

	close(fd);
}