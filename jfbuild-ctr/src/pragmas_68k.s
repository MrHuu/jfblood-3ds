;
; written by Cosmos
;

; muls.l A,A:B
mulsAAB    MACRO
_muls\1\1\2:
	fmove.l  \2,fp0
	muls.l   \1,\2
	bvc.b    .prosmall                ; if no overflow
	fmul.s   #$2F800000,fp0        ; (1/4294967295)
	fmul.l   \1,fp0
	fmove.l  fp0,\1
	rts
.prosmall
	smi      \1                ; if negative
	extb.l   \1
	rts

	even

	EndM

	mulsAAB d1,d0
	mulsAAB d3,d2
	mulsAAB d5,d4

; divs.l A,B:C
; C = dividend.hi
; B = dividend.lo
; A = divisor
; -> C = quotient
divsABC    MACRO
_divs\1\2\3:
;	tst.l    \2
;	bne.b    .fpudivision
;	cmp.l    \3,\1                ; divisor ? dividend.lo
;	bhi.b    .nodivision
;	divul.l    \1,\2:\3            ; 32/32 = 32r:32q
;.nodivision
;	rts
;.fpudivision
	fmove.l  \2,fp0
	fmul.s   #4294967296,fp0
	fmove.l  \3,fp1
	tst.l    \3
	bge.b    .skip
	fadd.s   #4294967296,fp1
.skip
	fadd.x   fp1,fp0
	fdiv.l   \1,fp0
	fintrz.x fp0
	fmove.l  fp0,\3            ; quotient
	rts

	even

	EndM

	divsABC d2,d1,d0
	divsABC d1,d3,d0
	divsABC d1,d2,d0
	divsABC d1,d0,d2

;
; get/set the FPU control register
;

	XDEF    _setfpcr
_setfpcr:
	ifd M68060
		fmove.l    d0,fpcr
	endif
	rts

	even

	XDEF    _getfpcr
_getfpcr:
	ifnd M68060
		moveq    #0,d0
	else
		fmove.l    fpcr,d0
	endif
	rts

	even

;
; Dante/Oxyron 2003
;

;-----------------------------------------------------------------------------
; scale
;-----------------------------------------------------------------------------
	XDEF    _scale
_scale:
	move.l  d2,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
		divs.l  d2,d1:d0
	else
		bsr _mulsd1d1d0
		bsr _divsd2d1d0
	endif

	move.l  (sp)+,d2
	rts

	even

;-----------------------------------------------------------------------------
; mulscale
;-----------------------------------------------------------------------------
	XDEF    _mulscale
_mulscale:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
	else
		bsr _mulsd1d1d0
	endif
	moveq   #32,d3
	sub.l   d2,d3
	lsr.l   d2,d0
	lsl.l   d3,d1
	or.l    d1,d0

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even

;-----------------------------------------------------------------------------
; divscale
;-----------------------------------------------------------------------------
	XDEF _divscale
_divscale:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	move.l  d0,d3
	lsl.l   d2,d0
	neg.b   d2
	and.b   #$1f,d2
	asr.l   d2,d3
	ifnd M68060
		divs.l  d1,d3:d0
	else
		bsr _divsd1d3d0
	endif

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even

;-----------------------------------------------------------------------------
; dmulscale
;-----------------------------------------------------------------------------
	XDEF _dmulscale
_dmulscale:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
		muls.l  d3,d3:d2
	else
		bsr _mulsd1d1d0
		bsr _mulsd3d3d2
	endif
	add.l   d2,d0
	addx.l  d3,d1

	moveq   #32,d2
	sub.l   d4,d2
	lsr.l   d4,d0
	lsl.l   d2,d1
	or.l    d1,d0

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even

;-----------------------------------------------------------------------------
; boundmulscale
;-----------------------------------------------------------------------------
	XDEF _boundmulscale
_boundmulscale:
	movem.l d2-d4,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
	else
		bsr _mulsd1d1d0
	endif
	moveq   #32,d4
	move.l  d1,d3
	sub.l   d2,d4
	lsr.l   d2,d0
	lsl.l   d4,d1
	or.l    d1,d0
	asr.l   d2,d1
	eor.l   d0,d1
	bmi.b   .checkit
	eor.l   d0,d1
	beq.b   .skipboundit
.checkit
	moveq   #31,d4
	move.l  d3,d0
	asr.l   d4,d0
	eor.l   #$7fffffff,d0
.skipboundit
	movem.l (sp)+,d2-d4
	rts

	even

;-----------------------------------------------------------------------------
; mulscale1-8
;-----------------------------------------------------------------------------
mulscalesA    MACRO
	XDEF _mulscale\1
_mulscale\1:
	move.l  d2,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
	else
		bsr _mulsd1d1d0
	endif
	moveq   #32-\1,d2
	lsr.l   #\1,d0
	lsl.l   d2,d1
	or.l    d1,d0

	move.l  (sp)+,d2
	rts

	even

	EndM

	mulscalesA 1
	mulscalesA 2
	mulscalesA 3
	mulscalesA 4
	mulscalesA 5
	mulscalesA 6
	mulscalesA 7
	mulscalesA 8

;-----------------------------------------------------------------------------
; mulscale9-23
;-----------------------------------------------------------------------------
mulscalesB   MACRO
	XDEF _mulscale\1
_mulscale\1:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
	else
		bsr _mulsd1d1d0
	endif
	moveq   #\1,d3
	moveq   #32-\1,d2
	lsr.l   d3,d0
	lsl.l   d2,d1
	or.l    d1,d0

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even

	EndM

	mulscalesB 9
	mulscalesB 10
	mulscalesB 11
	mulscalesB 12
	mulscalesB 13
	mulscalesB 14
	mulscalesB 15
	mulscalesB 16
	mulscalesB 17
	mulscalesB 18
	mulscalesB 19
	mulscalesB 20
	mulscalesB 21
	mulscalesB 22
	mulscalesB 23

;-----------------------------------------------------------------------------
; mulscale24-31
;-----------------------------------------------------------------------------
mulscalesC   MACRO
	XDEF _mulscale\1
_mulscale\1:
	move.l  d2,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
	else
		bsr _mulsd1d1d0
	endif
	moveq   #\1,d2
	lsr.l   d2,d0
	lsl.l   #32-\1,d1
	or.l    d1,d0

	move.l  (sp)+,d2
	rts

	even

	EndM

	mulscalesC 24
	mulscalesC 25
	mulscalesC 26
	mulscalesC 27
	mulscalesC 28
	mulscalesC 29
	mulscalesC 30
	mulscalesC 31

;-----------------------------------------------------------------------------
; mulscale32
;-----------------------------------------------------------------------------
	XDEF _mulscale32
_mulscale32:
	ifnd M68060
		muls.l  d1,d1:d0
	else
		bsr _mulsd1d1d0
	endif
	move.l  d1,d0

	rts

	even

;-----------------------------------------------------------------------------
; divscale1
;-----------------------------------------------------------------------------
	XDEF _divscale1
_divscale1:
	move.l  d2,-(sp)

	add.l   d0,d0
	subx.l  d2,d2
	ifnd M68060
		divs.l  d1,d2:d0
	else
		bsr _divsd1d2d0
	endif

	move.l  (sp)+,d2
	rts

	even

;-----------------------------------------------------------------------------
; divscale2-8
;-----------------------------------------------------------------------------
divscalesA MACRO
	XDEF _divscale\1
_divscale\1:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	move.l  d0,d2
	move.l  #32-\1,d3
	lsl.l   #\1,d0
	asr.l   d3,d2
	ifnd M68060
		divs.l  d1,d2:d0
	else
		bsr _divsd1d2d0
	endif

	move.l (sp)+,d3
	move.l (sp)+,d2
	rts

	even

	ENDM

	divscalesA 2
	divscalesA 3
	divscalesA 4
	divscalesA 5
	divscalesA 6
	divscalesA 7
	divscalesA 8

;-----------------------------------------------------------------------------
; divscale9-23
;-----------------------------------------------------------------------------
divscalesB MACRO
	XDEF _divscale\1
_divscale\1:
	movem.l d2-d4,-(sp)

	move.l  d0,d2
	move.l  #\1,d4
	move.l  #32-\1,d3
	lsl.l   d4,d0
	asr.l   d3,d2
	ifnd M68060
		divs.l  d1,d2:d0
	else
		bsr _divsd1d2d0
	endif

	movem.l (sp)+,d2-d4
	rts

	even

	ENDM

	divscalesB 9
	divscalesB 10
	divscalesB 11
	divscalesB 12
	divscalesB 13
	divscalesB 14
	divscalesB 15
	divscalesB 16
	divscalesB 17
	divscalesB 18
	divscalesB 19
	divscalesB 20
	divscalesB 21
	divscalesB 22
	divscalesB 23

;-----------------------------------------------------------------------------
; divscale24-31
;-----------------------------------------------------------------------------
divscalesC MACRO
	XDEF _divscale\1
_divscale\1:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	move.l  d0,d2
	move.l  #\1,d3
	asr.l   #32-\1,d2
	lsl.l   d3,d0
	ifnd M68060
		divs.l  d1,d2:d0
	else
		bsr _divsd1d2d0
	endif

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even

	ENDM

	divscalesC 24
	divscalesC 25
	divscalesC 26
	divscalesC 27
	divscalesC 28
	divscalesC 29
	divscalesC 30
	divscalesC 31

;-----------------------------------------------------------------------------
; divscale32
;-----------------------------------------------------------------------------
	XDEF _divscale32
_divscale32:
	move.l  d2,-(sp)

	moveq   #0,d2
	ifnd M68060
		divs.l  d1,d0:d2
	else
		bsr _divsd1d0d2
	endif
	move.l  d2,d0

	move.l  (sp)+,d2
	rts

	even


;-----------------------------------------------------------------------------
; dmulscale1-8
;-----------------------------------------------------------------------------
dmulscalesA MACRO
	XDEF _dmulscale\1
_dmulscale\1:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
		muls.l  d3,d3:d2
	else
		bsr _mulsd1d1d0
		bsr _mulsd3d3d2
	endif
	add.l   d2,d0
	addx.l  d3,d1

	moveq   #32-\1,d2
	lsr.l   #\1,d0
	lsl.l   d2,d1
	or.l    d1,d0

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even

	ENDM

	dmulscalesA 1
	dmulscalesA 2
	dmulscalesA 3
	dmulscalesA 4
	dmulscalesA 5
	dmulscalesA 6
	dmulscalesA 7
	dmulscalesA 8

;-----------------------------------------------------------------------------
; dmulscale9-23
;-----------------------------------------------------------------------------
dmulscalesB MACRO
	XDEF _dmulscale\1
_dmulscale\1:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
		muls.l  d3,d3:d2
	else
		bsr _mulsd1d1d0
		bsr _mulsd3d3d2
	endif
	add.l   d2,d0
	addx.l  d3,d1

	moveq   #\1,d2
	moveq   #32-\1,d3
	lsr.l   d2,d0
	lsl.l   d3,d1
	or.l    d1,d0

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even

	ENDM

	dmulscalesB 9
	dmulscalesB 10
	dmulscalesB 11
	dmulscalesB 12
	dmulscalesB 13
	dmulscalesB 14
	dmulscalesB 15
	dmulscalesB 16
	dmulscalesB 17
	dmulscalesB 18
	dmulscalesB 19
	dmulscalesB 20
	dmulscalesB 21
	dmulscalesB 22
	dmulscalesB 23

;-----------------------------------------------------------------------------
; dmulscale24-31
;-----------------------------------------------------------------------------
dmulscalesC MACRO
	XDEF _dmulscale\1
_dmulscale\1:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
		muls.l  d3,d3:d2
	else
		bsr _mulsd1d1d0
		bsr _mulsd3d3d2
	endif
	add.l   d2,d0
	addx.l  d3,d1

	moveq   #\1,d3
	lsr.l   d3,d0
	lsl.l   #32-\1,d1
	or.l    d1,d0

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even

	ENDM

	dmulscalesC 24
	dmulscalesC 25
	dmulscalesC 26
	dmulscalesC 27
	dmulscalesC 28
	dmulscalesC 29
	dmulscalesC 30
	dmulscalesC 31

;-----------------------------------------------------------------------------
; dmulscale32
;-----------------------------------------------------------------------------
	XDEF _dmulscale32
_dmulscale32:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
		muls.l  d3,d3:d2
	else
		bsr _mulsd1d1d0
		bsr _mulsd3d3d2
	endif
	add.l   d2,d0
	addx.l  d3,d1
	move.l  d1,d0

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even

;-----------------------------------------------------------------------------
; tmulscale1-8
;-----------------------------------------------------------------------------
tmulscalesA MACRO
	XDEF _tmulscale\1
_tmulscale\1:
	movem.l d2-d5,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
		muls.l  d3,d3:d2
		muls.l  d5,d5:d4
	else
		bsr _mulsd1d1d0
		bsr _mulsd3d3d2
		bsr _mulsd5d5d4
	endif
	add.l   d2,d0
	addx.l  d3,d1
	add.l   d4,d0
	addx.l  d5,d1
	moveq   #32-\1,d3
	lsr.l   #\1,d0
	lsl.l   d3,d1
	or.l    d1,d0

	movem.l (sp)+,d2-d5
	rts

	even

	ENDM

	tmulscalesA 1
	tmulscalesA 2
	tmulscalesA 3
	tmulscalesA 4
	tmulscalesA 5
	tmulscalesA 6
	tmulscalesA 7
	tmulscalesA 8

;-----------------------------------------------------------------------------
; tmulscale9-23
;-----------------------------------------------------------------------------
tmulscalesB MACRO
	XDEF _tmulscale\1
_tmulscale\1:
	movem.l d2-d5,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
		muls.l  d3,d3:d2
		muls.l  d5,d5:d4
	else
		bsr _mulsd1d1d0
		bsr _mulsd3d3d2
		bsr _mulsd5d5d4
	endif
	add.l   d2,d0
	addx.l  d3,d1
	add.l   d4,d0
	addx.l  d5,d1
	moveq   #\1,d2
	moveq   #32-\1,d3
	lsr.l   d2,d0
	lsl.l   d3,d1
	or.l    d1,d0

	movem.l (sp)+,d2-d5
	rts

	even

	ENDM

	tmulscalesB 9
	tmulscalesB 10
	tmulscalesB 11
	tmulscalesB 12
	tmulscalesB 13
	tmulscalesB 14
	tmulscalesB 15
	tmulscalesB 16
	tmulscalesB 17
	tmulscalesB 18
	tmulscalesB 19
	tmulscalesB 20
	tmulscalesB 21
	tmulscalesB 22
	tmulscalesB 23

;-----------------------------------------------------------------------------
; tmulscale24-31
;-----------------------------------------------------------------------------
tmulscalesC MACRO
	XDEF _tmulscale\1
_tmulscale\1:
	movem.l d2-d5,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
		muls.l  d3,d3:d2
		muls.l  d5,d5:d4
	else
		bsr _mulsd1d1d0
		bsr _mulsd3d3d2
		bsr _mulsd5d5d4
	endif
	add.l   d2,d0
	addx.l  d3,d1
	add.l   d4,d0
	addx.l  d5,d1
	moveq   #\1,d2
	lsl.l   #32-\1,d1
	lsr.l   d2,d0
	or.l    d1,d0

	movem.l (sp)+,d2-d5
	rts

	even

	ENDM

	tmulscalesC 24
	tmulscalesC 25
	tmulscalesC 26
	tmulscalesC 27
	tmulscalesC 28
	tmulscalesC 29
	tmulscalesC 30
	tmulscalesC 31

;-----------------------------------------------------------------------------
; tmulscale32
;-----------------------------------------------------------------------------
	XDEF _tmulscale32
_tmulscale32:
	movem.l d2-d5,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
		muls.l  d3,d3:d2
		muls.l  d5,d5:d4
	else
		bsr _mulsd1d1d0
		bsr _mulsd3d3d2
		bsr _mulsd5d5d4
	endif
	add.l   d2,d0
	addx.l  d3,d1
	add.l   d4,d0
	addx.l  d5,d1
	move.l  d1,d0

	movem.l (sp)+,d2-d5
	rts

	even

;-----------------------------------------------------------------------------
; msqrtasm
;-----------------------------------------------------------------------------
	XDEF    _msqrtasm
_msqrtasm:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	move.l  #$40000000,d1
	move.l  #$20000000,d2
.begit
	cmp.l   d1,d0
	blt.b   .skip
	sub.l   d1,d0
	move.l  d2,d3
	lsl.l   #2,d3
	add.l   d3,d1
.skip
	sub.l   d2,d1
	lsr.l   #1,d1
	lsr.l   #2,d2
	bne.b   .begit

	cmp.l   d1,d0
	bcs.b   .fini
	addq.l  #1,d1
.fini
	lsr.l   #1,d1
	move.l  d1,d0

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even

;-----------------------------------------------------------------------------
; clearbuf
;-----------------------------------------------------------------------------
	XDEF _clearbuf
_clearbuf:
.loop
	move.l  d1,(a0)+
	subq.l  #1,d0
	bne.b   .loop
	rts

	even

;-----------------------------------------------------------------------------
; clearbufbyte
;-----------------------------------------------------------------------------
	XDEF _clearbufbyte
_clearbufbyte:
	move.l d2,-(sp)

	cmp.l   #1,d0
	blt.b   .end
	bne.b   .cb2
	move.b  d1,(a0)
	bra.b   .end
.cb2
	cmp.l   #2,d0
	bne.b   .cb3
	move.w  d1,(a0)
	bra.b   .end
.cb3
	cmp.l   #3,d0
	bne.b   .cbdefault
	move.w  d1,(a0)
	move.b  d1,2(a0)
	bra.b   .end
.cbdefault
	move.l  a0,d2
	btst    #0,d2
	beq.b   .cbshort
	move.b  d1,(a0)+
	subq.l  #1,d0
.cbshort
	move.l  a0,d2
	btst    #1,d2
	beq.b   .cblong
	move.w  d1,(a0)+
	subq.l  #2,d0
.cblong
	move.l  d0,d2
	lsr.l   #2,d2
	beq.b   .cbrest
.loop
	move.l  d1,(a0)+
	subq.l  #1,d2
	bne.b   .loop
.cbrest
	btst    #1,d0
	beq.b   .cbchar
	move.w  d1,(a0)+
.cbchar
	btst    #0,d0
	beq.b   .end
	move.b  d1,(a0)
.end
	move.l (sp)+,d2
	rts

	even

;-----------------------------------------------------------------------------
; copybuf
;-----------------------------------------------------------------------------
	XDEF _copybuf
_copybuf:
	tst.l   d0
	beq.b   .end
.loop
	move.l  (a0)+,(a1)+
	subq.l  #1,d0
	bne.b   .loop
.end
	rts

	even

;-----------------------------------------------------------------------------
; copybufbyte
;-----------------------------------------------------------------------------
	XDEF _copybufbyte
_copybufbyte:
	cmp.l   #1,d0
	blt.b   .end
	bne.b   .cb2
	move.b  (a0),(a1)
	bra.b   .end
.cb2
	cmp.l   #2,d0
	bne.b   .cb3
	move.w  (a0),(a1)
	bra.b   .end
.cb3
	cmp.l   #3,d0
	bne.b   .cbdefault
	move.w  (a0),(a1)
	move.b  2(a0),2(a1)
	bra.b   .end
.cbdefault
	move.l  a0,d1
	btst    #0,d1
	beq.b   .cbshort
	move.b  (a0)+,(a1)+
	subq.l  #1,d0
.cbshort
	move.l  a0,d1
	btst    #1,d1
	beq.b   .cblong
	move.w  (a0)+,(a1)+
	subq.l  #2,d0
.cblong
	move.l  d0,d1
	lsr.l   #2,d1
	beq.b   .cbrest
.loop
	move.l  (a0)+,(a1)+
	subq.l  #1,d1
	bne.b   .loop
.cbrest
	btst    #1,d0
	beq.b   .cbchar
	move.w  (a0)+,(a1)+
.cbchar
	btst    #0,d0
	beq.b   .end
	move.b  (a0),(a1)
.end
	rts

	even

;-----------------------------------------------------------------------------
; copybufreverse
;-----------------------------------------------------------------------------
	XDEF _copybufreverse
_copybufreverse:
	tst.l   d0
	beq.b   .end
.loop
	move.b  (a0),(a1)+
	subq.l  #1,d0
	subq.l  #1,a0
	bne.b   .loop
.end
	rts

	even

;-----------------------------------------------------------------------------
; qinterpolatedown16
;-----------------------------------------------------------------------------
	XDEF _qinterpolatedown16
_qinterpolatedown16:
	movem.l d2-d4,-(sp)

	moveq   #16,d3
	tst.l   d0
	bne.b   .q1
	lsr.l   d3,d1
	move.l  d1,(a0)
	bra.b   .end
.q1
	move.l  d1,d4
	lsr.l   d3,d4
	add.l   d2,d1
	move.l  d4,(a0)+
	subq.l  #1,d0
	bne.b   .q1
.end
	movem.l (sp)+,d2-d4
	rts

	even

;-----------------------------------------------------------------------------
; qinterpolatedown16short
;-----------------------------------------------------------------------------
	XDEF _qinterpolatedown16short
_qinterpolatedown16short:
	movem.l d2-d5,-(sp)

	tst.l   d0
	beq.b   .end

	moveq   #16,d3
	move.l  a0,d4
	btst    #1,d4
	beq.b   .q1
	move.l  d1,d4
	lsr.l   d3,d4
	add.l   d2,d1
	move.w  d4,(a0)+
	subq.l  #1,d0
	beq.b   .end
.q1
	subq.l  #2,d0
	bpl.b   .q2
	lsr.l   d3,d1
	move.w  d1,(a0)
	bra.b   .end
.q2
	move.l  d1,d4
	add.l   d2,d1
	move.l  d1,d5
	lsr.l   d3,d5
	move.w  d5,d4
	add.l   d2,d1
	move.l  d4,(a0)+
	subq.l  #2,d0
	bpl.b   .q2

	btst    #0,d0
	beq.b   .end
	lsr.l   d3,d1
	move.w  d1,(a0)
.end
	movem.l (sp)+,d2-d5
	rts

	even

;
; rounding variants for Blood
;

;-----------------------------------------------------------------------------
; mulscale16r
;-----------------------------------------------------------------------------
	XDEF _mulscale16r
_mulscale16r:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
	else
		bsr _mulsd1d1d0
	endif
	; round start
	add.l   #32768,d0
	moveq   #0,d2
	addx.l  d2,d1
	; round end
	moveq   #16,d3
	moveq   #16,d2
	lsr.l   d3,d0
	lsl.l   d2,d1
	or.l    d1,d0

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even


;-----------------------------------------------------------------------------
; mulscale30r
;-----------------------------------------------------------------------------
	XDEF _mulscale30r
_mulscale30r:
	move.l  d2,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
	else
		bsr _mulsd1d1d0
	endif
	; round start
	add.l   #536870912,d0
	moveq   #0,d2
	addx.l  d2,d1
	; round end
	moveq   #30,d2
	lsr.l   d2,d0
	lsl.l   #2,d1
	or.l    d1,d0

	move.l  (sp)+,d2
	rts

	even

;-----------------------------------------------------------------------------
; dmulscale30r
;-----------------------------------------------------------------------------
	XDEF _dmulscale30r
_dmulscale30r:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	ifnd M68060
		muls.l  d1,d1:d0
		muls.l  d3,d3:d2
	else
		bsr _mulsd1d1d0
		bsr _mulsd3d3d2
	endif
	add.l   d2,d0
	addx.l  d3,d1
	; round start
	add.l   #536870912,d0
	moveq   #0,d2
	addx.l  d2,d1
	; round end

	moveq   #30,d3
	lsr.l   d3,d0
	lsl.l   #2,d1
	or.l    d1,d0

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even

	END
