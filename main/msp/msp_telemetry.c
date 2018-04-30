#include <string.h>

#include <hal/log.h>

#include "util/crc.h"
#include "util/macros.h"

#include "msp_telemetry.h"

#define MSP_TELEMETRY_MSP_VERSION 1
#define MSP_TELEMETRY_TIMEOUT MILLIS_TO_TICKS(1000)

#define MSP_TELEMETRY_MAX_CHUNK_DATA_SIZE(tr) (tr->max_size - 1)
// headers in msp_telemetry_req_chunk_t and msp_transport_resp_chunk_t take 1 byte

static const char *TAG = "MSP.Transport.Telemetry";

enum
{
    MSP_TELEMETRY_ERROR_VERSION_MISMATCH = 0,
    MSP_TELEMETRY_ERROR_VERSION_SEQ_ERROR = 1, // Invalid sequence numbers
    MSP_TELEMETRY_ERROR_VERSION_MSP_ERROR = 2, // Processing the request produced an MSP error
};

// An intermediate representation used to writes into the ring buffers
// for both output requests and responses.
typedef struct msp_telemetry_chunk_s
{
    bool start;
    uint8_t size;
    uint8_t data[];
} msp_telemetry_chunk_t;

// Used to piece together chunks received into whole MSP requests.
// CRC is at the end.
typedef struct msp_telemetry_blob_s
{
    uint8_t payload_size;
    uint8_t cmd;
    uint8_t data[];
} PACKED msp_telemetry_blob_t;

// Packed MSP for sending over telemetry.
//
// The MSP payload must omit the preamble and direction data (1st 3 bytes
// in MSPv1 "$M<"").
typedef struct msp_telemetry_req_chunk_s
{
    unsigned seq : 4;     // sequence number
    unsigned start : 1;   // 1 for the 1st chunk in a sequence, zero for the rest
    unsigned version : 3; // must be MSP_TRANSPORT_MSP_VERSION
    uint8_t data[];
} PACKED msp_telemetry_req_chunk_t;

// MSP over telemetry reply. Note that there's no MSP code, so we need to
// store it ourselves.
typedef struct msp_telemetry_resp_chunk_s
{
    unsigned seq : 4; // sequence number
    unsigned start : 1;
    unsigned error : 1;
    unsigned reserved1 : 2; // 2 bits reserved
    uint8_t data[];
} PACKED msp_telemetry_resp_chunk_t;

static bool msp_telemetry_in_use(msp_telemetry_t *tr)
{
    return tr->in_use_since > 0 && !(time_ticks_now() - tr->in_use_since > MSP_TELEMETRY_TIMEOUT);
}

static void msp_telemetry_init(msp_telemetry_t *tr, size_t max_size)
{
    assert(max_size > 0);
    RING_BUFFER_INIT(&tr->req, uint8_t, MSP_TELEMETRY_REQ_QUEUE_SIZE);
    RING_BUFFER_INIT(&tr->resp, uint8_t, MSP_TELEMETRY_RESP_QUEUE_SIZE);
    tr->max_size = max_size;
    tr->req_seq = 0;
    tr->resp_seq = 0;
}

static bool msp_telemetry_push_data(ring_buffer_t *rb, const void *data, size_t size)
{
    uint8_t *ptr = (uint8_t *)data;
    for (int ii = 0; ii < size; ii++, ptr++)
    {
        if (!ring_buffer_push(rb, ptr))
        {
            return false;
        }
    }
    return true;
}

static bool msp_telemetry_pop_data(ring_buffer_t *rb, void *data, size_t size)
{
    uint8_t *ptr = (uint8_t *)data;
    for (int ii = 0; ii < size; ii++, ptr++)
    {
        if (!ring_buffer_pop(rb, ptr))
        {
            return false;
        }
    }
    return true;
}

static bool msp_telemetry_push_chunk(ring_buffer_t *rb, msp_telemetry_chunk_t *chunk)
{
    return msp_telemetry_push_data(rb, chunk, sizeof(*chunk));
}

static bool msp_telemetry_next_chunk_starts(ring_buffer_t *rb)
{
    // This works because the 1st byte in msp_telemetry_chunk_t is the
    // start byte
    _Static_assert(offsetof(msp_telemetry_chunk_t, start) == 0, "msp_telemetry_chunk_t start byte offset not zero");
    bool start;
    return ring_buffer_peek(rb, &start) && start;
}

static bool msp_telemetry_pop_chunk(ring_buffer_t *rb, msp_telemetry_chunk_t *chunk)
{
    return msp_telemetry_pop_data(rb, chunk, sizeof(*chunk));
}

static bool msp_telemetry_push_blob(ring_buffer_t *rb, msp_telemetry_blob_t *blob)
{
    return msp_telemetry_push_data(rb, blob, sizeof(*blob));
}

static bool msp_telemetry_pop_blob(ring_buffer_t *rb, msp_telemetry_blob_t *blob)
{
    return msp_telemetry_pop_data(rb, blob, sizeof(*blob));
}

static int msp_telemetry_read(msp_telemetry_t *tr, ring_buffer_t *rb, uint16_t *cmd, void *payload, size_t size)
{
    if (tr->count <= 0)
    {
        return MSP_EOF;
    }
    tr->count--;

    msp_telemetry_blob_t blob;

    if (!msp_telemetry_pop_blob(rb, &blob))
    {
        LOG_E(TAG, "Could not pop blob");
        return MSP_EOF;
    }

    uint8_t ccrc = 0;

    uint8_t size8 = (uint8_t)blob.payload_size;
    ccrc = crc_xor(ccrc, size8);

    uint8_t cmd8 = (uint8_t)blob.cmd;
    ccrc = crc_xor(ccrc, cmd8);

    uint8_t b;
    unsigned ptr_pos = 0;
    uint8_t *ptr = payload;

    for (int ii = 0; ii < blob.payload_size; ii++, ptr_pos++)
    {
        if (!ring_buffer_pop(rb, &b))
        {
            LOG_E(TAG, "Error popping byte %d with payload_size = %u", ii, blob.payload_size);
            return MSP_EOF;
        }
        ccrc = crc_xor(ccrc, b);
        if (ptr_pos < size)
        {
            ptr[ptr_pos] = b;
        }
    }

    // Read the checksum
    uint8_t crc;
    if (!ring_buffer_pop(rb, &crc))
    {
        LOG_E(TAG, "Error popping CRC");
        return MSP_EOF;
    }

    if (crc != ccrc)
    {
        LOG_W(TAG, "Invalid CRC %u, expecting %u", crc, ccrc);
    }

    if (ptr_pos >= size)
    {
        return MSP_BUF_TOO_SMALL;
    }

    *cmd = blob.cmd;
    return blob.payload_size;
}

static int msp_telemetry_write(msp_telemetry_t *tr, ring_buffer_t *rb, msp_direction_e direction, uint16_t cmd, const void *payload, size_t size)
{
    // Messages to the FC need 3 additional bytes (size, cmd and crc),
    // while messages from the FC omit the code (however the crc must
    // include the cmd byte). Callers are supposed to keep a list of
    // sent messages to handle that.

    // Size byte + crc byte
    size_t total_size = size + 1 + 1;
    if (direction == MSP_DIRECTION_TO_MWC)
    {
        // + msp cmd byte
        total_size++;
    }
    size_t remaining = total_size;
    size_t max_data_size = MSP_TELEMETRY_MAX_CHUNK_DATA_SIZE(tr);
    size_t chunk_size = MIN(remaining, max_data_size);
    msp_telemetry_chunk_t chunk = {
        .start = true,
        .size = chunk_size,
    };
    if (!msp_telemetry_push_chunk(rb, &chunk))
    {
        return -1;
    }
    uint8_t crc = 0;
    // Push size and cmd bytes
    uint8_t size8 = (uint8_t)size;
    crc = crc_xor(crc, size8);
    if (!ring_buffer_push(rb, &size8))
    {
        return -1;
    }
    uint8_t cmd8 = (uint8_t)cmd;
    crc = crc_xor(crc, cmd8);
    remaining--;
    chunk_size--;
    if (direction == MSP_DIRECTION_TO_MWC)
    {
        if (!ring_buffer_push(rb, &cmd8))
        {
            return -1;
        }
        remaining--;
        chunk_size--;
    }
    // Now push the payload. Might require more chunks.
    const uint8_t *payload_ptr = payload;
    while (remaining > 1)
    {
        if (!ring_buffer_push(rb, payload_ptr))
        {
            return -1;
        }
        crc = crc_xor(crc, *payload_ptr);
        remaining--;
        chunk_size--;
        payload_ptr++;
        if (chunk_size == 0)
        {
            // Push another chunk. We will always need at least
            // another byte for the payload.
            chunk.start = false;
            chunk_size = chunk.size = MIN(remaining, max_data_size);
            if (!msp_telemetry_push_chunk(rb, &chunk))
            {
                return -1;
            }
        }
    }
    // Finally, push the crc
    if (!ring_buffer_push(rb, &crc))
    {
        return -1;
    }
    return size;
}

static int msp_telemetry_input_read(void *transport, msp_direction_e *direction, uint16_t *cmd, void *payload, size_t size)
{
    msp_telemetry_t *tr = transport;
    int n = msp_telemetry_read(tr, &tr->req, cmd, payload, size);
    if (n >= 0)
    {
        LOG_D(TAG, "Got MSP request %u (%d payload bytes)", *cmd, n);
        *direction = MSP_DIRECTION_TO_MWC;
    }
    return n;
}

static int msp_telemetry_input_write(void *transport, msp_direction_e direction, uint16_t cmd, const void *payload, size_t size)
{
    LOG_D(TAG, "Got MSP response %u (%u payload bytes)", cmd, size);
    msp_telemetry_t *tr = transport;
    return msp_telemetry_write(tr, &tr->resp, direction, cmd, payload, size);
}

void msp_telemetry_init_input(msp_telemetry_t *tr, size_t max_resp_chunk_size)
{
    msp_telemetry_init(tr, max_resp_chunk_size);
    tr->transport.vtable.read = msp_telemetry_input_read;
    tr->transport.vtable.write = msp_telemetry_input_write;
}

bool msp_telemetry_push_request_chunk(msp_telemetry_t *tr, const void *payload, size_t size)
{
    const msp_telemetry_req_chunk_t *req_chunk = payload;
    if (req_chunk->version != MSP_TELEMETRY_MSP_VERSION)
    {
        return false;
    }
    const uint8_t *ptr = payload;
    if (req_chunk->start)
    {
        if (msp_telemetry_in_use(tr))
        {
            LOG_W(TAG, "Request chunk with request in flight");
            return false;
        }
        msp_telemetry_blob_t blob;
        blob.payload_size = req_chunk->data[0];
        blob.cmd = req_chunk->data[1];
        if (!msp_telemetry_push_blob(&tr->req, &blob))
        {
            return false;
        }
        tr->req_seq = req_chunk->seq;
        // Payload size (data[0]) + size byte + cmd byte + crc byte
        tr->size = blob.payload_size + 3;
        tr->recv = 2;
        // Skip size and cmd
        ptr += 2;
        tr->in_use_since = time_ticks_now();
    }
    else if (++tr->req_seq != req_chunk->seq)
    {
        LOG_W(TAG, "MSP request with invalid seq, expected %u but got %u", tr->req_seq, req_chunk->seq);
        ring_buffer_empty(&tr->req);
        return false;
    }
    size_t data_size = MIN(size, tr->size - tr->recv);
    for (size_t ii = 0; ii < data_size; ii++, ptr++)
    {
        ring_buffer_push(&tr->req, ptr);
    }
    tr->recv += data_size;
    if (tr->size == tr->recv)
    {
        tr->in_use_since = 0;
        tr->count++;
        LOG_D(TAG, "MSP req complete");
    }
    return true;
}

size_t msp_telemetry_pop_response_chunk(msp_telemetry_t *tr, void *buf)
{
    msp_telemetry_chunk_t chunk;
    if (msp_telemetry_pop_chunk(&tr->resp, &chunk))
    {
        msp_telemetry_resp_chunk_t *resp_chunk = buf;
        resp_chunk->seq = tr->resp_seq++;
        resp_chunk->start = chunk.start ? 1 : 0;
        resp_chunk->error = 0;
        resp_chunk->reserved1 = 0;
        uint8_t *ptr = resp_chunk->data;
        for (int ii = 0; ii < chunk.size; ii++)
        {
            if (!ring_buffer_pop(&tr->resp, ptr++))
            {
                return 0;
            }
        }
        // Padding added by the caller if required
        return chunk.size + 1;
    }
    return 0;
}

static int msp_telemetry_output_read(void *transport, msp_direction_e *direction, uint16_t *cmd, void *payload, size_t size)
{
    msp_telemetry_t *tr = transport;
    int n = msp_telemetry_read(tr, &tr->resp, cmd, payload, size);
    if (n >= 0)
    {
        *direction = MSP_DIRECTION_FROM_MWC;
    }
    return n;
}

static int msp_telemetry_output_write(void *transport, msp_direction_e direction, uint16_t cmd, const void *payload, size_t size)
{
    msp_telemetry_t *tr = transport;
    return msp_telemetry_write(tr, &tr->req, direction, cmd, payload, size);
}

void msp_telemetry_init_output(msp_telemetry_t *tr, size_t max_req_chunk_size)
{
    msp_telemetry_init(tr, max_req_chunk_size);
    tr->transport.vtable.read = msp_telemetry_output_read;
    tr->transport.vtable.write = msp_telemetry_output_write;
}

bool msp_telemetry_push_response_chunk(msp_telemetry_t *tr, const void *payload, size_t size)
{
#define Y_N(x) (x ? "Y" : "N")
    msp_telemetry_resp_chunk_t *chunk = (msp_telemetry_resp_chunk_t *)payload;
    LOG_D(TAG, "MSP transport got chunk %u bytes (error: %s, start: %s)", size, Y_N(chunk->error), Y_N(chunk->start));
    LOG_BUFFER_D(TAG, payload, size);
    if (chunk->error)
    {
        // TODO: Handle error
        LOG_W(TAG, "MSP reply with error flag");
        tr->in_use_since = 0;
        return false;
    }
    // Check start flag and seq number
    const uint8_t *ptr = chunk->data;
    if (chunk->start)
    {
        tr->resp_seq = chunk->seq;
        msp_telemetry_blob_t blob = {
            .payload_size = chunk->data[0],
            .cmd = tr->cmd,
        };
        if (!msp_telemetry_push_blob(&tr->resp, &blob))
        {
            return false;
        }
        // Grab expected response size. We expect to get the payload, plus the size byte plus the CRC
        tr->size = blob.payload_size + 1;
        // Initialize the number of received bytes from the response
        tr->recv = 0;
        LOG_D(TAG, "Expecting response of size %d", tr->size);
        // Skip the size byte
        ptr++;
        size--;
    }
    else if (++tr->resp_seq != chunk->seq)
    {
        // TODO: Handle invalid sequence
        LOG_W(TAG, "MSP response with invalid seq, expected %u but got %u", tr->resp_seq, chunk->seq);
        tr->in_use_since = 0;
        return false;
    }
    for (int ii = 0; ii < size && tr->recv < tr->size; ii++, tr->recv++, ptr++)
    {
        LOG_D(TAG, "Push byte %u", *ptr);
        ring_buffer_push(&tr->resp, ptr);
    }
    if (tr->recv == tr->size)
    {
        tr->in_use_since = 0;
        tr->count++;
        LOG_D(TAG, "MSP resp complete");
    }
    return true;
}

size_t msp_telemetry_pop_request_chunk(msp_telemetry_t *tr, void *buf)
{
    msp_telemetry_chunk_t chunk;

    if (msp_telemetry_next_chunk_starts(&tr->req) && msp_telemetry_in_use(tr))
    {
        return 0;
    }

    if (!msp_telemetry_pop_chunk(&tr->req, &chunk))
    {
        // No MSP data to write
        return 0;
    }

    // Prepare an output chunk pointing to the given buffer
    msp_telemetry_req_chunk_t *output_chunk = buf;
    output_chunk->seq = tr->req_seq++;
    output_chunk->start = chunk.start;
    output_chunk->version = MSP_TELEMETRY_MSP_VERSION;
    uint8_t *ptr = output_chunk->data;
    for (unsigned ii = 0; ii < chunk.size; ii++, ptr++)
    {
        if (!ring_buffer_pop(&tr->req, ptr))
        {
            return 0;
        }
    }
    if (chunk.start)
    {
        tr->in_use_since = time_ticks_now();
        // 1st byte is cmd size, 2nd is the cmd itself
        tr->cmd = output_chunk->data[1];
    }
    return chunk.size + 1;
}