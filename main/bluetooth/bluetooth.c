#include <stdio.h>

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>

#include "bluetooth/bluetooth_hal.h"
#include "bluetooth/bluetooth_ota.h"
#include "bluetooth/bluetooth_uuid.h"

#include "io/io.h"

#include "rc/rc.h"
#include "rc/telemetry.h"

#include "msp/msp.h"
#include "msp/msp_serial.h"

#include "util/macros.h"
#include "util/version.h"

#include "bluetooth.h"

static const char *TAG = "Bluetooth";

#define TELEMETRY_UUID_OFFSET 0x8000

static esp_gatt_status_t hm_serial_read(gatt_server_t *server, gatt_server_char_t *chr, struct gatts_read_evt_param *r, esp_gatt_rsp_t *rsp, void *user_data);
static esp_gatt_status_t hm_serial_write(gatt_server_t *server, gatt_server_char_t *chr, gatt_server_write_info_t *write_info, void *user_data);

#if 0
static gatt_server_char_t msp_characteristic = {
    .uuid = RAVEN_UUID(0x0002),
    .value = NULL,
    .read = NULL,
    .prop = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
    .perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
    .description = NULL,
};
#endif

static gatt_server_char_t telemetry_chars[TELEMETRY_COUNT];

static esp_attr_value_t manufacturer_attribute_value = ATTR_VALUE_STR("Alberto GH");
static esp_attr_value_t model_attribute_value = ATTR_VALUE_STR("Raven RC");
#ifdef SOFTWARE_VERSION
static esp_attr_value_t swvers_attribute_value = ATTR_VALUE_STR(SOFTWARE_VERSION);
#endif

static gatt_server_char_t device_info_characteristics[] = {
    // Manufacturer Name String
    {
        .uuid = BT_UUID_16(0x2A29),
        .value = &manufacturer_attribute_value,
        .read = NULL,
        .prop = ESP_GATT_CHAR_PROP_BIT_READ,
        .perm = ESP_GATT_PERM_READ,
        .description = NULL,
    },
    // Model Name String
    {
        .uuid = BT_UUID_16(0x2A24),
        .value = &model_attribute_value,
        .read = NULL,
        .prop = ESP_GATT_CHAR_PROP_BIT_READ,
        .perm = ESP_GATT_PERM_READ,
        .description = NULL,
    },
#ifdef SOFTWARE_VERSION
    // Software Revision String
    {
        .uuid = BT_UUID_16(0x2A28),
        .value = &swvers_attribute_value,
        .read = NULL,
        .prop = ESP_GATT_CHAR_PROP_BIT_READ,
        .perm = ESP_GATT_PERM_READ,
        .description = NULL,
    },
#endif
};

static gatt_server_char_t hm10_serial_characteristic = {
    .uuid = BT_UUID_16(0xFFE1),
    .value = NULL,
    .read = hm_serial_read,
    .write = hm_serial_write,
    .prop = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
    .perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
    .description = "TX & RX",
};

static gatt_server_service_t services[] = {
    // Device info
    {
        .uuid = BT_UUID_16(0x180A),
        .chars = device_info_characteristics,
        .chars_len = ARRAY_COUNT(device_info_characteristics),
    },
    // OTA
    {
        .uuid = RAVEN_SERVICE_UUID(0),
        .chars = bt_ota_characteristics,
        .chars_len = ARRAY_COUNT(bt_ota_characteristics),
    },
    // Telemetry
    {
        .uuid = RAVEN_SERVICE_UUID(1),
        .chars = telemetry_chars,
        .chars_len = ARRAY_COUNT(telemetry_chars),
    },
    // HM-10/11 Bluetooh serial service
    {
        .uuid = BT_UUID_16(0xFFE0),
        .chars = &hm10_serial_characteristic,
        .chars_len = 1,
    },
#if 0
    // MSP
    {
        .uuid = BT_UUID_16(0x0001),
        .chars = &msp_characteristic,
        .chars_len = 1,
    },
#endif
};

static gatt_server_t gatt_server = {
    .services = services,
    .services_len = ARRAY_COUNT(services),
};

GAP_EVENT_HANDLER_FUNC(gatt_server, gap_event_handler);
GATT_EVENT_HANDLER_FUNC(gatt_server, gatts_event_handler);

#define HM_SERIAL_RB_CAPACITY MSP_MAX_PAYLOAD_SIZE
#define HM_SERIAL_MAX_READ_SIZE 20

typedef struct hm_serial_data_s
{
    msp_conn_t msp; // Used to decode incoming raw MSP via BT
    msp_serial_t msp_serial;
    RING_BUFFER_DECLARE(rb, uint8_t, HM_SERIAL_RB_CAPACITY);
} hm_serial_data_t;

static hm_serial_data_t hm_serial_data;

static int hm_serial_msp_io_read(void *data, void *buf, size_t size, time_ticks_t timeout)
{
    hm_serial_data_t *serial_data = data;
    uint8_t *ptr = buf;
    uint8_t c;
    int n = 0;
    while (n < size && ring_buffer_pop(&serial_data->rb, &c))
    {
        ptr[n++] = c;
    }
    return n;
}

static int hm_serial_msp_io_write(void *data, const void *buf, size_t size)
{
    const uint8_t *ptr = buf;
    for (int ii = 0; ii < size; ii += HM_SERIAL_MAX_READ_SIZE)
    {
        size_t rem = MIN(HM_SERIAL_MAX_READ_SIZE, size - ii);
        ESP_ERROR_CHECK(gatt_server_notify_char(&gatt_server, &hm10_serial_characteristic, &ptr[ii], rem));
    }
    return size;
}

static esp_gatt_status_t hm_serial_read(gatt_server_t *server, gatt_server_char_t *chr, struct gatts_read_evt_param *r, esp_gatt_rsp_t *rsp, void *user_data)
{
    // TODO: No support for reading right now, only notifications
    return ESP_GATT_OK;
}

static esp_gatt_status_t hm_serial_write(gatt_server_t *server, gatt_server_char_t *chr, gatt_server_write_info_t *write_info, void *user_data)
{
    hm_serial_data_t *serial_data = user_data;
    for (int ii = 0; ii < write_info->data_len; ii++)
    {
        ring_buffer_push(&serial_data->rb, &write_info->data[ii]);
    }
    msp_conn_update(&serial_data->msp);
    return ESP_GATT_OK;
}

static esp_gatt_status_t telemetry_read(gatt_server_t *server, gatt_server_char_t *chr, struct gatts_read_evt_param *r, esp_gatt_rsp_t *rsp, void *user_data)
{
    rc_t *rc = user_data;
    int pos = BT_UUID_128_GET16(chr->uuid) - TELEMETRY_UUID_OFFSET;
    int telemetry_id = telemetry_get_id_at(pos);
    telemetry_t *tel = rc_data_get_telemetry(&rc->data, telemetry_id);
    switch (telemetry_get_type(telemetry_id))
    {
    case TELEMETRY_TYPE_UINT8:
        rsp->attr_value.len = 1;
        memcpy(rsp->attr_value.value, &tel->val.u8, 1);
        break;
    case TELEMETRY_TYPE_INT8:
        rsp->attr_value.len = 1;
        memcpy(rsp->attr_value.value, &tel->val.i8, 1);
        break;
    case TELEMETRY_TYPE_UINT16:
        rsp->attr_value.len = 2;
        memcpy(rsp->attr_value.value, &tel->val.u16, 2);
        break;
    case TELEMETRY_TYPE_INT16:
        rsp->attr_value.len = 2;
        memcpy(rsp->attr_value.value, &tel->val.i16, 2);
        break;
    case TELEMETRY_TYPE_UINT32:
        rsp->attr_value.len = 4;
        memcpy(rsp->attr_value.value, &tel->val.u32, 4);
        break;
    case TELEMETRY_TYPE_INT32:
        rsp->attr_value.len = 4;
        memcpy(rsp->attr_value.value, &tel->val.i32, 4);
        break;
    case TELEMETRY_TYPE_STRING:
        rsp->attr_value.len = strlen(tel->val.s) + 1;
        memcpy(rsp->attr_value.value, tel->val.s, rsp->attr_value.len);
        break;
    }
    return ESP_GATT_OK;
}

static void bluetooth_update_device_name(rc_t *rc)
{
    const char *name = "";

    switch (config_get_rc_mode())
    {
    case RC_MODE_TX:
        name = "Raven TX";
        break;
    case RC_MODE_RX:
        name = "Raven RX";
        break;
    }
    gatt_server_set_name(&gatt_server, name);
}

static esp_err_t bluetooth_init(rc_t *rc)
{
    esp_err_t ret;

    if ((ret = nvs_flash_init()))
    {
        ESP_LOGE(TAG, "%s initialize NVS failed: %x", __func__, ret);
        return ret;
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)))
    {
        ESP_LOGE(TAG, "%s initialize controller failed: %x", __func__, ret);
        return ret;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BLE)))
    {
        ESP_LOGE(TAG, "%s enable controller failed: %x", __func__, ret);
        return ret;
    }
    if ((ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P7)))
    {
        ESP_LOGE(TAG, "%s set tx power failed: %x", __func__, ret);
        return ret;
    }

    gatt_server_init(&gatt_server);
    bluetooth_update_device_name(rc);

    io_t msp_io = {
        .read = hm_serial_msp_io_read,
        .write = hm_serial_msp_io_write,
        .data = &hm_serial_data,
    };

    msp_serial_init(&hm_serial_data.msp_serial, &msp_io);
    msp_conn_init(&hm_serial_data.msp, MSP_TRANSPORT(&hm_serial_data.msp_serial));
    rc_connect_msp_input(rc, &hm_serial_data.msp);
    RING_BUFFER_INIT(&hm_serial_data.rb, uint8_t, HM_SERIAL_RB_CAPACITY);
    hm10_serial_characteristic.read_data = &hm_serial_data;
    hm10_serial_characteristic.write_data = &hm_serial_data;

    for (int ii = 0; ii < ARRAY_COUNT(telemetry_chars); ii++)
    {
        gatt_server_char_t *chr = &telemetry_chars[ii];
        int telemetry_id = telemetry_get_id_at(ii);
        chr->uuid = RAVEN_UUID(TELEMETRY_UUID_OFFSET + ii);
        chr->value = NULL;
        chr->read = telemetry_read;
        chr->read_data = rc;
        chr->description = telemetry_get_name(telemetry_id);
        chr->prop = ESP_GATT_CHAR_PROP_BIT_READ;
        chr->perm = ESP_GATT_PERM_READ;
        switch (telemetry_get_type(telemetry_id))
        {
        case TELEMETRY_TYPE_UINT8:
            chr->format = BT_PRESENTATION_FORMAT(BT_FORMAT_UINT8, 0, BT_UNIT_UNITLESS);
            break;
        case TELEMETRY_TYPE_INT8:
            chr->format = BT_PRESENTATION_FORMAT(BT_FORMAT_INT8, 0, BT_UNIT_UNITLESS);
            break;
        case TELEMETRY_TYPE_UINT16:
            chr->format = BT_PRESENTATION_FORMAT(BT_FORMAT_UINT16, 0, BT_UNIT_UNITLESS);
            break;
        case TELEMETRY_TYPE_INT16:
            chr->format = BT_PRESENTATION_FORMAT(BT_FORMAT_INT16, 0, BT_UNIT_UNITLESS);
            break;
        case TELEMETRY_TYPE_UINT32:
            chr->format = BT_PRESENTATION_FORMAT(BT_FORMAT_UINT32, 0, BT_UNIT_UNITLESS);
            break;
        case TELEMETRY_TYPE_INT32:
            chr->format = BT_PRESENTATION_FORMAT(BT_FORMAT_INT32, 0, BT_UNIT_UNITLESS);
            break;
        case TELEMETRY_TYPE_STRING:
            chr->format = BT_PRESENTATION_FORMAT_UTF8;
            break;
        }
    }

    return gatt_server_start(&gatt_server, gap_event_handler, gatts_event_handler);
}

void task_bluetooh(void *arg)
{
    rc_t *rc = arg;
    ESP_ERROR_CHECK(bluetooth_init(rc));

    for (;;)
    {
        bluetooth_update_device_name(rc);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
