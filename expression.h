#ifndef __expression_h__
#define __expression_h__

#include <stdint.h>
#include <vector>

struct expr {
	expr(int t = 0, uint32_t v = 0, uint32_t s = 0)
		: tag(t), value(v), section(s)
	{}
	int tag = 0;
	uint32_t value = 0;
	unsigned section = 0;
};


struct expression {
	unsigned section = 0;
	uint32_t offset = 0;
	uint8_t size = 0;
	uint8_t relative = false;
	bool undefined = false;

	std::vector<expr> stack;

};

uint32_t evaluate_expression(expression &e, bool force = false);

bool simplify_expression(expression &e);

#endif
