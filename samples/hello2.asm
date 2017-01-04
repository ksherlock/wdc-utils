
	;
	; 2-segment code.
	;
	include 'hello.macros'

	macdelim {

	CODE
	pea #^text
	pea #text
	_WriteLine

	rtl
	ends

	KDATA
text
	pstr {'hello, world'}
	ends

	
