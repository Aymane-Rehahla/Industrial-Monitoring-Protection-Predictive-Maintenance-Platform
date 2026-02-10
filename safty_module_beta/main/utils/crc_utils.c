#include "crc_utils.h"

uint8_t crc8_calculate(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x31) : (crc << 1);
        }
    }
    return crc;
}

uint16_t crc16_calculate(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }
    return crc;
}

uint16_t crc16_struct(const void *data, size_t struct_size)
{
    /* Checksum field is always the last 2 bytes – exclude them */
    if (struct_size <= 2) { return 0; }
    return crc16_calculate((const uint8_t *)data, struct_size - sizeof(uint16_t));
}