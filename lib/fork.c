// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

#define PTE_READABLE(pte) (((pte)!=NULL) && ((*pte) & (PTE_P|PTE_U)) == (PTE_P|PTE_U))
#define PTE_WRITEABLE(pte) (((pte)!=NULL) && ((*pte) & (PTE_P|PTE_U|PTE_W)) == (PTE_P|PTE_U|PTE_W))
#define PTE_COWABLE(pte) (((pte)!=NULL) && ((*pte) & (PTE_P|PTE_U|PTE_COW)) == (PTE_P|PTE_U|PTE_COW))

void _pgfault_upcall(void);

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	// int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	unsigned pn = (unsigned)addr / PGSIZE;
	unsigned index_of_uvpd = pn / 1024;
	unsigned index_of_uvpt = pn % 1024;

	pde_t *pde = (pde_t *)((unsigned)uvpd + index_of_uvpd * 4);
	pte_t *pte = (pte_t *)(((unsigned)uvpt | (index_of_uvpd << 12)) + index_of_uvpt * 4);

	// cprintf("pde: %08x, pte: %08x, *pde: %08x, *pte: %08x, err: %d\n", pde, pte, *pde, *pte, err);

	if((err & FEC_WR) != FEC_WR || !PTE_READABLE(pde) || !PTE_COWABLE(pte))
		panic("Not COW page!\n");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	// panic("pgfault not implemented");

	addr = ROUNDDOWN(addr, PGSIZE);

	if(sys_page_map(0, addr, 0, UTEMP, PTE_P|PTE_U) < 0)
		panic("Failed to call sys_page_map in pgfault!\n");

	if(sys_page_alloc(0, addr, PTE_P|PTE_U|PTE_W) < 0)
		panic("Failed to call sys_page_alloc in pgfault!\n");

	memcpy(addr, UTEMP, PGSIZE);

	if(sys_page_unmap(0, UTEMP) < 0)
		panic("Failed to call sys_page_unmap in pgfault!\n");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	// panic("duppage not implemented");
	unsigned index_of_uvpd = pn / 1024;
	unsigned index_of_uvpt = pn % 1024;

	pde_t *pde = (pde_t *)((unsigned)(uvpd) + index_of_uvpd * 4);
	pte_t *pte = (pte_t *)(((unsigned)(uvpt) | (index_of_uvpd << 12)) + index_of_uvpt * 4);

	void *addr = (void *)(pn * PGSIZE);

	// 处理父子进程共享的页面
	if(PTE_READABLE(pde) && ((*pte) & PTE_SHARE) == PTE_SHARE) {
		if(sys_page_map(0, addr, envid, addr, PTE_SYSCALL) < 0) {
			cprintf("pte: %08x, addr: %08x\n", *pte, addr);
			panic("Failed to copy page mapping of PTE_SHARE!\n");
		}
	}
	// 处理COW和PTE_W
	else if(PTE_READABLE(pde) && PTE_READABLE(pte) && (PTE_WRITEABLE(pte) || PTE_COWABLE(pte))) {
		if(sys_page_map(0, addr, envid, addr, PTE_P|PTE_U|PTE_COW) < 0)
			panic("Failed to call sys_page_map for cow page for child env!\n");
		if(sys_page_map(0, addr, 0, addr, PTE_P|PTE_U|PTE_COW) < 0)
			panic("Failed to call sys_page_map for cow page for parent env!\n");
	}
	else if(PTE_READABLE(pde) && PTE_READABLE(pte)) {
		if(sys_page_map(0, addr, envid, addr, PTE_P|PTE_U) < 0)
			panic("Failed to call sys_page_map for read-only page!\n");
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	// panic("fork not implemented");
	set_pgfault_handler(pgfault);

	envid_t envid = 0;

	// 在子进程中
	if((envid = sys_exofork()) == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// 在父进程中
	if(envid < 0)
		panic("Failed to call sys_exofork!\n");


	// 拷贝父进程地址空间映射
	for(uintptr_t addr = 0; addr < USTACKTOP; addr += PGSIZE)
		duppage(envid, addr / PGSIZE);

	// 为子进程创建user exception stack
	if(sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W) < 0)
		panic("Failed to call sys_page_alloc for user exception stack for child env!\n");

	// 设置子进程pgfault_handler
	sys_env_set_pgfault_upcall(envid, _pgfault_upcall);

	// 使子进程处于可运行状态
	sys_env_set_status(envid, ENV_RUNNABLE);
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
