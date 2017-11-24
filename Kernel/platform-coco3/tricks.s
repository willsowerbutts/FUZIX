;;;
;;; CoCo3 ghoulish tricks (boo!)
;;;
        .module tricks

	;; imported
        .globl _newproc
        .globl _chksigs
        .globl _getproc
        .globl _trap_monitor
	.globl _get_common
	.globl _swap_finish

	;; exported
        .globl _switchout
        .globl _switchin
        .globl _dofork
	.globl _ramtop

        include "kernel.def"
        include "../kernel09.def"


	.area .data
;;; _ramrop cannot be in common, as this memory becomes per-process
;;; when we add better udata handling.
_ramtop:
	.dw 0

swapstack
	zmb	256
swapstacktop

fork_proc_ptr:
	.dw 0 ; (C type is struct p_tab *) -- address of child process p_tab entry

	.area .common

;;; Switchout switches out the current process, finds another that is READY,
;;; possibly the same process, and switches it in.  When a process is
;;; restarted after calling switchout, it thinks it has just returned
;;; from switchout().
;;;
;;; FIXME: make sure we optimise the switch to self case higher up the stack!
;;;
;;; This function can have no arguments or auto variables.
_switchout:
	orcc 	#0x10		; irq off
        jsr 	_chksigs	; check for signals

        ;; save machine state
        ldd 	#0		; return zero
	;; return code set here is ignored, but _switchin can
        ;; return from either _switchout OR _dofork, so they must both write
        ;; U_DATA__U_SP with the following on the stack:
	pshs 	d,y,u
	sts 	U_DATA__U_SP	; save SP

        jsr 	_getproc	; X = next process ptr
        jsr 	_switchin	; and switch it in
        ; we should never get here
        jsr 	_trap_monitor


badswitchmsg:
	.ascii 	"_switchin: FAIL"
        .db 	13
	.db 	10
	.db 	0

;;; Switch in a process
;;;   takes: X = process
;;;   returns: shouldn't
_switchin:
        orcc 	#0x10		; irq off

	stx 	swapstack	; save passed page table *

	lda	P_TAB__P_PAGE_OFFSET+1,x ; LSB of 16 page
	bne	notswapped		 ; clear
	;; Our choosen process is swapped out
	jsr	_get_common 	; realloc dead's common page
	;; B is now our hand-picked common page

        ; save the above as we are going to switch bank so they will
        ; cease to be accessible if in common (we could put then in
        ; kernel space and probably should)
        lds 	#swapstacktop           ; switch to swap stack
	tfr	b,a
	inca
        sta	0xffa7                  ; transfer to new common
        sta 	0xffaf			; *poof* we now run from new common
	;; save our stashed page no, as pagemap will detroy it
	ldx	swapstack
        jsr 	_swap_finish            ; need to call swapper with
        ;; fix up udata's copy of the page tables
        ;; and fix up the mmu's also
	ldx	swapstack
	ldd     P_TAB__P_PAGE_OFFSET+0,x
	std     U_DATA__U_PAGE
	sta     0xffa0
	inca    
	sta     0xffa1
	stb     0xffa2
	incb
	stb     0xffa3
	ldd     P_TAB__P_PAGE_OFFSET+2,x
	std     U_DATA__U_PAGE2 
	sta     0xffa4
	inca
	sta     0xffa5
	stb     0xffa6
	stx 	swapstack	; save passed page table *

	;; flip in the newly choosen task's common page
notswapped
	lda	P_TAB__P_PAGE_OFFSET+3,x
	inca
	sta	0xffa7
	sta	0xffaf
	
	;; --------- No Stack ! --------------

        ; check u_data->u_ptab matches what we wanted
	ldx 	swapstack
	cmpx 	U_DATA__U_PTAB
        bne 	switchinfail

	lda 	#P_RUNNING
	sta 	P_TAB__P_STATUS_OFFSET,x

	;; clear the 16 bit tick counter
	ldx 	#0
	stx 	_runticks

        ;; restore machine state -- note we may be returning from either
        ;; _switchout or _dofork
        lds 	U_DATA__U_SP
        puls 	x,y,u ; return code and saved U and Y

        ;; enable interrupts, if the ISR isn't already running
	lda	U_DATA__U_ININTERRUPT
	bne 	swtchdone
	andcc 	#0xef
swtchdone:
        rts

switchinfail:
	jsr 	outx
        ldx 	#badswitchmsg
        jsr 	outstring
	;; something went wrong and we didn't switch in what we asked for
        jmp 	_trap_monitor


;;;
;;;	Called from _fork. We are in a syscall, the uarea is live as the
;;;	parent uarea. The kernel is the mapped object.
;;;
_dofork:
        ;; always disconnect the vehicle battery before performing maintenance
        orcc 	#0x10	 ; should already be the case ... belt and braces.

	;; new process in X, get parent pid into y
	stx 	fork_proc_ptr
	ldx 	P_TAB__P_PID_OFFSET,x

        ;; Save the stack pointer and critical registers.
        ;; When this process (the parent) is switched back in, it will be as if
        ;; it returns with the value of the child's pid.
	;; Y has p->p_pid from above, the return value in the parent
        pshs 	x,y,u

        ;; save kernel stack pointer -- when it comes back in the
	;; parent we'll be in
	;; _switchin which will immediately return (appearing to be _dofork()
	;; returning) and with HL (ie return code) containing the child PID.
        ;; Hurray.
        sts 	U_DATA__U_SP

        ;; now we're in a safe state for _switchin to return in the parent
	;; process.

	;; --------- we switch stack copies in this call -----------
	jsr 	fork_copy	; copy 0x000 to udata.u_top and the
				; uarea and return on the childs
				; common
	;; We are now in the kernel child context

        ;; now the copy operation is complete we can get rid of the stuff
        ;; _switchin will be expecting from our copy of the stack.
	puls 	x

        ldx 	fork_proc_ptr	; get forked process
        jsr 	_newproc	; and set it up

	;; any calls to map process will now map the childs memory

	ldx 	#0		; zero out process's tick counter
	stx 	_runticks
        ;; in the child process, fork() returns zero.
	;;
	;; And we exit, with the kernel mapped, the child now being deemed
	;; to be the live uarea. The parent is frozen in time and space as
	;; if it had done a switchout().
        puls y,u,pc


;;; copy the process memory to the new process
;;; and stash parent uarea to old bank
fork_copy:
	;; calculate how many regular pages we need to copy
	ldd	U_DATA__U_TOP	       ; get size of process
	tfr	d,y		       ; make copy of progtop
	anda	#$3f		       ; mask off remainder
	pshs	cc		       ; push onto stack ( remcc )
	tfr	y,d		       ; get copy of progtop
	rola			       ; make low bits = whole pages
	rola			       ;
	rola			       ;
	anda	#$3		       ; bottom two bits are now whole pages
	puls	cc		       ; get remainder's cc (  )
	beq	skip@		       ; skip round-up if zero
	inca			       ; round up
	cmpa	#4		       ; is 4th bank copied?
	pshs	cc,a		       ; and put on stack ( 4th?  no )
	;; copy parent's whole pages to child's
skip@	ldx	fork_proc_ptr
	leax	P_TAB__P_PAGE_OFFSET,x ; X = * new process page tables (dest)
	ldu	#U_DATA__U_PAGE	       ; parent process page tables (src)
loop@	ldb	,x+		       ; B = child's next page
	lda	,u+		       ; A = parent's next page
	jsr	copybank	       ; copy bank
	dec	1,s		       ; bump counter
	bne	loop@
	;; copy UDATA + common (if needed)
	ldx	fork_proc_ptr	       ; X = new process ptr
	ldb	P_TAB__P_PAGE_OFFSET+3,x ;  B = child's UDATA/common page
	puls	cc,a		       ; pull 4th? condition codes
	beq	skip2@		       ; 4th bank already copied?
	lda	U_DATA__U_PAGE+3	 ;  A = parent's UDATA/common page
	jsr	copybank		 ; copy it
	;; remap common page in MMU to new process
skip2@	incb
	stb	0xffaf
	stb	0xffa7
	; --- we are now on the stack copy, parent stack is locked away ---
	rts	; this stack is copied so safe to return on

;;; Copy data from one bank to another
;;;   takes: B = dest bank, A = src bank
copybank
	pshs	d,x,u
	;; save mmu state
	ldd	0xffa8	
	pshs	d
	ldd	0xffaa
	pshs	d
	;; map in src and dest
	ldd	4,s		; D = banks
	stb	0xffa8
	incb
	stb	0xffa9
	sta	0xffaa
	inca
	sta	0xffab
	;; loop setup
	ldx	#0		; dest address
	ldu	#0x4000		; src address
	;; unrolled: 16 bytes at a time
a@	ldd	,u++
	std	,x++
	ldd	,u++
	std	,x++
	ldd	,u++
	std	,x++
	ldd	,u++
	std	,x++
	ldd	,u++
	std	,x++
	ldd	,u++
	std	,x++
	ldd	,u++
	std	,x++
	ldd	,u++
	std	,x++
	cmpx	#0x4000		; end of copy?
	bne	a@		; no repeat
	;; restore mmu
	puls	d
	std	0xffaa
	puls	d
	std	0xffa8
	;; return
	puls	d,x,u,pc		; return
