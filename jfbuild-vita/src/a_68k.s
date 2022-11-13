;
; 68030+ versions of most of the render-routines
; from a.c / a.nasm
;
; Dante/Oxyron 2003
;

;    section "dukerender",code

	XREF    _asm1,_asm2,_asm3,_asm4
	XREF    _vplce,_vince
	XREF    _palookupoffse,_bufplce
	XREF    _ylookup
	XREF    _reciptable,_globalx3,_globaly3
	XREF    _fpuasm

	;XREF    _slopemach_ebx,_slopemach_ecx,_slopemach_edx
	;XREF    _slopemach_ah1,_slopemach_ah2
	;XREF    _asm2_f

	;XREF    _shlookup,_sqrtable

	;XDEF    _fixchain


;-----------------------------------------------------------------------------
; sethlinesizes
;   clobbers: d3
;-----------------------------------------------------------------------------
	XDEF    _sethlinesizes
_sethlinesizes:
	move.l  d3,-(sp)

	neg.b   d0
	move.b  d1,machxbits_bl
	move.l  d0,d3
	and.b   #$1f,d3
	move.b  d3,machxbits_al
	moveq   #-1,d3
	sub.b   d1,d0
	move.l  d2,machxbits_ecx
	lsr.l   d0,d3
	move.l  d3,machxbits_edx

	move.l  (sp)+,d3
	rts

	even

machxbits_ecx   dc.l    0
machxbits_edx   dc.l    0
machxbits_al    dc.b    0
machxbits_bl    dc.b    0

	even

;-----------------------------------------------------------------------------
; setpalookupaddress
;-----------------------------------------------------------------------------
	XDEF    _setpalookupaddress
_setpalookupaddress:
	move.l  d0,pal_eax
	rts

	even

pal_eax     dc.l    0

	even

;-----------------------------------------------------------------------------
; hlineasm4
; ---------
;   d0 = count
;   d1 = source (STUB!!!)
;   d2 = shade
;   a0 = i4
;   a1 = i5
;   a2 = i6
;   clobbers: d2, d3, d5, d6, d7, a2, a3, a4, a5, a6
;-----------------------------------------------------------------------------
	XDEF    _hlineasm4
_hlineasm4:
	movem.l d2-d7/a2-a6,-(sp)

	move.l  pal_eax(pc),a3

	move.l  machxbits_ecx(pc),a4
	move.b  machxbits_al(pc),d5
	move.b  machxbits_bl(pc),d6
	moveq   #32,d7
	sub.b   d6,d7
	move.l  _asm1,a5
	move.l  _asm2,a6

	addq.l  #1,a2

	addq.l  #1,d0
	move.l  d0,count
	cmp.l   #3,d0
	ble.w   .writerest

	move.l  a2,d0
	and.l   #$00000003,d0
	beq.b   .writelong
	sub.l   d0,count

.writefirst
	move.l  a1,d1
	move.l  a0,d3
	lsr.l   d5,d1
	lsr.l   d7,d3
	lsl.l   d6,d1
	or.l    d3,d1
	sub.l   a5,a1
	move.b  (a4,d1.l),d2
	sub.l   a6,a0
	move.b  (a3,d2.l),-(a2)
	subq.l  #1,d0
	bne.b   .writefirst

.writelong
	move.l  count(pc),d3
	lsr.l   #2,d3
	move.l  d3,count2
	beq.b   .writerest
	bra.b   .loop1

	cnop 0,16

.loop1
	move.l  a1,d1
	move.l  a0,d3
	lsr.l   d5,d1
	lsr.l   d7,d3
	lsl.l   d6,d1
	or.l    d3,d1
	sub.l   a5,a1
	move.b  (a4,d1.l),d2
	sub.l   a6,a0
	move.b  (a3,d2.l),d0

	move.l  a1,d1
	move.l  a0,d3
	lsr.l   d5,d1
	lsr.l   d7,d3
	lsl.l   d6,d1
	or.l    d3,d1
	ror.l   #8,d0
	move.b  (a4,d1.l),d2
	sub.l   a5,a1
	move.b  (a3,d2.l),d0
	sub.l   a6,a0

	move.l  a1,d1
	move.l  a0,d3
	lsr.l   d5,d1
	lsr.l   d7,d3
	lsl.l   d6,d1
	or.l    d3,d1
	ror.l   #8,d0
	move.b  (a4,d1.l),d2
	sub.l   a5,a1
	move.b  (a3,d2.l),d0
	sub.l   a6,a0

	move.l  a1,d1
	move.l  a0,d3
	lsr.l   d5,d1
	lsr.l   d7,d3
	lsl.l   d6,d1
	or.l    d3,d1
	ror.l   #8,d0
	move.b  (a4,d1.l),d2
	sub.l   a5,a1
	move.b  (a3,d2.l),d0
	sub.l   a6,a0

	ror.l   #8,d0

	move.l  d0,-(a2)
	subq.l  #1,count2
	bne.b   .loop1

.writerest
	move.l  count(pc),d0
	and.l   #3,d0
	beq.b   .end
.loop2
	move.l  a1,d1
	move.l  a0,d3
	lsr.l   d5,d1
	lsr.l   d7,d3
	lsl.l   d6,d1
	or.l    d3,d1
	sub.l   a5,a1
	move.b  (a4,d1.l),d2
	sub.l   a6,a0
	move.b  (a3,d2.l),-(a2)
	subq.l  #1,d0
	bne.b   .loop2
.end
	movem.l (sp)+,a2-a6/d2-d7
	rts

	even

count:  dc.l    0
count2: dc.l    0

	even

;-----------------------------------------------------------------------------
; setuprhlineasm4
; setupqrhlineasm4
;-----------------------------------------------------------------------------
	XDEF    _setuprhlineasm4
	XDEF    _setupqrhlineasm4
_setuprhlineasm4:
_setupqrhlineasm4:
	movem.l d0-d4,rmach_eax
	rts

	even

rmach_eax   dc.l    0
rmach_ebx   dc.l    0
rmach_ecx   dc.l    0
rmach_edx   dc.l    0
rmach_esi   dc.l    0

	even

;-----------------------------------------------------------------------------
; rhlineasm4
; qrhlineasm4
; -----------
;   d0 = i1
;   d1 = i2
;   d2 = i3
;   d3 = i4
;   d4 = i5
;   d5 = i6
;   clobbers: d2, d3, d4, d5, d6, d7, a2, a3, a4
;-----------------------------------------------------------------------------
	XDEF    _rhlineasm4
	XDEF    _qrhlineasm4
_rhlineasm4:
_qrhlineasm4:
	movem.l d2-d7/a2-a4,-(sp)

	tst.l   d0
	ble.b   .end

	move.l  rmach_edx(pc),a2

	move.l  d5,a1

	sub.l   a0,a0
	move.l  rmach_eax(pc),a3
	move.l  rmach_ebx(pc),a4
	move.l  rmach_ecx(pc),d7
	move.l  rmach_esi(pc),d6

	bra.b   .loop

	cnop 0,16
.loop
	move.b  (a0,d1.l),d2
	sub.l   a3,d3
	subx.l  d5,d5
	sub.l   a4,d4
	subx.l  d7,d1
	and.l   d6,d5
	move.b  (a2,d2.l),-(a1)
	sub.l   d5,a0
	subq.l  #1,d0
	bne.b   .loop

.end
	movem.l (sp)+,a2-a4/d2-d7
	rts

	even

;-----------------------------------------------------------------------------
; setuprmhlineasm4
;-----------------------------------------------------------------------------
	XDEF    _setuprmhlineasm4
_setuprmhlineasm4:
	movem.l d0-d4,rmmach_eax
	rts

	even

rmmach_eax  dc.l    0
rmmach_ebx  dc.l    0
rmmach_ecx  dc.l    0
rmmach_edx  dc.l    0
rmmach_esi  dc.l    0

	even

;-----------------------------------------------------------------------------
; rmhlineasm4
; -----------
;   d0 = i1
;   d1 = i2
;   d2 = i3
;   d3 = i4
;   d4 = i5
;   d5 = i6
;   clobbers: d2, d3, d4, d5, d6, d7, a2, a3, a4
;-----------------------------------------------------------------------------
	XDEF    _rmhlineasm4
_rmhlineasm4:
	movem.l d2-d7/a2-a4,-(sp)

	tst.l   d0
	ble.b   .end

	move.l  rmmach_edx(pc),a2

	move.l  d5,a1
	subq.l  #1,a1

	sub.l   a0,a0
	move.l  rmmach_eax(pc),a3
	move.l  rmmach_ebx(pc),a4
	move.l  rmmach_ecx(pc),d7
	move.l  rmmach_esi(pc),d6

	bra.b   .loop

	cnop 0,16
.loop
	move.b  (a0,d1.l),d2
	sub.l   a3,d3
	subx.l  d5,d5
	sub.l   a4,d4
	subx.l  d7,d1
	and.l   d6,d5
	cmp.b   #$ff,d2
	beq.b   .j1
	move.b  (a2,d2.l),(a1)
.j1
	sub.l   d5,a0
	subq.l  #1,a1
	subq.l  #1,d0
	bne.b   .loop

.end
	movem.l (sp)+,a2-a4/d2-d7
	rts

	even


;-----------------------------------------------------------------------------
; setvlinebpl
;-----------------------------------------------------------------------------
	XDEF    _setvlinebpl
_setvlinebpl
	move.l  d0,_fixchain
	rts

	even

_fixchain:   dc.l    0

	even

;-----------------------------------------------------------------------------
; fixtransluscence
;-----------------------------------------------------------------------------
	XDEF    _fixtransluscence
_fixtransluscence
	move.l  d0,tmach
	rts

	even

tmach:  dc.l    0

	even

;-----------------------------------------------------------------------------
; setupvlineasm
;-----------------------------------------------------------------------------
	XDEF    _setupvlineasm
_setupvlineasm
	move.b  d0,d1
	and.l   #$1f,d1
	move.l  d1,mach3_al
	rts

	even

mach3_al:   dc.l    0

	even

;-----------------------------------------------------------------------------
; prevlineasm1
; ------------
;   d0 = i1
;   a0 = i2
;   d1 = i3
;   d2 = i4
;   a1 = i5
;   a2 = i6
;   clobbers: d2, d3, a2 + INT_vlineasm1 d4, d5, d6
;-----------------------------------------------------------------------------
	XDEF    _prevlineasm1
_prevlineasm1:
	;movem.l d2-d3/a2,-(sp)
	movem.l d2-d6/a2,-(sp)

	tst.l   d1
	bne.b   INT_vlineasm1

	move.l  mach3_al(pc),d3
	add.l   d2,d0
	lsr.l   d3,d2
	move.b  (a1,d2.l),d2
	move.b  (a0,d2.l),(a2)
.end
	;movem.l (sp)+,a2/d2-d3
	movem.l (sp)+,a2/d2-d6
	rts

	even

;-----------------------------------------------------------------------------
; vlineasm1
; ---------
;   d0 = vince
;   a0 = palookupoffse
;   d1 = i3
;   d2 = vplce
;   a1 = bufplce
;   a2 = i6
;   clobbers: d2, d3, d4, d5, d6, a2
;-----------------------------------------------------------------------------
	XDEF    _vlineasm1
_vlineasm1:
	movem.l d2-d6/a2,-(sp)
INT_vlineasm1:
	move.l  mach3_al(pc),d3
	moveq   #0,d5
	move.l  _fixchain(pc),d6
	addq.l  #1,d1
	beq.b   .end

	bra.b   .loop

	cnop 0,16
.loop
	move.l  d2,d4
	lsr.l   d3,d4
	move.b  (a1,d4.l),d5
	add.l   d0,d2
	move.b  (a0,d5.l),(a2)
	subq.l  #1,d1
	add.l   d6,a2
	bne.b   .loop
.end
	move.l  d2,d0
	movem.l (sp)+,a2/d2-d6
	rts

	even

;-----------------------------------------------------------------------------
; vlineasm4
; ---------
;   d0 = i1
;   d1 = i2
;   clobbers: d2, d3, d4, d5, d6, d7, a2, a3, a4, a5, a6
;-----------------------------------------------------------------------------
	XDEF    _vlineasm4
_vlineasm4:
	movem.l d2-d7/a2-a6,-(sp)

	lea     _ylookup,a0
	move.l  (a0,d0.l*4),d0

	move.l  d0,a0
	add.l   d1,a0

	neg.l   d0

	lea     _bufplce,a1
	lea     _palookupoffse,a2

	lea     _vince,a4
	move.l  (a4),d3
	move.l  4(a4),d4
	move.l  8(a4),d5
	move.l  12(a4),d6

	lea     _vplce,a3
	move.l  12(a3),a6
	move.l  8(a3),a5
	move.l  4(a3),a4
	move.l  (a3),a3

	move.l  mach3_al(pc),d7
	;moveq   #0,d1
	move.l  a3,d1
	lsr.l   d7,d1
	bra.b   .loop

	cnop 0,16
.loop
	move.b  ([a1],d1.w),d1
	add.l   d3,a3
	move.b  ([a2],d1.w),d2
	move.l  a4,d1
	lsl.l   #8,d2

	lsr.l   d7,d1
	move.b  ([4,a1],d1.w),d1
	add.l   d4,a4
	move.b  ([4,a2],d1.w),d2
	move.l  a5,d1
	lsl.l   #8,d2

	lsr.l   d7,d1
	move.b  ([8,a1],d1.w),d1
	add.l   d5,a5
	move.b  ([8,a2],d1.w),d2
	move.l  a6,d1
	lsl.l   #8,d2

	lsr.l   d7,d1
	move.b  ([12,a1],d1.w),d1
	add.l   d6,a6
	move.b  ([12,a2],d1.w),d2
	move.l  a3,d1

	move.l  d2,(a0,d0.l)
	lsr.l   d7,d1

	add.l   _fixchain(pc),d0
	bcc.b   .loop
.end
	lea     _vplce,a2
	move.l  a3,(a2)
	move.l  a4,4(a2)
	move.l  a5,8(a2)
	move.l  a6,12(a2)

	movem.l (sp)+,a2-a6/d2-d7
	rts

	even

;-----------------------------------------------------------------------------
; setuptvlineasm
;-----------------------------------------------------------------------------
	XDEF    _setuptvlineasm
_setuptvlineasm:
	move.l  d0,transmach3_al
	rts

	even

transmach3_al:  dc.l    0

	even

;-----------------------------------------------------------------------------
; tvlineasm1
; ----------
;   d0 = i1
;   a0 = i2
;   d1 = i3
;   d2 = i4
;   a1 = i5
;   a2 = i6
;   clobbers: d2, d3, d4, d5, d7, a2, a3
;-----------------------------------------------------------------------------
	XDEF    _tvlineasm1
_tvlineasm1:
	movem.l d2-d7/a2-a3,-(sp)

	move.l  tmach(pc),a3
	;moveq   #0,d7
	move.l  transmach3_al(pc),d7
	moveq   #0,d4
	moveq   #0,d5
	tst.b   transrev(pc)
	bne.w   tvlineasm1_rev
.loop
	move.l  d2,d3
	lsr.l   d7,d3
	move.b  (a1,d3.w),d4
	cmp.b   #255,d4
	beq.b   .l2

	    move.b  (a2),d5
	    lsl.w   #8,d5
	    move.b  (a0,d4.w),d5
	    move.b  (a3,d5.l),(a2)
.l2
	add.l   d0,d2
	add.l   _fixchain(pc),a2
	subq.l  #1,d1
	bpl.b   .loop

	move.l  d2,d0
	movem.l (sp)+,a2-a3/d2-d7
	rts

	even

tvlineasm1_rev:
.loop
	move.l  d2,d3
	lsr.l   d7,d3
	move.b  (a1,d3.w),d4
	cmp.b   #255,d4
	beq.b   .l2

	    move.b  (a0,d4.w),d5
	    lsl.w   #8,d5
	    move.b  (a2),d5
	    move.b  (a3,d5.l),(a2)
.l2
	add.l   d0,d2
	add.l   _fixchain(pc),a2
	subq.l  #1,d1
	bpl.b   .loop

	move.l  d2,d0
	movem.l (sp)+,a2-a3/d2-d7
	rts

	even

transrev:   dc.b    0

	even

;-----------------------------------------------------------------------------
; setuptvlineasm2
;-----------------------------------------------------------------------------
	XDEF    _setuptvlineasm2
_setuptvlineasm2:
	and.l   #$1f,d0
	move.l  d0,tran2shr
	move.l  d1,tran2pal_ebx
	move.l  d2,tran2pal_ecx
	rts

	even
tran2pal_ebx:   dc.l    0
tran2pal_ecx:   dc.l    0
tran2shr:       dc.l    0
	even

;-----------------------------------------------------------------------------
; tvlineasm2
; ----------
;   d0 = ebp
;   d1 = tran2inca
;   a0 = tran2bufa
;   a1 = tran2bufb
;   a2 = i5
;   d2 = i6
;   clobbers: d2, d3, d4, d5, d6, d7, a2, a3, a4, a5, a6
;-----------------------------------------------------------------------------
	XDEF    _tvlineasm2
_tvlineasm2:
	movem.l d2-d7/a2-a6,-(sp)

	move.l  d1,tran2inca
	move.l  _asm1,tran2incb

	move.l  tran2shr(pc),d7
	move.l  tran2pal_ebx(pc),a3
	move.l  tran2pal_ecx(pc),a4
	move.l  _asm2,a5
	move.l  tmach(pc),a6

	moveq   #0,d4
	moveq   #0,d5
	moveq   #0,d6

	sub.l   a5,d2

	tst.b   transrev(pc)
	bne.w   tvlineasm2_rev
.loop
	move.l  a2,d1
	lsr.l   d7,d1       ;d2 => i1=i5>>tran2shr
	move.l  d0,d3
	lsr.l   d7,d3       ;d3 => i2=ebp>>tran2shr

	add.l   tran2inca(pc),a2    ;a3 => i5+=tran2inca
	add.l   tran2incb(pc),d0    ;d0 => ebp+=tran2incb

	move.b  (a0,d1.w),d4 ;d4=i3
	move.b  (a1,d3.w),d5 ;d5=i4

	cmp.b   #255,d4
	bne.b   .skip
	    cmp.b   #255,d5
	    beq.b   .skip3

		move.b  1(a5,d2.l),d6
		lsl.w   #8,d6
		move.b  (a4,d5.w),d6
		move.b  (a6,d6.l),1(a5,d2.l)
		bra.b   .skip3
.skip
	cmp.b   #255,d5
	bne.b   .skip2

	    move.b  (a5,d2.l),d6
	    lsl.w   #8,d6
	    move.b  (a3,d4.w),d6
	    move.b  (a6,d6.l),(a5,d2.l)
	    bra.b   .skip3
.skip2
	move.b  (a5,d2.l),d6
	lsl.w   #8,d6
	move.b  (a3,d4.w),d6
	move.b  (a6,d6.l),d1

	move.b  1(a5,d2.l),d6
	lsl.w   #8,d1
	lsl.w   #8,d6
	move.b  (a4,d5.w),d6
	move.b  (a6,d6.l),d1

	move.w  d1,(a5,d2.l)
.skip3
	add.l   _fixchain(pc),d2
	bcc.b   .loop

	move.l  a2,_asm1
	move.l  d0,_asm2

	movem.l (sp)+,a2-a6/d2-d7
	rts

	even

tvlineasm2_rev:
.loop
	move.l  a2,d1
	lsr.l   d7,d1       ;d2 => i1=i5>>tran2shr
	move.l  d0,d3
	lsr.l   d7,d3       ;d3 => i2=ebp>>tran2shr

	add.l   tran2inca(pc),a2    ;a3 => i5+=tran2inca
	add.l   tran2incb(pc),d0    ;d0 => ebp+=tran2incb

	move.b  (a0,d1.w),d4 ;d4=i3
	move.b  (a1,d3.w),d5 ;d5=i4

	cmp.b   #255,d4
	bne.b   .skip
	    cmp.b   #255,d5
	    beq.b   .skip3

		move.b  (a4,d5.w),d6
		lsl.w   #8,d6
		move.b  1(a5,d2.l),d6
		move.b  (a6,d6.l),1(a5,d2.l)
		bra.b   .skip3
.skip
	cmp.b   #255,d5
	bne.b   .skip2

	    move.b  (a3,d4.w),d6
	    lsl.w   #8,d6
	    move.b  (a5,d2.l),d6
	    move.b  (a6,d6.l),(a5,d2.l)
	    bra.b   .skip3
.skip2
	move.b  (a3,d4.w),d6
	lsl.w   #8,d6
	move.b  (a5,d2.l),d6
	move.b  (a6,d6.l),d1

	move.b  (a4,d5.w),d6
	lsl.w   #8,d1
	lsl.w   #8,d6
	move.b  1(a5,d2.l),d6
	move.b  (a6,d6.l),d1

	move.w  d1,(a5,d2.l)
.skip3
	add.l   _fixchain(pc),d2
	bcc.b   .loop

	move.l  a2,_asm1
	move.l  d0,_asm2

	movem.l (sp)+,a2-a6/d2-d7
	rts

	even

tran2inca:  dc.l    0
tran2incb:  dc.l    0

	even

;-----------------------------------------------------------------------------
; settransnormal
;-----------------------------------------------------------------------------
	XDEF    _settransnormal
_settransnormal:
	clr.b   transrev
	rts

	even

;-----------------------------------------------------------------------------
; settransreverse
;-----------------------------------------------------------------------------
	XDEF    _settransreverse
_settransreverse
	move.b  #1,transrev
	rts

	even

;-----------------------------------------------------------------------------
; setupmvlineasm
;-----------------------------------------------------------------------------
	XDEF    _setupmvlineasm
_setupmvlineasm
	move.l  d0,machmv
	rts

	even

machmv:     dc.l    0

	even

;-----------------------------------------------------------------------------
; mvlineasm1
; ----------
;   d0=vince
;   a0=palookupoffse
;   d1=cnt
;   d2=vplce
;   a1=bufplce
;   a2=p
;   clobbers: d2, d3, d4, d6, d7, a2
;-----------------------------------------------------------------------------
	XDEF    _mvlineasm1
_mvlineasm1:
	movem.l d2-d7/a2,-(sp)

	moveq   #0,d4
	move.l  _fixchain(pc),d6
	move.l  machmv(pc),d7
.loop
	move.l  d2,d3
	lsr.l   d7,d3
	move.b  (a1,d3.w),d4
	cmp.b   #255,d4
	beq.b   .skip
	move.b  (a0,d4.w),(a2)
.skip
	add.l   d0,d2
	add.l   d6,a2
	subq.l  #1,d1
	bpl.b   .loop

	move.l  d2,d0
	movem.l (sp)+,a2/d2-d7
	rts

	even

;-----------------------------------------------------------------------------
; mvlineasm4
; ----------
;   d0=i1
;   d1=p
;   clobbers: d2, d3, d4, d5, d6, d7, a2, a3, a4, a5, a6
;-----------------------------------------------------------------------------
	XDEF    _mvlineasm4
_mvlineasm4:
	movem.l d2-d7/a2-a6,-(sp)

	lea     _ylookup,a0
	move.l  (a0,d0.l*4),d0

	move.l  d0,a0
	add.l   d1,a0

	neg.l   d0

	lea     _bufplce,a1
	lea     _palookupoffse,a2

	lea     _vince,a4
	move.l  (a4),d3           ; d3 = vince[0]
	move.l  4(a4),d4          ; d4 = vince[1]
	move.l  8(a4),d5          ; d5 = vince[2]
	move.l  12(a4),d6         ; d6 = vince[3]

	lea     _vplce,a3
	move.l  12(a3),a6         ; a6 = vplce[3]
	move.l  8(a3),a5          ; a5 = vplce[2]
	move.l  4(a3),a4          ; a4 = vplce[1]
	move.l  (a3),a3           ; a3 = vplce[0]

	move.l  machmv(pc),d7
	;moveq   #0,d1
	move.l  a3,d1            ; d1 = vplce[0]
	lsr.l   d7,d1            ; d1 = vplce[0] >> machmv
	bra.b   .loop

	cnop 0,16
.loop
	move.b  ([a1],d1.w),d1   ; bufplce[0][vplce[0] >> machmv]
	add.l   d3,a3            ; vplce[0] += vince[0]
	cmp.b   #255,d1
	beq.b   .l1a
	move.b  ([a2],d1.w),d2   ; d2 = palookupoffse[d1]
	move.b  d2,(a0,d0.l)     ; *p = d2
.l1a
	move.l  a4,d1            ; d1 = vplce[1]
	;cmp.b   #255,d2
	;beq.b   .l1
	;move.b  d2,(a0,d0.l)
;.l1
	lsr.l   d7,d1
	move.b  ([4,a1],d1.w),d1
	add.l   d4,a4
	cmp.b   #255,d1
	beq.b   .l2a
	move.b  ([4,a2],d1.w),d2
	move.b  d2,1(a0,d0.l)
.l2a
	move.l  a5,d1
	;cmp.b   #255,d2
	;beq.b   .l2
	;move.b  d2,1(a0,d0.l)
;.l2
	lsr.l   d7,d1
	move.b  ([8,a1],d1.w),d1
	add.l   d5,a5
	cmp.b   #255,d1
	beq.b   .l3a
	move.b  ([8,a2],d1.w),d2
	move.b  d2,2(a0,d0.l)
.l3a
	move.l  a6,d1
	;cmp.b   #255,d2
	;beq.b   .l3
	;move.b  d2,2(a0,d0.l)
;.l3
	lsr.l   d7,d1
	move.b  ([12,a1],d1.w),d1
	add.l   d6,a6
	cmp.b   #255,d1
	beq.b   .l4a
	move.b  ([12,a2],d1.w),d2
	move.b  d2,3(a0,d0.l)
.l4a
	move.l  a3,d1
	;cmp.b   #255,d2
	;beq.b   .l4
	;move.b  d2,3(a0,d0.l)
;.l4
	lsr.l   d7,d1
	add.l   _fixchain(pc),d0
	bcc.b   .loop
.end
	lea     _vplce,a2
	move.l  a3,(a2)
	move.l  a4,4(a2)
	move.l  a5,8(a2)
	move.l  a6,12(a2)

	movem.l (sp)+,a2-a6/d2-d7
	rts

	even

;-----------------------------------------------------------------------------
; tsetupspritevline
; ----------
;   d0=paloffs
;   d1=xinc???
;   d2=yinc???
;   d3=ysiz
;   d4=byinc
;   d5=unused
;   clobbers: d4, d6, d7
;-----------------------------------------------------------------------------
	XDEF    _tsetupspritevline
_tsetupspritevline:
	movem.l d4-d7,-(sp)

	moveq   #16,d7
	move.l  d0,tspal
	move.l  d4,d6
	lsl.l   d7,d4
	move.l  d4,tsmach_eax1
	lsr.l   d7,d6
	add.l   d1,d6
	move.l  d6,tsmach_eax2
	add.l   d3,d6
	move.l  d6,tsmach_eax3
	move.l  d2,tsmach_ecx

	movem.l (sp)+,d4-d7
	rts

	even

tspal:          dc.l    0
tsmach_eax1:    dc.l    0
tsmach_eax2:    dc.l    0
tsmach_eax3:    dc.l    0
tsmach_ecx:     dc.l    0

	even

;-----------------------------------------------------------------------------
; tspritevline
; ------------
;   d0=unused
;   d1=bx
;   d2=cnt
;   d3=by
;   a0=bufplc
;   a1=p
;   clobbers: d2, d3, d4, d5, d6, d7, a2, a3, a4, a5
;-----------------------------------------------------------------------------
	XDEF    _tspritevline
_tspritevline:
	movem.l d2-d7/a2-a5,-(sp)

	move.l  tspal(pc),a2
	move.l  tmach(pc),a3

	move.l  tsmach_eax1(pc),a4
	moveq   #0,d0
	move.l  tsmach_eax2(pc),d4
	moveq   #0,d6
	move.l  tsmach_eax3(pc),d5
	moveq   #0,d7
	move.l  tsmach_ecx(pc),a5

	subq.l  #1,d2
	beq.b   .end

	tst.b   transrev(pc)
	bne.b   tspritevline_rev
.loop
	move.b  (a0,d7.l),d0
	add.l   a5,d3
	bcs.b   .l1
	add.l   a4,d1
	addx.l  d4,d7
	cmp.b   #255,d0
	beq.b   .skip1
	move.b  (a1),d6
	lsl.w   #8,d6
	move.b  (a2,d0.w),d6
	move.b  (a3,d6.l),(a1)
.skip1
	subq.l  #1,d2
	add.l   _fixchain(pc),a1
	bne.b   .loop
	bra.b   .end

.l1
	add.l   a4,d1
	addx.l  d5,d7
	cmp.b   #255,d0
	beq.b   .skip2
	move.b  (a1),d6
	lsl.w   #8,d6
	move.b  (a2,d0.w),d6
	move.b  (a3,d6.l),(a1)
.skip2
	subq.l  #1,d2
	add.l   _fixchain(pc),a1
	bne.b   .loop
.end
	movem.l (sp)+,a2-a5/d2-d7
	rts

	even

tspritevline_rev:
.loop
	move.b  (a0,d7.l),d0
	add.l   a5,d3
	bcs.b   .l1
	add.l   a4,d1
	addx.l  d4,d7
	cmp.b   #255,d0
	beq.b   .skip1
	move.b  (a2,d0.w),d6
	lsl.w   #8,d6
	move.b  (a1),d6
	move.b  (a3,d6.l),(a1)
.skip1
	subq.l  #1,d2
	add.l   _fixchain(pc),a1
	bne.b   .loop
	bra.b   .end

.l1
	add.l   a4,d1
	addx.l  d5,d7
	cmp.b   #255,d0
	beq.b   .skip2
	move.b  (a2,d0.w),d6
	lsl.w   #8,d6
	move.b  (a1),d6
	move.b  (a3,d6.l),(a1)
.skip2
	subq.l  #1,d2
	add.l   _fixchain(pc),a1
	bne.b   .loop
.end
	movem.l (sp)+,a2-a5/d2-d7
	rts

	even

;-----------------------------------------------------------------------------
; mhline
; ------
;   d0=i1  - unused
;   d1=i2
;   d2=i3
;   d3=i4  - unused
;   a0=i5
;   a1=i6
;   clobbers: none
;-----------------------------------------------------------------------------
	XDEF    _mhline
_mhline:
	move.l  d0,mmach_eax
	move.l  _asm3,mmach_asm3
	move.l  _asm1,mmach_asm1
	move.l  _asm2,mmach_asm2

	move.l  _asm2,d0
	bra.b   _mhlineskipmodify

	even

mmach_eax:  dc.l    0
mmach_asm3: dc.l    0
mmach_asm1: dc.l    0
mmach_asm2: dc.l    0

	even

;-----------------------------------------------------------------------------
; mhlineskipmodify
; ----------------
;   d0=i1  - unused
;   d1=i2
;   d2=i3
;   d3=i4  - unused
;   a0=i5
;   a1=i6
;   clobbers: d2, d3, d4, d5, d6, d7, a2, a3, a4, a5
;-----------------------------------------------------------------------------
	XDEF    _mhlineskipmodify
_mhlineskipmodify:
	movem.l d2-d7/a2-a5,-(sp)
	move.l  mshift_al(pc),d3
	lsr.l   #8,d2
	move.l  mshift_bl(pc),d5
	lsr.l   #8,d2
	move.l  mshift_bl_r(pc),d6
	moveq   #0,d4
	movem.l mmach_eax(pc),a2-a5
.loop
	move.l  d1,d0
	lsr.l   d3,d0

	move.l  a0,d7
	lsl.l   d5,d0
	lsr.l   d6,d7
	or.l    d7,d0

	move.b  (a2,d0.l),d4
	cmp.b   #255,d4
	beq.b   .skip
	move.b  (a3,d4.w),(a1)
.skip
	add.l   a4,d1
	add.l   a5,a0
	addq.l  #1,a1
	subq.l  #1,d2
	bpl.b   .loop

	movem.l (sp)+,a2-a5/d2-d7
	rts

	even

;-----------------------------------------------------------------------------
; msethlineshift
;   clobbers: d2
;-----------------------------------------------------------------------------
	XDEF    _msethlineshift
_msethlineshift:
	move.l d2,-(sp)

	and.l   #$1f,d0
	and.l   #$1f,d1

	moveq   #32,d2
	sub.b   d0,d2
	move.l  d2,mshift_al

	move.l  d1,mshift_bl

	moveq   #32,d0
	sub.l   d1,d0
	move.l  d0,mshift_bl_r

	move.l (sp)+,d2
	rts

	even

mshift_bl_r:    dc.l    26
mshift_bl:      dc.l    6
mshift_al:      dc.l    26

	even

;-----------------------------------------------------------------------------
; thline
; ------
;   d0=i1  - unused
;   d1=i2
;   d2=i3
;   d3=i4  - unused
;   a0=i5
;   a1=i6
;   clobbers: none
;-----------------------------------------------------------------------------
	XDEF    _thline
_thline:
	move.l  d0,tmach_eax
	move.l  _asm3,tmach_asm3
	move.l  _asm1,tmach_asm1
	move.l  _asm2,tmach_asm2

	move.l  _asm2,d0
	bra.b   _thlineskipmodify

	even

tmach_eax:  dc.l    0
tmach_asm3: dc.l    0
tmach_asm1: dc.l    0
tmach_asm2: dc.l    0

	even

;-----------------------------------------------------------------------------
; thlineskipmodify
; ----------------
;   d0=i1  - unused
;   d1=i2
;   d2=i3
;   d3=i4  - unused
;   a0=i5
;   a1=i6
;   clobbers: d2, d3, d4, d5, d6, d7, a2, a3, a4, a5, a6
;-----------------------------------------------------------------------------
	XDEF    _thlineskipmodify
_thlineskipmodify:
	movem.l d2-d7/a2-a6,-(sp)
	move.l  tshift_al(pc),d3
	lsr.l   #8,d2
	move.l  tshift_bl(pc),d5
	lsr.l   #8,d2
	move.l  tshift_bl_r(pc),d6
	moveq   #0,d4
	movem.l tmach_eax(pc),a2-a5
	move.l  tmach(pc),a6

	tst.b   transrev(pc)
	bne.b   thlineskipmodify_rev
.loop
	move.l  d1,d0
	lsr.l   d3,d0

	move.l  a0,d7
	lsl.l   d5,d0
	lsr.l   d6,d7
	or.l    d7,d0

	move.b  (a2,d0.l),d4
	cmp.b   #255,d4
	beq.b   .skip

	move.b  (a1),d7
	lsl.w   #8,d7
	move.b  (a3,d4.w),d7
	and.l   #$0000ffff,d7
	move.b  (a6,d7.l),(a1)
.skip
	add.l   a4,d1
	add.l   a5,a0
	addq.l  #1,a1
	subq.l  #1,d2
	bpl.b   .loop

	movem.l (sp)+,a2-a6/d2-d7
	rts

	even

thlineskipmodify_rev:
.loop
	move.l  d1,d0
	lsr.l   d3,d0

	move.l  a0,d7
	lsl.l   d5,d0
	lsr.l   d6,d7
	or.l    d7,d0

	move.b  (a2,d0.l),d4
	cmp.b   #255,d4
	beq.b   .skip

	move.b  (a3,d4.w),d7
	lsl.w   #8,d7
	move.b  (a1),d7
	and.l   #$0000ffff,d7
	move.b  (a6,d7.l),(a1)
.skip
	add.l   a4,d1
	add.l   a5,a0
	addq.l  #1,a1
	subq.l  #1,d2
	bpl.b   .loop

	movem.l (sp)+,a2-a6/d2-d7
	rts

	even

;-----------------------------------------------------------------------------
; tsethlineshift
;   clobbers: d2
;-----------------------------------------------------------------------------
	XDEF    _tsethlineshift
_tsethlineshift:
	move.l d2,-(sp)

	and.l   #$1f,d0
	and.l   #$1f,d1

	moveq   #32,d2
	sub.b   d0,d2
	move.l  d2,tshift_al

	move.l  d1,tshift_bl

	moveq   #32,d0
	sub.l   d1,d0
	move.l  d0,tshift_bl_r

	move.l (sp)+,d2
	rts

	even

tshift_bl_r:    dc.l    26
tshift_bl:      dc.l    6
tshift_al:      dc.l    26

	even

;-----------------------------------------------------------------------------
; setupslopevlin
;   clobbers: d2, d3
;-----------------------------------------------------------------------------
	XDEF    _setupslopevlin
_setupslopevlin:
	move.l  d2,-(sp)
	move.l  d3,-(sp)

	move.l  d1,slopemach_ebx
	move.l  d2,slopemach_ecx

	move.l  d0,d2
	moveq   #1,d3
	and.l   #$0000001f,d2
	lsl.l   d2,d3
	subq.l  #1,d3

	move.l  d0,d2
	and.l   #$00001f00,d2
	lsr.l   #8,d2
	lsl.l   d2,d3

	move.l  d3,slopemach_edx


	move.l  d0,d2
	and.l   #$0000ff00,d2
	lsr.l   #8,d2
	move.l  #256,d3
	sub.l   d2,d3

	and.l   #$000000ff,d0
	move.l  d3,d2
	sub.l   d0,d2

	and.l   #$1f,d3
	and.l   #$1f,d2

	move.l  d3,slopemach_ah1
	move.l  d2,slopemach_ah2

	fmove.l _asm1,fp0
	fmove.s fp0,asm2_f
	move.l  asm2_f(pc),_asm2

	move.l  (sp)+,d3
	move.l  (sp)+,d2
	rts

	even

slopemach_ebx:  dc.l    0
slopemach_ecx:  dc.l    0
slopemach_edx:  dc.l    0
slopemach_ah1:  dc.l    0
slopemach_ah2:  dc.l    0
asm2_f:         dc.l    0

	even

;-----------------------------------------------------------------------------
; slopevlin
; ---------
; a0=i1
; a1=i2
; a2=i3
; d0=i4
; d1=i5
; a4=i6
;   clobbers: d2, d3, d4, d5, d6, d7, a2, a3, a4, a5, a6
;-----------------------------------------------------------------------------
	XDEF    _slopevlin
_slopevlin:
	movem.l d2-d7/a2-a6,-(sp)

	move.l  d0,_asm4

	move.l  slopemach_ah1(pc),d6
	move.l  slopemach_ah2(pc),d7
	move.l  slopemach_ebx(pc),a5


	fmove.l _asm3,fp0
	fmove.s asm2_f(pc),fp1
	fadd.s  fp1,fp0  ; fp0 = a

	sub.l   slopemach_ecx(pc),a0

	move.l  a1,d3
	lsl.l   #3,d3
	move.l  _globalx3,d4
	;mulu.l  d3,d5:d4
	mulu.l  d3,d4
	add.l   d4,d1
	move.l  _globaly3,d4
	;mulu.l  d3,d5:d4
	mulu.l  d3,d4
	add.l   d4,a4

.outerloop
	fmove.s fp0,_fpuasm
	lea     _reciptable,a3
	move.l  _fpuasm,d3
	add.l   d3,d3
	subx.l  d4,d4
	move.l  d3,d5
	and.l   #$ff000000,d5
	and.l   #$00ffe000,d3
	rol.l   #8,d5
	lsr.l   #8,d3
	subq.b  #2,d5
	lsr.l   #3,d3
	and.l   #$1f,d5
	move.l  (a3,d3.w),d3
	lsr.l   d5,d3
	eor.l   d4,d3
	;--------------------------------------

	move.l  d3,d4
	sub.l   a1,d4
	move.l  d3,a1
	move.l  d4,d5

	;mulu.l  _globaly3,d3:d4
	mulu.l  _globaly3,d4
	;mulu.l  _globalx3,d3:d5
	mulu.l  _globalx3,d5

	fadd.s  fp1,fp0

	move.b  _asm4+3,d5
	cmp.l   #8,_asm4
	blt.b   .o2
	move.b  #8,d5

.o2
	move.l  slopemach_ecx(pc),a3
	move.l  slopemach_edx(pc),d3

	move.l  d1,d0
	move.l  a4,d2

.innerloop
	lsr.l   d7,d0
	add.l   d5,d1

	and.l   d3,d0
	lsr.l   d6,d2

	add.l   a3,a0
	add.l   d0,d2

	move.l  (a2),a6
	and.w   #$0000,d0

	subq.l  #4,a2
	move.b  (a5,d2.l),d0

	add.l   d4,a4
	move.b  (a6,d0.w),(a0)

	move.l  a4,d2
	move.l  d1,d0

	subq.b  #1,d5
	bne.b   .innerloop
.loopend
	subq.l  #8,_asm4
	bgt.w   .outerloop

	movem.l (sp)+,a2-a6/d2-d7
	rts

	even

	END
