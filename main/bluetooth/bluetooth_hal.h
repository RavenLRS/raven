#pragma once

#include <esp_bt.h>
#include <esp_bt_defs.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gatts_api.h>

#define GATT_SERVER_PREPARE_WRITE_BUFSIZE 1024

struct gatt_server_s;
struct gatt_server_char_s;

typedef struct gatt_server_write_info_s
{
    uint16_t conn_id;
    uint32_t trans_id;
    esp_bd_addr_t bda;
    bool need_rsp;
    const uint8_t *data;
    unsigned data_len;
} gatt_server_write_info_t;

typedef esp_gatt_status_t (*gatt_char_read_f)(struct gatt_server_s *server, struct gatt_server_char_s *chr, struct gatts_read_evt_param *r, esp_gatt_rsp_t *rsp, void *user_data);
typedef esp_gatt_status_t (*gatt_char_write_f)(struct gatt_server_s *server, struct gatt_server_char_s *chr, gatt_server_write_info_t *write_info, void *user_data);

// According to https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.characteristic_presentation_format.xml
typedef enum
{
    BT_FORMAT_BOOL = 1,
    BT_FORMAT_UINT2 = 2,
    BT_FORMAT_UINT4 = 3,
    BT_FORMAT_UINT8 = 4,
    BT_FORMAT_UINT12 = 5,
    BT_FORMAT_UINT16 = 6,
    BT_FORMAT_UINT24 = 7,
    BT_FORMAT_UINT32 = 8,
    BT_FORMAT_UINT48 = 9,
    BT_FORMAT_UINT64 = 10,
    BT_FORMAT_UINT128 = 11,
    BT_FORMAT_INT8 = 12,
    BT_FORMAT_INT12 = 13,
    BT_FORMAT_INT16 = 14,
    BT_FORMAT_INT24 = 15,
    BT_FORMAT_INT32 = 16,
    BT_FORMAT_INT48 = 17,
    BT_FORMAT_INT64 = 18,
    BT_FORMAT_INT128 = 19,
    BT_FORMAT_FLOAT32 = 20,
    BT_FORMAT_FLOAT64 = 21,
    BT_FORMAT_IEEE_11073_FLOAT16 = 22,
    BT_FORMAT_IEEE_11063_FLOAT32 = 23,
    BT_FORMAT_IEEE_20601 = 25,
    BT_FORMAT_UTF8_STR = 25,
    BT_FORMAT_UTF16_STR = 26,
    BT_FORMAT_OPAQUE_STRUCT = 27,
} bt_format_e;

// https://www.bluetooth.com/specifications/assigned-numbers/units
typedef enum
{
    BT_UNIT_UNITLESS = 0x2700,
    BT_UNIT_METRE = 0x2701, // length (metre)
    BT_UNIT_KG = 0x2702,    // mass (kilogram)
} bt_unit_e;

typedef struct bt_presentation_format_s
{
    bt_format_e format;
    int8_t exponent;
    bt_unit_e unit;
    uint8_t ns;
    uint16_t description;
} bt_presentation_format_t;

#define BT_PRESENTATION_FORMAT_NONE ((bt_presentation_format_t){0, 0, 0, 0, 0})
#define BT_PRESENTATION_FORMAT(format, exponent, unit) ((bt_presentation_format_t){format, exponent, unit, 0, 0})
#define BT_PRESENTATION_FORMAT_UTF8 BT_PRESENTATION_FORMAT(BT_FORMAT_UTF8_STR, 0, BT_UNIT_UNITLESS)

typedef struct gatt_server_char_s
{
    esp_bt_uuid_t uuid;
    esp_gatt_char_prop_t prop;
    esp_gatt_perm_t perm;
    esp_attr_value_t *value;
    gatt_char_read_f read;
    void *read_data;
    gatt_char_write_f write;
    void *write_data;
    const char *description;
    bt_presentation_format_t format;
    struct
    {
        uint16_t handle;
        uint16_t client_conf_handle;
        uint16_t client_conf;
        uint16_t description_handle;
        uint16_t presentation_handle;
    } state;
} gatt_server_char_t;

typedef struct gatt_server_service_s
{
    esp_bt_uuid_t uuid;
    gatt_server_char_t *chars;
    unsigned chars_len;
    struct
    {
        uint16_t gatt_if;
        esp_gatt_srvc_id_t service_id;
        uint16_t handle;
        unsigned current_char;
    } state;
} gatt_server_service_t;

typedef struct gatt_server_conn_s
{
    esp_bd_addr_t addr;
    uint16_t conn_id;
    struct
    {
        uint8_t data[GATT_SERVER_PREPARE_WRITE_BUFSIZE];
        uint16_t pos;
        uint16_t handle;
    } write_buf;
} gatt_server_conn_t;

#define GATT_SERVER_MAX_CONNECTIONS 8
#define GATT_SERVER_MAX_NAME_LENGTH 128

typedef struct gatt_server_s
{
    gatt_server_service_t *services;
    unsigned services_len;
    struct
    {
        uint8_t adv_config_done;
        bool is_advertising;
        unsigned current_service;
        gatt_server_conn_t conns[GATT_SERVER_MAX_CONNECTIONS];
        int num_conns;
        char name[GATT_SERVER_MAX_NAME_LENGTH];
    } state;
} gatt_server_t;

void gatt_server_init(gatt_server_t *server);
void gatt_server_set_name(gatt_server_t *server, const char *name);
esp_err_t gatt_server_start(gatt_server_t *server, esp_gap_ble_cb_t gap_event_handler, esp_gatts_cb_t gatts_event_handler);
void gatt_server_gap_event_handler(gatt_server_t *server, esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void gatt_server_gatts_event_handler(gatt_server_t *server, esp_gatts_cb_event_t event, esp_gatt_if_t gatt_if, esp_ble_gatts_cb_param_t *param);
esp_err_t gatt_server_notify_char(gatt_server_t *server, gatt_server_char_t *chr, const void *data, size_t size);

#define BT_UUID_16(n) ((esp_bt_uuid_t){ \
    .len = ESP_UUID_LEN_16,             \
    .uuid.uuid16 = n,                   \
})

#define BT_UUID_16_FROM(n, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11) ((esp_bt_uuid_t){    \
    .len = ESP_UUID_LEN_128,                                                                      \
    .uuid.uuid128 = {b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, (n) >> 8, (n)&0xff, 0, 0}, \
})

#define BT_UUID_128_GET16(u) ((u.uuid.uuid128[12] << 8) | u.uuid.uuid128[13])

#define GAP_EVENT_HANDLER_FUNC(server, name) \
    static void name(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) { gatt_server_gap_event_handler(&server, event, param); }

#define GATT_EVENT_HANDLER_FUNC(server, name) \
    static void name(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) { gatt_server_gatts_event_handler(&server, event, gatts_if, param); }

#define ATTR_VALUE_STR(s) ((esp_attr_value_t){ \
    .attr_value = (uint8_t *)s,                \
    .attr_len = sizeof(s) - 1,                 \
    .attr_max_len = sizeof(s) - 1,             \
})
