
	;
	; test ref-only sections (PAGE0, UDATA)
	;
	;

	module part_1

	globals on
	externs on

	page0
page0_start

offset_0	ds 2
offset_2	ds 4
	ends

	udata

uoffset_0	ds 2
uoffset_4	ds 4
			ds 1024-4
	ends

	code
	longa on
	longi on
	lda offset_0
	lda offset_2
	lda uoffset_0
	lda uoffset_4
	ends


	endmod

	module part_2

	globals on
	externs on


	page0
uoffset_6	ds 2
uoffset_8	ds 4
	ends

	udata
uoffset_1024 ds 1024
uoffset_2048 ds 1024
	ends

	code
	longa on
	longi on

	lda #_BEG_PAGE0
	lda #_END_PAGE0
	lda #_BEG_UDATA
	lda #_END_UDATA
	lda #_BEG_DATA
	lda #_END_DATA
	ends


	endmod
