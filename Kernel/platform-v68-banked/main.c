#include <kernel.h>
#include <timer.h>
#include <kdata.h>
#include <printf.h>
#include <devtty.h>
#include <buddy.h>

void platform_idle(void)
{
	/* FIXME: disable IRQ, run tty interrupt, re-enable ? */
}

void do_beep(void)
{
}

/*
 *	MMU initialize
 */

void map_init(void)
{
}

void pagemap_init(void)
{
	/* 0 means swapped */
	/* We number 1-4 and reply on the hardware wrapping to 0,3,2,1...
	   in the more general case if you use 0 then you need to invent
	   it */
	pagemap_add(3);
	pagemap_add(2);
	pagemap_add(1);
	pagemap_add(4);
}

uaddr_t ramtop;
uint8_t need_resched;

/* Each process is mapped into the same banked address */
uaddr_t pagemap_base(void)
{
	return MAPBASE;
}

uint8_t platform_param(char *p)
{
	return 0;
}

void platform_discard(void)
{
}

void memzero(void *p, usize_t len)
{
	memset(p, 0, len);
}

arg_t _memalloc(void)
{
	udata.u_error = ENOSYS;
	return -1;
}

arg_t _memfree(void)
{
	udata.u_error = ENOSYS;
	return -1;
}

/* Live udata and kernel stack for each process */
/* FIXME: we need to find a way to make these smaller or bank them out but
   that has a material cost */

u_block udata_block[PTABSIZE];
uint16_t irqstack[128];

/* This will belong in the core 68K code once finalized */

void install_vdso(void)
{
	extern uint8_t vdso[];
	/* Should be uput etc */
	memcpy((void *)udata.u_codebase, &vdso, 0x40);
}

extern void *get_usp(void);
extern void set_usp(void *p);

void signal_frame(uint8_t *trapframe, uint32_t d0, uint32_t d1, uint32_t a0,
	uint32_t a1)
{
	extern void *udata_shadow;
	uint8_t *usp = get_usp();
	udata_ptr = udata_shadow;
	uint16_t ccr = *(uint16_t *)trapframe;
	uint32_t addr = *(uint32_t *)(trapframe + 2);
	int err = 0;

	/* Build the user stack frame */

	/* FIXME: eventually we should put the trap frame details and trap
	   info into the frame */
	usp -= 4;
	err |= uputl(addr, usp);
	usp -= 4;
	err |= uputw(ccr, usp);
	usp -= 2;
	err |=uputl(a1, usp);
	usp -= 4;
	err |= uputl(a0, usp);
	usp -= 4;
	err |= uputl(d1, usp);
	usp -= 4;
	err |= uputl(d0, usp);
	usp -= 4;
	err |= uputl(udata.u_codebase + 4, usp);
	set_usp(usp);

	if (err) {
		kprintf("%d: stack fault\n", udata.u_ptab->p_pid);
		doexit(dump_core(SIGKILL));
	}
	/* Now patch up the kernel frame */
	*(uint16_t *)trapframe = 0;
	*(uint32_t *)(trapframe + 2) = (uint32_t)udata.u_sigvec[udata.u_cursig];
	udata.u_sigvec[udata.u_cursig] = SIG_DFL;
	udata.u_cursig = 0;
}
