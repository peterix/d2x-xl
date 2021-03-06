// checker[HA] added 05/17/99 Matt Mueller
// FD_* on linux use asm, but checker doesn't like it.  Borrowed these non-asm versions outta <selectbits.h>
#include <setjmp.h>

#ifdef __CHECKER__

#undef FD_ZERO(set)
#undef FD_SET(d, set)
#undef FD_CLR(d, set)
#undef FD_ISSET(d, set)

#define FD_ZERO(set) \
    do { \
        uint32_t __i; \
        for (__i = 0; __i < sizeof(__fd_set) / sizeof(__fd_mask); ++__i) \
            (reinterpret_cast<__fd_mask *> set)[__i] = 0; \
    } while (0)
#define FD_SET(d, set) ((set)->fds_bits[__FDELT(d)] |= __FDMASK(d))
#define FD_CLR(d, set) ((set)->fds_bits[__FDELT(d)] &= ~__FDMASK(d))
#define FD_ISSET(d, set) ((set)->fds_bits[__FDELT(d)] & __FDMASK(d))

// checker doesn't seem to handle jmp's correctly...
#undef setjmp(env)
#define setjmp(env) __chcksetjmp(env)
#undef longjmp(env, val)
#define longjmp(env, val) __chcklongjmp(env, val)

int32_t __chcklongjmp(jmp_buf buf, int32_t val);
int32_t __chcksetjmp(jmp_buf buf);

void chcksetwritable(char *p, int32_t size);
void chcksetunwritable(char *p, int32_t size);

#endif
