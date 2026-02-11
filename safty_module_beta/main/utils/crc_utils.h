/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  crc_utils.h - CRC Calculation Functions                                     ║
 * ╠═══════════════════════════════════════════════════════════════════════════════╣
 * ║  Rule 4.2: All critical structures have checksums                            ║
 * ║  Safety Level: MEDIUM                                                         ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#ifndef CRC_UTILS_H
#define CRC_UTILS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief CRC-8 for Sensirion sensors (SHT45)
 * Polynomial: 0x31, Init: 0xFF
 */
uint8_t crc8_sensirion(const uint8_t *data, size_t len);

/**
 * @brief CRC-16-CCITT for UART protocol
 * Polynomial: 0x1021, Init: 0xFFFF
 */
uint16_t crc16_ccitt(const uint8_t *data, size_t len);

/**
 * @brief CRC-16 for data structures
 * Same as CCITT but takes void* for convenience
 */
uint16_t crc16_calculate(const void *data, size_t len);

/**
 * @brief Verify CRC-16 of structure
 * @param data Pointer to structure (CRC must be last field)
 * @param len Total size including CRC field
 * @return true if CRC valid
 */
bool crc16_verify(const void *data, size_t len);

#endif /* CRC_UTILS_H */