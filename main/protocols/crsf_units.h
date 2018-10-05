#pragma once

#include <stdint.h>

int16_t deg_to_crsf_rad(int16_t deg);
uint16_t volts_to_crsf_volts(uint16_t volts);
uint16_t amps_to_crsf_amps(int16_t amps);
uint32_t mah_to_crsf_mah(int32_t mah);
int32_t coord_to_crsf_coord(int32_t coord);
uint16_t speed_to_crsf_speed(uint16_t cms);
uint16_t alt_to_crsf_alt(int32_t cms);
uint16_t heading_to_crsf_heading(uint16_t deg);
