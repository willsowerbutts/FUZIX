; Simple standalone monitor for P112 board.
; Based on YM.MAC and M580 monitor.

; WRS - pulled in from UZI-180, modified for sdas

; TODO
; - this could be shared with Z80 with minor tweaks only
; - platform "outchar" routine must now preserve all registers (this is now the case for P112 but not other platforms)
; - mon_getch should be platform specific (code in here is ESCC specific) -> move to new platform "inchar" routine
; - make the "bad instruction" trap optional - in0 is used in this code but is the only z180-specific instruction (outside of mon_getch which is to be replaced with inchar instead)

	.module z180monitor
	.z180

	; exported symbols
	.globl mon_ept
	.globl trap_ept
	.globl _trap_monitor

	; imported symbols
	.globl outchar
	.globl outcharhex
	.globl outhl
	.globl outstring
	.globl outnewline

	.include "kernel.def"
	.include "../cpu-z180/z180.def"

	.area _COMMONMEM

; ASCII mnemonics
CR	.equ 13
LF	.equ 10
ESC	.equ 27

mon_ept:
	ld	sp, #mon_stack
	ld	hl, #prompt
	call	outstring
	call	getln
	ld	hl, #linbfr
	ld	a,(hl)
	call	uc
	ld	hl, #mon_ept
	push	hl
	cp	#CR
	ret	z
	push	af
	call	get_args
	ld	bc,(arg3)
	ld	de,(arg2)
	ld	hl,(arg1)
	pop	af
	cp	#'D'
	jp	z,dump
	cp	#'C'
	jp	z,comp
	cp	#'F'
	jp	z,fill
	cp	#'S'
	jp	z,search
	cp	#'T'
	jp	z,xfer
	cp	#'M'
	jp	z,modif
	cp	#'G'
	jp	z,run
	cp	#'I'
	jp	z,inport
	cp	#'O'
	jp	z,outport
	cp	#'X'
	jp	z,showregs
	cp	#'N'
	jp	z,continue
;	cp	'R'
;	jp	z,RLOAD
error:	ld	a, #'?'
	call	outchar
	jr	mon_ept

prompt:	
	.ascii "\r\nMON>"
	.db 0

getln:	ld	hl,#linbfr
	ld	c, #0
get:	call	mon_getch
	cp	#8
	jr	z,del
	cp	#0x7F
	jr	z,del
	cp	#3
	jr	z,ctrlc
	cp	#ESC
	jr	z,ctrlc
	call	outchar
	ld	(hl),a
	cp	#CR
	ret	z
	ld	a,#20
	cp	c
	jp	z,error
	inc	hl
	inc	c
	jr	get
del:	ld	a,c
	or	a
	jr	z,get
	ld	a,#8
	call	outchar
	ld	a,#' '
	call	outchar
	ld	a,#8
	call	outchar
	dec	hl
	dec	c
	jr	get
ctrlc:	ld	hl, #ctlcm
	call	outstring
	jp	mon_ept

ctlcm:	.ascii	'^C'
	.db	CR,LF,0

uc:	cp	#'a'
	ret	c
	cp	#'z'+1
	ret	nc
	and	#0x5F
	ret

; get command line arguments

get_args:
	ld	hl,#0
	ld	(arg1),hl
	ld	(arg2),hl
	ld	(arg3),hl
	ld	de,#(linbfr+1)
	call	gethex
	ld	(arg1),hl
	ld	(arg2),hl
	ret	c
	call	gethex
	ld	(arg2),hl
	ret	c
	call	gethex
	ld	(arg3),hl
	ret	c
	jp	error

gethex:	ld	hl,#0
gh1:	ld	a,(de)
	call	uc
	inc	de
	cp	#CR
	jp	z,aend
	cp	#','
	ret	z
	cp	#' '
	jr	z,gh1
	sub	#'0'
	jp	m,error
	cp	#10
	jp	m,dig
	cp	#0x11
	jp	m,error
	cp	#0x17
	jp	p,error
	sub	#7
dig:	ld	c,a
	ld	b,#0
	add	hl,hl
	add	hl,hl
	add	hl,hl
	add	hl,hl
	jp	c,error
	add	hl,bc
	jp	gh1
aend:	scf
	ret

hl_eq_de:	ld	a,h
	cp	d
	ret	nz
	ld	a,l
	cp	e
	ret

last:	call	hl_eq_de
	jp	z,cmp_eq
	dec	de
	ret
next:	call	shldstop
next1:	call	hl_eq_de
	jp	z,cmp_eq
	inc	hl
	ret
cmp_eq:	inc	sp
	inc	sp
	ret

shldstop:	call	mon_status
	or	a
	ret	z
	call	mon_getch
	cp	#3
	jp	z,ctrlc
	cp	#ESC
	jp	z,ctrlc
	cp	#0x20
	jr	z,wkey
	cp	#0x13	; CTRL/S
	ret	nz
wkey:	call	mon_getch
	ret

; T addr1,addr2,addr3
; Transfer region addr1...addr2 to addr3,
; source and dest regions may overlap

xfer:	ld	a,l	; modify to use LDIR/LDDR
	sub	c
	ld	a,h
	sbc	a,b
	jr	nc,m_inc
	push	de
	ex	de,hl
	or	a
	sbc	hl,de
	add	hl,bc
	ld	c,l
	ld	b,h
	ex	de,hl
	pop	de
m_dcr:	ld	a,(de)
	ld	(bc),a
	dec	bc
	call	last
	jr	m_dcr
m_inc:	ld	a,(hl)
	ld	(bc),a
	inc	bc
	call	next1
	jr	m_inc

; D addr1,addr2
; Dump region addr1...addr2

dump:	call	out_addr
	push	hl
dmph:   ld	a,(hl)
	call	outbyte
	call	shldstop
	call	hl_eq_de
	jr	z,enddmp
	inc	hl
	ld	a,l
	and	#0x0F
	jr	nz,dmph
	pop	hl
	call	dumpl
	jr	dump
enddmp:	pop	hl
dumpl:  ld	a,(hl)
	cp	#0x20
	jr	c,outdot
	cp	#0x7f
	jr	c,char
outdot:	ld	a,#'.'
char:	call	outchar
	call	shldstop
	call	hl_eq_de
	ret	z
	inc	hl
	ld	a,l
	and	#0x0F
	jr	nz,dumpl
	ret

; F addr1,addr2,byte
; Fill region addr1...addr2 with byte

fill:	ld	(hl),c
	call	next
	jp	fill

; C addr1,addr2,addr3
; Compare region addr1...addr2 with region at addr3

comp:	ld	a,(bc)
	cp	(hl)
	jr	z,same
	call	out_addr
	ld	a,(hl)
	call	outbyte
	ld	a,(bc)
	call	outbyte
same:	inc	bc
	call	next
	jr	comp

; S addr1,addr2,byte
; Search region addr1...addr2 for byte

search:	ld	a,c
	cp	(hl)
	jr	nz,scont
	call	out_addr
	dec	hl
	ld	a,(hl)
	call	outbyte
	ld	a,#'('
	call	outchar
	inc	hl
	ld	a,(hl)
	call	outcharhex
	ld	a,#')'
	call	outchar
	ld	a,#' '
	call	outchar
	inc	hl
	ld	a,(hl)
	call	outbyte
	dec	hl
scont:	call	next
	jr	search

; M addr
; Modify memory starting at addr

modif:	call	out_addr
	ld	a,(hl)
	call	outbyte
	push	hl
	call	getln
	pop	hl
	ld	de,#linbfr
	ld	a,(de)
	cp	#CR
	jp	z,cont
	push	hl
	call	gethex
	ld	a,l
	pop	hl
	ld	(hl),a
cont:	inc	hl
	jr	modif

; I port
; Input from port

inport:	call	outnewline
	ld	a,l
	call	outcharhex
	ld	a,#'='
	call	outchar
	ld	c,l
	ld	b,h		; or ld b,0
	in	a,(c)
	call	outcharhex
	ret

; O port,byte
; Output to port

outport:ld	c,l
	ld	b,h		; or ld b,0
	ld	a,e
	out	(c),a
	ret

; G addr
; Go (execute) program at addr. Progam may use return 
; instruction to return to monitor

run:	call	r1
	ret
r1:	jp	(hl)

out_addr:
	call	outnewline
	call	outhl
	ld	a,#':'
	call	outchar
	ld	a,#' '
	jp	outchar

outbyte:
	call	outcharhex
	ld	a,#' '
	jp	outchar

; should save the current MMU status as well

_trap_monitor:
trap_ept:
	di	; turn off those pesky interrupts
	ld	(pgm_sp),sp
	ld	sp,#reg_stack
	ex	af,af'
	push	af
	ex	af,af'
	exx	
	push	bc
	push	de
	push	hl
	exx
	push	af
	push	bc
	push	de
	push	hl
	push	ix
	push	iy
	ld	sp,#mon_stack
	ld	hl,(pgm_sp)
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	(pgm_pc),de
	ld	hl,#brk_msg
	in0	a,(INT_ITC)
	ld	b,a
	and	#0x80		; TRAP bit set?
	jr	z,no_trap
	dec	de		; PC-1
	ld	a,b
	and	#0x40		; UFO bit set?
	jr	z,no_ufo_adj
	dec	de		; PC-2 if UFO was set
no_ufo_adj:
	ld	hl,#trap_msg
no_trap:
	call	outnewline
	call	outstring
	ld	hl,#atpc_msg
	call	outstring
	ex	de,hl
	call	outhl
	call	outnewline
	jp	mon_ept

trap_msg:
	.ascii	'Illegal instruction trap'
	.db	0
brk_msg:
	.ascii	'Break'
	.db	0
atpc_msg:
	.ascii	' at PC='
	.db	0

; N
; coNtinue execution from the last break

continue:
	ld	sp,#pgm_iy
	pop	iy
	pop	ix
	pop	hl
	pop	de
	pop	bc
	pop	af
	exx
	pop	hl
	pop	de
	pop	bc
	exx
	ex	af,af'
	pop	af
	ex	af,af'
	ld	sp,(pgm_sp)
	ret

; X
; show the contents of the CPU registers

showregs:
	ld	hl,#regdmp
showr1:	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	inc	hl
	ld	a,d
	or	e
	ret	z
	ex	de,hl
	ld	c,(hl)
	inc	hl
	ld	b,(hl)
	push	bc
	ex	de,hl
	call	outstring
	ex	(sp),hl
	call	outhl
	pop	hl
	inc	hl
	jr	showr1

regdmp:	.dw	pgm_af
	.db	CR,LF
	.ascii	'AF='
	.db	0
	.dw	pgm_bc
	.ascii	' BC='
	.db	0
	.dw	pgm_de
	.ascii	' DE='
	.db	0
	.dw	pgm_hl
	.ascii	' HL='
	.db	0
	.dw	pgm_ix
	.ascii	' IX='
	.db	0
	.dw	pgm_iy
	.ascii	' IY='
	.db	0
	.dw	pgm_af1
	.db	CR,LF
	.ascii	'AF'
	.db	0x27,0
	.dw	pgm_bc1
	.ascii	' BC'
	.db	0x27,0
	.dw	pgm_de1
	.ascii	' DE'
	.db	0x27,0
	.dw	pgm_hl1
	.ascii	' HL'
	.db	0x27,0
	.dw	pgm_pc
	.ascii	' PC='
	.db	0
	.dw	pgm_sp
	.ascii	' SP='
	.db	0
	.dw	0

mon_getch:
	call	mon_status
	or	a
	jr	z,mon_getch
	in0	a,(ESCC_DATA_A)
	and	#0x7F
	ret

mon_status:
	in0	a,(ESCC_CTRL_A)
	rra
	sbc	a,a		; return FF if ready, 0 otherwise
	ret

arg1:	.ds	2
arg2:	.ds	2
arg3:	.ds	2
linbfr:	.ds	21

pgm_iy:	.ds	2
pgm_ix:	.ds	2
pgm_hl:	.ds	2
pgm_de:	.ds	2
pgm_bc:	.ds	2
pgm_af:	.ds	2
pgm_hl1:.ds	2
pgm_de1:.ds	2
pgm_bc1:.ds	2
pgm_af1:.ds	2
reg_stack:
pgm_sp:	.ds	2
pgm_pc:	.ds	2

	; monitor uses a private stack
	.ds	100
mon_stack:
