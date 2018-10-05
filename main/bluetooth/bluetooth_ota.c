#include <stdint.h>
#include <string.h>

#include "bluetooth/bluetooth_uuid.h"

#include "ota/ota.h"

#include "util/macros.h"

#include "bluetooth_ota.h"

typedef struct ota_control_req_begin_s
{
    uint32_t image_size;
    uint32_t data_size;
    uint8_t data_format; // from ota_data_format_e
} PACKED ota_control_req_begin_t;

typedef enum
{
    OTA_CONTROL_REQ_IGNORE = 0, // Used to test max write size
    OTA_CONTROL_REQ_BEGIN = 1,  // Begins the OTA. See ota_control_req_begin_t
    OTA_CONTROL_REQ_END = 2,
} ota_control_req_e;

typedef struct ota_control_status_s
{
    uint32_t offset;
    uint8_t status;
    uint8_t data_format; // from ota_data_format_e
} PACKED ota_control_status_t;

static esp_gatt_status_t ota_control_read(gatt_server_t *server, gatt_server_char_t *chr, struct gatts_read_evt_param *r, esp_gatt_rsp_t *rsp, void *user_data)
{
    ota_control_status_t status;
    ota_data_format_e data_format;
    status.status = ota_get_status(&status.offset, &data_format);
    status.data_format = data_format;
    rsp->attr_value.len = sizeof(status);
    memcpy(rsp->attr_value.value, &status, sizeof(status));
    return ESP_GATT_OK;
}

static esp_gatt_status_t ota_control_write(gatt_server_t *server, gatt_server_char_t *chr, gatt_server_write_info_t *write_info, void *user_data)
{
    esp_gatt_status_t result = ESP_GATT_OK;
    ota_control_req_begin_t begin_req;

    if (write_info->data_len > 0)
    {
        switch ((ota_control_req_e)write_info->data[0])
        {
        case OTA_CONTROL_REQ_IGNORE:
            break;
        case OTA_CONTROL_REQ_BEGIN:
            if (write_info->data_len < sizeof(begin_req) + 1)
            {
                result = ESP_GATT_INVALID_ATTR_LEN;
                break;
            }
            memcpy(&begin_req, &write_info->data[1], sizeof(begin_req));
            if (begin_req.image_size == 0 || begin_req.data_size == 0)
            {
                result = ESP_GATT_ILLEGAL_PARAMETER;
                break;
            }
            ota_begin(begin_req.image_size, begin_req.data_size, begin_req.data_format);
            break;
        case OTA_CONTROL_REQ_END:
            ota_end();
            break;
        }
    }
    return result;
}

static esp_gatt_status_t ota_data_write(gatt_server_t *server, gatt_server_char_t *chr, gatt_server_write_info_t *write_info, void *user_data)
{
    uint32_t offset;
    uint8_t offset_data[3];
    memcpy(offset_data, write_info->data, sizeof(offset_data));
    offset = (offset_data[2] << 16) | (offset_data[1] << 8) | (offset_data[0]);
    const uint8_t *p = write_info->data;
    p += sizeof(offset_data);
    ota_write(offset, p, write_info->data_len - sizeof(offset_data));
    return ESP_GATT_OK;
}

gatt_server_char_t bt_ota_characteristics[2] = {
    // Control
    {
        .uuid = RAVEN_UUID(0x11),
        .value = NULL,
        .read = ota_control_read,
        .write = ota_control_write,
        .prop = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
        .perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
        .description = "OTA Update Control",
    },
    // Data
    {
        .uuid = RAVEN_UUID(0x12),
        .value = NULL,
        .read = NULL,
        .write = ota_data_write,
        .prop = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
        .perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
        .description = "OTA Update Data",
    },
};