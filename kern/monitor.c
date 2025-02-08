// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include "inc/mmu.h"
#include "inc/types.h"
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line

struct Command {
  const char *name;
  const char *desc;
  // return -1 to force monitor to exit
  int (*func)(int argc, char **argv, struct Trapframe *tf);
};

// LAB 1: add your command to here...
static struct Command commands[] = {
    {"help", "Display this list of commands", mon_help},
    {"kerninfo", "Display information about the kernel", mon_kerninfo},
    {"backtrace", "Backtrace the entire stack", mon_backtrace},
    {"show", "Print a pretty image", mon_show},
    {"showmappings", "Easy to read format of physical page mappings", mon_showmappings},
};

/***** Implementations of basic kernel monitor commands *****/

int mon_help(int argc, char **argv, struct Trapframe *tf) {
  int i;

  for (i = 0; i < ARRAY_SIZE(commands); i++)
    cprintf("%s - %s\n", commands[i].name, commands[i].desc);
  return 0;
}

int mon_show(int argc, char **argv, struct Trapframe *tf) {
  cprintf("           \033[32mboing\033[m         \033[33mboing\033[m         "
          "\033[35mboing\033[m              \n"
          " \033[31me-e\033[m           . - .         . - .         . - .   "
          "       \n"
          "\033[36m(\\_/)\\\033[m       '       `.   ,'       `.   ,'       "
          ".    "
          "    \n"
          " \033[36m`-'\\ `--.___,\033[m         . .           . .          "
          ".       \n"
          "    \033[36m'\\( ,_.-'\033[m                                     "
          "        \n"
          "       \033[36m\\\\\033[m               "
          "            a:f    \n"
          "       \033[36m^'\033[m\n");

  return 0;
}

int mon_kerninfo(int argc, char **argv, struct Trapframe *tf) {
  extern char _start[], entry[], etext[], edata[], end[];

  cprintf("Special kernel symbols:\n");
  cprintf("  _start                  %08x (phys)\n", _start);
  cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
  cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
  cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
  cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
  cprintf("Kernel executable memory footprint: %dKB\n",
          ROUNDUP(end - entry, 1024) / 1024);
  return 0;
}

int mon_backtrace(int argc, char **argv, struct Trapframe *tf) {
  // LAB 1: Your code here.
  // HINT 1: use read_ebp().
  // HINT 2: print the current ebp on the first line (not current_ebp[0])

  cprintf("Stack backtrace:\n");

  // read ebp to get location on the stack
  uint32_t *p_ebp = (uint32_t *)read_ebp();

  while (p_ebp) {
    // eip is return address, right below the ebp
    uint32_t eip_val = *(p_ebp + 1);

    // Print ebp and eip
    cprintf("  ebp %08x  eip %08x  args", p_ebp, eip_val);

    // obtain 5 arguments; args start follow ebp, eip on stack
    for (int i = 0; i < 5; i++) {
      cprintf(" %08x", *(p_ebp + i + 2));
    }

    cprintf("\n");

    // Grab debug info and output
    struct Eipdebuginfo info;
    debuginfo_eip(eip_val, &info);

    cprintf("         %s:%d: %.*s+%d\n", info.eip_file, info.eip_line,
            info.eip_fn_namelen, info.eip_fn_name, eip_val - info.eip_fn_addr);

    // update the ebp to move to the last stack frame, * chain to however many
    // stacks there is
    p_ebp = (uint32_t *)(*p_ebp);
  }

  // call
  return 0;
}

static void print_flags(pte_t pte, physaddr_t phys_addr, uint32_t virt_addr) {
    char perms[9] = "--------\0";
    if (pte & PTE_U) perms[0] = 'U';
    if (pte & PTE_W) perms[1] = 'W';
    if (pte & PTE_PWT) perms[2] = 'T';
    if (pte & PTE_PCD) perms[3] = 'C';
    if (pte & PTE_A) perms[4] = 'A';
    if (pte & PTE_D) perms[5] = 'D';
    if (pte & PTE_PS) perms[6] = 'S';
    if (pte & PTE_G) perms[7] = 'G';

    cprintf("0x%08x         0x%08x          %s\n", virt_addr, phys_addr, perms);
}

int mon_showmappings(int argc, char **argv, struct Trapframe *tf) {
  
  // showmappings is argv[0]
  // convert addresses to integers 
  if (argc < 2 || argc > 3) {
    cprintf("i am writing this so i don't have to restart the kernel every time i type the function invocation incorrectly.\n");
    return -1;
  }

    // turn the inputs into values that can be later cast as an address
  uint32_t l_bound = strtol(argv[1], NULL, 16); 
  uint32_t r_bound = argc == 2 ? l_bound : strtol(argv[2], NULL, 16) ;
   
  // check to make sure that l_bound > r_bound and r_bound - l_bound 
  if(l_bound > r_bound) {
    cprintf("the addresses that you have provided do not constitute a valid page range. try again.\n"); 
    return -1;
  }

  // Align bounds with page size
  l_bound = ROUNDDOWN(l_bound, PGSIZE);
  r_bound = ROUNDUP(r_bound, PGSIZE);

  cprintf("Displaying Page Info for VA [0x%08x] - [0x%08x]\n", l_bound, r_bound);
  cprintf("Virtual Address    Physical Address    Permissions\n");
  cprintf("--------------------------------------------------\n");
  for(uint32_t i = l_bound; i <= r_bound; i += PGSIZE) {
    // Get pte
    pte_t *pte = pgdir_walk(kern_pgdir, (void*)i, 0);

    // Check if pte exists
    if (!pte || !(*pte & PTE_P)) {
      cprintf("0x%08x         Not Mapped          --\n");
      continue;
    } else {
      print_flags(*pte, PTE_ADDR(*pte), i);
    }
    
  }

  return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int runcmd(char *buf, struct Trapframe *tf) {
  int argc;
  char *argv[MAXARGS];
  int i;

  // Parse the command buffer into whitespace-separated arguments
  argc = 0;
  argv[argc] = 0;
  while (1) {
    // gobble whitespace
    while (*buf && strchr(WHITESPACE, *buf))
      *buf++ = 0;
    if (*buf == 0)
      break;

    // save and scan past next arg
    if (argc == MAXARGS - 1) {
      cprintf("Too many arguments (max %d)\n", MAXARGS);
      return 0;
    }
    argv[argc++] = buf;
    while (*buf && !strchr(WHITESPACE, *buf))
      buf++;
  }
  argv[argc] = 0;

  // Lookup and invoke the command
  if (argc == 0)
    return 0;
  for (i = 0; i < ARRAY_SIZE(commands); i++) {
    if (strcmp(argv[0], commands[i].name) == 0)
      return commands[i].func(argc, argv, tf);
  }
  cprintf("Unknown command '%s'\n", argv[0]);
  return 0;
}

void monitor(struct Trapframe *tf) {
  char *buf;

  cprintf("Welcome to the JOS kernel monitor!\n");
  cprintf("Type 'help' for a list of commands.\n");

  while (1) {
    buf = readline("K> ");
    if (buf != NULL)
      if (runcmd(buf, tf) < 0)
        break;
  }
}
