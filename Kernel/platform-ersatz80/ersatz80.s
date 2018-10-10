; 2018-10-07 William R Sowerbutts
; ersatz80 hardware specific code
; based on Sergey Kiselev's zeta-v2 code

        .module ersatz80

        ; exported symbols
        .globl init_hardware
	.globl _program_vectors
	.globl map_kernel
	.globl map_process
	.globl map_process_always
	.globl map_save
	.globl map_restore
	.globl _irqvector
	.globl platform_interrupt_all
	.globl mpgsel_cache
	.globl _kernel_pages
	.globl _trap_reboot
	.globl _bufpool

        ; imported symbols
        .globl _ramsize
        .globl _procmem
        .globl _init_hardware_c
        .globl outhl
        .globl outnewline
	.globl interrupt_handler
	.globl unix_syscall_entry
	.globl nmi_handler
	.globl null_handler

	; exported debugging tools
	.globl inchar
	.globl outchar

        .include "kernel.def"
        .include "../kernel.def"

;=========================================================================
; Buffers
;=========================================================================
        .area _BUFFERS
_bufpool:
        .ds (BUFSIZE * 4) ; adjust NBUFS in config.h in line with this

;=========================================================================
; Initialization code
;=========================================================================
        .area _DISCARD
init_hardware:
        ; program vectors for the kernel
        ld hl, #0
        push hl
        call _program_vectors
        pop hl

        ; WRS -- I'd prefer to use IM 2 but this will require a hardware revision
        ; so that we can trap the int ack sequence so the Teensy can place the
        ; low bits of the vector onto the bus at the right time.
	;; ld hl,#intvectors
	;; ld a,h				; get bits 15-8 of int. vectors table
	;; ld i,a				; load to I register
	;; im 2				; set Z80 CPU interrupt mode 2
        im 1                            ; set Z80 CPU interrupt mode 1 (single vector at 0x0038)
        jp _init_hardware_c             ; pass control to C, which returns for us

;=========================================================================
; Kernel code
;=========================================================================
        .area _CODE

_trap_reboot:
        ; TODO
        di
        halt


;=========================================================================
; Common Memory (0xF000 upwards)
;=========================================================================
        .area _COMMONMEM

;=========================================================================
; Interrupt stuff
;=========================================================================
; IM2 interrupt verctors table
; Note: this is linked after the udata block, so it is aligned on 256 byte
; boundary
;; intvectors:
;; 	.dw	ctc0_int		; CTC CH0 used as prescaler for CH1
;; 	.dw	ctc1_int		; timer interrupt handler
;; 	.dw	serial_int		; UART interrupt handler
;; 	.dw	ppi_int			; PPI interrupt handler

_irqvector:
	.db	0			; used to identify interrupt vector

;; ; CTC CH0 shouldn't be used to generate interrupts
;; ; but we'll implement it just in case
;; ctc0_int:
;; 	push af
;; 	xor a				; IRQ vector = 0
;; 	ld (_irqvector),a		; store it
;; 	pop af
;; 	jp interrupt_handler
;; 
;; ; periodic timer interrupt
;; ctc1_int:
;; 	push af
;; 	ld a,#1				; IRQ vector = 1
;; 	ld (_irqvector),a		; store it
;; 	pop af
;; 	jp interrupt_handler
;; 
;; ; UART interrupt
;; serial_int:
;; 	push af
;; 	ld a,#2				; IRQ vector = 2
;; 	ld (_irqvector),a		; store it
;; 	pop af
;; 	jp interrupt_handler
;; 
;; ; PPI interrupt - not used for now
;; ppi_int:
;; 	push af
;; 	ld a,#3				; IRQ vector = 3
;; 	ld (_irqvector),a		; store it
;; 	pop af
;; 	jp interrupt_handler

platform_interrupt_all:
	ret

; install interrupt vectors
_program_vectors:
	di
	pop de				; temporarily store return address
	pop hl				; function argument -- base page number
	push hl				; put stack back as it was
	push de

	; At this point the common block has already been copied
	call map_process

	; write zeroes across all vectors
	ld hl,#0
	ld de,#1
	ld bc,#0x007f			; program first 0x80 bytes only
	ld (hl),#0x00
	ldir

	; now install the interrupt vector at 0x0038
	ld a,#0xC3			; JP instruction
	ld (0x0038),a
	ld hl,#interrupt_handler
	ld (0x0039),hl

	; set restart vector for UZI system calls
	ld (0x0030),a			; rst 30h is unix function call vector
	ld hl,#unix_syscall_entry
	ld (0x0031),hl

	ld (0x0000),a
	ld hl,#null_handler		; to Our Trap Handler
	ld (0x0001),hl

	ld (0x0066),a			; Set vector for NMI
	ld hl,#nmi_handler
	ld (0x0067),hl

	jr map_kernel

;=========================================================================
; map_process_always - map process pages
; Inputs: page table address in #U_DATA__U_PAGE
; Outputs: none; all registers preserved
;=========================================================================
map_process_always:
	push hl
	ld hl,#U_DATA__U_PAGE
        jr map_process_2_pophl_ret

;=========================================================================
; map_process - map process or kernel pages
; Inputs: page table address in HL, map kernel if HL == 0
; Outputs: none; A and HL destroyed
;=========================================================================
map_process:
	ld a,h
	or l				; HL == 0?
	jr nz,map_process_2		; HL == 0 - map the kernel

;=========================================================================
; map_kernel - map kernel pages
; Inputs: none
; Outputs: none; all registers preserved
;=========================================================================
map_kernel:
	push hl
	ld hl,#_kernel_pages
        jr map_process_2_pophl_ret

;=========================================================================
; map_process_2 - map process or kernel pages, update mpgsel_cache
; Inputs: page table address in HL
; Outputs: none, HL destroyed
;=========================================================================
map_process_2:
        push af
        ld a, (hl)
        ld (mpgsel_cache+0), a
        out (MPGSEL_0), a
        inc hl
        ld a, (hl)
        ld (mpgsel_cache+1), a
        out (MPGSEL_1), a
        inc hl
        ld a, (hl)
        ld (mpgsel_cache+2), a
        out (MPGSEL_2), a
        pop af
        ret

;=========================================================================
; map_restore - restore a saved page mapping
; Inputs: none
; Outputs: none, all registers preserved
;=========================================================================
map_restore:
	push hl
	ld hl,#map_savearea
map_process_2_pophl_ret:
	call map_process_2
	pop hl
	ret

;=========================================================================
; map_save - save the current page mapping to map_savearea
; Inputs: none
; Outputs: none
;=========================================================================
map_save:
	push hl
	ld hl,(mpgsel_cache)
	ld (map_savearea),hl
	ld hl,(mpgsel_cache+2)
	ld (map_savearea+2),hl
	pop hl
	ret

; we cache MPGSEL registers much like zeta-v2 although on this hardware
; we could just read out the values instead...
mpgsel_cache:
	.db	0,0,0,0

; kernel page mapping -- first 4 banks
_kernel_pages:
	.db	0,1,2,3

; memory page mapping save area for map_save/map_restore
map_savearea:
	.db	0,0,0,0

;=========================================================================
; Basic console I/O
;=========================================================================

;=========================================================================
; outchar - Wait for UART TX idle, then print the char in A
; Inputs: A - character to print
; Outputs: none
;=========================================================================
outchar:
	push af
	; wait for transmitter to be idle
ocloop: in a, (UART0_STATUS)            ; read status register
        bit 6, a                        ; ready?
        jr nz, ocloop                   ; loop if not
	; now output the char to serial port
	pop af
	out (UART0_DATA),a
	ret

;=========================================================================
; inchar - Wait for character on UART, return in A
; Inputs: none
; Outputs: A - received character, F destroyed
;=========================================================================
inchar:
        in a, (UART0_STATUS)            ; read status register
        bit 7, a                        ; ready?
        jr z, inchar                    ; loop if not
        in a, (UART0_DATA)              ; read character from UART
	ret