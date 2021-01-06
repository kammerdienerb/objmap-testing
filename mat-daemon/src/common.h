#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#define likely(x)   (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))

#define UINT(w) uint##w##_t
#define SINT(w) int##w##_t

#define u8  UINT(8 )
#define u16 UINT(16)
#define u32 UINT(32)
#define u64 UINT(64)

#define i8  SINT(8 )
#define i16 SINT(16)
#define i32 SINT(32)
#define i64 SINT(64)

#define PAGE_SIZE (4096)

#define UTIL_FN_PRELUDE static __inline__

UTIL_FN_PRELUDE
u64 next_power_of_2(u64 x) {
    if (x == 0) {
        return 2;
    }

    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    x++;

    return x;
}

UTIL_FN_PRELUDE
u64 measure_time_now_ms(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return 1000ULL * tv.tv_sec + (tv.tv_usec / 1000ULL);
}

#endif
