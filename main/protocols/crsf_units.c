#include <math.h>

#include "protocols/crsf_units.h"

// XXX: CRSF always uses big endian on the wire

static uint16_t xhtons(uint16_t _x)
{
    return ((uint16_t)((_x >> 8) | ((_x << 8) & 0xff00)));
}

static uint32_t xhtonl(uint32_t _x)
{
    return ((__uint32_t)((_x >> 24) | ((_x >> 8) & 0xff00) | ((_x << 8) & 0xff0000) | ((_x << 24) & 0xff000000)));
}

// Internal representation is 0.01deg in [-180, 180]
// CRSF uses radians/10000 as angles
int16_t deg_to_crsf_rad(int16_t deg)
{
    int16_t rad = deg * (M_PI / 180.0) * 100;
    return xhtons(rad);
}

// 0.01V internal to 0.1V in CRSF
uint16_t volts_to_crsf_volts(uint16_t volts)
{
    volts /= 10;
    return xhtons(volts);
}

// 0.01A int16_t internal, 0.1A uint16_t CRSF
uint16_t amps_to_crsf_amps(int16_t amps)
{
    if (amps < 0)
    {
        amps = 0;
    }
    amps /= 10;
    return xhtons(amps);
}

// 24 unsigned bits
uint32_t mah_to_crsf_mah(int32_t mah)
{
    if (mah < 0)
    {
        mah = 0;
    }
    mah &= 0xFFFFFF;
    return ((mah & 0xFF) << 16) | (mah & 0xFF00) | ((mah & 0xFF0000) >> 16);
}

// Same format, just BE
int32_t coord_to_crsf_coord(int32_t coord)
{
    return xhtonl(coord);
}

// cms/s to km/h / 10
uint16_t speed_to_crsf_speed(uint16_t cms)
{
    return xhtons(cms * 0.36f);
}

// cm to meters with +1000 offset (e.g. 0 is -1000)
uint16_t alt_to_crsf_alt(int32_t cms)
{
    if (cms < -1000 * 100)
    {
        cms = -1000 * 100;
    }
    return xhtons(cms / 100 + 1000);
}

// Same unit (deg / 100), just BE
uint16_t heading_to_crsf_heading(uint16_t deg)
{
    return xhtons(deg);
}
