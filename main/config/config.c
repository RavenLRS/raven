#include <stdio.h>
#include <string.h>

#include <hal/log.h>
#include <hal/rand.h>

#include "config/settings.h"

#include "platform/storage.h"

#include "util/macros.h"

#include "config.h"

#define CONFIG_STORAGE_NAME "config"
#define CONFIG_ADDR_KEY "addr"
#define CONFIG_PAIRED_TX_KEY "paired_tx"
#define CONFIG_PAIRED_RX_KEY_PREFIX "prx"
#define CONFIG_RX_SEQ_KEY "rx_seq"
#define CONFIG_RX_SEQ_MIN 1 // We never assign the zero to check for valid ones
#define CONFIG_RX_SEQ_MAX 0xFF

#define CONFIG_AIR_INFO_KEY_PREFIX "a:"

#define CONFIG_KEY_BUFSIZE 16 // big enough for CONFIG_PAIRED_RX_KEY_PREFIX and CONFIG_AIR_INFO_KEY_PREFIX

static const char *TAG = "config";

typedef struct config_paired_rx_s
{
    air_pairing_t pairing;
    uint8_t seq;
} PACKED config_paired_rx_t;

typedef struct config_air_info_blob_s
{
    char name[AIR_MAX_NAME_LENGTH + 1];
    air_band_e band; // Last band seen in this node
    air_info_t info;
} PACKED config_air_info_blob_t;

typedef struct config_s
{
    air_addr_t addr;
    air_pairing_t paired_tx;                             // Pairing with a TX, used in RX mode
    config_paired_rx_t paired_rxs[CONFIG_MAX_PAIRED_RX]; // Pairings with an RX, used in TX mode
    // To keep track of the last seen RX, we use a rolling sequence number to avoid having
    // to rewrite all entries every time we switch RX. This way, we only do a full
    // renumbering once the seq overflows.
    uint8_t rx_seq;
    const setting_t *rc_mode;
} config_t;

static config_t config;
static storage_t storage;

static void config_generate_addr(air_addr_t *addr)
{
    // Generate an addr.
    uint32_t val = hal_rand_u32();
    addr->addr[0] = (val >> 0) & 0xFF;
    addr->addr[1] = (val >> 8) & 0xFF;
    addr->addr[2] = (val >> 16) & 0xFF;
    addr->addr[3] = (val >> 24) & 0xFF;
    val = hal_rand_u32();
    addr->addr[4] = (val >> 0) & 0xFF;
    addr->addr[5] = (val >> 8) & 0xFF;
}

static void config_format_paired_rx_key(char *buf, size_t size, int num)
{
    snprintf(buf, size, "%s%d", CONFIG_PAIRED_RX_KEY_PREFIX, num);
}

static void config_format_air_info_key(char *buf, size_t size, const air_addr_t *addr)
{
    char addr_buf[AIR_ADDR_STRING_BUFFER_SIZE];
    air_addr_format(addr, addr_buf, sizeof(addr_buf));
    strlcpy(buf, CONFIG_AIR_INFO_KEY_PREFIX, size);
    strlcat(buf, addr_buf, size);
}

static bool config_paired_rx_is_valid(const config_paired_rx_t *rx)
{
    return rx->seq >= CONFIG_RX_SEQ_MIN;
}

static bool config_get_air_info_blob(config_air_info_blob_t *blob, char *buf, size_t size, const air_addr_t *addr)
{
    config_format_air_info_key(buf, size, addr);
    bool found = storage_get_sized_blob(&storage, buf, blob, sizeof(*blob));
    if (!found)
    {
        memset(blob, 0, sizeof(*blob));
    }
    return found;
}

void config_init(void)
{
    char rx_key[CONFIG_KEY_BUFSIZE];

    settings_init();
    storage_init(&storage, CONFIG_STORAGE_NAME);

    bool commit = false;
    if (!storage_get_sized_blob(&storage, CONFIG_ADDR_KEY, &config.addr, sizeof(config.addr)))
    {
        config_generate_addr(&config.addr);
        storage_set_blob(&storage, CONFIG_ADDR_KEY, &config.addr, sizeof(config.addr));
        commit = true;
    }

    memset(&config.paired_tx, 0, sizeof(config.paired_tx));
    storage_get_sized_blob(&storage, CONFIG_PAIRED_TX_KEY, &config.paired_tx, sizeof(config.paired_tx));

    memset(&config.paired_rxs, 0, sizeof(config.paired_rxs));
    // Load each RX separately, to avoid losing all paired models if
    // we change CONFIG_MAX_PAIRED_RX.
    for (int ii = 0; ii < CONFIG_MAX_PAIRED_RX; ii++)
    {
        config_format_paired_rx_key(rx_key, sizeof(rx_key), ii);
        storage_get_sized_blob(&storage, rx_key, &config.paired_rxs[ii], sizeof(config.paired_rxs[ii]));
    }

    config.rx_seq = 0;
    storage_get_u8(&storage, CONFIG_RX_SEQ_KEY, &config.rx_seq);

    config.rc_mode = settings_get_key("rc_mode");

    char buf[AIR_ADDR_STRING_BUFFER_SIZE];
    air_addr_format(&config.addr, buf, sizeof(buf));
    LOG_I(TAG, "Module addr is %s", buf);

    if (commit)
    {
        storage_commit(&storage);
    }
}

#if defined(USE_TX_SUPPORT) && defined(USE_RX_SUPPORT)
rc_mode_e config_get_rc_mode(void)
{
    return setting_get_u8(config.rc_mode);
}
#endif

bool config_get_paired_rx(air_pairing_t *pairing, const air_addr_t *addr)
{
    uint8_t max_seq = 0;
    int idx = -1;
    for (int ii = 0; ii < ARRAY_COUNT(config.paired_rxs); ii++)
    {
        config_paired_rx_t *rx = &config.paired_rxs[ii];
        if (!config_paired_rx_is_valid(rx))
        {
            continue;
        }
        if (addr != NULL)
        {
            if (air_addr_equals(&rx->pairing.addr, addr))
            {
                // Found it
                if (pairing)
                {
                    air_pairing_cpy(pairing, &rx->pairing);
                }
                return true;
            }
        }
        else
        {
            if (idx == -1 || rx->seq > max_seq)
            {
                idx = ii;
                max_seq = rx->seq;
            }
        }
    }
    if (addr == NULL && idx >= 0)
    {
        // Return the last active one
        if (pairing)
        {
            air_pairing_cpy(pairing, &config.paired_rxs[idx].pairing);
        }
        return true;
    }
    return false;
}

void config_add_paired_rx(const air_pairing_t *pairing)
{
    char rx_key[CONFIG_KEY_BUFSIZE];
    config_paired_rx_t *dest = NULL;

    // Check if we already have a pairing for this addr. In that case,
    // just increase its seq number.
    for (int ii = 0; ii < ARRAY_COUNT(config.paired_rxs); ii++)
    {
        config_paired_rx_t *p = &config.paired_rxs[ii];
        if (air_addr_equals(&p->pairing.addr, &pairing->addr))
        {
            // Found it
            dest = p;
            break;
        }
    }
    if (!dest)
    {
        // Adding a new RX. Look for a free slot or delete the last
        // seen one.
        uint8_t min_seq = CONFIG_RX_SEQ_MAX;
        int min_seq_idx = -1;
        for (int ii = 0; ii < ARRAY_COUNT(config.paired_rxs); ii++)
        {
            if (!config_paired_rx_is_valid(&config.paired_rxs[ii]))
            {
                dest = &config.paired_rxs[ii];
                break;
            }
            // Keep track of the last recently used one
            if (min_seq_idx == -1 || config.paired_rxs[ii].seq < min_seq)
            {
                min_seq_idx = ii;
                min_seq = config.paired_rxs[ii].seq;
            }
        }
        if (!dest)
        {
            // Must delete the oldest one
            config_remove_paired_rx_at(min_seq_idx);
            dest = &config.paired_rxs[min_seq_idx];
        }
    }
    air_pairing_cpy(&dest->pairing, pairing);
    if (config.rx_seq == CONFIG_RX_SEQ_MAX)
    {
        // Time to renumber all known RXs
        config.rx_seq = 0;
        for (int ii = CONFIG_RX_SEQ_MIN; ii < CONFIG_RX_SEQ_MAX; ii++)
        {
            for (int jj = 0; jj < ARRAY_COUNT(config.paired_rxs); jj++)
            {
                if (!config_paired_rx_is_valid(&config.paired_rxs[jj]))
                {
                    continue;
                }
                if (config.paired_rxs[jj].seq == ii)
                {
                    config.paired_rxs[jj].seq = ++config.rx_seq;
                    config_format_paired_rx_key(rx_key, sizeof(rx_key), jj);
                    storage_set_blob(&storage, rx_key, &config.paired_rxs[jj], sizeof(config.paired_rxs[jj]));
                    break;
                }
            }
        }
    }
    dest->seq = ++config.rx_seq;
    config_format_paired_rx_key(rx_key, sizeof(rx_key), dest - config.paired_rxs);
    storage_set_blob(&storage, rx_key, dest, sizeof(*dest));
    storage_set_u8(&storage, CONFIG_RX_SEQ_KEY, config.rx_seq);
    storage_commit(&storage);
}

bool config_get_paired_rx_at(air_pairing_t *pairing, int idx)
{
    if (idx >= 0 && idx < CONFIG_MAX_PAIRED_RX)
    {
        config_paired_rx_t *p = &config.paired_rxs[idx];
        if (config_paired_rx_is_valid(p))
        {
            if (pairing)
            {
                air_pairing_cpy(pairing, &p->pairing);
            }
            return true;
        }
    }
    return false;
}

bool config_remove_paired_rx_at(int idx)
{
    char key[CONFIG_KEY_BUFSIZE];

    if (idx >= 0 && idx < CONFIG_MAX_PAIRED_RX)
    {
        // Delete info
        config_format_air_info_key(key, sizeof(key), &config.paired_rxs[idx].pairing.addr);
        storage_set_blob(&storage, key, NULL, 0);

        // Delete pairing
        memset(&config.paired_rxs[idx], 0, sizeof(config.paired_rxs[idx]));
        config_format_paired_rx_key(key, sizeof(key), idx);
        storage_set_blob(&storage, key, NULL, 0);

        storage_commit(&storage);
    }
    return false;
}

bool config_get_air_name(char *buf, size_t size, const air_addr_t *addr)
{
    char key[CONFIG_KEY_BUFSIZE];
    config_air_info_blob_t info;

    if (config_get_air_info_blob(&info, key, sizeof(key), addr))
    {
        if (strlen(info.name) > 0)
        {
            if (buf)
            {
                strlcpy(buf, info.name, size);
            }
            return true;
        }
    }
    return false;
}

bool config_set_air_name(const air_addr_t *addr, const char *name)
{
    char key[CONFIG_KEY_BUFSIZE];
    config_air_info_blob_t blob;

    config_get_air_info_blob(&blob, key, sizeof(key), addr);
    memset(blob.name, 0, sizeof(blob.name));
    strlcpy(blob.name, name, sizeof(blob.name));
    storage_set_blob(&storage, key, &blob, sizeof(blob));
    storage_commit(&storage);
    return true;
}

bool config_get_air_info(air_info_t *info, air_band_e *band, const air_addr_t *addr)
{
    char key[CONFIG_KEY_BUFSIZE];
    config_air_info_blob_t blob;

    if (config_get_air_info_blob(&blob, key, sizeof(key), addr))
    {
        if (info)
        {
            memcpy(info, &blob.info, sizeof(*info));
        }
        if (band)
        {
            *band = blob.band;
        }
        return true;
    }
    return false;
}

bool config_set_air_info(const air_addr_t *addr, const air_info_t *info, air_band_e band)
{
    char key[CONFIG_KEY_BUFSIZE];
    config_air_info_blob_t blob;

    config_get_air_info_blob(&blob, key, sizeof(key), addr);
    bool changed = memcmp(&blob.info, info, sizeof(*info));
    memcpy(&blob.info, info, sizeof(*info));
    changed |= blob.band != band;
    blob.band = band;
    if (changed)
    {
        storage_set_blob(&storage, key, &blob, sizeof(blob));
        storage_commit(&storage);
    }
    return changed;
}

bool config_get_paired_tx(air_pairing_t *pairing)
{
    if (air_addr_is_valid(&config.paired_tx.addr))
    {
        if (pairing)
        {
            air_pairing_cpy(pairing, &config.paired_tx);
        }
        return true;
    }
    return false;
}

void config_set_paired_tx(const air_pairing_t *pairing)
{
    if (pairing)
    {
        air_pairing_cpy(&config.paired_tx, pairing);
    }
    else
    {
        memset(&config.paired_tx, 0, sizeof(config.paired_tx));
    }
    storage_set_blob(&storage, CONFIG_PAIRED_TX_KEY, &config.paired_tx, sizeof(config.paired_tx));
    storage_commit(&storage);
}

bool config_get_pairing(air_pairing_t *pairing, const air_addr_t *addr)
{
    switch (config_get_rc_mode())
    {
    case RC_MODE_TX:
        if (config_get_paired_rx(pairing, addr))
        {
            return true;
        }
        break;
    case RC_MODE_RX:
    {
        air_pairing_t tx_pairing;
        if (config_get_paired_tx(&tx_pairing) && air_addr_equals(&tx_pairing.addr, addr))
        {
            if (pairing)
            {
                air_pairing_cpy(pairing, &tx_pairing);
            }
            return true;
        }
        break;
    }
    }
    return false;
}

tx_input_type_e config_get_input_type(void)
{
    return setting_get_u8(settings_get_key(SETTING_KEY_TX_INPUT));
}

rx_output_type_e config_get_output_type(void)
{
    return setting_get_u8(settings_get_key(SETTING_KEY_RX_OUTPUT));
}

air_addr_t config_get_addr(void)
{
    return config.addr;
}

const char *config_get_name(void)
{
    switch (config_get_rc_mode())
    {
    case RC_MODE_TX:
        return settings_get_key_string(SETTING_KEY_TX_PILOT_NAME);
    case RC_MODE_RX:
        return settings_get_key_string(SETTING_KEY_RX_CRAFT_NAME);
    }
    UNREACHABLE();
    return NULL;
}

const char *config_get_mode_name(void)
{
    switch (config_get_rc_mode())
    {
    case RC_MODE_TX:
        return "TX";
    case RC_MODE_RX:
        return "RX";
    }
    UNREACHABLE();
    return NULL;
}

air_band_e config_get_air_band(config_air_band_e band)
{
    switch (band)
    {
#if defined(USE_AIR_BAND_147)
    case CONFIG_AIR_BAND_147:
        return AIR_BAND_147;
#endif
#if defined(USE_AIR_BAND_169)
    case CONFIG_AIR_BAND_169:
        return AIR_BAND_169;
#endif
#if defined(USE_AIR_BAND_315)
    case CONFIG_AIR_BAND_315:
        return AIR_BAND_315;
#endif
#if defined(USE_AIR_BAND_433)
    case CONFIG_AIR_BAND_433:
        return AIR_BAND_433;
#endif
#if defined(USE_AIR_BAND_470)
    case CONFIG_AIR_BAND_470:
        return AIR_BAND_470;
#endif
#if defined(USE_AIR_BAND_868)
    case CONFIG_AIR_BAND_868:
        return AIR_BAND_868;
#endif
#if defined(USE_AIR_BAND_915)
    case CONFIG_AIR_BAND_915:
        return AIR_BAND_915;
#endif
    case CONFIG_AIR_BAND_COUNT:
        break;
    }
    return 0;
}

air_band_mask_t config_get_air_band_mask(void)
{
    air_band_mask_t mask = 0;
    for (int ii = 0; ii < CONFIG_AIR_BAND_COUNT; ii++)
    {
        air_band_e band = config_get_air_band(ii);
        if (band != AIR_BAND_INVALID)
        {
            mask |= AIR_BAND_BIT(band);
        }
    }
    return mask;
}

bool config_supports_air_band(air_band_e band)
{
    return config_get_air_band_mask() & AIR_BAND_BIT(band);
}

air_supported_modes_e config_get_air_modes(config_air_mode_e modes)
{
    switch (modes)
    {
    case CONFIG_AIR_MODES_1_5:
        return AIR_SUPPORTED_MODES_1_TO_5;
    case CONFIG_AIR_MODES_2_5:
        return AIR_SUPPORTED_MODES_2_TO_5;
    case CONFIG_AIR_MODES_FIXED_1:
        return AIR_SUPPORTED_MODES_FIXED_1;
    case CONFIG_AIR_MODES_FIXED_2:
        return AIR_SUPPORTED_MODES_FIXED_2;
    case CONFIG_AIR_MODES_FIXED_3:
        return AIR_SUPPORTED_MODES_FIXED_3;
    case CONFIG_AIR_MODES_FIXED_4:
        return AIR_SUPPORTED_MODES_FIXED_4;
    case CONFIG_AIR_MODES_FIXED_5:
        return AIR_SUPPORTED_MODES_FIXED_5;
    case CONFIG_AIR_MODES_COUNT:
        break;
    }
    return 0;
}