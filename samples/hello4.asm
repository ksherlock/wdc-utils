
	;
	; 2-segment, 2 module code.
	; with intersegment linkage.

	include 'hello.macros'

	macdelim {

	module code
	CODE
	extern text
	pea #^text
	pea #text
	_WriteLine

	rtl
	ends
	endmod

	module data
XDATA	SECTION
	public text
text
	pstr {'hello, world'}
	ends
	endmod

	
