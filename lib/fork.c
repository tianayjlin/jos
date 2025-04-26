// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>


// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	void* pg_addr = ROUNDDOWN(addr, PGSIZE); // find the page that your fault is on
	pte_t fault_pte = uvpt[(uint32_t)addr / PGSIZE]; // obtain the index of the fault

	if((fault_pte & PTE_COW) == 0 && (err & FEC_WR) == 0) {
		panic("faulting access was not to a COW (moo) or not a write");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	 
	
	// LAB 4: Your code here.

	// curenv -> envid is "invalid" at this point
	envid_t curr_envid = sys_getenvid();

	if(sys_page_alloc(curr_envid, PFTEMP, PTE_W | PTE_U | PTE_P) < 0) {
		panic("sys_page_alloc failed :(");
	}

	// deep copy into temporary page
	memcpy((void*) PFTEMP, pg_addr, PGSIZE);

	// at this point, allow the child process to share memory mappings with the parent UNTIL IT'S WRITTEN TO.
	if(sys_page_map(curr_envid, (void*)PFTEMP, curr_envid, pg_addr, PTE_W | PTE_U | PTE_P) < 0){
		panic("failed to map parent memory to child memory.");
	}
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
// 
// you need to mark COW again because although they are the same virtual address, they 
// are different pages because they are in a different environment
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.

	pte_t pte_pn = uvpt[pn]; 
	void* pg_addr = (void*)(pn * PGSIZE);

	envid_t curenv = sys_getenvid();
	if ((pte_pn & PTE_SHARE) != 0) {
		// pg_addr is virtual, which means the same values can be reused amongst different envs
		r = sys_page_map(curenv, pg_addr, envid, pg_addr, uvpt[pn] & PTE_SYSCALL);
	}
	
	// if parent process is write or COW, apply those COW permissions to the child. 
	else if ((pte_pn & PTE_W) != 0 || (pte_pn & PTE_COW) != 0) {
		r = sys_page_map(curenv, pg_addr, envid, pg_addr, PTE_COW | PTE_U | PTE_P);

		// if parent is not COW, but child is, make sure it is also COW to avoid altering memory 
		if (r == 0 && (pte_pn & PTE_COW) == 0) {
			r = sys_page_map(curenv, pg_addr, curenv, pg_addr, PTE_COW | PTE_U | PTE_P);
		}
	}

	else {
		r = sys_page_map(curenv, pg_addr, envid, pg_addr, PTE_U | PTE_P);
	}
	
	return r; 
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
	
	// change the page fault handler to our custom one for children
	set_pgfault_handler(pgfault);

	// create a new env
	envid_t child_pid = sys_exofork();
	if (child_pid < 0) {
		panic("exofork failed to produce a child");
	} 
	else if (child_pid == 0) {  // current process is the child 
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0; 
	}

	// create child !!! map all stuff from parent to child
	for (unsigned pn = 0; pn < (USTACKTOP / PGSIZE); pn++) {
		// check permission and dupe present and user (short if kernel page dne)
		if((uvpd[PDX(pn * PGSIZE)] & PTE_P) != 0 && (uvpt[pn] & PTE_P) != 0 && (uvpt[pn] & PTE_U) != 0) {
			duppage(child_pid, pn);
		} 
	}
	
	if (sys_page_alloc(child_pid, (void*)(UXSTACKTOP - PGSIZE), PTE_W | PTE_U | PTE_P) < 0 ){
		panic("sys page alloc failed:( pages are not children friendly");
	}

	if(sys_env_set_pgfault_upcall(child_pid, thisenv -> env_pgfault_upcall) < 0) {
		panic("your child cannot fault");
	}

	if(sys_env_set_status(child_pid, ENV_RUNNABLE) < 0) {
		panic("your child is not free range");
	}

	return child_pid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
