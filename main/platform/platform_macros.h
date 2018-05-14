#include <stdint.h>

#define PIN_MAX 63
#define PIN_FULL_MASK 0xFFFFFFFFFFFFFFFFll
#define PIN_N(n) ((uint64_t)1 << n)