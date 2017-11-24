;
;	Common elements of low level interrupt and other handling. We
; collect this here to minimise the amount of platform specific gloop
; involved in a port
;
; Some features are controlled by Z80_TYPE which should be declared in
; platform/kernel.def as one of the following values:
;     0   CMOS Z80
;     1   NMOS Z80
;     2   Z180
;
;	Based upon code (C) 2013 William R Sowerbutts
;

	.module lowlevel

	; debugging aids
	.globl outcharhex
	.globl outbc, outde, outhl
	.globl outnewline
	.globl outstring
	.globl outstringhex
	.globl outnibble

	; platform provided functions
	.globl map_kernel
	.globl map_process_always
        .globl map_save
        .globl map_restore
	.globl outchar
	.globl _inint
	.globl _platform_interrupt
	.globl platform_interrupt_all
	.globl _need_resched
	.globl _switchout

        ; exported symbols
	.globl _chksigs
	.globl null_handler
	.globl unix_syscall_entry
        .globl _doexec
        .globl trap_illegal
	.globl nmi_handler
	.globl interrupt_handler
	.globl ___hard_ei
	.globl ___hard_di
	.globl ___hard_irqrestore
	.globl _out
	.globl _in

	.globl mmu_irq_ret

        ; imported symbols
        .globl _trap_monitor
        .globl _unix_syscall
        .globl outstring
        .globl kstack_top
	.globl istack_switched_sp
	.globl istack_top
	.globl _ssig

        .include "platform/kernel.def"
        .include "kernel.def"

; these make the code below more readable. sdas allows us only to 
; test if an expression is zero or non-zero.
CPU_CMOS_Z80	    .equ    Z80_TYPE-0
CPU_NMOS_Z80	    .equ    Z80_TYPE-1
CPU_Z180	    .equ    Z80_TYPE-2

        .area _COMMONMEM
;
;	Called on the user stack in order to process signals that
;	are pending. A user process can longjmp out of this loop so
;	care is needed. Call with interrupts disabled and user mapped.
;
;	Returns with interrupts disabled and user mapped, but may
;	enable interrupts and change mappings.
;
deliver_signals:
	; Pending signal
	ld a, (U_DATA__U_CURSIG)
	or a
	ret z

deliver_signals_2:
	ld l, a
	ld h, #0
	push hl		; signal number as C argument to the handler

	; Handler to use
	add hl, hl
	ld de, #U_DATA__U_SIGVEC
	add hl, de
	ld e, (hl)
	inc hl
	ld d,(hl)

	; Indicate processed
	xor a
	ld (U_DATA__U_CURSIG), a

	; Semantics for now: signal delivery clears handler
	ld (hl), a
	dec hl
	ld (hl), a

	ld bc, #signal_return
	push bc		; bc is passed in as the return vector

	ex de, hl
	ei
	.ifne Z80_MMU_HOOKS
	call mmu_user		; must preserve HL
	.endif
	jp (hl)		; return to user space. This will then return via
			; the return path handler passed in BC

;
;	Syscall signal return path
;
signal_return:
	pop hl		; argument
	di
	.ifne Z80_MMU_HOOKS
	call mmu_kernel
	.endif
	;
	;	We must keep IRQ disabled in the kernel mapped
	;	element of this processing, as we don't want to
	;	set INSYS flags here.
	;
	ld (U_DATA__U_SYSCALL_SP), sp
	ld sp, #kstack_top
	call map_kernel
	call _chksigs
	call map_process_always
	ld sp, (U_DATA__U_SYSCALL_SP)
	jr deliver_signals


;
;	Syscall processing path
;
unix_syscall_entry:
        di
        ; store processor state
        ex af, af'
        push af
        ex af, af'
        exx
        push bc		; FIXME we don't I tihnk need to save bc/de/hl
        push de		; as they are compiler caller save
        push hl
        exx
        push bc
        push de
        push ix
        push iy
	; We don't save AF or HL
        ; locate function call arguments on the userspace stack
        ld hl, #18     ; 16 bytes machine state, plus 2 bytes return address
        add hl, sp

	.ifne Z80_MMU_HOOKS
	call mmu_kernel		; must preserve HL
	.endif
        ; save system call number
        ld a, (hl)
        ld (U_DATA__U_CALLNO), a
        ; advance to syscall arguments
        inc hl
        inc hl
        ; copy arguments to common memory
        ld bc, #8      ; four 16-bit values
        ld de, #U_DATA__U_ARGN
        ldir           ; copy  FIXME use LDI x 8

	ld a, #1
	ld (U_DATA__U_INSYS), a

        ; save process stack pointer
        ld (U_DATA__U_SYSCALL_SP), sp
        ; switch to kernel stack
        ld sp, #kstack_top

        ; map in kernel keeping common
	call map_kernel

        ; re-enable interrupts
        ei

        ; now pass control to C
        call _unix_syscall

	;
	; WARNING: There are two special cases to beware of here
	; 1. fork() will return twice from _unix_syscall
	; 2. execve() will not return here but will hit _doexec()
	;
	; The fork case returns with a different U_DATA mapped so the
	; U_DATA referencing code is fine, but globals are usually not

        di


	call map_process_always

	xor a
	ld (U_DATA__U_INSYS), a

	; Back to the user stack
	ld sp, (U_DATA__U_SYSCALL_SP)

	ld hl, (U_DATA__U_ERROR)
	ld de, (U_DATA__U_RETVAL)

	ld a, (U_DATA__U_CURSIG)
	or a

	; Fast path the normal case
	jr nz, via_signal

	; Restore stacks and go
	;
	; Should we change the ABI and just return in DE/HL ?
	;
unix_return:
	ld a, h
	or l
	jr z, not_error
	scf		; carry flag on return state for errors
	jr unix_pop

not_error:
	ex de, hl	; return the retval instead
	;
	; Undo the stacking and go back to user space
	;
unix_pop:
	.ifne Z80_MMU_HOOKS
	call mmu_user		; must preserve HL
	.endif
        ; restore machine state
        pop iy
        pop ix
        pop de
        pop bc
        exx
        pop hl
        pop de
        pop bc
        exx
        ex af, af'
        pop af
        ex af, af'
        ei
        ret ; must immediately follow EI


via_signal:
	; Get off the kernel syscall stack before we start signal
	; handling. Our signal handlers may themselves elect to make system
	; calls. This means we must also save the error/return code
	ld hl, (U_DATA__U_ERROR)
	push hl
	ld hl, (U_DATA__U_RETVAL)
	push hl

	; Signal processing. This may longjmp back into userland
	call deliver_signals_2

	; If not then we recover the syscall return values and
	; exit via the syscall return path
	pop de			; retval
	pop hl			; errno
	jr unix_return


;
;	Final component of execve()
;
_doexec:
        di
        call map_process_always

        pop bc ; return address
        pop de ; start address

        ld hl, (U_DATA__U_ISP)
        ld sp, hl      ; Initialize user stack, below main() parameters and the environment

        ; u_data.u_insys = false
        xor a
        ld (U_DATA__U_INSYS), a

        ex de, hl

	; for the relocation engine - tell it where it is
	ld iy, #PROGLOAD
	.ifne Z80_MMU_HOOKS
	call mmu_user		; must preserve HL
	.endif
        ei
        jp (hl)

;
;  Called from process context (hopefully)
;
;  FIXME: hardcoded RST30 won't work on all boxes
;
null_handler:
	; kernel jump to NULL is bad
	ld a, (U_DATA__U_INSYS)
	or a
	jp nz, trap_illegal
	ld a, (_inint)
	jp nz, trap_illegal
	; user is merely not good
	ld hl, #7
	push hl
	ld ix, (U_DATA__U_PTAB)
	ld l,P_TAB__P_PID_OFFSET(ix)
	ld h,P_TAB__P_PID_OFFSET+1(ix)
	push hl
	ld hl, #39		; signal (getpid(), SIGBUS)
	push hl
	call unix_syscall_entry		; syscall
	ld hl, #0xFFFF
	push hl
	dec hl			; #0
	push hl
	call unix_syscall_entry



illegalmsg: .ascii "[illegal]"
            .db 13, 10, 0

trap_illegal:
        ld hl, #illegalmsg
traphl:
        call outstring
        call _trap_monitor

nmimsg: .ascii "[NMI]"
        .db 13,10,0

nmi_handler:
	.ifne Z80_MMU_HOOKS
	call mmu_kernel
	.endif
	call map_kernel
        ld hl, #nmimsg
	jr traphl

;
;	Interrupt handler. Not quite the same as syscalls, we need to
;	stack everything and we must get off the IRQ stack and then
;	process need_resched and signals
;
interrupt_handler:
        ; store machine state
        ex af,af'
        push af
        ex af,af'
        exx
        push bc
        push de
        push hl
        exx
        push af
        push bc
        push de
        push hl
        push ix
        push iy
	;
	; This is a bit exciting - if our MMU enforces r/o then the entire
	; stack state might be bogus!
	;
	.ifne Z80_MMU_HOOKS
	ld hl, #mmu_irq_ret
	jp mmu_kernel_irq
	.endif
mmu_irq_ret:

	; Some platforms (MSX for example) have devices we *must*
	; service irrespective of kernel state in order to shut them
	; up. This code must be in common and use small amounts of stack
	call platform_interrupt_all
	; FIXME: add profil support here (need to keep profil ptrs
	; unbanked if so ?)
.ifeq CPU_Z180
        ; On Z180 we have more than one IRQ, so we need to track of which one
        ; we arrived through. The IRQ handler sets irqvector_hw when each
        ; interrupt arrives. If we are not already handling an interrupt then
        ; we copy this into _irqvector which is the value the kernel code
        ; examines (and will not change even if reentrant interrupts arrive).
        ; Generally the only place that irqvector_hw should be used is in
        ; the platform_interrupt_all routine.
        .globl hw_irqvector
        .globl _irqvector
        ld a, (hw_irqvector)
        ld (_irqvector), a
.endif

	; Get onto the IRQ stack
	ld (istack_switched_sp), sp
	ld sp, #istack_top

	ld a, (0)

	call map_save
	;
	;	FIXME: re-implement sanity checks and add a stack one
	;

	; We need the kernel mapped for the IRQ handling
	call map_kernel

	cp #0xC3
	call nz, null_pointer_trap

	; So the kernel can check rapidly for interrupt status
	; FIXME: move to the C code
	ld a, #1
	ld (_inint), a
	; So we know that this task should resume with IRQs off
	ld (U_DATA__U_ININTERRUPT), a

	call _platform_interrupt

	xor a
	ld (_inint), a

	ld a, (_need_resched)
	or a
	jr nz, preemption

	; Back to the old memory map
	call map_restore

	;
	; Back on user stack
	;
	ld sp, (istack_switched_sp)

intout:
	xor a
	ld (U_DATA__U_ININTERRUPT), a

	ld hl, #intret
	push hl
	reti			; We have now 'left' the interrupt
				; and the controllers have seen the
				; reti M1 cycle. However we still
				; have DI set
intret:
	di
	ld a, (U_DATA__U_INSYS)
	or a
	jr nz, interrupt_pop

	; Loop through any pending signals. These could longjmp out
	; of the handler so ensure everything is fixed before this !

	call deliver_signals
	.ifne Z80_MMU_HOOKS
	call mmu_restore_irq
	.endif

	; Then unstack and go.
interrupt_pop:
        pop iy
        pop ix
        pop hl
        pop de
        pop bc
        pop af
        exx
        pop hl
        pop de
        pop bc
        exx
        ex af, af'
        pop af
        ex af, af'
        ei			; Must be instruction before ret
	ret			; runs in the ei interrupt shadow

;
;	Called with the kernel mapped, mid interrupt and on the IRQ stack
;
null_pointer_trap:
	ld a, #0xC3
	ld (0), a
	ld hl, #11		; SIGSEGV
trap_signal:
	push hl
	ld hl, (U_DATA__U_PTAB);
	push hl
        call _ssig
        pop hl
        pop hl
	ret


;
;	Pre-emption. We need to get off the interrupt stack, switch task
;	and clean up the IRQ state carefully
;

	.globl preemption

preemption:
	xor a
	ld (_need_resched), a	; Task done

	; Back to the old memory map
	call map_restore

	ld hl, (istack_switched_sp)
	ld (U_DATA__U_SYSCALL_SP), hl

	ld sp, #kstack_top	; We don't pre-empt in a syscall
				; so this is fine
	ld hl, #intret2
	push hl
	reti			; We have now 'left' the interrupt
				; and the controllers have seen the
				; reti M1 cycle. However we still
				; have DI set

	;
	; We are now on the syscall stack (which is fine, we don't
	; pre-empt mid syscall so therefore it is free.  We will now
	; task switch. The process being pre-empted will disappear into
	; switchout() and whoever is next will come out of the same -
	; hence the need to reti

	;
intret2:call map_kernel

	;
	; Semantically we are doing a null syscall for pre-empt. We need
	; to record ourselves as in a syscall so we can't be recursively
	; pre-empted when switchout re-enables interrupts.
	;
	ld a, #1
	ld (U_DATA__U_INSYS), a
	;
	; Process status is offset 0
	;
	ld hl, (U_DATA__U_PTAB)
	ld (hl), #P_READY
	call _switchout
	;
	; We are no longer in an interrupt or a syscall
	;
	xor a
	ld (U_DATA__U_ININTERRUPT), a
	ld (U_DATA__U_INSYS), a
	;
	; We have been rescheduled, remap ourself and go back to user
	; space via signal handling
	;
	call map_process_always	; Get our user mapping back


	; We were pre-empted but have now been rescheduled
	; User stack
	ld sp, (U_DATA__U_SYSCALL_SP)
	ld a, (U_DATA__U_CURSIG)
	or a
	call nz, deliver_signals_2
	;
	; pop the stack and go
	;
	.ifne Z80_MMU_HOOKS
	call mmu_user
	.endif
	jr interrupt_pop

;
;	Debugging helpers
;

	.area _COMMONMEM

; outstring: Print the string at (HL) until 0 byte is found
; destroys: AF HL
outstring:
        ld a, (hl)     ; load next character
        and a          ; test if zero
        ret z          ; return when we find a 0 byte
        call outchar
        inc hl         ; next char please
        jr outstring

; print the string at (HL) in hex (continues until 0 byte seen)
outstringhex:
        ld a, (hl)     ; load next character
        and a          ; test if zero
        ret z          ; return when we find a 0 byte
        call outcharhex
        ld a, #0x20 ; space
        call outchar
        inc hl         ; next char please
        jr outstringhex

; output a newline
outnewline:
        ld a, #0x0d  ; output newline
        call outchar
        ld a, #0x0a
        jp outchar

outhl:  ; prints HL in hex.
	push af
        ld a, h
        call outcharhex
        ld a, l
        call outcharhex
	pop af
        ret

outbc:  ; prints BC in hex.
	push af
        ld a, b
        call outcharhex
        ld a, c
        call outcharhex
	pop af
        ret

outde:  ; prints DE in hex.
	push af
        ld a, d
        call outcharhex
        ld a, e
        call outcharhex
	pop af
        ret

; print the byte in A as a two-character hex value
outcharhex:
        push bc
	push af
        ld c, a  ; copy value
        ; print the top nibble
        rra
        rra
        rra
        rra
        call outnibble
        ; print the bottom nibble
        ld a, c
        call outnibble
	pop af
        pop bc
        ret

; print the nibble in the low four bits of A
outnibble:
        and #0x0f ; mask off low four bits
        cp #10
        jr c, numeral ; less than 10?
        add a, #0x07 ; start at 'A' (10+7+0x30=0x41='A')
numeral:add a, #0x30 ; start at '0' (0x30='0')
        jp outchar

;
;	I/O helpers for cases we don't use __sfr
;
_out:
	pop hl
	pop bc
	out (c), b
	push bc
	jp (hl)

_in:
	pop hl
	pop bc
	push bc
	push hl
	in l, (c)
	ret

;
;	Enable interrupts
;
___hard_ei:
	ei
	ret

;
;	Pull in the CPU specific workarounds
;

.ifeq CPU_NMOS_Z80
	.include "lowlevel-z80-nmos.s"
.else
	.include "lowlevel-z80-cmos.s"
.endif
