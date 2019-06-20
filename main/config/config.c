#include <string.h>

#include "platform.h"

#include <hal/log.h>
#include <hal/rand.h>

#include "config/settings.h"

#include "io/storage.h"

#include "util/macros.h"

#include "config.h"

enum
{
    CONFIG_ADDR_KEY = 1,
    CONFIG_PAIRED_TX_KEY = 2,
    CONFIG_PAIRED_RX_KEY_PREFIX = 3,
    CONFIG_RX_SEQ_KEY = 4,
    CONFIG_AIR_INFO_KEY_PREFIX = 5,
    CONFIG_FS_CHANS_KEY = 6,
};

#define CONFIG_RX_SEQ_MIN 1 // We never assign the zero to check for valid ones
#define CONFIG_RX_SEQ_MAX 0xFF
#define CONFIG_KEY_BUFSIZE (1 + sizeof(air_addr_t)) // big enough for CONFIG_PAIRED_RX_KEY_PREFIX and CONFIG_AIR_INFO_KEY_PREFIX

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

typedef struct common_config_s
{
    air_addr_t addr;
#if defined(USE_TX_SUPPORT) && defined(USE_RX_SUPPORT)
    const setting_t *rc_mode;
#endif
} common_config_t;

typedef struct tx_config_s
{
    config_paired_rx_t paired_rxs[CONFIG_MAX_PAIRED_RX]; // Pairings with an RX, used in TX mode
    // To keep track of the last seen RX, we use a rolling sequence number to avoid having
    // to rewrite all entries every time we switch RX. This way, we only do a full
    // renumbering once the seq overflows.
    uint8_t rx_seq;
} tx_config_t;

typedef struct rx_config_s
{
    air_pairing_t paired_tx; // Pairing with a TX, used in RX mode
} rx_config_t;

static common_config_t config;

#if defined(USE_TX_SUPPORT)
static tx_config_t tx_config;
#endif

#if defined(USE_RX_SUPPORT)
static rx_config_t rx_config;
#endif

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

#if defined(USE_TX_SUPPORT)

static int config_format_paired_rx_key(uint8_t *buf, int num)
{
    buf[0] = CONFIG_PAIRED_RX_KEY_PREFIX;
    buf[1] = num & 0xFF;
    return 2;
}

static bool config_paired_rx_is_valid(const config_paired_rx_t *rx)
{
    return rx->seq >= CONFIG_RX_SEQ_MIN;
}

#endif

static int config_format_air_info_key(uint8_t *buf, const air_addr_t *addr)
{
    buf[0] = CONFIG_AIR_INFO_KEY_PREFIX;
    memcpy(&buf[1], addr, sizeof(*addr));
    return sizeof(*addr) + 1;
}

static bool config_get_air_info_blob(config_air_info_blob_t *blob, uint8_t *buf, int *ks, const air_addr_t *addr)
{
    *ks = config_format_air_info_key(buf, addr);
    bool found = storage_get_sized_blob(&storage, buf, *ks, blob, sizeof(*blob));
    if (!found)
    {
        memset(blob, 0, sizeof(*blob));
    }
    return found;
}

#if defined(USE_TX_SUPPORT)
static void config_init_tx(void)
{
    uint8_t rx_key[CONFIG_KEY_BUFSIZE];
    uint8_t ckey;
    int ks;

    memset(&tx_config.paired_rxs, 0, sizeof(tx_config.paired_rxs));
    // Load each RX separately, to avoid losing all paired models if
    // we change CONFIG_MAX_PAIRED_RX.
    for (int ii = 0; ii < CONFIG_MAX_PAIRED_RX; ii++)
    {
        ks = config_format_paired_rx_key(rx_key, ii);
        storage_get_sized_blob(&storage, rx_key, ks, &tx_config.paired_rxs[ii], sizeof(tx_config.paired_rxs[ii]));
    }

    tx_config.rx_seq = 0;
    ckey = CONFIG_RX_SEQ_KEY;
    storage_get_u8(&storage, &ckey, sizeof(ckey), &tx_config.rx_seq);
}
#endif

#if defined(USE_RX_SUPPORT)
static void config_init_rx(void)
{
    uint8_t ckey;
    memset(&rx_config.paired_tx, 0, sizeof(rx_config.paired_tx));
    ckey = CONFIG_PAIRED_TX_KEY;
    storage_get_sized_blob(&storage, &ckey, sizeof(ckey), &rx_config.paired_tx, sizeof(rx_config.paired_tx));
}
#endif

void config_init(void)
{
    uint8_t ckey;

    settings_init();
    storage_init(&storage, STORAGE_NS_CONFIG);

    bool commit = false;
    ckey = CONFIG_ADDR_KEY;
    if (!storage_get_sized_blob(&storage, &ckey, sizeof(ckey), &config.addr, sizeof(config.addr)))
    {
        config_generate_addr(&config.addr);
        storage_set_blob(&storage, &ckey, sizeof(ckey), &config.addr, sizeof(config.addr));
        commit = true;
    }

    char buf[AIR_ADDR_STRING_BUFFER_SIZE];
    air_addr_format(&config.addr, buf, sizeof(buf));
    LOG_I(TAG, "Module addr is %s", buf);

#if defined(USE_TX_SUPPORT)
    config_init_tx();
#endif
#if defined(USE_RX_SUPPORT)
    config_init_rx();
#endif

    if (commit)
    {
        storage_commit(&storage);
    }

#if defined(USE_TX_SUPPORT) && defined(USE_RX_SUPPORT)
    config.rc_mode = settings_get_key("rc_mode");
#endif
}

#if defined(USE_TX_SUPPORT) && defined(USE_RX_SUPPORT)
rc_mode_e config_get_rc_mode(void)
{
    return setting_get_u8(config.rc_mode);
}
#endif

bool config_get_paired_rx(air_pairing_t *pairing, const air_addr_t *addr)
{
#if defined(USE_TX_SUPPORT)
    uint8_t max_seq = 0;
    int idx = -1;
    for (size_t ii = 0; ii < ARRAY_COUNT(tx_config.paired_rxs); ii++)
    {
        config_paired_rx_t *rx = &tx_config.paired_rxs[ii];
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
            air_pairing_cpy(pairing, &tx_config.paired_rxs[idx].pairing);
        }
        return true;
    }
#else
    UNUSED(pairing);
    UNUSED(addr);
#endif
    return false;
}

void config_add_paired_rx(const air_pairing_t *pairing)
{
#if defined(USE_TX_SUPPORT)
    uint8_t rx_key[CONFIG_KEY_BUFSIZE];
    config_paired_rx_t *dest = NULL;
    uint8_t ckey;
    int ks;

    // Check if we already have a pairing for this addr. In that case,
    // just increase its seq number.
    for (int ii = 0; ii < ARRAY_COUNT(tx_config.paired_rxs); ii++)
    {
        config_paired_rx_t *p = &tx_config.paired_rxs[ii];
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
        for (int ii = 0; ii < ARRAY_COUNT(tx_config.paired_rxs); ii++)
        {
            if (!config_paired_rx_is_valid(&tx_config.paired_rxs[ii]))
            {
                dest = &tx_config.paired_rxs[ii];
                break;
            }
            // Keep track of the last recently used one
            if (min_seq_idx == -1 || tx_config.paired_rxs[ii].seq < min_seq)
            {
                min_seq_idx = ii;
                min_seq = tx_config.paired_rxs[ii].seq;
            }
        }
        if (!dest)
        {
            // Must delete the oldest one
            config_remove_paired_rx_at(min_seq_idx);
            dest = &tx_config.paired_rxs[min_seq_idx];
        }
    }
    air_pairing_cpy(&dest->pairing, pairing);
    if (tx_config.rx_seq == CONFIG_RX_SEQ_MAX)
    {
        // Time to renumber all known RXs
        tx_config.rx_seq = 0;
        for (int ii = CONFIG_RX_SEQ_MIN; ii < CONFIG_RX_SEQ_MAX; ii++)
        {
            for (int jj = 0; jj < ARRAY_COUNT(tx_config.paired_rxs); jj++)
            {
                if (!config_paired_rx_is_valid(&tx_config.paired_rxs[jj]))
                {
                    continue;
                }
                if (tx_config.paired_rxs[jj].seq == ii)
                {
                    tx_config.paired_rxs[jj].seq = ++tx_config.rx_seq;
                    ks = config_format_paired_rx_key(rx_key, jj);
                    storage_set_blob(&storage, rx_key, ks, &tx_config.paired_rxs[jj], sizeof(tx_config.paired_rxs[jj]));
                    break;
                }
            }
        }
    }
    dest->seq = ++tx_config.rx_seq;
    ks = config_format_paired_rx_key(rx_key, dest - tx_config.paired_rxs);
    storage_set_blob(&storage, rx_key, ks, dest, sizeof(*dest));
    ckey = CONFIG_RX_SEQ_KEY;
    storage_set_u8(&storage, &ckey, sizeof(ckey), tx_config.rx_seq);
    storage_commit(&storage);
#else
    UNUSED(pairing);
#endif
}

bool config_get_paired_rx_at(air_pairing_t *pairing, int idx)
{
#if defined(USE_TX_SUPPORT)
    if (idx >= 0 && idx < CONFIG_MAX_PAIRED_RX)
    {
        config_paired_rx_t *p = &tx_config.paired_rxs[idx];
        if (config_paired_rx_is_valid(p))
        {
            if (pairing)
            {
                air_pairing_cpy(pairing, &p->pairing);
            }
            return true;
        }
    }
#else
    UNUSED(pairing);
    UNUSED(idx);
#endif
    return false;
}

bool config_remove_paired_rx_at(int idx)
{
#if defined(USE_TX_SUPPORT)
    uint8_t key[CONFIG_KEY_BUFSIZE];
    int ks;

    if (idx >= 0 && idx < CONFIG_MAX_PAIRED_RX)
    {
        // Delete info
        ks = config_format_air_info_key(key, &tx_config.paired_rxs[idx].pairing.addr);
        storage_set_blob(&storage, key, ks, NULL, 0);

        // Delete pairing
        memset(&tx_config.paired_rxs[idx], 0, sizeof(tx_config.paired_rxs[idx]));
        ks = config_format_paired_rx_key(key, idx);
        storage_set_blob(&storage, key, ks, NULL, 0);

        storage_commit(&storage);
    }
#else
    UNUSED(idx);
#endif
    return false;
}

bool config_get_air_name(char *buf, size_t size, const air_addr_t *addr)
{
    uint8_t key[CONFIG_KEY_BUFSIZE];
    config_air_info_blob_t info;
    int ks;

    if (config_get_air_info_blob(&info, key, &ks, addr))
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
    uint8_t key[CONFIG_KEY_BUFSIZE];
    int ks;
    config_air_info_blob_t blob;

    config_get_air_info_blob(&blob, key, &ks, addr);
    memset(blob.name, 0, sizeof(blob.name));
    strlcpy(blob.name, name, sizeof(blob.name));
    storage_set_blob(&storage, key, ks, &blob, sizeof(blob));
    storage_commit(&storage);
    return true;
}

bool config_get_air_info(air_info_t *info, air_band_e *band, const air_addr_t *addr)
{
    uint8_t key[CONFIG_KEY_BUFSIZE];
    int ks;
    config_air_info_blob_t blob;

    if (config_get_air_info_blob(&blob, key, &ks, addr))
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
    uint8_t key[CONFIG_KEY_BUFSIZE];
    int ks;
    config_air_info_blob_t blob;

    config_get_air_info_blob(&blob, key, &ks, addr);
    bool changed = memcmp(&blob.info, info, sizeof(*info));
    memcpy(&blob.info, info, sizeof(*info));
    changed |= blob.band != band;
    blob.band = band;
    if (changed)
    {
        storage_set_blob(&storage, key, ks, &blob, sizeof(blob));
        storage_commit(&storage);
    }
    return changed;
}

bool config_get_paired_tx(air_pairing_t *pairing)
{
#if defined(USE_RX_SUPPORT)
    if (air_addr_is_valid(&rx_config.paired_tx.addr))
    {
        if (pairing)
        {
            air_pairing_cpy(pairing, &rx_config.paired_tx);
        }
        return true;
    }
#endif
    return false;
}

void config_set_paired_tx(const air_pairing_t *pairing)
{
#if defined(USE_RX_SUPPORT)
    uint8_t ckey;
    if (pairing)
    {
        air_pairing_cpy(&rx_config.paired_tx, pairing);
    }
    else
    {
        memset(&rx_config.paired_tx, 0, sizeof(rx_config.paired_tx));
    }
    ckey = CONFIG_PAIRED_TX_KEY;
    storage_set_blob(&storage, &ckey, sizeof(ckey), &rx_config.paired_tx, sizeof(rx_config.paired_tx));
    storage_commit(&storage);
#else
    UNUSED(pairing);
#endif
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

#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
rx_fs_mode_e config_get_fs_mode(void)
{
    return setting_get_u8(settings_get_key(SETTING_KEY_RX_FS_MODE));
}
#endif

#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
bool config_get_fs_channels(uint16_t *blob, size_t size)
{
    bool found = storage_get_sized_blob(&storage, CONFIG_FS_CHANS_KEY, blob, size);
    if (!found)
    {
        memset(blob, 0, size);
    }
    return found;
}

bool config_set_fs_channels(const rc_data_t *rc_data)
{
    uint16_t channels[RC_CHANNELS_NUM];
    for (int ii = 0; ii < RC_CHANNELS_NUM; ii++)
    {
        channels[ii] = rc_data_get_channel_value(rc_data, ii);
    }

    storage_set_blob(&storage, CONFIG_FS_CHANS_KEY, channels, sizeof(channels));
    storage_commit(&storage);
    return true;
}
#endif

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
