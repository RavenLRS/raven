#include "rc/rc_data.h"

#include "input.h"

bool input_open(rc_data_t *data, input_t *input, void *config)
{
    if (input)
    {
        failsafe_init(&input->failsafe);
        rc_data_reset_input(data);
        input->rc_data = data;
        if (!input->is_open && input->vtable.open)
        {
            input->is_open = input->vtable.open(input, config);
            return input->is_open;
        }
    }
    return false;
}

bool input_update(input_t *input, time_micros_t now)
{
    bool updated = false;
    if (input && input->is_open && input->vtable.update)
    {
        updated = input->vtable.update(input, input->rc_data, now);
        failsafe_update(&input->failsafe, now);
        // Input to this msp_io is redirected to the output's msp_io
        // (if any) by rc_t
        msp_io_update(&input->msp);
    }
    return updated;
}

void input_close(input_t *input, void *config)
{
    if (input && input->is_open && input->vtable.close)
    {
        input->vtable.close(input, config);
        input->is_open = false;
    }
}