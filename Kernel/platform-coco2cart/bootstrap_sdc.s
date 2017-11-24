;;; The ROM-resident fuzix loader for the CoCoSDC drive.  Mostly copied from
;;; Alan's IDE booter.

;
;	Bootstrap for cartridge start up. For now load from block 1 (second
;	block). We can refine this later. We do 51 block loads from block 1
;	for 1A00 to 7FFF and then copy 1A00 to 0100 for vectors
;
;	Needs some kind of global timeout -> error handling
;
;
;	Load block b
;
	.module bootstrap

	.globl load_image
	.globl _cocoswap_dev

	.area .text
_cocoswap_dev:
	.dw $0900		; SD card slice 0 rest of

;;; Load a sector from SDC
;;;   takes: B - sector number, X = dest address
;;;   returns: nothing
load_block:
	clr <$ff49		; put lba to sdc
	clr <$ff4a		;
	stb <$ff4b		;
	ldb #$80		; read sector op
	stb <$ff48		; 
	exg x,x			; wait a few cycles for sdc status reg
	exg x,x			;   to be valid. - 2 long noops
	ldb #$2			; wait till sdc is ready
	;; FIXME: test for failure in this loop 
a@	bitb <$ff48		;
	beq a@			;
	;; tranfer data
	lda #128		; 128 x 16 bits = 256 byte sectors
	pshs a			; push count on stack
c@	ldd <$ff4a		; get 16bit data from drive
	std ,x++		; stick it in memory
	dec ,s			; bump counter
	bne c@			; repeat
	leas 1,s		; drop counter
	;; wait for return value
	ldb #$1
	;; FIXME: test for failure in the loop, also.
b@	bitb <$ff48		;
	bne b@			; wait for busy
	;; FIXME: get return status here and test?
	puls pc			; return



load_image:
	orcc #$50
	ldy #$0400		; display at this point
	lda #'G'		;
	sta ,y+			;
	ldd #$ff43		; set DP to io page
	tfr a,dp		; good for size + speed
	stb <$ff40		; turn on SDC's LBA mode 
	;; we have to wait maybe 12us before doing anything
	;; with the SDC now, but the code below should
	;; delay SDC access enough.
	;; load kernel
	ldb #0			; start block no (* 2 for 256 byte sectors)
	lda #102		; number of blocks (* 2 for 256 bytes sectors)
	ldx #$1A00		; dest address
load_loop:
	pshs a,b
	bsr load_block
	lda #'*'
	sta ,y+
	puls a,b
	incb
	deca
	bne load_loop
	ldx #$1A00
	ldu #$0100
vec_copy:
	ldd ,x++
	std ,u++
	cmpu #$0200
	bne vec_copy

	ldx #$1A00
	clra
	clrb
ud_wipe:
	std ,x++
	cmpx #$1C00
	bne ud_wipe
	lda ,x+
	cmpa #$15
	bne wrong_err
	lda ,x+
	cmpa #$C0
	bne wrong_err
	rts

wrong_err:
	ldu #wrong
	bra l2
load_error:
	ldu #fail
l2:	bsr copy_str
l1:	bra l1

copy_str:
	lda ,u+
	beq endstr
	sta ,y+
	bra copy_str
endstr:	rts
fail:
	.ascii "Fail"
	.db 0
wrong:
	.ascii "Wrong image"
	.db 0
