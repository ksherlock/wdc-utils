	; test relative expressions.

	module code1
	globals on
	externs on
branchb
	nop
	nop
	endmod

	module code2
	globals on
	externs on
	jmp branchb
	bra branchb
	brl branchb
	per branchb
	;
	nop
	nop
	;
	jmp branchf
	bra branchf
	brl branchf
	per branchf

	endmod

	module code3
	globals on
	externs on
branchf
	nop
	nop
	nop
	rtl
	endmod
