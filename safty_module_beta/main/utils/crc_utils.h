/**
 * @file crc_utils.h
 * @brief CRC-8 and CRC-16 utilities. BUG 27 fix.
 */
#ifndef CRC_UTILS_H
#define CRC_UTILS_H

#include <stdint.h>
#include <stddef.h>

/** CRC-8 polynomial 0x31, init 0xFF (SHT45) */
uint8_t  crc8_calculate(const uint8_t *data, size_t len);

/** CRC-16-CCITT polynomial 0x1021, init 0xFFFF (UART, data integrity) */
uint16_t crc16_calculate(const uint8_t *data, size_t len);

/** Convenience: compute checksum for a struct, skipping its last 2 bytes (the checksum field) */
uint16_t crc16_struct(const void *data, size_t struct_size);

#endif /* CRC_UTILS_H */