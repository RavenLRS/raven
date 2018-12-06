#include <stdio.h>
#include <string.h>

#include <hal/rand.h>

#include "config/config.h"

#include "platform/system.h"

#include "rc/rc_data.h"

#include "util/crc.h"

#include "air.h"

static const air_addr_t air_addr_invalid = ((air_addr_t){.addr = {0, 0, 0, 0, 0, 0}});
const air_addr_t *AIR_ADDR_INVALID = &air_addr_invalid;
static const air_addr_t air_addr_broadcast = ((air_addr_t){.addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}});
const air_addr_t *AIR_ADDR_BROADCAST = &air_addr_broadcast;

typedef enum
{
    PACKET_TYPE_BIND_REQ,
    PACKET_TYPE_BIND_ACCEPT,
} packet_type_e;

#define RAVEN_BIND_PKT_VERSION 0

#define RAVEN_EXPLICIT_PKT_MARKER "RVN" // Used for packets with explicit header
#define RAVEN_EXPLICIT_PKT_MARKER_LEN 3

void air_pairing_cpy(air_pairing_t *dst, const air_pairing_t *src)
{
    memmove(dst, src, sizeof(*dst));
}

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

bool air_addr_equals(const air_addr_t *addr1, const air_addr_t *addr2)
{
    return memcmp(addr1->addr, addr2->addr, AIR_ADDR_LENGTH) == 0;
}

static bool air_addr_is_byte(const air_addr_t *addr, uint8_t b)
{
    for (int ii = 0; ii < AIR_ADDR_LENGTH; ii++)
    {
        if (addr->addr[ii] != b)
        {
            return false;
        }
    }
    return true;
}

// Returns true iff addr is not all zeros
bool air_addr_is_valid(const air_addr_t *addr)
{
    return !air_addr_is_byte(addr, 0);
}

bool air_addr_is_broadcast(const air_addr_t *addr)
{
    return air_addr_is_byte(addr, 0xFF);
}

void air_addr_cpy(air_addr_t *dst, const air_addr_t *src)
{
    memmove(dst, src, sizeof(*dst));
}

air_key_t air_key_generate(void)
{
    return hal_rand_u32();
}

void air_bind_packet_prepare(air_bind_packet_t *packet)
{
    packet->version = AIR_PROTOCOL_VERSION;
    packet->info.max_tx_power = 20;
    packet->info.capabilities = 0;
    if (config_supports_air_band(AIR_BAND_147))
    {
        packet->info.capabilities |= AIR_CAP_FREQUENCY_147MHZ;
    }
    if (config_supports_air_band(AIR_BAND_169))
    {
        packet->info.capabilities |= AIR_CAP_FREQUENCY_169MHZ;
    }
    if (config_supports_air_band(AIR_BAND_315))
    {
        packet->info.capabilities |= AIR_CAP_FREQUENCY_315MHZ;
    }
    if (config_supports_air_band(AIR_BAND_433))
    {
        packet->info.capabilities |= AIR_CAP_FREQUENCY_433MHZ;
    }
    if (config_supports_air_band(AIR_BAND_470))
    {
        packet->info.capabilities |= AIR_CAP_FREQUENCY_470MHZ;
    }
    if (config_supports_air_band(AIR_BAND_868))
    {
        packet->info.capabilities |= AIR_CAP_FREQUENCY_868MHZ;
    }
    if (config_supports_air_band(AIR_BAND_915))
    {
        packet->info.capabilities |= AIR_CAP_FREQUENCY_915MHZ;
    }
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