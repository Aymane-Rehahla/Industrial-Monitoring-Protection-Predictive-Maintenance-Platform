/**
 * @file    crc_utils.h
 * @brief   CRC-16/CCITT-FALSE utility.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — Used for packet and data integrity validation.
 */

#ifndef CRC_UTILS_H
#define CRC_UTILS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief  Compute CRC-16/CCITT-FALSE over a byte buffer.
 *
 * Polynomial: 0x1021, Init: 0xFFFF, No final XOR.
 * Same algorithm used by sensor_reading_t checksums and comm packets.
 *
 * @param  data  Pointer to data bytes.  Must not be NULL.
 * @param  len   Number of bytes.  Zero returns 0xFFFF.
 * @return CRC-16 value.
 * @wcet   O(len) — approximately 16 cycles per byte on Xtensa.
 * @thread_safety  Thread-safe (pure function, no state).
 * @isr_safety     ISR-safe.
 */
uint16_t crc16_calc(const uint8_t *data, size_t len);

#endif /* CRC_UTILS_H */