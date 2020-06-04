#include <string.h>

#include <hal/log.h>

#include "util/macros.h"

#include "smartport.h"

#define SMARTPORT_POLL_INTERVAL MILLIS_TO_TICKS(11)
#define SMARTPORT_DATA_FRAME_ID 0x10

#define SMARTPORT_MSP_VERSION 1
#define SMARTPORT_MSP_SENSOR_ID 0x0D
#define SMARTPORT_MSP_CLIENT_FRAME_ID 0x30
#define SMARTPORT_MSP_SERVER_FRAME_ID 0x32
#define SMARTPORT_MSP_PAYLOAD_CHUNK_SIZE 6

static const char *TAG = "S.Port";

/* SmartPort works by polling sensors by their sensor ID at regular intervals (approximately 11ms).
   Polling starts by writing SMARTPORT_START_STOP, <sensor_id> to the wire.
   Note that not every sensor ID is valid, there are 28 valid ones enumerated
   below. If the sensor is present, it will reply with one packet described
   by smartport_payload_t. Note that a given sensor might reply with a different
   packet type on every request.

   When polling without any sensors found, the master will just poll for one
   sensor id at a time. When any sensors are found, the master will poll once
   from the found list and once from the not-found list.

   For example, betaflight and inav only reply to the 0x1B sensor, but send
   different packet ids on every poll.
 */

static uint8_t smartport_sensor_ids[] = {
    0x00, // 01: Vari-H (altimeter high precision)
    0xA1, // 02: FLVSS / MLVSS (LiPo)
    0x22, // 03: FAS (current)
    0x83, // 04: GPS / Vari-N (altimeter normal precision)
    0xE4, // 05: RPM
    0x45, // 06: SP2UH
    0xC6, // 07: SP2UR
    0x67, // 08: -
    0x48, // 09: -
    0xE9, // 10: ASS (air speed)
    0x6A, // 11: -
    0xCB, // 12: -
    0xAC, // 13: -
    0x0D, // 14: -
    0x8E, // 15: -
    0x2F, // 16: -
    0xD0, // 17: -
    0x71, // 18: -
    0xF2, // 19: -
    0x53, // 20: -
    0x34, // 21: -
    0x95, // 22: -
    0x16, // 23: -
    0xB7, // 24: -
    0x98, // 25: RX / TX internal telemetry
    0x39, // 26: PowerBox (aka Redudancy Bus)
    0xBA, // 27: -
    0x1B, // 28: Used by betaflight and inav
};

_Static_assert(sizeof(smartport_sensor_ids) == SMARTPORT_SENSOR_ID_COUNT, "invalid sport sensor id map");

// Sent to poll a sensor
typedef struct smartport_sensor_req_s
{
    uint8_t start_stop; // must be SMARTPORT_START_STOP
    uint8_t sensor_id;  // one of the smartport_sensor_ids values
} PACKED smartport_sensor_req_t;

// MSP via S.Port requests are sent in chunks enclosed in a valid
// smartport_payload_t with the following structure. It must be sent
// followed by the CRC and using appropriate escaping
// for SMARTPORT_START_STOP and SMARTPORT_BYTE_STUFF.
typedef struct smartport_msp_req_chunk_s
{
    uint8_t start_stop;                             // must be SMARTPORT_START_STOP
    uint8_t sensor_id;                              // must be SMARTPORT_MSP_SENSOR_ID
    uint8_t frame_id;                               // SMARTPORT_MSP_CLIENT_FRAME_ID
    uint8_t data[SMARTPORT_MSP_PAYLOAD_CHUNK_SIZE]; // 0 padded
} PACKED smartport_msp_req_chunk_t;

// Check size. Without start_stop and sensor_id it should be the same bytes as
// smartport_payload_t
_Static_assert(sizeof(smartport_msp_req_chunk_t) - 2 == sizeof(smartport_payload_t), "invalid smartport_msp_req_chunk_t size");

// MSP over S.Port reply. Note that there's no MSP code, so we need to
// store it ourselves.
typedef struct smartport_msp_reply_chunk_s
{
    uint8_t frame_id;                               // must be SMARTPORT_MSP_SERVER_FRAME_ID
    uint8_t data[SMARTPORT_MSP_PAYLOAD_CHUNK_SIZE]; // 0 padded
} PACKED smartport_msp_reply_chunk_t;

_Static_assert(sizeof(smartport_msp_reply_chunk_t) == sizeof(smartport_payload_t), "invalid smartport_msp_reply_chunk_t size");

typedef enum
{
    FSSP_DATAID_SPEED = 0x0830,    // BF, INAV: GPS speed in knots/1000 (1cm/s = 0.0194384449 knots) (uint32_t)
    FSSP_DATAID_VFAS = 0x0210,     // BF, INAV: might be total voltage or cell voltage. 0.1V per unit (uint16_t)
    FSSP_DATAID_CURRENT = 0x0200,  // BF, INAV: Current amp draw given in 10mA steps (int32_t)
    FSSP_DATAID_ALTITUDE = 0x0100, // BF, INAV: Altitude in cm (int32_t)
    FSSP_DATAID_FUEL = 0x0600,     // BF: Always mah drawn (int32_t), INAV: Might be like BF or might be percentage (uint8_t) if smartport_fuel_percent = true
    FSSP_DATAID_LATLONG = 0x0800,  // BF: might be LAT or LON, see decoding code (uint32_t)
    FSSP_DATAID_VARIO = 0x0110,    // BF, INAV: Vertical speed in cm/s
    FSSP_DATAID_HEADING = 0x0840,  // BF, INAV: Heading in 10000 = 100 deg
    FSSP_DATAID_ACCX = 0x0700,     // BF, INAV: Acceleration in X axis, 0.01G (int32_t)
    FSSP_DATAID_ACCY = 0x0710,     // Same as ACCX for Y axis
    FSSP_DATAID_ACCZ = 0x0720,     // Same as ACCX for Z axis
    FSSP_DATAID_T1 = 0x0400,       // BF, INAV: Flight mode codified as bits (int32_t)
    FSSP_DATAID_T2 = 0x0410,       // BF: GPS fix status fix => +1000, home +2000, +num_sats, INAV: sats, hdop and fix, see decoding (uint32_t)
    FSSP_DATAID_GPS_ALT = 0x0820,  // BF: Altitude in mm, INAV: Altitude in cm (int32_t)
    FSSP_DATAID_A4 = 0x0910,       // BF, INAV: Cell voltage in 0.01V (uint16_t)
} smartport_value_id_e;

static uint8_t smartport_payload_checksum(smartport_payload_t *payload)
{
    uint16_t checksum = 0;
    const uint8_t *ptr = (uint8_t *)payload;
    for (size_t ii = 0; ii < sizeof(*payload); ii++)
    {
        checksum += ptr[ii];
    }
    return 0xff - ((checksum & 0xff) + (checksum >> 8));
}

static void smartport_payload_frame_init(smartport_payload_frame_t *frame)
{
    frame->state = SMARTPORT_PAYLOAD_FRAME_STATE_INCOMPLETE;
    frame->pos = 0;
}

static void smartport_payload_frame_append(smartport_payload_frame_t *frame, uint8_t c)
{
    uint8_t checksum;
    switch (frame->state)
    {
    case SMARTPORT_PAYLOAD_FRAME_STATE_INCOMPLETE:
        if (c == SMARTPORT_START_STOP || c == SMARTPORT_BYTE_STUFF)
        {
            frame->state = SMARTPORT_PAYLOAD_FRAME_STATE_BYTESTUFF;
            return;
        }
        break;
    case SMARTPORT_PAYLOAD_FRAME_STATE_BYTESTUFF:
        c ^= SMARTPORT_XOR;
        frame->state = SMARTPORT_PAYLOAD_FRAME_STATE_INCOMPLETE;
        break;
    case SMARTPORT_PAYLOAD_FRAME_STATE_CHECKSUM:
        checksum = smartport_payload_checksum(&frame->payload);
        if (checksum == c)
        {
            frame->state = SMARTPORT_PAYLOAD_FRAME_STATE_COMPLETE;
        }
        else
        {
            LOG_W(TAG, "Invalid checksum: expect 0x%02x got 0x%02x", checksum, c);
            frame->state = SMARTPORT_PAYLOAD_FRAME_STATE_INVALID;
        }
        return;
    case SMARTPORT_PAYLOAD_FRAME_STATE_INVALID:
    case SMARTPORT_PAYLOAD_FRAME_STATE_COMPLETE:
        // Nothing to do since this function won't be called in these
        // states. Just listed in this switch for completion
        LOG_E(TAG, "Invalid S.Port frame state");
        break;
    }
    frame->bytes[frame->pos++] = c;
    if (frame->pos == sizeof(smartport_payload_t))
    {
        frame->state = SMARTPORT_PAYLOAD_FRAME_STATE_CHECKSUM;
    }
}

static bool smartport_master_decode_data_payload(smartport_master_t *sp, const smartport_payload_t *payload)
{
    uint32_t data = payload->data;
    telemetry_val_t v;
    telemetry_downlink_id_e id;
    switch (payload->value_id)
    {
    case FSSP_DATAID_SPEED:
        // Comes in knots/1000
        id = TELEMETRY_ID_GPS_SPEED;
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT16);
        v.u16 = (data * 100) / 1944;
        break;
    case FSSP_DATAID_VFAS:
        // Comes in 0.1V, we want 0.01V
        // TODO: Might be cell voltage on iNav
        id = TELEMETRY_ID_BAT_VOLTAGE;
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT16);
        v.u16 = data * 10;
        break;
    case FSSP_DATAID_CURRENT:
        id = TELEMETRY_ID_CURRENT;
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT16);
        v.i16 = ((int32_t)data) * 10;
        break;
    case FSSP_DATAID_ALTITUDE:
        id = TELEMETRY_ID_ALTITUDE;
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT32);
        v.i32 = (int32_t)data;
        break;
    case FSSP_DATAID_FUEL:
        // TODO: Might be battery percentage
        id = TELEMETRY_ID_CURRENT_DRAWN;
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT32);
        v.i32 = (int32_t)data;
        break;
    case FSSP_DATAID_VARIO:
        id = TELEMETRY_ID_VERTICAL_SPEED;
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT16);
        v.i16 = (int32_t)data;
        break;
    case FSSP_DATAID_HEADING:
        id = TELEMETRY_ID_HEADING;
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT16);
        v.u16 = (data / 10) + 180;
        break;
    case FSSP_DATAID_ACCX:
        id = TELEMETRY_ID_ACC_X;
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT32);
        v.i32 = (int32_t)data;
        break;
    case FSSP_DATAID_ACCY:
        id = TELEMETRY_ID_ACC_Y;
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT32);
        v.i32 = (int32_t)data;
        break;
    case FSSP_DATAID_ACCZ:
        id = TELEMETRY_ID_ACC_Z;
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT32);
        v.i32 = (int32_t)data;
        break;
    case FSSP_DATAID_A4:
        id = TELEMETRY_ID_AVG_CELL_VOLTAGE;
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT16);
        v.u16 = data;
        break;
    default:
        LOG_W(TAG, "Unknown S.Port value ID 0x%04x", payload->value_id);
        return false;
    }
    sp->telemetry_found(sp->telemetry_data, id, &v);
    return true;
}

static bool smartport_master_decode_msp_server_payload(smartport_master_t *sp, const smartport_payload_t *payload)
{
    const smartport_msp_reply_chunk_t *chunk = (smartport_msp_reply_chunk_t *)payload;
    return msp_telemetry_push_response_chunk(&sp->msp_telemetry, chunk->data, SMARTPORT_MSP_PAYLOAD_CHUNK_SIZE);
}

static bool smartport_master_decode(smartport_master_t *sp)
{
    smartport_payload_t *payload = &sp->frame.payload;
    return smartport_master_decode_payload(sp, payload);
}

static bool smartport_master_read_payload(smartport_master_t *sp)
{
    // Duplicate the size because SMARTPORT_START_STOP and SMARTPORT_BYTE_STUFF
    // get escaped to two bytes when they appear in the payload.
    uint8_t buf[sizeof(smartport_payload_t) * 2];
    int n = io_read(&sp->io, buf, sizeof(buf), 0);
    if (n <= 0)
    {
        return false;
    }
    LOG_D(TAG, "Got %d bytes", n);
    LOG_BUFFER_D(TAG, buf, n);
    // Unescape
    for (int ii = 0; ii < n; ii++)
    {
        smartport_payload_frame_append(&sp->frame, buf[ii]);
    }
    if (sp->frame.state == SMARTPORT_PAYLOAD_FRAME_STATE_COMPLETE)
    {
        LOG_D(TAG, "Got S.Port payload, value ID 0x%04x", sp->frame.payload.value_id);
        smartport_master_decode(sp);
        if (!sp->last_poll_from_found && !sp->found[sp->last_polled])
        {
            sp->found[sp->last_polled] = true;
            sp->found_count++;
            sp->last_poll_from_found = true;
        }
        return true;
    }
    return false;
}

static void smartport_master_poll(smartport_master_t *sp)
{
    uint8_t sensor_id;
    uint8_t sensor_pos;
    if (sp->found_count < SMARTPORT_SENSOR_ID_COUNT && (sp->last_poll_from_found || sp->found_count == 0))
    {
        // Either no sensors found or last poll was from found. We poll
        // from the non-found list.
        sensor_pos = sp->last_polled + 1;
        if (sensor_pos >= SMARTPORT_SENSOR_ID_COUNT)
        {
            sensor_pos = 0;
        }
        // Since found_count is < SMARTPORT_SENSOR_ID_COUNT, we're guaranteed
        // to find at least one sensor to poll.
        while (sp->found[sensor_pos])
        {
            sensor_pos++;
            if (sensor_pos >= SMARTPORT_SENSOR_ID_COUNT)
            {
                sensor_pos = 0;
            }
        }
        sensor_id = smartport_sensor_ids[sensor_pos];
        sp->last_polled = sensor_pos;
        sp->last_poll_from_found = false;
    }
    else
    {
        // Poll from found sensors, which must be greater than one
        sensor_pos = sp->last_found_polled + 1;
        if (sensor_pos >= SMARTPORT_SENSOR_ID_COUNT)
        {
            sensor_pos = 0;
        }
        while (!sp->found[sensor_pos])
        {
            sensor_pos++;
            if (sensor_pos >= SMARTPORT_SENSOR_ID_COUNT)
            {
                sensor_pos = 0;
            }
        }
        sensor_id = smartport_sensor_ids[sensor_pos];
        sp->last_found_polled = sensor_pos;
        sp->last_poll_from_found = true;
    }
    smartport_sensor_req_t req = {
        .start_stop = SMARTPORT_START_STOP,
        .sensor_id = sensor_id,
    };
    LOG_D(TAG, "Will poll sensor id 0x%X", sensor_id);
    io_write(&sp->io, &req, sizeof(req));
}

static int smartport_master_msp_write_chunk(smartport_master_t *sp, smartport_msp_req_chunk_t *chunk, size_t size)
{
    chunk->start_stop = SMARTPORT_START_STOP;
    chunk->sensor_id = SMARTPORT_MSP_SENSOR_ID;
    chunk->frame_id = SMARTPORT_MSP_CLIENT_FRAME_ID;

    if (size < SMARTPORT_MSP_PAYLOAD_CHUNK_SIZE)
    {
        size_t rem = SMARTPORT_MSP_PAYLOAD_CHUNK_SIZE - size;
        memset(&chunk->data[size], 0, rem);
    }

    uint16_t checksum = 0;
    const uint8_t *ptr = (uint8_t *)chunk;
    uint8_t buf[sizeof(*chunk) * 2];
    buf[0] = *ptr++;
    buf[1] = *ptr++;
    int buf_pos = 2;
    for (size_t ii = 0; ii < sizeof(smartport_msp_req_chunk_t) - 2; ii++)
    {
        uint8_t c = ptr[ii];
        checksum += c;
        checksum += checksum >> 8;
        checksum &= 0x00FF;
        if (c == SMARTPORT_START_STOP || c == SMARTPORT_BYTE_STUFF)
        {
            buf[buf_pos++] = SMARTPORT_BYTE_STUFF;
            c ^= SMARTPORT_XOR;
        }
        buf[buf_pos++] = c;
    }
    uint8_t c = 0xff - checksum;
    buf[buf_pos++] = c;
    LOG_D(TAG, "Will send MSP req (%d bytes)", buf_pos);
    LOG_BUFFER_D(TAG, buf, buf_pos);
    return io_write(&sp->io, buf, buf_pos);
}

void smartport_master_init(smartport_master_t *sp, io_t *io)
{
    memset(sp, 0, sizeof(*sp));
    sp->io = *io;
    msp_telemetry_init_output(&sp->msp_telemetry, SMARTPORT_MSP_PAYLOAD_CHUNK_SIZE);
}

void smartport_master_update(smartport_master_t *sp)
{
    time_ticks_t now = time_ticks_now();
    smartport_msp_req_chunk_t chunk;
    size_t chunk_size;
    // If we found a payload, go into send mode again
    if (smartport_master_read_payload(sp) || sp->next_poll < now)
    {
        smartport_payload_frame_init(&sp->frame);
        // Check if we have some queued S.port payloads to send
        // e.g. MSP.
        if ((chunk_size = msp_telemetry_pop_request_chunk(&sp->msp_telemetry, chunk.data)) > 0)
        {
            smartport_master_msp_write_chunk(sp, &chunk, chunk_size);
        }
        else
        {
            smartport_master_poll(sp);
        }
        sp->next_poll = now + SMARTPORT_POLL_INTERVAL;
    }
}

bool smartport_master_decode_payload(smartport_master_t *sp, const smartport_payload_t *payload)
{
    switch (payload->frame_id)
    {
    case SMARTPORT_DATA_FRAME_ID:
        return smartport_master_decode_data_payload(sp, payload);
    case SMARTPORT_MSP_SERVER_FRAME_ID:
        return smartport_master_decode_msp_server_payload(sp, payload);
    }
    LOG_W(TAG, "Unknown frame ID 0x%x", payload->frame_id);
    return false;
}

smartport_payload_t *smartport_master_get_last_payload(smartport_master_t *sp)
{
    return sp->frame.state == SMARTPORT_PAYLOAD_FRAME_STATE_COMPLETE ? &sp->frame.payload : NULL;
}
