#include "output/output_none.h"

static bool output_none_open(void *output, void *config)
{
    return true;
}

static bool output_none_update(void *output, rc_data_t *data, bool update_rc, time_micros_t now)
{
    return true;
}

static void output_none_close(void *output, void *config)
{
}

void output_none_init(output_none_t *output)
{
    output->output.flags = OUTPUT_FLAG_LOCAL;
    output->output.vtable = (output_vtable_t){
        .open = output_none_open,
        .update = output_none_update,
        .close = output_none_close,
    };
}
