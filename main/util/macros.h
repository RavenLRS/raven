#pragma once

#include <assert.h>

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))
#define ARRAY_ASSERT_COUNT(arr, count, msg) _Static_assert(ARRAY_COUNT(arr) == count, msg)
#define PACKED __attribute__((packed))

#ifndef MAX
#define MAX(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#endif

#ifndef MIN
#define MIN(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _b : _a; })
#endif

#define CONSTRAIN(x, min, max) MIN(MAX(x, min), max)
#define CONSTRAIN_TO_I8(x) CONSTRAIN(x, INT8_MIN, INT8_MAX)

#define STR_EQUAL(s1, s2) (strcmp(s1, s2) == 0)
#define STR_HAS_PREFIX(s, p) (strstr(s, p) == s)

#define ASSERT(x)            \
    do                       \
    {                        \
        if (!(x))            \
        {                    \
            assert(0 && #x); \
        }                    \
    } while (0)

#define UNREACHABLE() (assert(0 && "unreachable"))

#define LIKELY(x) __builtin_expect((x), 1)
#define UNLIKELY(x) __builtin_expect((x), 0)
