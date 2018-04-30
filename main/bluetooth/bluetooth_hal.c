#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_system.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <esp_bt_defs.h>
#include <esp_bt_main.h>
#include <esp_gatts_api.h>
#include <esp_gatt_common_api.h>

#include "util/macros.h"

#include "bluetooth_hal.h"

static const char *TAG = "Bluetooth.HAL";

#define GATT_SERVER_CONN_INVALID 0xff
#define GATT_SERVER_HANDLE_INVALID 0xffff

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40

uint8_t char1_str[] = {0x11, 0x22, 0x33};

esp_attr_value_t gatts_demo_char1_val =
    {
        .attr_max_len = GATTS_DEMO_CHAR_VAL_LEN_MAX,
        .attr_len = sizeof(char1_str),
        .attr_value = char1_str,
};

#define adv_config_flag (1 << 0)
#define scan_rsp_config_flag (1 << 1)

static void esp_bt_uuid_add(esp_bt_uuid_t *out, esp_bt_uuid_t *uuid)
{
    switch (uuid->len)
    {
    case ESP_UUID_LEN_16:
        memcpy(&out->uuid.uuid128[12], &uuid->uuid.uuid16, ESP_UUID_LEN_16);
        break;
    case ESP_UUID_LEN_32:
        memcpy(&out->uuid.uuid128[12], &uuid->uuid.uuid32, ESP_UUID_LEN_32);
        break;
    case ESP_UUID_LEN_128:
        memcpy(out->uuid.uuid128, uuid->uuid.uuid128, ESP_UUID_LEN_128);
        break;
    default:
        assert(0 && "invalid BT UUID length");
    }
}

#define BT_DEFAULT_UUID_BASE 0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

static void esp_bt_uuid_full(esp_bt_uuid_t *out, esp_bt_uuid_t *uuid)
{
    static uint8_t default_uuid[ESP_UUID_LEN_128] = {BT_DEFAULT_UUID_BASE};
    out->len = ESP_UUID_LEN_128;
    memcpy(out->uuid.uuid128, default_uuid, ESP_UUID_LEN_128);
    esp_bt_uuid_add(out, uuid);
}

static void gatt_server_start_advertising(gatt_server_t *server);
static esp_err_t gatt_server_register_next_app(gatt_server_t *server);

void gatt_server_init(gatt_server_t *server)
{
    server->state.name[0] = '\0';
    server->state.is_advertising = false;
}

void gatt_server_set_name(gatt_server_t *server, const char *name)
{
    if (!name)
    {
        name = "";
    }
    if (strcmp(server->state.name, name) != 0)
    {
        strlcpy(server->state.name, name, sizeof(server->state.name));
        esp_ble_gap_set_device_name(server->state.name);
        if (server->state.is_advertising)
        {
            ESP_ERROR_CHECK(esp_ble_gap_stop_advertising());
            gatt_server_start_advertising(server);
        }
    }
}

esp_err_t gatt_server_start(gatt_server_t *server, esp_gap_ble_cb_t gap_event_handler, esp_gatts_cb_t gatts_event_handler)
{
    esp_err_t ret;

    server->state.adv_config_done = 0;
    server->state.current_service = 0;

    // Prepare services
    for (int ii = 0; ii < server->services_len; ii++)
    {
        gatt_server_service_t *service = &server->services[ii];
        service->state.gatt_if = ESP_GATT_IF_NONE;
        service->state.current_char = 0;

        esp_gatt_srvc_id_t *srv = &service->state.service_id;
        srv->is_primary = true;
        srv->id.inst_id = ii;
        memcpy(&srv->id.uuid, &service->uuid, sizeof(srv->id.uuid));

        for (int jj = 0; jj < service->chars_len; jj++)
        {
            gatt_server_char_t *chr = &service->chars[jj];
            chr->state.client_conf_handle = GATT_SERVER_HANDLE_INVALID;
            chr->state.description_handle = GATT_SERVER_HANDLE_INVALID;
            chr->state.presentation_handle = GATT_SERVER_HANDLE_INVALID;
        }
    }

    memset(&server->state.conns, 0, sizeof(server->state.conns));
    server->state.num_conns = 0;
    for (int ii = 0; ii < GATT_SERVER_MAX_CONNECTIONS; ii++)
    {
        server->state.conns[ii].conn_id = GATT_SERVER_CONN_INVALID;
    }

    if ((ret = esp_bluedroid_init()))
    {
        ESP_LOGE(TAG, "%s init bluetooth failed: %x", __func__, ret);
        return ret;
    }
    if ((ret = esp_bluedroid_enable()))
    {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %x", __func__, ret);
        return ret;
    }

    if ((ret = esp_ble_gatts_register_callback(gatts_event_handler)))
    {
        ESP_LOGE(TAG, "gatts register error, error code = %x", ret);
        return ret;
    }
    if ((ret = esp_ble_gap_register_callback(gap_event_handler)))
    {
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return ret;
    }

    if ((ret = esp_ble_gatt_set_local_mtu(500)))
    {
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", ret);
        return ret;
    }

    return gatt_server_register_next_app(server);
}

static void gatt_server_prepare_adv_data(esp_ble_adv_data_t *adv_data, gatt_server_t *server, uint8_t *service_uuid, int service_uuid_len, bool scan_rsp)
{
    adv_data->set_scan_rsp = scan_rsp;
    adv_data->include_name = true;
    adv_data->include_txpower = true;
    adv_data->min_interval = 0x20;
    adv_data->max_interval = 0x40;
    adv_data->appearance = 0x00;
    adv_data->manufacturer_len = 0;
    adv_data->p_manufacturer_data = NULL;
    adv_data->service_data_len = 0;
    adv_data->p_service_data = NULL;
    adv_data->service_uuid_len = service_uuid_len;
    adv_data->p_service_uuid = service_uuid;
    adv_data->flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
}

static esp_err_t gatt_server_update_adv_data(gatt_server_t *server)
{
    esp_ble_adv_data_t adv_data;
    uint8_t service_uuid[16 * server->services_len];
    //config adv data
    esp_bt_uuid_t full_uuid;
    for (int ii = 0; ii < server->services_len; ii++)
    {
        esp_bt_uuid_full(&full_uuid, &server->services[ii].uuid);
        memcpy(&service_uuid[16 * ii], full_uuid.uuid.uuid128, ESP_UUID_LEN_128);
    }
    gatt_server_prepare_adv_data(&adv_data, server, service_uuid, sizeof(service_uuid), false);
    esp_err_t ret;

    if ((ret = esp_ble_gap_config_adv_data(&adv_data)))
    {
        ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        return ret;
    }
    if (strlen(server->state.name) > 0)
    {
        if ((ret = esp_ble_gap_set_device_name(server->state.name)))
        {
            ESP_LOGE(TAG, "set device name failed, error code = %x", ret);
            return ret;
        }
    }
    server->state.adv_config_done |= adv_config_flag;
    //config scan response data
    gatt_server_prepare_adv_data(&adv_data, server, service_uuid, sizeof(service_uuid), true);
    if ((ret = esp_ble_gap_config_adv_data(&adv_data)))
    {
        ESP_LOGE(TAG, "config scan response data failed, error code = %x", ret);
        return ret;
    }
    server->state.adv_config_done |= scan_rsp_config_flag;
    return ESP_OK;
}

static void gatt_server_prepare_adv_params(esp_ble_adv_params_t *adv_params, gatt_server_t *server)
{
    adv_params->adv_int_min = 0x20;
    adv_params->adv_int_max = 0x40;
    adv_params->adv_type = ADV_TYPE_IND;
    adv_params->own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    //.peer_addr            =
    //.peer_addr_type       =
    adv_params->channel_map = ADV_CHNL_ALL;
    adv_params->adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
}

static void gatt_server_start_advertising(gatt_server_t *server)
{
    esp_ble_adv_params_t adv_params;
    gatt_server_prepare_adv_params(&adv_params, server);
    ESP_ERROR_CHECK(esp_ble_gap_start_advertising(&adv_params));
    server->state.is_advertising = true;
}

static bool gatt_server_register_conn(gatt_server_t *server, struct gatts_connect_evt_param *param, bool *is_full)
{
    *is_full = false;
    for (int ii = 0; ii < server->state.num_conns; ii++)
    {
        gatt_server_conn_t *conn = &server->state.conns[ii];
        if (memcmp(conn->addr, param->remote_bda, sizeof(conn->addr)) == 0 && conn->conn_id == param->conn_id)
        {
            // Already registered
            return false;
        }
    }
    // Try to register it
    for (int ii = 0; ii < GATT_SERVER_MAX_CONNECTIONS; ii++)
    {
        gatt_server_conn_t *conn = &server->state.conns[ii];
        if (conn->conn_id == GATT_SERVER_CONN_INVALID)
        {
            // Found an empty slot
            conn->conn_id = param->conn_id;
            conn->write_buf.pos = 0;
            conn->write_buf.handle = GATT_SERVER_HANDLE_INVALID;
            memcpy(conn->addr, param->remote_bda, sizeof(conn->addr));
            server->state.num_conns++;
            return true;
        }
    }
    // Max number reached
    *is_full = true;
    return false;
}

static gatt_server_conn_t *gatt_server_get_conn(gatt_server_t *server, uint16_t conn_id)
{
    for (int ii = 0; ii < server->state.num_conns; ii++)
    {
        gatt_server_conn_t *conn = &server->state.conns[ii];
        if (conn->conn_id == conn_id)
        {
            return conn;
        }
    }
    return NULL;
}

static void gatt_server_remove_conn(gatt_server_t *server, struct gatts_disconnect_evt_param *param)
{
    for (int ii = 0; ii < server->state.num_conns; ii++)
    {
        gatt_server_conn_t *conn = &server->state.conns[ii];
        if (memcmp(conn->addr, param->remote_bda, sizeof(conn->addr)) == 0 && conn->conn_id == param->conn_id)
        {
            conn->conn_id = GATT_SERVER_CONN_INVALID;
            server->state.num_conns--;
            break;
        }
    }
}

static void gatt_server_get_subscribers(gatt_server_t *server, gatt_server_service_t *service,
                                        gatt_server_char_t *chr,
                                        uint16_t *subscribers, size_t *size)
{
    // TODO: Returning all active connections, fix this
    *size = 0;
    for (int ii = 0; ii < server->state.num_conns; ii++)
    {
        if (server->state.conns[ii].conn_id != GATT_SERVER_CONN_INVALID)
        {
            subscribers[*size] = server->state.conns[ii].conn_id;
            (*size)++;
        }
    }
}

static bool gatt_server_char_needs_client_conf_desc(gatt_server_char_t *chr)
{
    return chr->prop & (ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_INDICATE);
}

static bool gatt_server_char_needs_description_desc(gatt_server_char_t *chr)
{
    return chr->description && *chr->description;
}

static bool gatt_server_char_needs_presentation_desc(gatt_server_char_t *chr)
{
    return chr->format.format != 0;
}

static bool gatt_server_register_next_descriptor(gatt_server_t *server, gatt_server_service_t *service, gatt_server_char_t *chr)
{
    if (gatt_server_char_needs_client_conf_desc(chr) && chr->state.client_conf_handle == GATT_SERVER_HANDLE_INVALID)
    {
        esp_bt_uuid_t uuid = BT_UUID_16(ESP_GATT_UUID_CHAR_CLIENT_CONFIG);
        ESP_ERROR_CHECK(esp_ble_gatts_add_char_descr(service->state.handle, &uuid,
                                                     ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL));
        return true;
    }
    if (gatt_server_char_needs_description_desc(chr) && chr->state.description_handle == GATT_SERVER_HANDLE_INVALID)
    {
        esp_bt_uuid_t uuid = BT_UUID_16(ESP_GATT_UUID_CHAR_DESCRIPTION);
        esp_attr_value_t value = {
            .attr_max_len = strlen(chr->description),
            .attr_len = strlen(chr->description),
            .attr_value = (uint8_t *)chr->description,
        };
        esp_attr_control_t control = {
            .auto_rsp = ESP_GATT_AUTO_RSP,
        };
        ESP_ERROR_CHECK(esp_ble_gatts_add_char_descr(service->state.handle, &uuid,
                                                     ESP_GATT_PERM_READ, &value, &control));
        return true;
    }
    if (gatt_server_char_needs_presentation_desc(chr) && chr->state.presentation_handle == GATT_SERVER_HANDLE_INVALID)
    {
        esp_bt_uuid_t uuid = BT_UUID_16(ESP_GATT_UUID_CHAR_PRESENT_FORMAT);
        ESP_ERROR_CHECK(esp_ble_gatts_add_char_descr(service->state.handle, &uuid,
                                                     ESP_GATT_PERM_READ, NULL, NULL));
        return true;
    }
    return false;
}

static int gatt_server_service_num_handles(gatt_server_service_t *service)
{
    int handles = 1; // Service handle
    for (int ii = 0; ii < service->chars_len; ii++)
    {
        // Characteristic handle
        handles++;
        // Characteristic value handle
        handles++;
        if (gatt_server_char_needs_client_conf_desc(&service->chars[ii]))
        {
            // Characteristic client configuration descriptor handle
            handles++;
        }
        if (gatt_server_char_needs_description_desc(&service->chars[ii]))
        {
            // Characteristic user description descriptor handle
            handles++;
        }
        if (gatt_server_char_needs_presentation_desc(&service->chars[ii]))
        {
            // Characteristic presentation format descriptor handle
            handles++;
        }
    }
    return handles;
}

static esp_err_t gatt_server_register_next_app(gatt_server_t *server)
{
    esp_err_t ret = ESP_OK;
    if (server->state.current_service < server->services_len)
    {
        // XXX: Note that this needs to be reentrant because calling esp_ble_gatts_app_register()
        // will unleash a series of callbacks where we will end up calling gatt_server_register_next_app()
        // again before esp_ble_gatts_app_register() returns.
        ret = esp_ble_gatts_app_register(server->state.current_service++);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "gatts app register error at index %u, error code = %x", server->state.current_service, ret);
        }
    }
    return ret;
}

static esp_err_t gatt_server_add_service_at(gatt_server_t *server, esp_gatt_if_t gatts_if, unsigned pos)
{
    gatt_server_service_t *service = &server->services[pos];
    int handles = gatt_server_service_num_handles(service);
    ESP_LOGD(TAG, "Adding service %u (%d handles)", pos, handles);
    return esp_ble_gatts_create_service(gatts_if, &service->state.service_id, handles);
}

static bool gatt_server_service_is_completely_added(gatt_server_service_t *service)
{
    return service->state.current_char >= service->chars_len;
}

static esp_err_t gatt_server_add_next_char(gatt_server_t *server, gatt_server_service_t *service)
{
    if (service->state.current_char < service->chars_len)
    {
        ESP_LOGD(TAG, "Adding char %u in service %u", service->state.current_char, server->state.current_service - 1);
        // Don't increment here, we do that after registering the descriptors
        gatt_server_char_t *characteristic = &service->chars[service->state.current_char];

        esp_err_t ret = esp_ble_gatts_add_char(service->state.handle, &characteristic->uuid,
                                               characteristic->perm,
                                               characteristic->prop,
                                               characteristic->value, NULL);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "add char failed, error code = %x", ret);
        }
        return ret;
    }
    return ESP_OK;
}

static esp_err_t gatt_server_add_next_entry(gatt_server_t *server, gatt_server_service_t *service)
{
    // We just have one descriptor per characteristic right now, so we can
    // move to the next characteristic here
    service->state.current_char++;
    if (gatt_server_service_is_completely_added(service))
    {
        return gatt_server_register_next_app(server);
    }
    return gatt_server_add_next_char(server, service);
}

static esp_err_t gatt_server_service_read_handler(gatt_server_t *server, gatt_server_service_t *service, esp_gatt_if_t gatts_if, struct gatts_read_evt_param *param)
{
    esp_gatt_rsp_t rsp;
    esp_gatt_status_t status = ESP_GATT_INVALID_HANDLE;
    memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
    rsp.attr_value.len = 0;
    rsp.attr_value.handle = param->handle;

    for (int ii = 0; ii < service->chars_len; ii++)
    {
        gatt_server_char_t *chr = &service->chars[ii];
        if (chr->state.handle == param->handle)
        {
            if (chr->read)
            {
                status = chr->read(server, chr, param, &rsp, chr->read_data);
            }
            else if (chr->value)
            {
                rsp.attr_value.len = chr->value->attr_len;
                memcpy(rsp.attr_value.value, chr->value->attr_value, rsp.attr_value.len);
                status = ESP_GATT_OK;
            }
            break;
        }
        if (chr->state.client_conf_handle == param->handle)
        {
            rsp.attr_value.len = sizeof(chr->state.client_conf);
            memcpy(rsp.attr_value.value, &chr->state.client_conf, sizeof(chr->state.client_conf));
            status = ESP_GATT_OK;
            break;
        }
        if (chr->state.presentation_handle == param->handle)
        {
            rsp.attr_value.len = 7;
            rsp.attr_value.value[0] = chr->format.format;
            rsp.attr_value.value[1] = chr->format.exponent;
            rsp.attr_value.value[2] = chr->format.unit & 0xFF;
            rsp.attr_value.value[3] = chr->format.unit >> 8;
            rsp.attr_value.value[4] = chr->format.ns;
            rsp.attr_value.value[5] = chr->format.description & 0xFF;
            rsp.attr_value.value[5] = chr->format.description >> 8;
            status = ESP_GATT_OK;
            break;
        }
    }
    return esp_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id,
                                       status, &rsp);
}

static esp_err_t gatt_server_service_write_prep_handler(gatt_server_t *server, gatt_server_service_t *service, esp_gatt_if_t gatts_if, struct gatts_write_evt_param *param)
{
    esp_gatt_status_t status;
    esp_gatt_rsp_t rsp;
    gatt_server_conn_t *conn;

    if (param->offset >= GATT_SERVER_PREPARE_WRITE_BUFSIZE)
    {
        status = ESP_GATT_INVALID_OFFSET;
    }
    else if (param->offset + param->len > GATT_SERVER_PREPARE_WRITE_BUFSIZE)
    {
        status = ESP_GATT_INVALID_ATTR_LEN;
    }
    else
    {
        conn = gatt_server_get_conn(server, param->conn_id);
        if (conn)
        {
            if (conn->write_buf.handle != GATT_SERVER_HANDLE_INVALID && conn->write_buf.handle != param->handle)
            {
                status = ESP_GATT_INVALID_HANDLE;
            }
            else
            {
                // TODO: If need_rsp is true, confirm the response before commiting the
                // data to memory.
                conn->write_buf.handle = param->handle;
                memcpy(conn->write_buf.data + param->offset, param->value, param->len);
                uint16_t pos = param->offset + param->len;
                if (pos > conn->write_buf.pos)
                {
                    conn->write_buf.pos = pos;
                }
                status = ESP_GATT_OK;
            }
        }
        else
        {
            status = ESP_GATT_INVALID_HANDLE;
        }
    }

    if (param->need_rsp)
    {
        rsp.attr_value.len = param->len;
        rsp.handle = param->handle;
        rsp.attr_value.offset = param->offset;
        rsp.attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
        memcpy(rsp.attr_value.value, param->value, param->len);
        return esp_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, status, &rsp);
    }
    return ESP_OK;
}

static esp_err_t gatt_server_service_write_data_handler(gatt_server_t *server, gatt_server_service_t *service,
                                                        esp_gatt_if_t gatts_if,
                                                        gatt_server_write_info_t *info,
                                                        uint16_t handle)
{
    esp_gatt_status_t status = ESP_GATT_INVALID_HANDLE;

    for (int ii = 0; ii < service->chars_len; ii++)
    {
        gatt_server_char_t *chr = &service->chars[ii];
        if (chr->state.handle == handle)
        {
            if (!(chr->perm & ESP_GATT_PERM_WRITE))
            {
                // Not writable
                status = ESP_GATT_WRITE_NOT_PERMIT;
                break;
            }
            if (chr->write)
            {
                status = chr->write(server, chr, info, chr->write_data);
            }
            else if (chr->value != NULL)
            {
                if (info->data_len > chr->value->attr_max_len)
                {
                    status = ESP_GATT_INVALID_ATTR_LEN;
                    break;
                }
                memcpy(chr->value->attr_value, info->data, info->data_len);
                chr->value->attr_len = info->data_len;
                status = ESP_GATT_OK;
            }
            break;
        }
        if (chr->state.client_conf_handle != GATT_SERVER_HANDLE_INVALID && chr->state.client_conf_handle == handle)
        {
            if (info->data_len != 2)
            {
                status = ESP_GATT_INVALID_ATTR_LEN;
                break;
            }
            uint16_t conf = info->data[1] << 8 | info->data[0];
            switch (conf)
            {
            case 0x0001:
                if (!(chr->prop & ESP_GATT_CHAR_PROP_BIT_NOTIFY))
                {
                    status = ESP_GATT_INVALID_HANDLE;
                    break;
                }
                status = ESP_GATT_OK;
                // TODO: Subscribe
                break;
            case 0x0002:
                if (!(chr->prop & ESP_GATT_CHAR_PROP_BIT_INDICATE))
                {
                    status = ESP_GATT_INVALID_HANDLE;
                    break;
                }
                status = ESP_GATT_OK;
                // TODO: Subscribe
                break;
            case 0x0000:
                status = ESP_GATT_OK;
                // TODO: Unsubscribe
                break;
            default:
                status = ESP_GATT_INVALID_HANDLE;
            }
            break;
        }
    }
    if (info->need_rsp)
    {
        return esp_ble_gatts_send_response(gatts_if, info->conn_id, info->trans_id, status, NULL);
    }
    return ESP_OK;
#if 0
        if (!param->write.is_prep)
        {
            ESP_LOGI(TAG, "GATT_WRITE_EVT, value len %d, value :", param->write.len);
            esp_log_buffer_hex(TAG, param->write.value, param->write.len);
            if (gl_profile_tab[PROFILE_A_APP_ID].descr_handle == param->write.handle && param->write.len == 2)
            {
                uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
                if (descr_value == 0x0001)
                {
                    if (a_property & ESP_GATT_CHAR_PROP_BIT_NOTIFY)
                    {
                        ESP_LOGI(TAG, "notify enable");
                        uint8_t notify_data[15];
                        for (int i = 0; i < sizeof(notify_data); ++i)
                        {
                            notify_data[i] = i % 0xff;
                        }
                        //the size of notify_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                                                    sizeof(notify_data), notify_data, false);
                    }
                }
                else if (descr_value == 0x0002)
                {
                    if (a_property & ESP_GATT_CHAR_PROP_BIT_INDICATE)
                    {
                        ESP_LOGI(TAG, "indicate enable");
                        uint8_t indicate_data[15];
                        for (int i = 0; i < sizeof(indicate_data); ++i)
                        {
                            indicate_data[i] = i % 0xff;
                        }
                        //the size of indicate_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                                                    sizeof(indicate_data), indicate_data, true);
                    }
                }
                else if (descr_value == 0x0000)
                {
                    ESP_LOGI(TAG, "notify/indicate disable ");
                }
                else
                {
                    ESP_LOGE(TAG, "unknown descr value");
                    esp_log_buffer_hex(TAG, param->write.value, param->write.len);
                }
            }
        }
        example_write_event_env(gatts_if, &a_prepare_write_env, param);
#endif
}

static esp_err_t gatt_server_service_write_handler(gatt_server_t *server, gatt_server_service_t *service, esp_gatt_if_t gatt_if, struct gatts_write_evt_param *param)
{
    if (param->is_prep)
    {
        return gatt_server_service_write_prep_handler(server, service, gatt_if, param);
    }
    gatt_server_write_info_t write_info = {
        .conn_id = param->conn_id,
        .trans_id = param->trans_id,
        .need_rsp = param->need_rsp,
        .data = param->value,
        .data_len = param->len,
    };
    memcpy(write_info.bda, param->bda, sizeof(write_info.bda));
    return gatt_server_service_write_data_handler(server, service, gatt_if, &write_info, param->handle);
}

void gatt_server_gap_event_handler(gatt_server_t *server, esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ESP_LOGD(TAG, "GAP event %d", event);
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        server->state.adv_config_done &= (~adv_config_flag);
        if (server->state.adv_config_done == 0)
        {
            gatt_server_start_advertising(server);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        server->state.adv_config_done &= (~scan_rsp_config_flag);
        if (server->state.adv_config_done == 0)
        {
            gatt_server_start_advertising(server);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Advertising start failed\n");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Advertising stop failed\n");
        }
        else
        {
            ESP_LOGD(TAG, "Stop adv successfully\n");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d, conn_int = %d, latency = %d, timeout = %d",
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

static void gatt_server_gatts_event_service_handler(gatt_server_t *server, gatt_server_service_t *service, esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    gatt_server_conn_t *conn;
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        ESP_LOGD(TAG, "REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);
        ESP_ERROR_CHECK(gatt_server_update_adv_data(server));
        ESP_ERROR_CHECK(gatt_server_add_service_at(server, gatts_if, param->reg.app_id));
        break;
    case ESP_GATTS_READ_EVT:
        ESP_LOGD(TAG, "GATT_READ_EVT, conn_id %d, trans_id %d, handle %d\n", param->read.conn_id, param->read.trans_id, param->read.handle);
        ESP_ERROR_CHECK(gatt_server_service_read_handler(server, service, gatts_if, &param->read));
        break;
    case ESP_GATTS_WRITE_EVT:
        ESP_LOGD(TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d (prep: %s, rsp: %s)",
                 param->write.conn_id, param->write.trans_id, param->write.handle,
                 param->write.is_prep ? "Y" : "N",
                 param->write.need_rsp ? "Y" : "N");
        ESP_LOGD(TAG, "GATT_WRITE_EVT, value len %d, value :", param->write.len);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, param->write.value, param->write.len, ESP_LOG_DEBUG);
        ESP_ERROR_CHECK(gatt_server_service_write_handler(server, service, gatts_if, &param->write));
        break;
    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_EXEC_WRITE_EVT, conn_id %d, trans_id %d (exec: %s)",
                 param->exec_write.conn_id, param->exec_write.trans_id,
                 param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC ? "Y" : "N");
        conn = gatt_server_get_conn(server, param->exec_write.conn_id);
        if (!conn)
        {
            break;
        }
        if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC)
        {
            gatt_server_write_info_t write_info = {
                .conn_id = param->exec_write.conn_id,
                .trans_id = param->exec_write.trans_id,
                .need_rsp = true,
                .data = conn->write_buf.data,
                .data_len = conn->write_buf.pos,
            };
            memcpy(write_info.bda, param->exec_write.bda, sizeof(write_info.bda));
            ESP_ERROR_CHECK(gatt_server_service_write_data_handler(server, service, gatts_if, &write_info, conn->write_buf.handle));
        }
        else
        {
            ESP_ERROR_CHECK(esp_ble_gatts_send_response(gatts_if, param->exec_write.conn_id, param->exec_write.trans_id, ESP_GATT_OK, NULL));
        }
        conn->write_buf.pos = 0;
        conn->write_buf.handle = GATT_SERVER_HANDLE_INVALID;
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGD(TAG, "CREATE_SERVICE_EVT, status %d,  service_handle %d\n", param->create.status, param->create.service_handle);
        service->state.handle = param->create.service_handle;
        esp_ble_gatts_start_service(service->state.handle);

        // This will add the first characteristic (if any). Then we continue
        // adding its descriptors in the ESP_GATTS_ADD_CHAR_DESCR_EVT handler.
        ESP_ERROR_CHECK(gatt_server_add_next_char(server, service));
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
    {
        ESP_LOGD(TAG, "ADD_CHAR_EVT, status %d, attr_handle %d, service_handle %d\n",
                 param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
        gatt_server_char_t *characteristic = &service->chars[service->state.current_char];
        characteristic->state.handle = param->add_char.attr_handle;
#if 0
        uint16_t length = 0;
        const uint8_t *prf_char;
        esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle, &length, &prf_char);
        if (get_attr_ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "ILLEGAL HANDLE");
        }

        ESP_LOGI(TAG, "the gatts demo char length = %x", length);
        for (int i = 0; i < length; i++)
        {
            ESP_LOGI(TAG, "prf_char[%x] = %x", i, prf_char[i]);
        }
#endif

        if (!gatt_server_register_next_descriptor(server, service, characteristic))
        {
            ESP_ERROR_CHECK(gatt_server_add_next_entry(server, service));
        }
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
    {
        ESP_LOGD(TAG, "ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d\n",
                 param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
        gatt_server_char_t *chr = &service->chars[service->state.current_char];
        if (gatt_server_char_needs_client_conf_desc(chr) && chr->state.client_conf_handle == GATT_SERVER_HANDLE_INVALID)
        {
            chr->state.client_conf_handle = param->add_char_descr.attr_handle;
        }
        else if (gatt_server_char_needs_description_desc(chr) && chr->state.description_handle == GATT_SERVER_HANDLE_INVALID)
        {
            chr->state.description_handle = param->add_char_descr.attr_handle;
        }
        else if (gatt_server_char_needs_presentation_desc(chr) && chr->state.presentation_handle == GATT_SERVER_HANDLE_INVALID)
        {
            chr->state.presentation_handle = param->add_char_descr.attr_handle;
        }
        if (!gatt_server_register_next_descriptor(server, service, chr))
        {
            ESP_ERROR_CHECK(gatt_server_add_next_entry(server, service));
        }
        break;
    }
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGD(TAG, "SERVICE_START_EVT, status %d, service_handle %d\n",
                 param->start.status, param->start.service_handle);
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONF_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_CONF_EVT, status %d", param->conf.status);
        if (param->conf.status != ESP_GATT_OK)
        {
            esp_log_buffer_hex(TAG, param->conf.value, param->conf.len);
        }
        break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}

static bool gatt_server_gatts_event_server_handler(gatt_server_t *server, esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    esp_ble_conn_update_params_t conn_params;
    bool is_full;
    switch (event)
    {
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_CONNECT_EVT, conn_id %u, remote " ESP_BD_ADDR_STR,
                 param->connect.conn_id, ESP_BD_ADDR_HEX(param->connect.remote_bda));
        // The bluetooth stack will produce one ESP_GATTS_CONNECT_EVT per service
        // (maybe per app since we're using one app for each service?), so we need
        // to keep track of the active connections ourselves.
        if (gatt_server_register_conn(server, &param->connect, &is_full))
        {
            ESP_LOGD(TAG, "REGISTER, conn_id %u, remote " ESP_BD_ADDR_STR,
                     param->connect.conn_id, ESP_BD_ADDR_HEX(param->connect.remote_bda));
            if (is_full)
            {
                // Max number of connections reached
                ESP_ERROR_CHECK(esp_ble_gap_disconnect(param->connect.remote_bda));
                break;
            }
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x20; // max_int = 0x20*1.25ms = 40ms
            conn_params.min_int = 0x10; // min_int = 0x10*1.25ms = 20ms
            conn_params.timeout = 400;  // timeout = 400*10ms = 4000ms
            // start sent the update connection parameters to the peer device.
            ESP_ERROR_CHECK(esp_ble_gap_update_conn_params(&conn_params));
        }
        server->state.is_advertising = false;
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_DISCONNECT_EVT, conn_id %u, remote " ESP_BD_ADDR_STR,
                 param->disconnect.conn_id, ESP_BD_ADDR_HEX(param->disconnect.remote_bda));
        gatt_server_remove_conn(server, &param->disconnect);
        gatt_server_start_advertising(server);
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        break;
    default:
        return false;
    }
    return true;
}

void gatt_server_gatts_event_handler(gatt_server_t *server, esp_gatts_cb_event_t event, esp_gatt_if_t gatt_if, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGD(TAG, "GATTS event %d, gatt_if = %d", event, gatt_if);
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            ESP_LOGD(TAG, "app_id %d => gatt_if = %x", param->reg.app_id, gatt_if);
            server->services[param->reg.app_id].state.gatt_if = gatt_if;
        }
        else
        {
            ESP_LOGD(TAG, "Reg app failed, app_id %04x, status %d\n",
                     param->reg.app_id,
                     param->reg.status);
            return;
        }
    }

    if (!gatt_server_gatts_event_server_handler(server, event, gatt_if, param))
    {
        // Service event, call specific handler
        assert(gatt_if != ESP_GATT_IF_NONE);
        for (int ii = 0; ii < server->services_len; ii++)
        {
            if (gatt_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                gatt_if == server->services[ii].state.gatt_if)
            {
                gatt_server_gatts_event_service_handler(server, &server->services[ii], event, gatt_if, param);
            }
        }
    }
}

esp_err_t gatt_server_notify_char(gatt_server_t *server, gatt_server_char_t *chr, const void *data, size_t size)
{
    // Find the service for this characteristic
    gatt_server_service_t *service = NULL;
    for (int ii = 0; ii < server->services_len; ii++)
    {
        gatt_server_service_t *srv = &server->services[ii];
        for (int jj = 0; jj < srv->chars_len; jj++)
        {
            if (&srv->chars[jj] == chr)
            {
                service = srv;
                break;
            }
        }
    }
    if (!service)
    {
        return ESP_ERR_NOT_FOUND;
    }
    uint16_t subscribers[GATT_SERVER_MAX_CONNECTIONS];
    size_t count;
    gatt_server_get_subscribers(server, service, chr, subscribers, &count);
    for (int ii = 0; ii < count; ii++)
    {
        esp_err_t ret = esp_ble_gatts_send_indicate(service->state.gatt_if, subscribers[ii], chr->state.handle, size, (uint8_t *)data, false);
        if (ret != ESP_OK)
        {
            return ret;
        }
    }
    return ESP_OK;
}
