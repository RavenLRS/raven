#include "data_state.h"

void data_state_init(data_state_t *ds)
{
    ds->dirty_since = 0;
    ds->last_sent = 0;
    ds->last_update = 0;
    data_state_reset_ack(ds);
}

uint32_t data_state_score(data_state_t *ds, time_micros_t now)
{
    if (ds->dirty_since > 0)
    {
        return ((now - ds->dirty_since) * 50) + (now - ds->last_sent);
    }
    // Not dirty
    return now - ds->last_sent;
}

void data_state_update(data_state_t *ds, bool changed, time_micros_t now)
{
    if (changed)
    {
        ds->ack_at_seq = -1;
        ds->ack_received = false;
        if (ds->dirty_since == 0)
        {
            ds->dirty_since = now;
        }
    }
    ds->last_update = now;
}

void data_state_sent(data_state_t *ds, int ack_at_seq, time_micros_t now)
{
    ds->ack_at_seq = ack_at_seq;
    ds->dirty_since = 0;
    ds->last_sent = now;
}

void data_state_stop_ack(data_state_t *ds)
{
    ds->ack_at_seq = -1;
}

void data_state_reset_ack(data_state_t *ds)
{
    data_state_stop_ack(ds);
    ds->ack_received = false;
}

void data_state_update_ack_received(data_state_t *ds, int seq)
{
    if (!ds->ack_received)
    {
        if (ds->ack_at_seq >= 0 && ds->ack_at_seq == seq)
        {
            ds->ack_received = true;
            ds->ack_at_seq = -1;
        }
    }
}
