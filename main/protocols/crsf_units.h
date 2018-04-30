#pragma once

#include <math.h>
#include <stdint.h>

#include <arpa/inet.h>

// XXX: CRSF always uses big endian on the wire

// Internal representation is 0.01deg in [-180, 180]
// CRSF uses radians/10000 as angles

inline int16_t deg_to_crsf_rad(int16_t deg)
{
    int16_t rad = deg * (M_PI / 180.0) * 100;
    return htons(rad);
}

// 0.01V internal to 0.1V in CRSF
inline uint16_t volts_to_crsf_volts(uint16_t volts)
{
    volts /= 10;
    return htons(volts);
}

// 0.01A int16_t internal, 0.1A uint16_t CRSF
inline uint16_t amps_to_crsf_amps(int16_t amps)
{
    if (amps < 0)
    {
        amps = 0;
    }
    amps /= 10;
    return htons(amps);
}

// 24 unsigned bits
inline uint32_t mah_to_crsf_mah(int32_t mah)
{
    if (mah < 0)
    {
        mah = 0;
    }
    mah &= 0xFFFFFF;
    return ((mah & 0xFF) << 16) | (mah & 0xFF00) | ((mah & 0xFF0000) >> 16);
}

// Same format, just BE
inline int32_t coord_to_crsf_coord(int32_t coord)
{
    return htonl(coord);
}

// cms/s to km/h / 10
inline uint16_t speed_to_crsf_speed(uint16_t cms)
{
    return htons(cms * 0.36f);
}

// cm to meters with +1000 offset (e.g. 0 is -1000)
inline uint16_t alt_to_crsf_alt(int32_t cms)
{
    if (cms < -1000 * 100)
    {
        cms = -1000 * 100;
    }
    return htons(cms / 100 + 1000);
}

// Same unit (deg / 100), just BE
inline uint16_t heading_to_crsf_heading(uint16_t deg)
{
    return htons(deg);
}
