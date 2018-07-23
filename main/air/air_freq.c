#include <hal/log.h>

#include "air/air_freq.h"

#include "util/macros.h"

#define MAX_OFFSET (23 * 2) // in 0.125mhz steps, so 64/8 = 8Mhz up/down

static const char *TAG = "Air.Freq";

void air_freq_table_init(air_freq_table_t *tbl, air_key_t key, unsigned long base_freq)
{
    LOG_D(TAG, "Calculating freq table with key %u, base %lu", key, base_freq);
    uint32_t lfsr = key;
    uint32_t b;
    for (int ii = 0; ii < ARRAY_COUNT(tbl->freqs); ii++)
    {
        b = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
        lfsr = (lfsr >> 1) | (b << 15);
        tbl->freqs[ii] = base_freq + (((int64_t)lfsr % (MAX_OFFSET * 2) - MAX_OFFSET)) * 1e6 / 8;
        LOG_D(TAG, "Freq %d = %lu", ii, tbl->freqs[ii]);
        tbl->abs_errors[ii] = 0;
        tbl->last_errors[ii] = 0;
    }
}