#include <math.h>

#include "config/config.h"

#include "rmp/rmp_air.h"

#include "air_io.h"

void air_io_init(air_io_t *io, air_addr_t addr, air_io_bind_t *bind, rmp_air_t *rmp)
{
    io->addr = addr;
    io->last_frame_received = 0;
    if (bind)
    {
        io->bind = *bind;
    }
    else
    {
        memset(&io->bind, 0, sizeof(io->bind));
    }
    io->rmp = rmp;
    memset(&io->pairing, 0, sizeof(io->pairing));
    lpf_init(&io->rssi, 0.1);
    lpf_init(&io->snr, 0.1);
    lpf_init(&io->lq, 0.5);
    lpf_init(&io->average_frame_interval, 1);
}

bool air_io_has_bind_request(air_io_t *io, air_bind_packet_t *packet, air_lora_band_e *band, bool *needs_confirmation)
{
    air_bind_packet_t p;
    bool nc;
    air_lora_band_e nb;
    air_io_has_request_f has_request = io->bind.has_request;
    if (has_request)
    {
        if (!packet)
        {
            packet = &p;
        }
        if (!band)
        {
            band = &nb;
        }
        if (!needs_confirmation)
        {
            needs_confirmation = &nc;
        }
        return has_request(io->bind.user_data, packet, band, needs_confirmation);
    }
    return false;
}

bool air_io_accept_bind_request(air_io_t *io)
{
    if (io->bind.accept_request)
    {
        return io->bind.accept_request(io->bind.user_data);
    }
    return false;
}

void air_io_bind(air_io_t *io, air_pairing_t *pairing)
{
    io->pairing = *pairing;
    if (!config_get_air_info(&io->pairing_info, NULL, &pairing->addr))
    {
        memset(&io->pairing_info, 0, sizeof(io->pairing_info));
    }
    if (io->rmp)
    {
        rmp_air_set_bound_addr(io->rmp, &pairing->addr);
    }
}

bool air_io_is_bound(air_io_t *io)
{
    return air_addr_is_valid(&io->pairing.addr);
}

bool air_io_get_bound_addr(air_io_t *io, air_addr_t *addr)
{
    if (air_io_is_bound(io))
    {
        *addr = io->pairing.addr;
        return true;
    }
    return false;
}

void air_io_on_frame(air_io_t *io, time_micros_t now)
{
    if (io->last_frame_received > 0)
    {
        lpf_update(&io->average_frame_interval, (now - io->last_frame_received) * 1e-6f, now);
    }
    io->last_frame_received = now;
}

void air_io_update_rssi(air_io_t *io, int rssi, int snr, int lq, time_micros_t now)
{
    lpf_update(&io->rssi, rssi, now);
    lpf_update(&io->snr, snr, now);
    lpf_update(&io->lq, lq, now);
}

void air_io_update_reset_rssi(air_io_t *io)
{
    lpf_reset(&io->rssi, 0);
    lpf_reset(&io->snr, 0);
    lpf_reset(&io->lq, 0);
}

unsigned air_io_get_update_frequency(const air_io_t *io)
{
    float value = lpf_value(&io->average_frame_interval);
    if (value > 0)
    {
        return roundf(1 / value);
    }
    return 0;
}