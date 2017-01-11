#include "omf.h"

#include <vector>
#include <string>
#include <algorithm>

#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <sysexits.h>


#ifndef O_BINARY
#define O_BINARY 0
#endif

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

struct omf_express_header {
	uint32_t lconst_mark;
	uint32_t lconst_size;
	uint32_t reloc_mark;
	uint32_t reloc_size;
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

	// expressload doesn't support links to other files. 
	// fortunately, we don't either.

	std::vector<uint8_t> expr_headers;
	std::vector<unsigned> expr_offsets;

	int fd;
	fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
	if (fd < 0) {
		err(EX_CANTCREAT, "Unable to open %s", path.c_str());
	}


	uint32_t offset = 0;
	if (expressload) {
		for (auto &s : segments) {
			s.segnum++;
			for (auto &r : s.intersegs) r.segment++;
		}

		// calculate express load segment size.
		// sizeof includes the trailing 0, so no need to add in byte size.
		offset = sizeof(omf_header) + 10 + sizeof("~ExpressLoad");

		offset += 6; // lconst + end
		offset += 6;  // header.
		for (auto &s : segments) {
			offset += 8 + 2;
			offset += sizeof(omf_express_header) + 10;
			offset += s.segname.length() + 1;
		}

		lseek(fd, offset, SEEK_SET);
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



		uint32_t lconst_offset = offset + sizeof(omf_header) + data.size() + 5;
		uint32_t lconst_size = h.length;

		//lconst record
		push(data, (uint8_t)0xf2);
		push(data, (uint32_t)h.length);
		data.insert(data.end(), s.data.begin(), s.data.end());


		uint32_t reloc_offset = offset + sizeof(omf_header) + data.size();
		uint32_t reloc_size = 0;

		// should interseg/reloc records be sorted?
		// todo -- compress into super records.
		for (const auto &r : s.relocs) {
			if (r.can_compress()) {
				push(data, (uint8_t)0xf5);
				push(data, (uint8_t)r.size);
				push(data, (uint8_t)r.shift);
				push(data, (uint16_t)r.offset);
				push(data, (uint16_t)r.value);
				reloc_size += 7;
			} else {
				push(data, (uint8_t)0xe5);
				push(data, (uint8_t)r.size);
				push(data, (uint8_t)r.shift);
				push(data, (uint32_t)r.offset);
				push(data, (uint32_t)r.value);
				reloc_size += 11;
			}
		}

		for (const auto &r : s.intersegs) {
			if (r.can_compress()) {
				push(data, (uint8_t)0xf6);
				push(data, (uint8_t)r.size);
				push(data, (uint8_t)r.shift);
				push(data, (uint16_t)r.offset);
				push(data, (uint8_t)r.segment);
				push(data, (uint16_t)r.segment_offset);
				reloc_size += 8;
			} else {
				push(data, (uint8_t)0xe3);
				push(data, (uint8_t)r.size);
				push(data, (uint8_t)r.shift);
				push(data, (uint32_t)r.offset);
				push(data, (uint16_t)r.file);
				push(data, (uint16_t)r.segment);
				push(data, (uint32_t)r.segment_offset);
				reloc_size += 15;
			}
		}


		// end-of-record
		push(data, (uint8_t)0x00);

		h.bytecount = data.size() + sizeof(omf_header);

		// todo -- byteswap to little-endian!
		offset += write(fd, &h, sizeof(h));
		offset += write(fd, data.data(), data.size());

		if (expressload) {

			expr_offsets.emplace_back(expr_headers.size());

			if (lconst_size == 0) lconst_offset = 0;
			if (reloc_size == 0) reloc_offset = 0;


			push(expr_headers, (uint32_t)lconst_offset);
			push(expr_headers, (uint32_t)lconst_size);
			push(expr_headers, (uint32_t)reloc_offset);
			push(expr_headers, (uint32_t)reloc_size);

			push(expr_headers, h.unused1);
			push(expr_headers, h.lablen);
			push(expr_headers, h.numlen);
			push(expr_headers, h.version);
			push(expr_headers, h.banksize);
			push(expr_headers, h.kind);
			push(expr_headers, h.unused2);
			push(expr_headers, h.org);
			push(expr_headers, h.alignment);
			push(expr_headers, h.numsex);
			push(expr_headers, h.unused3);
			push(expr_headers, h.segnum);
			push(expr_headers, h.entry);
			push(expr_headers, (uint16_t)(h.dispname-4));
			push(expr_headers, h.dispdata);

			expr_headers.insert(expr_headers.end(), 10, ' ');
			push(expr_headers, s.segname);
		}

	}

	if (expressload) {
		omf_header h;
		h.segnum = 1;
		h.banksize = 0x00010000;
		h.kind = 0x8001;
		h.dispname = 0x2c;
		h.dispdata = 0x43;

		unsigned fudge = 10 * segments.size();

		h.length = 6 + expr_headers.size() + fudge;

		std::vector<uint8_t> data;
		data.insert(data.begin(), 10, ' ');
		push(data, std::string("~ExpressLoad"));
		push(data, (uint8_t)0xf2); // lconst.
		push(data, (uint32_t)h.length);

		push(data, (uint32_t)0); // reserved
		push(data, (uint16_t)(segments.size() - 1)); // seg count - 1


		for (auto &offset : expr_offsets) {
			push(data, (uint16_t)(fudge + offset));
			push(data, (uint16_t)0);
			push(data, (uint32_t)0);
		}

		for (auto &s : segments) {
			push(data, (uint16_t)s.segnum);
		}

		data.insert(data.end(), expr_headers.begin(), expr_headers.end());
		push(data, (uint8_t)0); // end.

		h.bytecount = data.size() + sizeof(omf_header);

		lseek(fd, 0, SEEK_SET);
		write(fd, &h, sizeof(h));
		write(fd, data.data(), data.size());

	}

	close(fd);
}