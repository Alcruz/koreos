#ifndef _PANIC_H
#define _PANIC_H

/* Ways to stop the machine. Neither returns. */

/* Park the current core forever in a low-power wait loop. */
void halt(void) __attribute__((noreturn));

/* Report an unrecoverable error on the console, then halt. */
void panic(const char *msg) __attribute__((noreturn));

#endif /* _PANIC_H */
