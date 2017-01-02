#ifndef __disassembler_h__
#define __disassembler_h__

#include <stdint.h>
#include <string>
#include <functional>

class disassembler {
	
	public:
		disassembler() = default;

		void operator()(uint8_t byte);
		void operator()(const std::string &expr, unsigned size);

		template<class Iter>
		void operator()(Iter begin, Iter end) { while (begin != end) code(*begin++); }

		template<class T>
		void operator()(const T &t) { code(std::begin(t), std::end(t)); }


		bool m() const { return _flags & 0x20; }
		bool x() const { return _flags & 0x10; }

		void set_m(bool x) {
			if (x) _flags |= 0x20;
			else _flags &= ~0x20;
		}

		void set_x(bool x) {
			if (x) _flags |= 0x10;
			else _flags &= ~0x10;
		}

		uint32_t pc() const { return _pc; }
		void set_pc(uint32_t pc) { if (_pc != pc) { flush(); _pc = pc; } }

		bool code() const { return _code; }
		void set_code(bool code) { if (_code != code) { flush(); _code = code; } }

		void flush();


		void set_label_callback(std::function<int32_t(int32_t)> callback) {
			_label_callback = callback;
			_next_label = -1;
			if (_label_callback) _next_label = _label_callback(-1);
		}

		void recalc_next_label() {
			if (_label_callback) _next_label = _label_callback(-1);
			else _next_label = -1;
		}


	private:

		void reset();

		void dump();
		void dump(const std::string &expr, unsigned size);

		void print();
		void print(const std::string &expr);

		void print_prefix();
		void print_suffix();

		std::string prefix();
		std::string suffix();

		void hexdump();
		void hexdump(std::string &);

		unsigned _st = 0;
		uint8_t _op = 0;
		unsigned _size = 0;
		unsigned _mode = 0;
		uint8_t _bytes[4];
		unsigned _flags = 0x30;
		unsigned _pc = 0;
		unsigned _arg = 0;

		bool _code = true;
		int32_t _next_label = -1;

		void check_labels();

		std::function<int32_t(int32_t)> _label_callback;
};

#endif
