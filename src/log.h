#ifndef OKRETY_LOG_H
#define OKRETY_LOG_H

void log_init(int my_pid);

/* Log zmiany stanu — zawsze drukowany. Format: "[pid] [tNNN] msg". */
void log_state(const char *fmt, ...);

/* Log diagnostyczny per-wiadomość — drukowany tylko z -DDEBUG. */
#ifdef DEBUG
void log_debug(const char *fmt, ...);
#else
static inline void log_debug(const char *fmt, ...) { (void)fmt; }
#endif

#endif
