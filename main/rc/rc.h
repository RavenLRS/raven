#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "config/config.h"

#include "input/input_air.h"
#include "input/input_air_bind.h"
#include "input/input_crsf.h"
#include "input/input_fake.h"
#include "input/input_ibus.h"
#include "input/input_ppm.h"

#include "output/output_air.h"
#include "output/output_air_bind.h"
#include "output/output_air_rf_power_test.h"
#include "output/output_crsf.h"
#include "output/output_fport.h"
#include "output/output_msp.h"
#include "output/output_none.h"
#include "output/output_sbus.h"

#include "rc/failsafe.h"
#include "rc/rc_data.h"
#include "rc/rc_rmp.h"

typedef struct air_bind_packet_s air_bind_packet_t;
typedef struct air_freq_table_s air_freq_table_t;
typedef struct air_radio_s air_radio_t;
typedef struct rc_s rc_t;
typedef struct rmp_s rmp_t;
typedef struct rmp_port_s rmp_port_t;
typedef struct msp_conn_s msp_conn_t;

typedef struct rc_rmp_msp_port_s
{
    const rmp_port_t *port;
    msp_conn_t *conn;
} rc_rmp_msp_port_t;

typedef struct rc_rmp_resp_ctx_s
{
    rc_t *rc;
    air_addr_t dst;
    uint8_t dst_port;
    time_ticks_t allocated_at;
} rc_rmp_resp_ctx_t;

typedef struct rc_s
{
    rc_data_t data;
    air_radio_t *radio;
    rmp_t *rmp;

    union {
        input_air_t air;
        input_air_bind_t air_bind;
        input_crsf_t crsf;
        input_ppm_t ppm;
        input_ibus_t ibus;
        input_fake_t fake;
    } inputs;
    union {
        output_air_t air;
        output_air_bind_t air_bind;
        output_air_rf_power_test_t air_power_test;
        output_crsf_t crsf;
        output_fport_t fport;
        output_msp_t msp;
        output_none_t none;
        output_sbus_t sbus;
    } outputs;

    void *input_config;
    input_t *input;
    void *output_config;
    output_t *output;

    struct
    {
#if defined(USE_TX_SUPPORT) && defined(USE_RX_SUPPORT)
        rc_mode_e rc_mode;
#endif
        bool invalidate_input;
        bool invalidate_output;
        bool bind_requested;
        bool bind_active;
        bool accept_bind;
        bool dirty;
        int dismissed_count;
        int dismissed_pairings;
        int tx_rf_power;
        time_ticks_t pair_air_config_next_req; // 0 zero means the data is confirmed
        // RMP messages handled by rc_t
        rc_rmp_t rc_rmp;
        // MSP/RMP Transport fields
        rc_rmp_msp_port_t rmp_msp_port[3];  // Used for sending MSP requests
        const rmp_port_t *msp_recv_port;    // Used for receiving MSP requests
        rc_rmp_resp_ctx_t msp_resp_ctx[30]; // Used for keeping data to handlea sync MSP responses via RMP
    } state;
} rc_t;

void rc_init(rc_t *rc, air_radio_t *radio, rmp_t *rmp);

rc_mode_e rc_get_mode(const rc_t *rc);
bool rc_is_binding(const rc_t *rc);
void rc_accept_bind(rc_t *rc);
bool rc_has_pending_bind_request(rc_t *rc, air_bind_packet_t *packet);

bool rc_is_failsafe_active(const rc_t *rc, failsafe_reason_e *reason);
int rc_get_rssi_db(rc_t *rc);
int rc_get_rssi_percentage(rc_t *rc);
float rc_get_snr(rc_t *rc);
unsigned rc_get_update_frequency(rc_t *rc);
bool rc_get_frequencies_table(rc_t *rc, air_freq_table_t *freqs);

const char *rc_get_pilot_name(rc_t *rc);
const char *rc_get_craft_name(rc_t *rc);

// Connects the given MSP to the output's MSP and forwards
// all requests from msp to the output as well as its responses
// back to the input.
void rc_connect_msp_input(rc_t *rc, msp_conn_t *msp);

int rc_get_alternative_pairings(rc_t *rc, air_pairing_t *pairings, size_t size);
void rc_switch_pairing(rc_t *rc, air_pairing_t *pairing);
void rc_dismiss_alternative_pairings(rc_t *rc);

void rc_invalidate_input(rc_t *rc);
void rc_invalidate_output(rc_t *rc);

void rc_update(rc_t *rc);