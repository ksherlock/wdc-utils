#ifndef __disassembler_h__
#define __disassembler_h__

#include <stdint.h>
#include <string>
#include <vector>

// disassembler traits

class disassembler {

	public:

		enum {

			// pea #xxxx vs pea |xxxx
			pea_immediate = 1,
			// jml [|xxxx] vs jml [xxxx]
			jml_indirect_modifier = 2,
			// asl vs asl a
			explicit_implied_a = 4,
			// and & 07f hexdump
			msb_hexdump = 8,
			track_rep_sep = 16,
			bit_hacks = 32,

			orca = jml_indirect_modifier | explicit_implied_a,
			mpw = jml_indirect_modifier | explicit_implied_a,
			wdc = pea_immediate,
		};

	
		disassembler() = default;
		disassembler(unsigned traits) : _traits(traits)
		{}
		virtual ~disassembler();

		void operator()(uint8_t byte);
		void operator()(const std::string &expr, unsigned size, uint32_t value = 0);

		template<class Iter>
		void operator()(Iter begin, Iter end) { while (begin != end) (*this)(*begin++); }

		template<class T>
		void operator()(const T &t) { (*this)(std::begin(t), std::end(t)); }

		void space(unsigned bytes);

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


		void recalc_next_label() {
			_next_label = next_label(-1);
		}

		static std::string to_x(uint32_t value, unsigned bytes, char prefix = 0);

		static void emit(const std::string &label);
		static void emit(const std::string &label, const std::string &opcode);
		static void emit(const std::string &label, const std::string &opcode, const std::string &operand);
		static void emit(const std::string &label, const std::string &opcode, const std::string &operand, const std::string &comment);

		static int operand_size(uint8_t op, bool m = true, bool x = true);

	protected:


		virtual std::pair<std::string, std::string> format_data(unsigned size, const uint8_t *data);
		virtual std::pair<std::string, std::string> format_data(unsigned size, const std::string &);

		virtual std::string label_for_address(uint32_t address);
		virtual std::string label_for_zp(uint32_t address);


		virtual std::string ds() const { return "ds"; }


		void set_inline_data(int count) {
			flush();
			_code = count ? false : true;
			_inline_data = count;
		}

		virtual int32_t next_label(int32_t pc) {
			return -1;
		}

		virtual void event(uint8_t opcode, uint32_t operand) {}


	private:

		void reset();

		void dump();
		void dump(const std::string &expr, unsigned size, uint32_t value = 0);

		void print();
		void print(const std::string &expr);

		std::string prefix();
		std::string suffix();

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
		int _inline_data = 0;
		int32_t _next_label = -1;

		unsigned _traits = 0;

		void check_labels();
};

class analyzer {

public:

	analyzer(unsigned traits = 0) : _traits(traits)
	{}

	void set_pc(uint32_t pc) { _pc = pc; }
	uint32_t pc() const { return _pc; }


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



	void operator()(uint8_t x);
	void operator()(uint32_t x, unsigned size);


	const std::vector<uint32_t> &finish();

	bool state() const { return _st == 0; }

private:

	void reset();
	void process();

	unsigned _traits = 0;
	int _inline_data = 0;
	bool _code = true;
	unsigned _st = 0;
	uint8_t _op = 0;
	unsigned _size = 0;
	unsigned _flags = 0x30;
	unsigned _pc = 0;
	unsigned _arg = 0;
	unsigned _mode = 0;

	std::vector<uint32_t> _labels;
};

#endif
