	brk	$12
	ora	(<$12,x)
	cop	$12
	ora	<$12,s
	tsb	<$12
	ora	<$12
	asl	<$12
	ora	[<$12]
	php
	ora	#$1234
	asl
	phd
	tsb	|$1234
	ora	|$1234
	asl	|$1234
	ora	>$123456
	;
	bpl	*+2
	ora	(<$12),y
	ora	(<$12)
	ora	(<$12,s),y
	trb	<$12
	ora	<$12,x
	asl	<$12,x
	ora	[<$12],y
	clc
	ora	|$1234,y
	inc
	tcs
	trb	|$1234
	ora	|$1234,x
	asl	|$1234,x
	ora	>$123456,x
	;
	jsr	|$1234
	and	(<$12,x)
	jsl	>$123456
	and	<$12,s
	bit	<$12
	and	<$12
	rol	<$12
	and	[<$12]
	plp
	and	#$1234
	rol
	pld
	bit	|$1234
	and	|$1234
	rol	|$1234
	and	>$123456
	;
	bmi	*+2
	and	(<$12),y
	and	(<$12)
	and	(<$12,s),y
	bit	<$12,x
	and	<$12,x
	rol	<$12,x
	and	[<$12],y
	sec
	and	|$1234,y
	dec
	tsc
	bit	|$1234,x
	and	|$1234,x
	rol	|$1234,x
	and	>$123456,x
	;
	rti
	eor	(<$12,x)
	wdm	$12
	eor	<$12,s
	mvp	$12,$34
	eor	<$12
	lsr	<$12
	eor	[<$12]
	pha
	eor	#$1234
	lsr
	phk
	jmp	|$1234
	eor	|$1234
	lsr	|$1234
	eor	>$123456
	;
	bvc	*+2
	eor	(<$12),y
	eor	(<$12)
	eor	(<$12,s),y
	mvn	$12,$34
	eor	<$12,x
	lsr	<$12,x
	eor	[<$12],y
	cli
	eor	|$1234,y
	phy
	tcd
	jml	>$123456
	eor	|$1234,x
	lsr	|$1234,x
	eor	>$123456,x
	;
	rts
	adc	(<$12,x)
	per	*+3
	adc	<$12,s
	stz	<$12
	adc	<$12
	ror	<$12
	adc	[<$12]
	pla
	adc	#$1234
	ror
	rtl
	jmp	(|$1234)
	adc	|$1234
	ror	|$1234
	adc	>$123456
	;
	bvs	*+2
	adc	(<$12),y
	adc	(<$12)
	adc	(<$12,s),y
	stz	<$12,x
	adc	<$12,x
	ror	<$12,x
	adc	[<$12],y
	sei
	adc	|$1234,y
	ply
	tdc
	jmp	(|$1234,x)
	adc	|$1234,x
	ror	|$1234,x
	adc	>$123456,x
	;
	bra	*+2
	sta	(<$12,x)
	brl	*+3
	sta	<$12,s
	sty	<$12
	sta	<$12
	stx	<$12
	sta	[<$12]
	dey
	bit	#$1234
	txa
	phb
	sty	|$1234
	sta	|$1234
	stx	|$1234
	sta	>$123456
	;
	bcc	*+2
	sta	(<$12),y
	sta	(<$12)
	sta	(<$12,s),y
	sty	<$12,x
	sta	<$12,x
	stx	<$12,y
	sta	[<$12],y
	tya
	sta	|$1234,y
	txs
	txy
	stz	|$1234
	sta	|$1234,x
	stz	|$1234,x
	sta	>$123456,x
	;
	ldy	#$1234
	lda	(<$12,x)
	ldx	#$1234
	lda	<$12,s
	ldy	<$12
	lda	<$12
	ldx	<$12
	lda	[<$12]
	tay
	lda	#$1234
	tax
	plb
	ldy	|$1234
	lda	|$1234
	ldx	|$1234
	lda	>$123456
	;
	bcs	*+2
	lda	(<$12),y
	lda	(<$12)
	lda	(<$12,s),y
	ldy	<$12,x
	lda	<$12,x
	ldx	<$12,y
	lda	[<$12],y
	clv
	lda	|$1234,y
	tsx
	tyx
	ldy	|$1234,x
	lda	|$1234,x
	ldx	|$1234,y
	lda	>$123456,x
	;
	cpy	#$1234
	cmp	(<$12,x)
	rep	#$1234
	cmp	<$12,s
	cpy	<$12
	cmp	<$12
	dec	<$12
	cmp	[<$12]
	iny
	cmp	#$1234
	dex
	wai
	cpy	|$1234
	cmp	|$1234
	dec	|$1234
	cmp	>$123456
	;
	bne	*+2
	cmp	(<$12),y
	cmp	(<$12)
	cmp	(<$12,s),y
	pei	(<$12)
	cmp	<$12,x
	dec	<$12,x
	cmp	[<$12],y
	cld
	cmp	|$1234,y
	phx
	stp
	jml	[|$1234]
	cmp	|$1234,x
	dec	|$1234,x
	cmp	>$123456,x
	;
	cpx	#$1234
	sbc	(<$12,x)
	sep	#$1234
	sbc	<$12,s
	cpx	<$12
	sbc	<$12
	inc	<$12
	sbc	[<$12]
	inx
	sbc	#$1234
	nop
	xba
	cpx	|$1234
	sbc	|$1234
	inc	|$1234
	sbc	>$123456
	;
	beq	*+2
	sbc	(<$12),y
	sbc	(<$12)
	sbc	(<$12,s),y
	pea	|$1234
	sbc	<$12,x
	inc	<$12,x
	sbc	[<$12],y
	sed
	sbc	|$1234,y
	plx
	xce
	jsr	(|$1234,x)
	sbc	|$1234,x
	inc	|$1234,x
	sbc	>$123456,x
	;
