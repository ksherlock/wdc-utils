#ifndef __obj816_h__
#define __obj816_h__

#include <stdint.h>

#pragma pack(push, 1)

#define VERS " 3.01 "		/* version number for programs */
#define CDATE "1992-1997"		/* copyright date for programs */

typedef struct Mod_head {
	uint32_t h_magic;				/* magic number for detection */
	uint16_t h_version;			/* version number of object format */
	uint8_t h_filtyp;				/* file type, object or library */
	uint8_t h_namlen;				/* length of module name */
	uint32_t h_recsize;				/* sizeof records section */
	uint16_t h_secsize;			/* sizeof section section */
	uint32_t h_symsize;				/* sizeof symbol section */
	uint16_t h_optsize;			/* sizeof options section */
 	uint8_t h_tot_secs;	/*  total number of sections in module */
	uint8_t h_num_secs;	/*  number of sections referenced */
 	uint16_t h_num_syms;	/*  number of symbols */
} Mod_head;

typedef struct Lib_head {
	uint32_t l_magic;				/* magic number for detection */
	uint16_t l_version;			/* version number of object format */
	uint8_t l_filtyp;				/* file type, object or library */
	uint8_t l_unused1;
	uint32_t l_modstart;			/* offset of modules start */
	uint32_t l_numsyms;				/* number of symbol entries */
	uint32_t l_symsize;				/* sizeof symbol section */
 	uint32_t l_numfiles;			/* number of files */
} Lib_head;

#define MOD_CONVERT	"lwbblslsbbw"
#define LIB_CONVERT	"lwbbllll"

#define MOD_REC_OFF(x) (sizeof(x)+x.h_namlen)
#define MOD_SEC_OFF(x) (MOD_REC_OFF(x)+x.h_recsize)
#define MOD_SYM_OFF(x) (MOD_SEC_OFF(x)+x.h_secsize)
#define MOD_OPT_OFF(x) (MOD_SYM_OFF(x)+x.h_symsize)
#define MOD_NEXT_OFF(x) (MOD_OPT_OFF(x)+x.h_optsize)

#define MOD_MAGIC	0x5a44525a	/* 'ZRDZ' */
#define MOD_VERSION	1
#define MOD_OBJECT	1
#define MOD_LIBRARY	2
#define	MOD_OBJ68K	3

#define REC_END	0
/* 1-xx are numbers of constant data bytes */
#define REC_SECT	0xf0		/* next word is section number */
#define REC_EXPR	0xf1		/* expression follows */
#define REC_SPACE	0xf2		/* word count of bytes to reserve */
#define REC_ORG		0xf3		/* long word new pc */
#define REC_RELEXP	0xf4		/* pc-relative expression */
#define REC_DEBUG	0xf5		/* debug info record */
#define REC_LINE	0xf6		/* bump line counter */

enum {
		OP_END=0,					/* end of expression */
		OP_SYM,						/* ref to extern symbol */
		OP_VAL,						/* constant value */
		OP_LOC,						/* ref to offset from start of section */

		OP_UNA=10,
		OP_NOT=10,
		OP_NEG,
		OP_FLP,

		OP_BIN=20,
			OP_EXP=20, OP_MUL, OP_DIV, OP_MOD, OP_SHR,
			OP_SHL, OP_ADD, OP_SUB, OP_AND, OP_OR,
			OP_XOR, OP_EQ, OP_GT, OP_LT, OP_UGT,
			OP_ULT,
			OP_LAST
};

enum { S_UND, S_ABS, S_REL, S_EXP, S_REG, S_FREG };		/* symbol type */
enum { ST_NOSIZE, ST_8BIT, ST_16BIT, ST_32BIT,
		ST_FLOAT, ST_DOUBLE, ST_8051, ST_Z8, ST_DS, ST_EQU };

enum {
		D_C_FILE=100,
		D_C_LINE,
		D_C_SYM,
		D_C_STAG,
		D_C_ETAG,
		D_C_UTAG,
		D_C_MEMBER,
		D_C_EOS,
		D_C_FUNC,
		D_C_ENDFUNC,
		D_C_BLOCK,
		D_C_ENDBLOCK,
		D_LONGA_ON,
		D_LONGA_OFF,
		D_LONGI_ON,
		D_LONGI_OFF
};

/* used for generating source level debugging information */

enum { DT_NON, DT_PTR, DT_FCN, DT_ARY, DT_FPTR, DT_FFCN };
enum { T_NULL, T_VOID, T_SCHAR, T_CHAR, T_SHORT, T_INT16, T_INT32, T_LONG,
		T_FLOAT, T_DOUBLE, T_STRUCT, T_UNION, T_ENUM, T_LDOUBLE,
		T_UCHAR, T_USHORT, T_UINT16, T_UINT32, T_ULONG };
enum { C_NULL, C_AUTO, C_EXT, C_STAT, C_REG, C_EXTDEF, C_ARG,
		C_STRTAG, C_MOS, C_EOS, C_UNTAG, C_MOU, C_ENTAG, C_MOE,
		C_TPDEF, C_USTATIC, C_REGPARM, C_FIELD, C_UEXT, C_STATLAB,
		C_EXTLAB, C_BLOCK, C_EBLOCK, C_FUNC, C_EFUNC, C_FILE, C_LINE,
		C_FRAME };


#define SF_GBL	0x01	/* label is global */
#define SF_DEF	0x02	/* label is defined in this module */
#define SF_REF	0x04	/* label is referenced in this module */
#define SF_VAR	0x08	/* label is variable */
#define SF_PG0	0x10	/* label is Page0 type */
#define SF_TMP	0x20	/* label is temporary (LLCHAR) */
#define	SF_DEF2	0x40	/* label has been defined in pass 2 ( ZAS only ) */
#define SF_LIB	0x40	/* label in library (used by ZLN) */

#define SEC_OFFSET		0x0001
#define SEC_INDIRECT	0x0002
#define SEC_STACKED		0x0004
#define SEC_REF_ONLY	0x0008
#define SEC_CONST		0x0010
#define SEC_DIRECT		0x0020
#define SEC_NONAME		0x0040
#define SEC_DATA		0x0080

			/* pre-defined sections */
enum {SECT_PAGE0, SECT_CODE, SECT_KDATA, SECT_DATA, SECT_UDATA };

/*
	Module format:
		Module header
		s: Module name (null terminated)
		Records
			Each record is in stack order terminated by REC_END
		Section info
			Section info format: --- for each section that has references
				b: section number
				b: section flags
				l: size
				l: org
				s: name of section	(only if SEC_NONAME not in flags)
		Symbol info
			Symbol info format: --- for each symbol
				b: type
				b: flags
				b: section number
				l: offset (only if type != S_UND)
				s: name of symbol (null terminated)

	Library format:
		Library header
		File info - for each file
			w: file number
			b: file name len
			c: file name (no null)
		Symbol data - for each symbol
			w: offset of name
			w: file number
			l: module offset - Hdr.l_modstart
		Symbol names - for each symbol
			b: length of name
			c: symbol name (no null)
		Modules - each module
*/
 

/**************************************************/
/*    End of File OBJ816.H                        */
/**************************************************/

#pragma pack(pop)

#endif