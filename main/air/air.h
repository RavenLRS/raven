#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "util/macros.h"

#define AIR_MAX_PACKET_SIZE 64
#define AIR_BIND_PACKET_INTERVAL_MS 500
#define AIR_BIND_PACKET_EXPIRATION_MS 2000
#define AIR_PROTOCOL_VERSION 1
#define AIR_MAX_NAME_LENGTH 32
#define AIR_ADDR_LENGTH 6
#define AIR_ADDR_STRING_BUFFER_SIZE ((AIR_ADDR_LENGTH * 2) + (AIR_ADDR_LENGTH - 1) + 1)

#define AIR_DATA_START_STOP 0x7E
#define AIR_DATA_BYTE_STUFF 0x7D
#define AIR_DATA_XOR 0x20

typedef enum {
    AIR_ROLE_TX = 1,
    AIR_ROLE_RX,
    AIR_ROLE_RX_AWAITING_CONFIRMATION, // Used by RX without screen to tell the TX it's ready to pair
} air_role_e;

typedef enum {
    AIR_CAP_FREQUENCY_433MHZ = 1 << 0,
    AIR_CAP_FREQUENCY_868MHZ = 1 << 1,
    AIR_CAP_FREQUENCY_915MHZ = 1 << 2,

    AIR_CAP_P2P_2_4GHZ = 1 << 9,       // 2.4ghz unrestricted
    AIR_CAP_P2P_2_4GHZ_WIFI = 1 << 10, // 2.4ghz but restricted to valid raw WiFi packets
    AIR_CAP_P2P_FLARM = 1 << 11,       // flarm support

    // Hardware
    AIR_CAP_BATTERY = 1 << 24,           // Node has an on-board battery
    AIR_CAP_SCREEN = 1 << 25,            // Node has a screen
    AIR_CAP_BUTTON = 1 << 26,            // Node has buttons (might be a single button)
    AIR_CAP_ANTENNA_DIVERSITY = 1 << 27, // Node has antenna diversity for air protocol
    AIR_CAP_TRUE_DIVERSITY = 1 << 28,    // Node has at least 2 TRX for the air protocol
} air_capability_e;

typedef struct air_addr_s
{
    uint8_t addr[AIR_ADDR_LENGTH];
} air_addr_t;

#define AIR_ADDR_INVALID ((air_addr_t){.addr = {0, 0, 0, 0, 0, 0}})
#define AIR_ADDR_BROADCAST ((air_addr_t){.addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}})

typedef uint32_t air_key_t;

typedef struct air_info_s
{
    uint32_t capabilities; // from air_capability_e
    uint8_t max_tx_power;  // Maximum power for the air protocol in db
    uint8_t channels;      // number of supported channels.
    uint8_t modes;         // supported modes from air_lora_mode_e - node n makes bit n=1
} PACKED air_info_t;

typedef struct air_bind_packet_s
{
    char prefix[3];
    uint8_t version;                    // Highest supported protocol version. TX should support all past versions.
    air_addr_t addr;                    // Node address
    air_key_t key;                      // Pairing key
    uint8_t role;                       // air_role_e
    air_info_t info;                    // See air_info_t above
    char name[AIR_MAX_NAME_LENGTH + 1]; // Node name. Zero padded.
    uint8_t reserved[8];                // Reserved, must be zero
    uint8_t crc;                        // DVB S2 CRC
} PACKED air_bind_packet_t;

#define AIR_BIND_PACKET_SIZE (sizeof(air_bind_packet_t))

_Static_assert(AIR_BIND_PACKET_SIZE == AIR_MAX_PACKET_SIZE, "bind_packet_t incorrect size");

typedef struct air_pairing_s
{
    air_addr_t addr;
    uint32_t key;
} PACKED air_pairing_t;

inline void air_pairing_cpy(air_pairing_t *dst, const air_pairing_t *src)
{
    memmove(dst, src, sizeof(*dst));
}

void air_pairing_format(const air_pairing_t *pairing, char *buf, size_t bufsize);

#define AIR_CHANNEL_BITS 9
#define AIR_SEQ_BITS 4
#define AIR_SEQ_COUNT (1 << AIR_SEQ_BITS)
#define AIR_NUM_HOPPING_FREQS AIR_SEQ_COUNT
#define AIR_UPLINK_DATA_BYTES 2
#define AIR_DOWNLINK_DATA_BYTES 3
#define AIR_MAX_DATA_BYTES (AIR_UPLINK_DATA_BYTES > AIR_DOWNLINK_DATA_BYTES ? AIR_UPLINK_DATA_BYTES : AIR_DOWNLINK_DATA_BYTES)
#define AIR_SEQ_TO_SEND(seq, count, per_packet) ((seq + (count + per_packet - 1) / per_packet) % AIR_SEQ_COUNT)
#define AIR_SEQ_TO_SEND_UPLINK(seq, count) AIR_SEQ_TO_SEND(seq, count, AIR_UPLINK_DATA_BYTES)
#define AIR_SEQ_TO_SEND_DOWNLINK(seq, count) AIR_SEQ_TO_SEND(seq, count, AIR_DOWNLINK_DATA_BYTES)

typedef struct tx_packet_s
{
    unsigned seq : AIR_SEQ_BITS; // 4 bits of sequence
                                 // + 36 bits for first 4 channels, 40 = 5 bytes
    unsigned ch0 : AIR_CHANNEL_BITS;
    unsigned ch1 : AIR_CHANNEL_BITS;
    unsigned ch2 : AIR_CHANNEL_BITS;
    unsigned ch3 : AIR_CHANNEL_BITS;

    uint8_t data[AIR_UPLINK_DATA_BYTES]; // Arbitrary data. Can be a channel or a chunk of telemetry data. Now at 7 bytes
    uint8_t crc;                         // With CRC we're at 8 bytes
} PACKED air_tx_packet_t;

_Static_assert(sizeof(air_tx_packet_t) == 8, "invalid air_tx_packet_t size");

typedef struct air_rx_packet_s
{
    unsigned seq : AIR_SEQ_BITS;           // Seq from the RX
    unsigned tx_seq : AIR_SEQ_BITS;        // Seq from the TX, just echoed so the TX can know if we received an specific packet
    uint8_t data[AIR_DOWNLINK_DATA_BYTES]; // Arbitrary data
    uint8_t crc;                           // With CRC we're at 5 bytes
} PACKED air_rx_packet_t;

_Static_assert(sizeof(air_rx_packet_t) == 5, "invalid air_rx_packet_t size");

void air_addr_format(const air_addr_t *addr, char *buf, size_t bufsize);

inline bool air_addr_equals(const air_addr_t *addr1, const air_addr_t *addr2)
{
    return memcmp(addr1->addr, addr2->addr, AIR_ADDR_LENGTH) == 0;
}

inline bool air_addr_is_byte(const air_addr_t *addr, uint8_t b)
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
inline bool air_addr_is_valid(const air_addr_t *addr)
{
    return !air_addr_is_byte(addr, 0);
}

inline bool air_addr_is_broadcast(const air_addr_t *addr)
{
    return air_addr_is_byte(addr, 0xFF);
}

inline void air_addr_cpy(air_addr_t *dst, const air_addr_t *src)
{
    memmove(dst, src, sizeof(*dst));
}

air_key_t air_key_generate(void);

void air_bind_packet_prepare(air_bind_packet_t *packet);
bool air_bind_packet_validate(air_bind_packet_t *packet);
void air_bind_packet_cpy(air_bind_packet_t *dst, const air_bind_packet_t *src);
void air_bind_packet_get_pairing(const air_bind_packet_t *packet, air_pairing_t *pairing);

void air_bind_req(uint64_t uuid, const char *name, uint8_t *buf, size_t *bufsize);
void air_bind_accept(uint64_t uuid, const char *name, uint8_t *buf, size_t *bufsize);
bool air_parse_bind_req(uint8_t *buf, size_t bufsize, uint64_t *uuid, char *name_buf, size_t *name_size);
bool air_parse_bind_accept(uint8_t *buf, size_t bufsize, uint64_t *uuid, char *name_buf, size_t *name_size);

void air_tx_packet_prepare(air_tx_packet_t *packet, air_key_t key);
bool air_tx_packet_validate(air_tx_packet_t *packet, air_key_t key);
void air_rx_packet_prepare(air_rx_packet_t *packet, air_key_t key);
bool air_rx_packet_validate(air_rx_packet_t *packet, air_key_t key);

uint8_t air_sync_word(air_key_t key);
