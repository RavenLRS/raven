#include <stdio.h>
#include <string.h>

#include <hal/rand.h>

#include "platform.h"

#include "air/air_lora.h"

#include "platform/system.h"

#include "rc/rc_data.h"

#include "util/crc.h"

#include "air.h"

typedef enum {
    PACKET_TYPE_BIND_REQ,
    PACKET_TYPE_BIND_ACCEPT,
} packet_type_e;

#define RAVEN_BIND_PKT_VERSION 0

#define RAVEN_EXPLICIT_PKT_MARKER "RVN" // Used for packets with explicit header
#define RAVEN_EXPLICIT_PKT_MARKER_LEN 3

void air_pairing_format(const air_pairing_t *pairing, char *buf, size_t bufsize)
{
    air_addr_format(&pairing->addr, buf, bufsize);
}

void air_addr_format(const air_addr_t *addr, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%02X:%02X:%02X:%02X:%02X:%02X",
             addr->addr[0], addr->addr[1], addr->addr[2],
             addr->addr[3], addr->addr[4], addr->addr[5]);
    buf[bufsize - 1] = '\0';
}

air_key_t air_key_generate(void)
{
    return rand_hal_u32();
}

void air_bind_packet_prepare(air_bind_packet_t *packet)
{
    packet->version = AIR_PROTOCOL_VERSION;
    packet->info.max_tx_power = 20;
    packet->info.capabilities = 0;
#if defined(AIR_LORA_BAND_433)
    packet->info.capabilities |= AIR_CAP_FREQUENCY_433MHZ;
#endif
#if defined(AIR_LORA_BAND_868)
    packet->info.capabilities |= AIR_CAP_FREQUENCY_868MHZ;
#endif
#if defined(AIR_LORA_BAND_915)
    packet->info.capabilities |= AIR_CAP_FREQUENCY_915MHZ;
#endif
    packet->info.capabilities |= AIR_CAP_P2P_2_4GHZ_WIFI;
    if (system_has_flag(SYSTEM_FLAG_BUTTON))
    {
        packet->info.capabilities |= AIR_CAP_BUTTON;
    }
    if (system_has_flag(SYSTEM_FLAG_SCREEN))
    {
        packet->info.capabilities |= AIR_CAP_SCREEN;
    }
    if (system_has_flag(SYSTEM_FLAG_BATTERY))
    {
        packet->info.capabilities |= AIR_CAP_BATTERY;
    }
    // No antenna nor true diversity supported yet
    packet->info.channels = RC_CHANNELS_NUM;
    memcpy(packet->prefix, RAVEN_EXPLICIT_PKT_MARKER, RAVEN_EXPLICIT_PKT_MARKER_LEN);
    memset(packet->reserved, 0, sizeof(packet->reserved));
    packet->crc = crc8_dvb_s2_bytes(&packet->version, sizeof(*packet) - offsetof(air_bind_packet_t, version) - 1);
}

bool air_bind_packet_validate(air_bind_packet_t *packet)
{
    if (memcmp(packet->prefix, RAVEN_EXPLICIT_PKT_MARKER, RAVEN_EXPLICIT_PKT_MARKER_LEN) != 0)
    {
        return false;
    }
    uint8_t crc = crc8_dvb_s2_bytes(&packet->version, sizeof(*packet) - offsetof(air_bind_packet_t, version) - 1);
    return crc == packet->crc;
}

void air_bind_packet_cpy(air_bind_packet_t *dst, const air_bind_packet_t *src)
{
    memmove(dst, src, sizeof(air_bind_packet_t));
}

void air_bind_packet_get_pairing(const air_bind_packet_t *packet, air_pairing_t *pairing)
{
    pairing->addr = packet->addr;
    pairing->key = packet->key;
}

static uint8_t air_packet_crc(const void *packet, size_t size, air_key_t key)
{
    uint8_t crc = crc8_dvb_s2_bytes(&key, sizeof(key));
    return crc8_dvb_s2_bytes_from(crc, packet, size - 1);
}

void air_tx_packet_prepare(air_tx_packet_t *packet, air_key_t key)
{
    packet->crc = air_packet_crc(packet, sizeof(*packet), key);
}

bool air_tx_packet_validate(air_tx_packet_t *packet, air_key_t key)
{
    return packet->crc == air_packet_crc(packet, sizeof(*packet), key);
}

void air_rx_packet_prepare(air_rx_packet_t *packet, air_key_t key)
{
    packet->crc = air_packet_crc(packet, sizeof(*packet), key);
}

bool air_rx_packet_validate(air_rx_packet_t *packet, air_key_t key)
{
    return packet->crc == air_packet_crc(packet, sizeof(*packet), key);
}

uint8_t air_sync_word(air_key_t key)
{
    return crc8_dvb_s2_bytes(&key, sizeof(key));
}