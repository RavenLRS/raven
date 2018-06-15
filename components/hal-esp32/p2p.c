#include <string.h>

#include <esp_event_loop.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <tcpip_adapter.h>

#include <hal/log.h>

#include <hal/p2p.h>

#define P2P_WIFI_CHANNEL 14

#define RAW_WIFI_DATA_SIZE 32
#define RAW_WIFI_CHECKSUM_SIZE 4 // Received after the actual payload

#define HW_ADDR_LENGTH 6

typedef struct
{
    unsigned frame_ctrl : 16;
    unsigned duration_id : 16;
    uint8_t addr1[6]; /* receiver address */
    uint8_t addr2[6]; /* sender address */
    uint8_t addr3[6]; /* filtering address */
    unsigned sequence_ctrl : 16;
    uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct
{
    wifi_ieee80211_mac_hdr_t hdr;
    uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

extern esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);

// Unfortunately esp_wifi_set_promiscuous_rx_cb only accepts a function, no additional data pointer
static p2p_hal_t *active_hal = NULL;

static void promiscuous_rx_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type)
{

    //if (type != WIFI_PKT_MGMT)
    //    return;

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;

#if 0
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
    printf("PACKET TYPE=%s, CHAN=%02d, RSSI=%02d,"
           " ADDR1=%02x:%02x:%02x:%02x:%02x:%02x,"
           " ADDR2=%02x:%02x:%02x:%02x:%02x:%02x,"
           " ADDR3=%02x:%02x:%02x:%02x:%02x:%02x,"
           " SIG_LEN=%u, PAYLOAD[0]=%u, PAYLOAD[1]=%u\n",
           wifi_sniffer_packet_type2str(type),
           ppkt->rx_ctrl.channel,
           ppkt->rx_ctrl.rssi,
           /* ADDR1 */
           hdr->addr1[0], hdr->addr1[1], hdr->addr1[2],
           hdr->addr1[3], hdr->addr1[4], hdr->addr1[5],
           /* ADDR2 */
           hdr->addr2[0], hdr->addr2[1], hdr->addr2[2],
           hdr->addr2[3], hdr->addr2[4], hdr->addr2[5],
           /* ADDR3 */
           hdr->addr3[0], hdr->addr3[1], hdr->addr3[2],
           hdr->addr3[3], hdr->addr3[4], hdr->addr3[5],
           ppkt->rx_ctrl.sig_len, ppkt->payload[0], ppkt->payload[33]);
#endif

    if (ppkt->rx_ctrl.sig_len <= RAW_WIFI_DATA_SIZE + RAW_WIFI_CHECKSUM_SIZE)
    {
        return;
    }
    size_t payload_size = ppkt->rx_ctrl.sig_len - RAW_WIFI_DATA_SIZE - RAW_WIFI_CHECKSUM_SIZE;
    const void *payload = ppkt->payload + RAW_WIFI_DATA_SIZE;
    if (active_hal && active_hal->callback)
    {
        active_hal->callback(active_hal, payload, payload_size, active_hal->user_data);
    }
}

void p2p_hal_init(p2p_hal_t *hal, p2p_hal_callback_f callback, void *user_data)
{
    assert(active_hal == NULL);

    hal->callback = callback;
    hal->user_data = user_data;
    active_hal = hal;
    ESP_ERROR_CHECK(esp_event_loop_init(NULL, NULL));
    tcpip_adapter_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // Disable WPA support, saves ~5K of flash, since LTO
    // is able to strip the related functions.
    memset(&cfg.wpa_crypto_funcs, 0, sizeof(cfg.wpa_crypto_funcs));
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_country_t cc = {
        .cc = "XXX",
        .schan = 1,
        .nchan = P2P_WIFI_CHANNEL,
        .policy = WIFI_COUNTRY_POLICY_MANUAL,
    };
    ESP_ERROR_CHECK(esp_wifi_set_country(&cc));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR));
}

void p2p_hal_start(p2p_hal_t *hal)
{
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(promiscuous_rx_packet_handler));
    ESP_ERROR_CHECK(esp_wifi_set_channel(P2P_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

void p2p_hal_stop(p2p_hal_t *hal)
{
    ESP_ERROR_CHECK(esp_wifi_stop());
}

void p2p_hal_broadcast(p2p_hal_t *p2p_hal, const void *data, size_t size)
{
    uint8_t buf[512 + RAW_WIFI_DATA_SIZE];
    // 0-1: Frame control
    // the API won't let us send valid packets, so we send a DATA
    // type packet in the reserved range bits
    buf[0] = 0x58;
    buf[1] = 0x00;
    // 2-3: Duration
    buf[2] = buf[3] = 0x00;
    // 4-9: Destination addr (broadcast = all 0xff)
    memset(&buf[4], 0xff, HW_ADDR_LENGTH);
    // 10-15: Source address
    memset(&buf[10], 0xff, HW_ADDR_LENGTH);
    // 16-21: BSSID
    memset(&buf[16], 0xff, HW_ADDR_LENGTH);
    // 22-23: Sequence / fragment number
    buf[22] = buf[23] = 0x00;
    // 24-31: Timestamp (GETS OVERWRITTEN TO 0 BY HARDWARE)

    // 32-end: our payload
    // The actual payload
    memcpy(&buf[RAW_WIFI_DATA_SIZE], data, size);
    ESP_ERROR_CHECK(esp_wifi_80211_tx(WIFI_IF_AP, buf, RAW_WIFI_DATA_SIZE + size, true));
}