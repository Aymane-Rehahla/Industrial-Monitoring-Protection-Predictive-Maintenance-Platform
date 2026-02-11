/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  crc_utils.c - CRC Implementation                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include "crc_utils.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════════ */

uint8_t crc8_sensirion(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        return 0;
    }
    
    uint8_t crc = 0xFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════════ */

uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        return 0;
    }
    
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════════ */

uint16_t crc16_calculate(const void *data, size_t len)
{
    return crc16_ccitt((const uint8_t *)data, len);
}

/* ═══════════════════════════════════════════════════════════════════════════════ */

bool crc16_verify(const void *data, size_t len)
{
    if (data == NULL || len < 3) {
        return false;
    }
    
    /* CRC is last 2 bytes */
    size_t data_len = len - 2;
    const uint8_t *bytes = (const uint8_t *)data;
    
    uint16_t stored_crc = (bytes[data_len] << 8) | bytes[data_len + 1];
    uint16_t calc_crc = crc16_ccitt(bytes, data_len);
    
    return (stored_crc == calc_crc);
}