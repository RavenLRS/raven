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

#define CONSTRAIN(v, min, max) ({ \
    __typeof(v) __v = v;          \
    __typeof(min) __min = min;    \
    __typeof(max) __max = max;    \
    assert(min < max);            \
    if (__v < __min)              \
    {                             \
        __v = __min;              \
    }                             \
    else if (__v > __max)         \
    {                             \
        __v = __max;              \
    }                             \
    __v;                          \
})

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

#define UNUSED1(a) (void)(a)
#define UNUSED2(a, b) (void)(a), UNUSED1(b)
#define UNUSED3(a, b, c) (void)(a), UNUSED2(b, c)
#define UNUSED4(a, b, c, d) (void)(a), UNUSED3(b, c, d)
#define UNUSED5(a, b, c, d, e) (void)(a), UNUSED4(b, c, d, e)
#define UNUSED6(a, b, c, d, e, f) (void)(a), UNUSED5(b, c, d, e, f)

#define VA_NUM_ARGS_IMPL(_1, _2, _3, _4, _5, N, ...) N
#define VA_NUM_ARGS(...) VA_NUM_ARGS_IMPL(__VA_ARGS__, 5, 4, 3, 2, 1)

#define UNUSED_IMPL_(nargs) UNUSED##nargs
#define UNUSED_IMPL(nargs) UNUSED_IMPL_(nargs)
#define UNUSED(...)                       \
    UNUSED_IMPL(VA_NUM_ARGS(__VA_ARGS__)) \
    (__VA_ARGS__)
