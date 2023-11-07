/* armflush.c - flush the instruction cache

   __clear_cache is used in noocrun.c,  It is a built-in
   intrinsic with gcc.  However nooc in order to compile
   itself needs this function */

#ifdef __TINYC__

/* syscall wrapper */
unsigned _noocsyscall(unsigned syscall_nr, ...);

/* arm-nooc supports only fake asm currently */
__asm__(
    ".global _noocsyscall\n"
    "_noocsyscall:\n"
    "push    {r7, lr}\n\t"
    "mov     r7, r0\n\t"
    "mov     r0, r1\n\t"
    "mov     r1, r2\n\t"
    "mov     r2, r3\n\t"
    "svc     #0\n\t"
    "pop     {r7, pc}"
    );

/* from unistd.h: */
#if defined(__thumb__) || defined(__ARM_EABI__)
# define __NR_SYSCALL_BASE      0x0
#else
# define __NR_SYSCALL_BASE      0x900000
#endif
#define __ARM_NR_BASE           (__NR_SYSCALL_BASE+0x0f0000)
#define __ARM_NR_cacheflush     (__ARM_NR_BASE+2)

#define syscall _noocsyscall

#else

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

#endif

/* Flushing for noocrun */
void __clear_cache(void *beginning, void *end)
{
/* __ARM_NR_cacheflush is kernel private and should not be used in user space.
 * However, there is no ARM asm parser in nooc so we use it for now */
    syscall(__ARM_NR_cacheflush, beginning, end, 0);
}
