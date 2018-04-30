#pragma once

#include <stddef.h>
#include <stdint.h>

uint8_t crc_xor(uint8_t crc, uint8_t data);
uint8_t crc_xor_bytes(const void *data, size_t size);

uint8_t crc8_dvb_s2(uint8_t crc, uint8_t data);
uint8_t crc8_dvb_s2_bytes(const void *data, size_t size);
uint8_t crc8_dvb_s2_bytes_from(uint8_t crc, const void *data, size_t size);
