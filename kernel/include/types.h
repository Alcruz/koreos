#ifndef _TYPES_H
#define _TYPES_H

/* Freestanding fixed-width integer types. No libc is available, so these are
 * defined directly against the AArch64 LP64 model. */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long      uint64_t;

typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long               int64_t;

typedef unsigned long      uintptr_t;
typedef unsigned long      size_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef _Bool bool;
#define true  1
#define false 0

#endif /* _TYPES_H */
