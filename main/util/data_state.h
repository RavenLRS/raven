#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "time.h"

typedef struct data_state_s
{
    // Time when the data became dirty. If zero, it means
    // it has been sent to the output and hasn't changed since
    // then.
    time_micros_t dirty_since;
    // Last time we sent this data via the output.
    time_micros_t last_sent;
    // Last time the data was received from the input.
    time_micros_t last_update;
    int ack_at_seq;
    bool ack_received;
} data_state_t;

void data_state_init(data_state_t *ds);
uint32_t data_state_score(data_state_t *ds, time_micros_t now);
void data_state_update(data_state_t *ds, bool changed, time_micros_t now);
inline time_micros_t data_state_get_last_update(const data_state_t *ds) { return ds->last_update; }
inline bool data_state_has_value(const data_state_t *ds) { return ds->last_update > 0; }
inline bool data_state_is_dirty(const data_state_t *ds) { return ds->dirty_since > 0; }
void data_state_sent(data_state_t *ds, int ack_at_seq, time_micros_t now);
// Stop the ACK if it's in progress
void data_state_stop_ack(data_state_t *ds);
// Stop the ACK and also cancel if it was already acknoledged
void data_state_reset_ack(data_state_t *ds);
void data_state_update_ack_received(data_state_t *ds, int seq);
inline bool data_state_is_ack_received(data_state_t *ds) { return ds->ack_received; }
