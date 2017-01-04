
	;
	; 1-segment code.
	;
	include 'hello.macros'

	macdelim {

	CODE
	pea #^text
	pea #text
	_WriteLine

	rtl

text
	pstr {'hello, world'}
	ends

	
