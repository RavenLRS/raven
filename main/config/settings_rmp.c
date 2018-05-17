#include <hal/log.h>

#include "config/config.h"

#include "rmp/rmp.h"

#include "util/stringutil.h"

#include "settings_rmp.h"

static const char *TAG = "Settings.RMP";

static bool settings_rmp_requires_auth(void)
{
    return true;
}

static void settings_rmp_send_setting(rmp_t *rmp, rmp_req_t *req, settings_rmp_msg_t *resp, settings_view_t *view, const setting_t *setting, settings_rmp_code_e code)
{
    assert(code == SETTINGS_RMP_READ || code == SETTINGS_RMP_WRITE);

    char sbuf[SETTING_NAME_BUFFER_SIZE];

    int payload_pos = 0;
    resp->code = code;
    resp->setting.parent_index = settings_view_get_parent_index(view, setting);
    resp->setting.type = setting->type;
    resp->setting.flags = setting->flags;
    switch (setting->type)
    {
    case SETTING_TYPE_U8:
        resp->setting.payload[0] = setting_get_u8(setting);
        resp->setting.payload[1] = setting_get_min(setting);
        resp->setting.payload[2] = setting_get_max(setting);
        resp->setting.payload[3] = setting_get_default(setting);
        payload_pos = 4;
        break;
    case SETTING_TYPE_STRING:
        // Use setting_format_value() rather than setting_get_string(), since the
        // latter doesn't handle DYNAMIC settings.
        setting_format_value(sbuf, sizeof(sbuf), setting);
        payload_pos = strput((char *)resp->setting.payload, sbuf, sizeof(resp->setting.payload));
        resp->setting.payload[payload_pos++] = SETTING_STRING_MAX_LENGTH;
        break;
    case SETTING_TYPE_FOLDER:
    {
        uint16_t fid = setting_get_folder_id(setting);
        memcpy(resp->setting.payload, &fid, sizeof(fid));
        payload_pos = 2;
        break;
    }
    }

    setting_format_name(sbuf, sizeof(sbuf), setting);

    payload_pos += strput((char *)&resp->setting.payload[payload_pos], sbuf, sizeof(resp->setting.payload) - payload_pos);
    if (setting->flags & SETTING_FLAG_NAME_MAP)
    {
        for (int ii = setting_get_min(setting); ii <= setting_get_max(setting); ii++)
        {
            const char *val_name = setting_map_name(setting, ii);
            payload_pos += strput((char *)&resp->setting.payload[payload_pos], val_name, sizeof(resp->setting.payload) - payload_pos);
        }
        resp->setting.payload[payload_pos] = '\0';
    }
#if !defined(NDEBUG)
    if (payload_pos >= SETTING_RMP_SETTING_MAX_PAYLOAD_SIZE)
    {
        LOG_E(TAG, "SETTING_RMP_SETTING_MAX_PAYLOAD_SIZE needs to be increased to %d", payload_pos + 1);
        ASSERT(payload_pos < SETTING_RMP_SETTING_MAX_PAYLOAD_SIZE);
    }
#endif
    req->resp(req->resp_data, resp, settings_rmp_msg_size(resp));
}

static void settings_rmp_handler(rmp_t *rmp, rmp_req_t *req, void *user_data)
{
    if (settings_rmp_msg_is_valid(req->msg))
    {
        settings_rmp_msg_t resp;
        const settings_rmp_msg_t *msg = req->msg->payload;
        settings_view_t view;
        const setting_t *setting;
        switch ((settings_rmp_code_e)msg->code)
        {
        case SETTINGS_RMP_HELO:
            // Send a EHLO back only if we can authenticate this peer
            // or no auth is required.
            // Otherwise it won't be able to retrieve the settings
            if (settings_rmp_requires_auth() && !rmp_can_authenticate_peer(rmp, &req->msg->src))
            {
                break;
            }
            LOG_D(TAG, "HELO request: view id %d, folder %d, recusive %d",
                  (int)msg->helo.view.id, (int)msg->helo.view.folder_id,
                  (int)msg->helo.view.recursive);
            settings_view_get_folder_view(&view, msg->helo.view.id, msg->helo.view.folder_id,
                                          msg->helo.view.recursive);
            resp.code = SETTINGS_RMP_EHLO;
            resp.ehlo.view = msg->helo.view;
            resp.ehlo.is_visible = settings_is_folder_visible(msg->helo.view.id, msg->helo.view.folder_id);
            resp.ehlo.settings_count = view.count;
            rmp_get_name(rmp, resp.ehlo.name, sizeof(resp.ehlo.name));
            req->resp(req->resp_data, &resp, settings_rmp_msg_size(&resp));
            break;
        case SETTINGS_RMP_READ_REQ:
            if (settings_rmp_requires_auth() && !req->is_authenticated)
            {
                break;
            }

            settings_view_get_folder_view(&view, msg->read_req.view.id, msg->read_req.view.folder_id,
                                          msg->read_req.view.recursive);
            if (msg->read_req.setting_index >= view.count)
            {
                break;
            }
            resp.setting.view = msg->read_req.view;
            resp.setting.setting_index = msg->read_req.setting_index;
            setting = settings_view_get_setting_at(&view, msg->read_req.setting_index);
            settings_rmp_send_setting(rmp, req, &resp, &view, setting, SETTINGS_RMP_READ);
            break;
        case SETTINGS_RMP_WRITE_REQ:
            if (settings_rmp_requires_auth() && !req->is_authenticated)
            {
                break;
            }

            settings_view_get_folder_view(&view, msg->write_req.view.id, msg->write_req.view.folder_id,
                                          msg->write_req.view.recursive);
            if (msg->write_req.setting_index >= view.count)
            {
                break;
            }
            resp.setting.view = msg->write_req.view;
            resp.setting.setting_index = msg->write_req.setting_index;
            setting = settings_view_get_setting_at(&view, msg->write_req.setting_index);
            switch (setting->type)
            {
            case SETTING_TYPE_U8:
                setting_set_u8(setting, msg->write_req.payload[0]);
                break;
            case SETTING_TYPE_STRING:
                setting_set_string(setting, (const char *)msg->write_req.payload);
                break;
            case SETTING_TYPE_FOLDER:
                break;
            }
            settings_rmp_send_setting(rmp, req, &resp, &view, setting, SETTINGS_RMP_WRITE);
            break;
        // Responses, not handled here
        case SETTINGS_RMP_EHLO:
        case SETTINGS_RMP_READ:
        case SETTINGS_RMP_WRITE:
            break;
        }
    }
}

void settings_rmp_init(rmp_t *rmp)
{
    rmp_open_port(rmp, RMP_PORT_SETTINGS, settings_rmp_handler, NULL);
}

bool settings_rmp_msg_is_valid(const rmp_msg_t *msg)
{
    return msg->payload && msg->payload_size > 0 && settings_rmp_msg_size(msg->payload) == msg->payload_size;
}

size_t settings_rmp_msg_size(const settings_rmp_msg_t *msg)
{
    switch ((settings_rmp_code_e)msg->code)
    {
    case SETTINGS_RMP_HELO:
        return 1 + sizeof(settings_rmp_helo_t);
    case SETTINGS_RMP_EHLO:
        for (int ii = 0; ii < sizeof(msg->ehlo.name); ii++)
        {
            if (msg->ehlo.name[ii] == '\0')
            {
                return 1 + sizeof(settings_rmp_ehlo_t) - sizeof(msg->ehlo.name) + ii + 1;
            }
        }
        // No null found, invalid
        return 0;
    case SETTINGS_RMP_READ_REQ:
        return 1 + sizeof(settings_rmp_read_req_t);
    case SETTINGS_RMP_WRITE_REQ:
        // TODO: Optimize
        return 1 + sizeof(settings_rmp_write_req_t);
    case SETTINGS_RMP_READ:
    case SETTINGS_RMP_WRITE:
        // TODO: Optimize
        return 1 + sizeof(settings_rmp_setting_t);
    }
    return 0;
}

int32_t settings_rmp_setting_get_value(const settings_rmp_setting_t *s)
{
    switch ((setting_type_e)s->type)
    {
    case SETTING_TYPE_U8:
        return s->payload[0];
    case SETTING_TYPE_STRING:
        break;
    case SETTING_TYPE_FOLDER:
        return (s->payload[1] << 8 | s->payload[0]);
        break;
    }
    return 0;
}

bool settings_rmp_setting_set_value(settings_rmp_setting_t *s, int32_t val)
{
    switch ((setting_type_e)s->type)
    {
    case SETTING_TYPE_U8:
        s->payload[0] = val;
        return true;
    case SETTING_TYPE_STRING:
    case SETTING_TYPE_FOLDER:
        break;
    }
    return false;
}

int32_t settings_rmp_setting_get_min(const settings_rmp_setting_t *s)
{
    switch ((setting_type_e)s->type)
    {
    case SETTING_TYPE_U8:
        return s->payload[1];
    case SETTING_TYPE_STRING:
        break;
    case SETTING_TYPE_FOLDER:
        break;
    }
    return 0;
}

int32_t settings_rmp_setting_get_max(const settings_rmp_setting_t *s)
{
    switch ((setting_type_e)s->type)
    {
    case SETTING_TYPE_U8:
        return s->payload[2];
    case SETTING_TYPE_STRING:
        break;
    case SETTING_TYPE_FOLDER:
        break;
    }
    return 0;
}

const char *settings_rmp_setting_get_str_value(const settings_rmp_setting_t *s)
{
    if (s->type == SETTING_TYPE_STRING)
    {
        return (const char *)s->payload;
    }
    return NULL;
}

bool settings_rmp_setting_set_str_value(settings_rmp_setting_t *s, const char *val)
{
    if (s->type == SETTING_TYPE_STRING)
    {
        strlcpy((char *)s->payload, val, sizeof(s->payload));
    }
    return false;
}

unsigned settings_rmp_setting_get_str_max_length(const settings_rmp_setting_t *s)
{
    if (s->type == SETTING_TYPE_STRING)
    {
        return s->payload[strlen(settings_rmp_setting_get_str_value(s)) + 1];
    }
    return 0;
}

const char *settings_rmp_setting_get_name(const settings_rmp_setting_t *s)
{
    switch (s->type)
    {
    case SETTING_TYPE_U8:
        return (const char *)&s->payload[4];
    case SETTING_TYPE_STRING:
        return (const char *)&s->payload[strlen(settings_rmp_setting_get_str_value(s)) + 2];
    case SETTING_TYPE_FOLDER:
        return (const char *)&s->payload[2];
    }
    return NULL;
}

unsigned settings_rmp_setting_get_mapped_names_count(const settings_rmp_setting_t *s)
{
    if (s->flags & SETTING_FLAG_NAME_MAP)
    {
        // Assume the setting is well formed
        return settings_rmp_setting_get_max(s) - settings_rmp_setting_get_min(s) + 1;
    }
    return 0;
}

const char *settings_rmp_setting_get_mapped_name(const settings_rmp_setting_t *s, unsigned idx)
{
    // Start by getting the name and go to its end
    const char *name = settings_rmp_setting_get_name(s);
    const char *start = name + strlen(name) + 1;
    const char *end = (const char *)&s->payload[sizeof(s->payload)];
    unsigned count = 0;
    for (const char *p = start; p < end - 1; p++)
    {
        if (*p == '\0')
        {
            if (count == idx)
            {
                return start;
            }
            // Check if we have another null after this one
            if (*(p + 1) == '\0')
            {
                // End of mapped names. No more follow.
                break;
            }
            // Move start to the character after this one
            start = p + 1;
            count++;
        }
    }
    return NULL;
}

setting_cmd_flag_e settings_rmp_setting_cmd_get_flags(const settings_rmp_setting_t *s)
{
    if (s->flags & SETTING_FLAG_CMD && s->type == SETTING_TYPE_U8)
    {
        // CMD flags are stored in the default value
        return s->payload[3];
    }
    return 0;
}

bool settings_rmp_setting_cmd_set_exec(settings_rmp_setting_t *s)
{
    if (s->flags & SETTING_FLAG_CMD && s->type == SETTING_TYPE_U8)
    {
        return settings_rmp_setting_set_value(s, SETTING_CMD_STATUS_COMMIT);
    }
    return false;
}

static bool settings_rmp_setting_move(settings_rmp_setting_t *s, int delta)
{
    if ((s->flags & SETTING_FLAG_READONLY) == 0)
    {
        int32_t min = settings_rmp_setting_get_min(s);
        int32_t max = settings_rmp_setting_get_max(s);
        int32_t val = settings_rmp_setting_get_value(s);
        switch (s->type)
        {
        case SETTING_TYPE_U8:
            if (delta < 0 && val == min)
            {
                val = max;
            }
            else if (delta > 0 && val == max)
            {
                val = min;
            }
            else
            {
                val += delta;
            }
            s->payload[0] = val & 0xFF;
            return true;
        case SETTING_TYPE_STRING:
        case SETTING_TYPE_FOLDER:
            break;
        }
    }
    return false;
}

bool settings_rmp_setting_increment(settings_rmp_setting_t *s)
{
    return settings_rmp_setting_move(s, 1);
}

bool settings_rmp_setting_decrement(settings_rmp_setting_t *s)
{
    return settings_rmp_setting_move(s, -1);
}

void settings_rmp_setting_prepare_write(const settings_rmp_setting_t *s, settings_rmp_msg_t *msg)
{
    msg->code = SETTINGS_RMP_WRITE_REQ;
    msg->write_req.view = s->view;
    msg->write_req.setting_index = s->setting_index;
    memmove(msg->write_req.payload, s->payload, sizeof(msg->write_req.payload));
}
