#pragma once

#include <stdbool.h>

#include "msp/msp_io.h"

#include "rc/failsafe.h"

#include "util/time.h"

#define INPUT_UPDATE_CHANNEL(input, channel, value)
#define INPUT_SET_MSP_TRANSPORT(input_impl, tr) msp_io_set_transport(&input_impl->input.msp, tr)

typedef struct rc_data_s rc_data_t;

typedef struct input_vtable_s
{
    bool (*open)(void *input, void *config);
    // Returns true iff new data was acquired
    bool (*update)(void *input, rc_data_t *data, time_micros_t now);
    void (*close)(void *input, void *config);
} input_vtable_t;

typedef struct msp_transport_s msp_transport_t;

// Each input type should embed this struct
typedef struct input_s
{
    bool is_open;
    failsafe_t failsafe;
    rc_data_t *rc_data;
    msp_io_t msp;
    input_vtable_t vtable;
} input_t;

bool input_open(rc_data_t *data, input_t *input, void *config);
// Returns true iff new data was acquired
bool input_update(input_t *input, time_micros_t now);
void input_close(input_t *input, void *config);