#include "expression.h"
#include <vector>
#include "obj816.h"

/* a ** b */
uint32_t power(uint32_t a, uint32_t b) {

	uint32_t rv = 1;
	for( ; b; b >>= 1) {
		if (b & 0x01) rv *= a;
		a = a * a;
	}
	return rv; 
}

bool unary_op(unsigned op, std::vector<expr> &v) {
	if (v.size() >=1 ) {
		expr &a = v.back();
		if (a.tag == OP_VAL) {
			switch(a.tag) {
			case OP_NOT: a.value = !a.value; break;
			case OP_NEG: a.value = -a.value; break;
			case OP_FLP: a.value = ~a.value; break;
			}
			return true;
		}
	}
	v.emplace_back(op);
	return false;
}


bool binary_op(unsigned op, std::vector<expr> &v) {
	if (v.size() >= 2) {
		expr &a = v[v.size() - 2];
		expr &b = v[v.size() - 1];

		if (a.tag == OP_VAL && b.tag == OP_VAL) {
			uint32_t value = 0;
			switch(op) {
			case OP_EXP: value = power(a.value, b.value); break;
			case OP_MUL: value = a.value * b.value; break;
			case OP_DIV: value = a.value / b.value; break;
			case OP_MOD: value = a.value % b.value; break;
			case OP_SHR: value = a.value >> b.value; break;
			case OP_SHL: value = a.value << b.value; break;
			case OP_ADD: value = a.value + b.value; break;
			case OP_SUB: value = a.value - b.value; break;
			case OP_AND: value = a.value & b.value; break;
			case OP_OR: value = a.value | b.value; break;
			case OP_XOR: value = a.value ^ b.value; break;
			case OP_EQ: value = a.value == b.value; break;
			case OP_GT: value = (int32_t)a.value > (int32_t)b.value; break;
			case OP_LT: value = (int32_t)a.value < (int32_t)b.value; break;
			case OP_UGT: value = a.value > b.value; break;
			case OP_ULT: value = a.value < b.value; break;
			}
			v.pop_back();
			v.back().value = value;
			return true;
		}
		if (a.tag == OP_VAL && b.tag == OP_LOC) {
			switch(op) {
				case OP_ADD: {
					// constant + symbol
					expr tmp = b;
					tmp.value = a.value + b.value;
					v.pop_back();
					v.pop_back();
					v.emplace_back(tmp);
					return true; 
				}
				#if 0
				case OP_SUB: {
					// constant - symbol .. does this even make sense?
					expr tmp = b;
					tmp.value = a.value - b.value;
					v.pop_back();
					v.pop_back();
					v.emplace_back(tmp);
					return true; 
				}
				#endif
			}
		}
		if (a.tag == OP_LOC && b.tag == OP_VAL) {
			switch(op) {
				case OP_ADD: a.value += b.value; return true;
				case OP_SUB: a.value -= b.value; return true;
			}
		}

		if (a.tag == OP_LOC && b.tag == OP_LOC && a.section == b.section) {
			uint32_t value = 0;
			bool rv = false;
			switch (op) {
				case OP_SUB:
					// end - start
					value = a.value - b.value;
					rv = true; break;
				case OP_EQ:
					value = a.value == b.value;
					rv = true; break;
				case OP_GT:
				case OP_UGT:
					value = a.value > b.value;
					rv = true; break;
				case OP_LT:
				case OP_ULT:
					value = a.value < b.value;
					rv = true; break;
			}
			if (rv) {
				v.pop_back();
				v.pop_back();
				v.emplace_back(OP_VAL, value);
				return true;
			}
		}


	}
	v.emplace_back(op);
	return false;
}


bool simplify_expression(expression &e) {
	std::vector<expr> tmp;
	bool rv = false;
	for (auto &t : e.stack) {
		if (t.tag >= OP_BIN) {
			rv = binary_op(t.tag, tmp) || rv;
		} else if (t.tag >= OP_UNA) {
			rv = unary_op(t.tag, tmp) || rv;
		} else {
			tmp.emplace_back(t);
		}
	}
	if (rv) e.stack = std::move(tmp);
	return rv;
}

/* 
 * if force is true, treat OP_LOC records as OP_VAL (for omf)
 */

optional<uint32_t> evaluate_expression(expression &e, bool force) {
	std::vector<expr> tmp;
	for (const auto &t : e.stack) {
		if (t.tag == OP_LOC && force) {
			tmp.emplace_back(OP_VAL, t.value);
			continue;				
		}
		if (t.tag >= OP_BIN) {
			binary_op(t.tag, tmp);
		} else if (t.tag >= OP_UNA) {
			unary_op(t.tag, tmp);
		} else {
			tmp.emplace_back(t);
		}
	}

	if (tmp.size() == 1 && tmp.front().tag == OP_VAL) return optional<uint32_t>(tmp.front().value);
	return optional<uint32_t>();
}

