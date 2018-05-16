#pragma once

#include "output/output.h"

typedef struct output_none_s
{
    output_t output;
} output_none_t;

void output_none_init(output_none_t *output);
