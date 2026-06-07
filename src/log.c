#include "log.h"
#include "clock.h"

#include <stdarg.h>
#include <stdio.h>

static int MY = -1;

void log_init(int my_pid) { MY = my_pid; }

void log_state(const char *fmt, ...) {
  fprintf(stdout, "[%d] [t%d] ", MY, clock_get());
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
  fputc('\n', stdout);
  fflush(stdout);
}

#ifdef DEBUG
void log_debug(const char *fmt, ...) {
  fprintf(stdout, "[%d] [t%d] (dbg) ", MY, clock_get());
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
  fputc('\n', stdout);
  fflush(stdout);
}
#endif
