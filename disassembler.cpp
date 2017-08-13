#include "disassembler.h"
#include <stdio.h>
#include <string.h>
#include <string>

static constexpr const char opcodes[] = 
	"brkoracoporatsboraaslora"
	"phporaaslphdtsboraaslora"
	"bploraoraoratrboraaslora"
	"clcorainctcstrboraaslora"
	"jsrandjslandbitandroland"
	"plpandrolpldbitandroland"
	"bmiandandandbitandroland"
	"secanddectscbitandroland"
	"rtieorwdmeormvpeorlsreor"
	"phaeorlsrphkjmpeorlsreor"
	"bvceoreoreormvneorlsreor"
	"clieorphytcdjmleorlsreor"
	"rtsadcperadcstzadcroradc"
	"plaadcrorrtljmpadcroradc"
	"bvsadcadcadcstzadcroradc"
	"seiadcplytdcjmpadcroradc"
	"brastabrlstastystastxsta"
	"deybittxaphbstystastxsta"
	"bccstastastastystastxsta"
	"tyastatxstxystzstastzsta"
	"ldyldaldxldaldyldaldxlda"
	"tayldataxplbldyldaldxlda"
	"bcsldaldaldaldyldaldxlda"
	"clvldatsxtyxldyldaldxlda"
	"cpycmprepcmpcpycmpdeccmp"
	"inycmpdexwaicpycmpdeccmp"
	"bnecmpcmpcmppeicmpdeccmp"
	"cldcmpphxstpjmlcmpdeccmp"
	"cpxsbcsepsbccpxsbcincsbc"
	"inxsbcnopxbacpxsbcincsbc"
	"beqsbcsbcsbcpeasbcincsbc"
	"sedsbcplxxcejsrsbcincsbc"
	;

static constexpr const int mImplied =      0x0000;
static constexpr const int mImmediate =    0x1000;
static constexpr const int mAbsolute =     0x2000;
static constexpr const int mAbsoluteI =    0x3000;
static constexpr const int mAbsoluteIL =   0x4000;
static constexpr const int mAbsoluteLong = 0x5000;
static constexpr const int mDP =           0x6000;
static constexpr const int mDPI =          0x7000;
static constexpr const int mDPIL =         0x8000;
static constexpr const int mRelative =     0x9000;
static constexpr const int mBlockMove =    0xa000;
static constexpr const int mImpliedA =     0xb000; // inc a, dec a, etc.

static constexpr const int m_S =          0x0100;
static constexpr const int m_X =          0x0200;
static constexpr const int m_Y =          0x0400;

static constexpr const int m_M =          0x0020;
static constexpr const int m_I =          0x0010;

static constexpr const int modes[] =
{
	1 | mAbsolute,              // 00 brk #imm
	1 | mDPI | m_X,             // 01 ora (dp,x)
	1 | mAbsolute,              // 02 cop #imm
	1 | mDP | m_S,              // 03 ora ,s
	1 | mDP,                    // 04 tsb <dp
	1 | mDP,                    // 05 ora <dp
	1 | mDP,                    // 06 asl <dp
	1 | mDPIL,                  // 07 ora [dp]
	0 | mImplied,               // 08 php
	1 | mImmediate | m_M,       // 09 ora #imm
	0 | mImpliedA,              // 0a asl a
	0 | mImplied,               // 0b phd
	2 | mAbsolute,              // 0c tsb |abs
	2 | mAbsolute,              // 0d ora |abs
	2 | mAbsolute,              // 0e asl |abs
	3 | mAbsoluteLong,          // 0f ora >abs

	1 | mRelative,              // 10 bpl
	1 | mDPI | m_Y,             // 11 ora (dp),y
	1 | mDPI,                   // 12 ora (dp)
	1 | mDPI | m_S | m_Y,       // 13 ora ,s,y
	1 | mDP,                    // 14 trb <dp
	1 | mDP | m_X,              // 15 ora <dp,x
	1 | mDP | m_X,              // 16 asl <dp,x
	1 | mDPIL | m_Y,            // 17 ora [dp],y
	0 | mImplied,               // 18 clc
	2 | mAbsolute | m_Y,        // 19 ora |abs,y
	0 | mImpliedA,              // 1a inc a
	0 | mImplied,               // 1b tcs
	2 | mAbsolute,              // 1c trb |abs
	2 | mAbsolute | m_X,        // 1d ora |abs,x
	2 | mAbsolute | m_X,        // 1e asl |abs,x
	3 | mAbsoluteLong | m_X,    // 1f ora >abs,x
	
	2 | mAbsolute,              // 20 jsr |abs
	1 | mDPI | m_X,             // 21 and (dp,x)
	3 | mAbsoluteLong,          // 22 jsl >abs
	1 | mDP | m_S,              // 23 and ,s
	1 | mDP,                    // 24 bit <dp
	1 | mDP,                    // 25 and <dp
	1 | mDP,                    // 26 rol <dp
	1 | mDPIL,                  // 27 and [dp]
	0 | mImplied,               // 28 plp
	1 | mImmediate | m_M,       // 29 and #imm
	0 | mImpliedA,              // 2a rol a
	0 | mImplied,               // 2b pld
	2 | mAbsolute,              // 2c bit |abs
	2 | mAbsolute,              // 2d and |abs
	2 | mAbsolute,              // 2e rol |abs
	3 | mAbsoluteLong,          // 2f and >abs
	
	1 | mRelative,              // 30 bmi 
	1 | mDPI | m_Y,             // 31 and (dp),y
	1 | mDPI,                   // 32 and (dp)
	1 | mDPI | m_S | m_Y,       // 33 and ,s,y
	1 | mDP | m_X,              // 34 bit dp,x
	1 | mDP | m_X,              // 35 and dp,x
	1 | mDP | m_X,              // 36 rol <dp,x
	1 | mDPIL | m_Y,            // 37 and [dp],y
	0 | mImplied,               // 38 sec
	2 | mAbsolute | m_Y,        // 39 and |abs,y
	0 | mImpliedA,              // 3a dec a
	0 | mImplied,               // 3b tsc
	2 | mAbsolute | m_X,        // 3c bits |abs,x
	2 | mAbsolute | m_X,        // 3d and |abs,x
	2 | mAbsolute | m_X,        // 3e rol |abs,x
	3 | mAbsoluteLong | m_X,    // 3f and >abs,x
	
	0 | mImplied,               // 40 rti
	1 | mDPI | m_X,             // 41 eor (dp,x)
	1 | mAbsolute,              // 42 wdm #imm
	1 | mDP | m_S,              // 43 eor ,s
	2 | mBlockMove,             // 44 mvp x,x
	1 | mDP,                    // 45 eor dp
	1 | mDP,                    // 46 lsr dp
	1 | mDPIL,                  // 47 eor [dp]
	0 | mImplied,               // 48 pha
	1 | mImmediate | m_M,       // 49 eor #imm
	0 | mImpliedA,              // 4a lsr a
	0 | mImplied,               // 4b phk
	2 | mAbsolute,              // 4c jmp |abs
	2 | mAbsolute,              // 4d eor |abs
	2 | mAbsolute,              // 4e lsr |abs
	3 | mAbsoluteLong,          // 4f eor >abs      
	
	1 | mRelative,                  // 50 bvc
	1 | mDPI | m_Y,                 // 51 eor (dp),y
	1 | mDPI,                       // 52 eor (dp)
	1 | mDPI | m_S | m_Y,           // 53 eor ,s,y
	2 | mBlockMove,                 // 54 mvn x,x
	1 | mDP | m_X,                  // 55 eor dp,x
	1 | mDP | m_X,                  // 56 lsr dp,x
	1 | mDPIL | m_Y,                // 57 eor [dp],y
	0 | mImplied,                   // 58 cli
	2 | mAbsolute | m_Y,            // 59 eor |abs,y
	0 | mImplied,                   // 5a phy
	0 | mImplied,                   // 5b tcd
	3 | mAbsoluteLong,              // 5c jml >abs
	2 | mAbsolute | m_X,            // 5d eor |abs,x
	2 | mAbsolute | m_X,            // 5e lsr |abs,x
	3 | mAbsoluteLong | m_X,        // 5f eor >abs,x

	0 | mImplied,                   // 60 rts
	1 | mDPI | m_X,                 // 61 adc (dp,x)
	2 | mRelative,                  // 62 per |abs
	1 | mDP | m_S,                  // 63 adc ,s
	1 | mDP,                        // 64 stz <dp
	1 | mDP,                        // 65 adc <dp
	1 | mDP,                        // 66 ror <dp
	1 | mDPIL,                      // 67 adc [dp]
	0 | mImplied,                   // 68 pla
	1 | mImmediate | m_M,           // 69 adc #imm
	0 | mImplied,                   // 6a ror a 
	0 | mImplied,                   // 6b rtl
	2 | mAbsoluteI,                 // 6c jmp (abs)
	2 | mAbsolute,                  // 6d adc |abs
	2 | mAbsolute,                  // 6e ror |abs
	3 | mAbsoluteLong,              // 6f adc >abs

	1 | mRelative,                  // 70 bvs
	1 | mDPI | m_Y,                 // 71 adc (dp),y
	1 | mDPI,                       // 72 adc (dp)
	1 | mDPI | m_S | m_Y,           // 73 adc ,s,y
	1 | mDP | m_X,                  // 74 stz dp,x
	1 | mDP | m_X,                  // 75 adc dp,x
	1 | mDP | m_X,                  // 76 ror dp,x
	1 | mDPIL | m_Y,                // 77 adc [dp],y
	0 | mImplied,                   // 78 sei
	2 | mAbsolute | m_Y,            // 79 adc |abs,y
	0 | mImplied,                   // 7a ply
	0 | mImplied,                   // 7b tdc
	2 | mAbsoluteI | m_X,           // 7c jmp (abs,x)
	2 | mAbsolute | m_X,            // 7d adc |abs,x
	2 | mAbsolute | m_X,            // 7e ror |abs,x
	3 | mAbsoluteLong | m_X,        // 7f adc >abs,x
	
	1 | mRelative,                  // 80 bra 
	1 | mDPI | m_X,                 // 81 sta (dp,x)
	2 | mRelative,                  // 82 brl |abs
	1 | mDP | m_S,                  // 83 sta ,s
	1 | mDP,                        // 84 sty <dp
	1 | mDP,                        // 85 sta <dp
	1 | mDP,                        // 86 stx <dp
	1 | mDPIL,                      // 87 sta [dp]
	0 | mImplied,                   // 88 dey
	1 | mImmediate | m_M,           // 89 bit #imm
	0 | mImplied,                   // 8a txa
	0 | mImplied,                   // 8b phb
	2 | mAbsolute,                  // 8c sty |abs
	2 | mAbsolute,                  // 8d sta |abs
	2 | mAbsolute,                  // 8e stx |abs
	3 | mAbsoluteLong,              // 8f sta >abs
	
	1 | mRelative,                  // 90 bcc
	1 | mDPI | m_Y,                 // 91 sta (dp),y
	1 | mDPI,                       // 92 sta (dp)
	1 | mDPI | m_S | m_Y,           // 93 sta ,s,y
	1 | mDP | m_X,                  // 94 sty dp,x
	1 | mDP | m_X,                  // 95 sta dp,x
	1 | mDP | m_Y,                  // 96 stx dp,y
	1 | mDPIL | m_Y,                // 97 sta [dp],y
	0 | mImplied,                   // 98 tya
	2 | mAbsolute | m_Y,            // 99 sta |abs,y
	0 | mImplied,                   // 9a txs
	0 | mImplied,                   // 9b txy
	2 | mAbsolute,                  // 9c stz |abs
	2 | mAbsolute | m_X,            // 9d sta |abs,x
	2 | mAbsolute | m_X,            // 9e stz |abs,x
	3 | mAbsoluteLong | m_X,        // 9f sta >abs,x
	
	1 | mImmediate | m_I,           // a0 ldy #imm
	1 | mDPI | m_X,                 // a1 lda (dp,x)
	1 | mImmediate | m_I,           // a2 ldx #imm
	1 | mDP | m_S,                  // a3 lda ,s
	1 | mDP,                        // a4 ldy <dp
	1 | mDP,                        // a5 lda <dp
	1 | mDP,                        // a6 ldx <dp
	1 | mDPIL,                      // a7 lda [dp]
	0 | mImplied,                   // a8 tay
	1 | mImmediate | m_M,           // a9 lda #imm
	0 | mImplied,                   // aa tax
	0 | mImplied,                   // ab plb
	2 | mAbsolute,                  // ac ldy |abs
	2 | mAbsolute,                  // ad lda |abs
	2 | mAbsolute,                  // ae ldx |abs
	3 | mAbsoluteLong,              // af lda >abs   
	
	1 | mRelative,                  // b0 bcs
	1 | mDPI | m_Y,                 // b1 lda (dp),y
	1 | mDPI,                       // b2 lda (dp)
	1 | mDPI | m_S | m_Y,           // b3 lda ,s,y
	1 | mDP | m_X,                  // b4 ldy <dp,x
	1 | mDP | m_X,                  // b5 lda <dp,x
	1 | mDP | m_Y,                  // b6 ldx <dp,y
	1 | mDPIL | m_Y,                // b7 lda [dp],y
	0 | mImplied,                   // b8 clv
	2 | mAbsolute | m_Y,            // b9 lda |abs,y
	0 | mImplied,                   // ba tsx
	0 | mImplied,                   // bb tyx
	2 | mAbsolute | m_X,            // bc ldy |abs,x
	2 | mAbsolute | m_X,            // bd lda |abs,x
	2 | mAbsolute | m_Y,            // be ldx |abs,y
	3 | mAbsoluteLong | m_X,        // bf lda >abs,x
	
	1 | mImmediate | m_I,           // c0 cpy #imm
	1 | mDPI | m_X,                 // c1 cmp (dp,x)
	1 | mImmediate,                 // c2 rep #
	1 | mDP | m_S,                  // c3 cmp ,s
	1 | mDP,                        // c4 cpy <dp
	1 | mDP,                        // c5 cmp <dp
	1 | mDP,                        // c6 dec <dp
	1 | mDPIL,                      // c7 cmp [dp]
	0 | mImplied,                   // c8 iny
	1 | mImmediate | m_M,           // c9 cmp #imm
	0 | mImplied,                   // ca dex
	0 | mImplied,                   // cb WAI
	2 | mAbsolute,                  // cc cpy |abs
	2 | mAbsolute,                  // cd cmp |abs
	2 | mAbsolute,                  // ce dec |abs
	3 | mAbsoluteLong,              // cf cmp >abs
	
	1 | mRelative,                  // d0 bne
	1 | mDPI | m_Y,                 // d1 cmp (dp),y
	1 | mDPI,                       // d2 cmp (dp)
	1 | mDPI | m_S | m_Y,           // d3 cmp ,s,y
	1 | mDP,                        // d4 pei (dp) --> pei <dp
	1 | mDP | m_X,                  // d5 cmp dp,x
	1 | mDP | m_X,                  // d6 dec dp,x
	1 | mDPIL | m_Y,                // d7 cmp [dp],y
	0 | mImplied,                   // d8 cld
	2 | mAbsolute | m_Y,            // d9 cmp |abs,y
	0 | mImplied,                   // da phx
	0 | mImplied,                   // db stp
	2 | mAbsoluteIL,                // dc jml [abs]
	2 | mAbsolute | m_X,            // dd cmp |abs,x
	2 | mAbsolute | m_X,            // de dec |abs,x
	3 | mAbsoluteLong | m_X,        // df cmp >abs,x
	
	1 | mImmediate | m_I,           // e0 cpx #imm
	1 | mDPI | m_X,                 // e1 sbc (dp,x)
	1 | mImmediate,                 // e2 sep #imm
	1 | mDP | m_S,                  // e3 sbc ,s
	1 | mDP,                        // e4 cpx <dp
	1 | mDP,                        // e5 sbc <dp
	1 | mDP,                        // e6 inc <dp
	1 | mDPIL,                      // e7 sbc [dp]
	0 | mImplied,                   // e8 inx
	1 | mImmediate| m_M,            // e9 sbc #imm
	0 | mImplied,                   // ea nop
	0 | mImplied,                   // eb xba
	2 | mAbsolute,                  // ec cpx |abs
	2 | mAbsolute,                  // ed abc |abs
	2 | mAbsolute,                  // ee inc |abs
	3 | mAbsoluteLong,              // ef sbc >abs
	
	1 | mRelative,                  // f0 beq
	1 | mDPI | m_Y,                 // f1 sbc (dp),y
	1 | mDPI,                       // f2 sbc (dp)
	1 | mDPI | m_S | m_Y,           // f3 sbc ,s,y
	2 | mAbsolute,                 // f4 pea |abs --> pea #imm
	1 | mDP | m_X,                  // f5 sbc dp,x
	1 | mDP | m_X,                  // f6 inc dp,x
	1 | mDPIL | m_Y,                // f7 sbc [dp],y
	0 | mImplied,                   // f8 sed
	2 | mAbsolute | m_Y,            // f9 sbc |abs,y
	0 | mImplied,                   // fa plx
	0 | mImplied,                   // fb xce
	2 | mAbsoluteI | m_X,           // fc jsr (abs,x)
	2 | mAbsolute | m_X,            // fd sbc |abs,x
	2 | mAbsolute | m_X,            // fe inc |abs,x
	3 | mAbsoluteLong | m_X,        // ff sbc >abs,x      

};

static bool branchlike(uint8_t op) {

	switch(op) {
		case 0x10: // bpl
		//case 0x20: // jsr
		//case 0x22: // jsl
		case 0x30: // bmi
		case 0x40: // rti
		case 0x4c: // jmp
		case 0x50: // bvc
		case 0x5c: // jml
		case 0x60: // rts
		case 0x6b: // rtl
		case 0x6c: // jmp
		case 0x70: // bvs
		case 0x7c: // jmp
		case 0x80: // bra
		case 0x82: // brl
		case 0x90: // bcc
		case 0xb0: // bcs
		case 0xd0: // bne
		case 0xdc: // jml
		case 0xf0: // beq
		//case 0xfc: // jsr
			return true;
		default:
			return false;
	}

}


constexpr const int kOpcodeTab = 20;
constexpr const int kOperandTab = 30;
constexpr const int kCommentTab = 80;


void indent_to(std::string &line, unsigned position) {
	if (line.length() < position)
		line.resize(position, ' ');
}


disassembler::~disassembler() {
}

std::string disassembler::to_x(uint32_t x, unsigned bytes, char prefix) {
	std::string s;
	char buffer[16];
	if (prefix) s.push_back(prefix);

	if (x > 0xff && bytes < 4) bytes = 4;
	if (x > 0xffff && bytes < 6) bytes = 6;
	if (x > 0xffffff && bytes < 8) bytes = 8;

	memset(buffer, '0', sizeof(buffer));
	int i = 16;
	while (x) {
		buffer[--i] = "0123456789abcdef"[x & 0x0f];
		x >>= 4;
	}

	s.append(buffer + 16 - bytes, buffer + 16);

	return s;
}

int disassembler::operand_size(uint8_t op, bool m, bool x) {
	unsigned mode = modes[op];
	unsigned size = mode & 0x0f;
	if ((mode & m_I) && x) size++; 
	if ((mode & m_M) && m) size++;
	return size;
}



void disassembler::emit(const std::string &label) {
	fputs(label.c_str(), stdout);
	fputc('\n', stdout);
}

void disassembler::emit(const std::string &label, const std::string &opcode) {
	std::string tmp;
	tmp = label;

	if (!opcode.empty()) {
		indent_to(tmp, kOpcodeTab);
		tmp += opcode;
	}

	tmp.push_back('\n');
	fputs(tmp.c_str(), stdout);
}



void disassembler::emit(const std::string &label, const std::string &opcode, const std::string &operand) {

	std::string tmp;
	tmp = label;


	if (!opcode.empty()) {
		indent_to(tmp, kOpcodeTab);
		tmp += opcode;
	}

	if (!operand.empty()) {
		indent_to(tmp, kOperandTab);
		tmp += operand;
	}

	tmp.push_back('\n');
	fputs(tmp.c_str(), stdout);
}


void disassembler::emit(const std::string &label, const std::string &opcode, const std::string &operand, const std::string &comment) {

	std::string tmp;
	tmp = label;

	if (!opcode.empty()) {
		indent_to(tmp, kOpcodeTab);
		tmp += opcode;
	}

	if (!operand.empty()) {
		indent_to(tmp, kOperandTab);
		tmp += operand;
	}

	if (!comment.empty()) {
		indent_to(tmp, kCommentTab);
		tmp += "; ";
		tmp += comment;	
	}

	tmp.push_back('\n');
	fputs(tmp.c_str(), stdout);
}





void disassembler::reset() {
	_arg = 0;
	_st = 0;
}


std::pair<std::string, std::string> 
disassembler::format_data(unsigned size, const uint8_t *data) {

	std::string tmp;

	for (unsigned i = 0; i < size; ++i) {
		if (i > 0) tmp += ", ";
		tmp += to_x(data[i], 2, '$');
	}

	return std::make_pair("db", tmp);
}

std::pair<std::string, std::string> 
disassembler::format_data(unsigned size, const std::string &data) {

	switch(size) {
		case 1: return std::make_pair("db", data);
		case 2: return std::make_pair("dw", data);
		case 3: return std::make_pair("da", data);
		case 4: return std::make_pair("dl", data);

		default: { 
			std::string tmp;
			tmp = std::to_string(size) + " bytes";
			return std::make_pair(tmp, data);

		}
	}
}

void disassembler::dump() {

	std::string line;

	if (!_st) return;


	auto p = format_data(_st, _bytes);

	indent_to(line, kOpcodeTab);
	line += p.first;
	indent_to(line, kOperandTab);
	line += p.second;

	hexdump(line);
	line.push_back('\n');
	fputs(line.c_str(), stdout);

	_pc += _st;
	reset();
}

void disassembler::dump(const std::string &expr, unsigned size, uint32_t value) {

	std::string line;

	if (_st) dump();

	for (_st = 0; _st < size; ++_st) {
		_bytes[_st] = value & 0xff;
		value >>= 8;
	}

	auto p = format_data(size, expr);

	indent_to(line, kOpcodeTab);
	line += p.first;
	indent_to(line, kOperandTab);
	line += p.second;

	hexdump(line);
	line.push_back('\n');
	fputs(line.c_str(), stdout);

	_pc += _st;
	reset();
}

void disassembler::flush() {
	if (_st) dump();
	check_labels();
}


void disassembler::check_labels() {

	if ( _next_label >= 0 && _pc + _st >= _next_label) {
		//flush(); // -- too recursive.  see above.
		if (_st) dump();
		_next_label = next_label(_pc);
	}

}


void disassembler::space(unsigned size) {
	flush();

	std::string line;

	indent_to(line, kOpcodeTab);
	line += ds();
	indent_to(line, kOperandTab);


	while (size) {
		uint32_t chunk;
		if (_next_label == -1) chunk = size;
		else {
			chunk = _next_label - _pc;
			chunk = std::min(chunk, size);
		}

		line.resize(kOperandTab);
		line += std::to_string(chunk);
		indent_to(line, kCommentTab);
		line += "; ";

		line += to_x(_pc, 4);
		line.push_back(':');
		line.push_back('\n');
		fputs(line.c_str(), stdout);	


		_pc += chunk;
		size -= chunk;
		if (_next_label == _pc) _next_label = next_label(_pc);
	}


}


void disassembler::operator()(const std::string &expr, unsigned size, uint32_t value) {

	if (_st == 0 || !_code)
		check_labels();

	if (!_code) {
		dump(expr, size, value);
		if (_inline_data) {
			_inline_data -= size;
			if (_inline_data <= 0) _code = true; 
		}
		return;
	}

	if (_st != 1 || size != _size) {
		dump(expr, size, value);
		return;
	}
	for (int i = 0; i < size; ++i) {
		_bytes[_st++] = value & 0xff;
		value >>= 8;
	}
	print(expr);
}



void disassembler::operator()(uint8_t byte) {

	if (_st == 0 || !_code)
		check_labels();

	if (!_code) {
		_bytes[_st++] = byte;

		if (_inline_data && --_inline_data <= 0) {
			dump();
			_code = true;
			return;
		}

		if (_st == 4) dump();
		return;
	}

	_bytes[_st++] = byte;
	if (_st == 1) {
		_op = byte;
		_mode = modes[_op];

		// bit hack
		if (_traits & bit_hacks && _op == 0x2c) {
			if (_next_label == _pc + 1) {
				dump();
				return;
			}
		}

		if (_traits & pea_immediate && _op == 0xf4) _mode = 2 | mImmediate;
		_size = _mode & 0x0f;
		if (_mode & _flags & m_I) _size++;
		if (_mode & _flags & m_M) _size++;

		if (!_size) {
			print();

			if (branchlike(byte)) fputs("\n", stdout);
		}
		return;
	}
	unsigned shift = (_st - 2) * 8;
	_arg = _arg + (byte << shift);
	if (_st <= _size) return;

	uint8_t op = _op;
	uint32_t arg = _arg;

	if (_traits & track_rep_sep) {
		switch(_op) {
			case 0xc2: // REP
				_flags |= (_arg & 0x30);
				break;
			case 0xe2: // SEP
				_flags &= ~(_arg & 0x30);
				break;
		}
	}

	// all done... now print it.
	print();

	if (branchlike(op)) fputs("\n", stdout);

	// todo -- subscribe to before/after events...
	switch(op) {
		case 0xc2:
		case 0xe2:
		case 0x22:
		case 0x5c:
		case 0xdc:
			event(op, arg);
			break;
	}



}


std::string disassembler::prefix() {

	std::string tmp;

	switch(_mode & 0xf000) {
		case mImmediate: tmp = "#"; break;
		case mDP: tmp = "<"; break;
		case mDPI: tmp = "(<"; break;
		case mDPIL: tmp = "[<"; break;
		case mAbsoluteIL: tmp = "["; break;

		case mRelative:
		case mBlockMove:
			break;

		// cop, brk are treated as absolute.
		case mAbsolute: 
			//if (_size == 1) printf("\t");
			if (_size > 1) tmp = "|";
			break;
		case mAbsoluteLong: tmp = ">"; break;
		case mAbsoluteI: tmp = "("; break;
	}
	return tmp;
}



std::string disassembler::suffix() {

	std::string tmp;

	switch(_mode & 0x0f00) {
		case m_X: tmp = ",x"; break;
		case m_Y: if (!(_mode & (mDPI|mDPIL))) tmp = ",y"; break;
		case m_S:
		case m_S | m_Y:
			tmp = ",s"; break;
	}

	switch(_mode & 0xf000) {
		case mAbsoluteI:
		case mDPI:
			tmp += ")"; break;
		case mAbsoluteIL:
		case mDPIL:
			tmp += "]"; break;
	}

	// (xxx,s),y
	// (xxx),y
	// [xxx],y
	switch(_mode & 0x0f00) {
		case m_Y:
			if (_mode & (mDPI|mDPIL)) tmp += ",y"; break;
		case m_S | m_Y:
			tmp += ",y"; break;
	}	
	return tmp;

}


void disassembler::hexdump(std::string &line) {
	// print pc and hexdump...

	indent_to(line, kCommentTab);
	line += "; ";

	line += to_x(_pc, 4);
	line.push_back(':');

	int i;
	for (i = 0; i < _st; ++i) {
		line.push_back(' ');
		line += to_x(_bytes[i], 2);
	}
	for ( ; i < 4; ++i) {
		line += "   ";
	}
	line += "  ";
	for (i = 0; i < _st; ++i) {
		uint8_t c = _bytes[i];
		// msb flag?
		if (_traits & msb_hexdump) c &= 0x7f;
		if (isprint(c) && isascii(c)) line += c;
		else line += '.';
	}
}


std::string disassembler::label_for_address(uint32_t address) { return ""; }
std::string disassembler::label_for_zp(uint32_t address) { return ""; }

void disassembler::print() {


	if (_size) {
		std::string tmp;

		switch(_mode & 0xf000) {
			case mRelative: {

				uint32_t pc = _pc + 1 + _size + _arg;

				if ((_size == 1) && (_arg & 0x80))
					pc += 0xff00;
				pc &= 0xffff;


				// it would be really fancy if it checked for a label name @pc...
				tmp = label_for_address(pc);
				if (tmp.empty()) tmp = to_x(pc, 4, '$');
				break;
			}
			case mBlockMove: {
				// todo -- verify order.
				tmp = to_x((_arg >> 8) & 0xff, 2, '$')
					+ ","
					+ to_x((_arg >> 0) & 0xff, 2, '$');
				break;
			}
			case mDP:
			case mDPI:
			case mDPIL:
				tmp = label_for_zp(_arg);
				if (tmp.empty()) tmp = to_x(_arg, _size * 2, '$');
				break;

			//case mImmediate:
			case mAbsolute:
			case mAbsoluteI:
			case mAbsoluteIL:
			case mAbsoluteLong:
				tmp = label_for_address(_arg);
				if (tmp.empty()) tmp = to_x(_arg, _size * 2, '$');
				break;

			default:
				tmp = to_x(_arg, _size * 2, '$');
				break;
		}
		print(tmp);
		return;

	}

	std::string line;




	indent_to(line, kOpcodeTab);
	line.append(&opcodes[_op * 3], 3);

	if (_mode == mImpliedA && (_traits & explicit_implied_a)) {
		indent_to(line, kOperandTab);
		line.append("a");
	}


	hexdump(line);
	line.push_back('\n');
	fputs(line.c_str(), stdout);
	_pc += _size + 1;
	reset();
}

void disassembler::print(const std::string &expr) {


	std::string line;

	indent_to(line, kOpcodeTab);
	line.append(&opcodes[_op * 3], 3);

	if (_size) {

		indent_to(line, kOperandTab);
		line += prefix();
		line += expr;
		line += suffix();
	}

	hexdump(line);
	line.push_back('\n');
	fputs(line.c_str(), stdout);

	_pc += _size + 1;
	reset();	
}


#pragma mark -

const std::vector<uint32_t> &analyzer::finish() {
	std::sort(_labels.begin(), _labels.end());
	auto end = std::unique(_labels.begin(), _labels.end());
	_labels.erase(end, _labels.end());
	return _labels;
}


void analyzer::operator()(uint8_t byte) {


	if (!_code) {

		if (_inline_data && --_inline_data <= 0) {
			_code = true;
			reset();
			return;
		}
		return;
	}

	_st++;
	if (_st == 1) {
		_op = byte;
		_mode = modes[_op];

		_size = _mode & 0x0f;
		if (_mode & _flags & m_I) _size++;
		if (_mode & _flags & m_M) _size++;

		if (!_size) { reset(); }
		return;
	}
	unsigned shift = (_st - 2) * 8;
	_arg = _arg + (byte << shift);
	if (_st <= _size) return;


	// all done... now process it
	process();
}

void analyzer::reset() {
	_pc += _st;
	_arg = 0;
	_st = 0;
}

void analyzer::process() {

	if (_traits & disassembler::track_rep_sep) {
		switch(_op) {
			case 0xc2: // REP
				_flags |= (_arg & 0x30);
				break;
			case 0xe2: // SEP
				_flags &= ~(_arg & 0x30);
				break;
		}
	}

	switch (_mode & 0xf000) {
		case mRelative: {

			uint32_t pc = _pc + 1 + _size + _arg;

			if ((_size == 1) && (_arg & 0x80))
				pc += 0xff00;
			pc &= 0xffff;

			_labels.push_back(pc);
			break;
		}
		case mAbsolute:
		case mAbsoluteI:
		case mAbsoluteLong: {
			_labels.push_back(_arg);
			break;
		}
	}
	reset();
}
