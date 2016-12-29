	.module devrd_hw

	; imported symbols - from zeta-v2.s
	.globl map_kernel,mpgsel_cache

	; exported symbols (used by devrd.c)
	.globl _rd_page_copy
	.globl _rd_src_page, _rd_src_offset, _rd_dst_page, _rd_dst_offset, _rd_cpy_count

	.include "kernel.def"

	.area _COMMONMEM

;=========================================================================
; _page_copy - Copy data from one physical page to another
; Inputs:
;   _src_page - page number of the source page (uint8_t)
;   _src_offset - offset in the source page (uint16_t)
;   _dst_page - page number of the destination page (uint8_t)
;   _dst_offset - offset in the destination page (uint16_t)
;   _cpy_count - number of bytes to copy (uint16_t)
; Outputs:
;   Data copied
;   Destroys AF, BC, DE, HL
;=========================================================================
_rd_page_copy:
	ld a,(_rd_src_page)
	ld (mpgsel_cache+1),a		; save the mapping
	out (MPGSEL_1),a		; map source page to bank #1
	ld a,(_rd_dst_page)
	ld (mpgsel_cache+2),a		; save the mapping
	out (MPGSEL_2),a		; map destination page to bank #2
	ld hl,(_rd_src_offset)		; load offset in source page
	ld a,#0x40			; add bank #1 offset - 0x4000
	add h				; to the source offset
	ld h,a
	ld de,(_rd_dst_offset)
	ld a,#0x80			; add bank #2 offset - 0x8000
	add d				; to the destination offset
	ld d,a
	ld bc,(_rd_cpy_count)		; bytes to copy
	ldir				; do the copy
	call map_kernel			; map back the kernel
	ret

; variables
_rd_src_page:
	.db	0
_rd_dst_page:
	.db	0
_rd_src_offset:
	.dw	0
_rd_dst_offset:
	.dw	0
_rd_cpy_count:
	.dw	0
;=========================================================================
