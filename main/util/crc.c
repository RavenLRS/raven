#include "crc.h"

uint8_t crc_xor(uint8_t crc, uint8_t data)
{
    return crc ^ data;
}

uint8_t crc_xor_bytes(const void *data, size_t size)
{
    uint8_t crc = 0;
    const uint8_t *p = data;
    for (unsigned ii = 0; ii < size; ii++, p++)
    {
        crc ^= *p;
    }
    return crc;
}

uint8_t crc8_dvb_s2(uint8_t crc, uint8_t data)
{
    crc ^= data;
    for (int ii = 0; ii < 8; ++ii)
    {
        if (crc & 0x80)
        {
            crc = (crc << 1) ^ 0xD5;
        }
        else
        {
            crc = crc << 1;
        }
    }
    return crc;
}

uint8_t crc8_dvb_s2_bytes(const void *data, size_t size)
{
    return crc8_dvb_s2_bytes_from(0, data, size);
}

uint8_t crc8_dvb_s2_bytes_from(uint8_t crc, const void *data, size_t size)
{
    const uint8_t *p = data;
    for (unsigned ii = 0; ii < size; ii++, p++)
    {
        crc = crc8_dvb_s2(crc, *p);
    }
    return crc;
}
