/* See COPYRIGHT for copyright information. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/pmap.h>
#include <kern/kclock.h>

#define GET_TF(tf)				  \
	asm volatile ("movl %%ebp, %0\n\t"        \
		      "movl 4(%%ebp), %%eax\n\t"  \
		      "movl %%eax, %1\n\t"        \
		      "movl 8(%%ebp), %%eax\n\t"  \
		      "movl %%eax, %2\n\t"        \
		      "movl 12(%%ebp), %%eax\n\t" \
		      "movl %%eax, %3\n\t"        \
		      "movl 16(%%ebp), %%eax\n\t" \
		      "movl %%eax, %4\n\t"        \
		      "movl 20(%%ebp), %%eax\n\t" \
		      "movl %%eax, %5"            \
		      :"=m"((tf).ebp), "=m"((tf).eip), "=m"((tf).args[0]), "=m"((tf).args[1]), "=m"((tf).args[2]), "=m"((tf).args[3]) \
		      : \
		      :"%eax" \
		);



// Test the stack backtrace function (lab 1 only)
void
test_backtrace(int x)
{
	// 获取栈桢
	struct Trapframe tf;
	GET_TF(tf);
	if (x > 0)
		test_backtrace(x-1);
	else
		mon_backtrace(0, 0, &tf);
	mon_backtrace(0, 0, &tf);
}

void
i386_init(void)
{
	extern char edata[], end[];

	// Before doing anything else, complete the ELF loading process.
	// Clear the uninitialized global data (BSS) section of our program.
	// This ensures that all static/global variables start out zero.
	memset(edata, 0, end - edata);

	// Initialize the console.
	// Can't call cprintf until after we do this!
	cons_init();

	cprintf("6828 decimal is %o octal!\n", 6828);

	// Lab 2 memory management initialization functions
	mem_init();
	// Test the stack backtrace function (lab 1 only)
	test_backtrace(5);
	// 记录i386_init栈桢结构
	struct Trapframe tf;
	GET_TF(tf);
	mon_backtrace(0, 0, &tf);

	// Drop into the kernel monitor.
	while (1)
		monitor(NULL);
}


/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	// Be extra sure that the machine is in as reasonable state
	asm volatile("cli; cld");

	va_start(ap, fmt);
	cprintf("kernel panic at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

dead:
	/* break into the kernel monitor */
	while (1)
		monitor(NULL);
}

/* like panic, but don't */
void
_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}
