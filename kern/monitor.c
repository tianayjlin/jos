// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/assert.h>
#include <inc/memlayout.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/kdebug.h>
#include <kern/monitor.h>

#include <kern/hidden.h>

#define CMDBUF_SIZE 80 // enough for one VGA text line

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
    {"hidden", "Run hidden test cases", exec_hidden_cases},
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

int exec_hidden_cases(int argc, char **argv, struct Trapframe *tf) {
  hidden_test_cases();
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
