#pragma once

#include "platform.h"

#define PIN_COUNT(v) (__builtin_popcountll(v))
#define PIN_USABLE_MAX 16
#define PIN_USABLE_COUNT PIN_COUNT(PIN_USABLE_MASK)

#define PIN_IS_USABLE(n) (PIN_USABLE_MASK & PIN_N(n))
#define PIN_USABLE_GET_IDX(n) (__builtin_popcountll(PIN_USABLE_MASK & (~0ull >> (PIN_MAX - n + 1))))

int pin_usable_at(int idx);
