// Simple implementation of cprintf console output for the kernel,
// based on printfmt() and the kernel console's cputchar().

#include <inc/stdarg.h>
#include <inc/stdio.h>
#include <inc/types.h>

//outputs the char and keeps track of length.
static void putch(int ch, int *cnt) {
  cputchar(ch);
  *cnt++;
}

//determines format specifiers
int vcprintf(const char *fmt, va_list ap) {
  int cnt = 0;

  vprintfmt((void *)putch, &cnt, fmt, ap);
  return cnt;
}

//accepts format string and optional arguments
int cprintf(const char *fmt, ...) {
  va_list ap;
  int cnt;

  va_start(ap, fmt);
  cnt = vcprintf(fmt, ap);
  va_end(ap);

  return cnt;
}

/*
Lab 1, Exercise 8
cprintf() -> vcprintf() -> putch() -> kern/console.c/putchar(); 
*/