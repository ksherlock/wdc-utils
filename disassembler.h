#ifndef __disassembler_h__
#define __disassembler_h__

#include <stdint.h>
#include <string>

class disassembler {
	
	public:
		disassembler() = default;

		void code(uint8_t byte);
		void code(const std::string &expr, unsigned size);

		template<class Iter>
		void code(Iter begin, Iter end) { while (begin != end) code(*begin++); }

		template<class T>
		void code(const T &t) { code(std::begin(t), std::end(t)); }


		void data(uint8_t byte);
		void data(const std::string &expr, unsigned size);


		bool m() const { return _flags & 0x20; }
		bool x() const { return _flags & 0x10; }
		uint32_t pc() const { return _pc; }

		void set_m(bool x) {
			if (x) _flags |= 0x20;
			else _flags &= ~0x20;
		}

		void set_x(bool x) {
			if (x) _flags |= 0x10;
			else _flags &= ~0x10;
		}

		void set_pc(uint32_t pc) { _pc = pc; }

		void flush();

	private:

		void reset();

		void dump();
		void dump(const std::string &expr, unsigned size);

		void print();
		void print(const std::string &expr);

		void print_prefix();
		void print_suffix();

		void hexdump();

		unsigned _st = 0;
		uint8_t _op = 0;
		unsigned _size = 0;
		unsigned _mode = 0;
		uint8_t _bytes[4];
		unsigned _flags = 0x30;
		unsigned _pc = 0;
		unsigned _arg = 0;

		unsigned _type = 0; // code / data.
};

#endif