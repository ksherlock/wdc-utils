#ifndef __disassembler_h__
#define __disassembler_h__

#include <stdint.h>
#include <string>

class disassembler {
	
	public:
		disassembler() = default;

		void process(uint8_t byte);
		void process(const std::string &expr, unsigned size);

		template<class Iter>
		void process(Iter begin, Iter end) { while (begin != end) process(*begin++); }

		template<class T>
		void process(const T &t) { process(std::begin(t), std::end(t)); }

		bool m() const { return _flags & 0x20; }
		bool x() const { return _flags & 0x10; }
		uint32_t pc() const { return _pc; }

		void set_m(bool x) {
			if (x) _flags |= 0x20;
			else _flags &= 0x20;
		}

		void set_x(bool x) {
			if (x) _flags |= 0x10;
			else _flags &= 0x10;
		}

		void set_pc(uint32_t pc) { pc = _pc; }

		void flush();

	private:

		void reset();

		void dump();
		void dump(const std::string &expr, unsigned size);

		void print();
		void print(const std::string &expr);

		void print_prefix();
		void print_suffix();

		unsigned _st = 0;
		uint8_t _op = 0;
		unsigned _size = 0;
		unsigned _mode = 0;
		uint8_t _bytes[4];
		unsigned _flags = 0x30;
		unsigned _pc = 0;
		unsigned _arg = 0;
};

#endif