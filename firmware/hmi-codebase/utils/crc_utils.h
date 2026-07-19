/**
 * @file    crc_utils.h
 * @brief   CRC-16/CCITT-FALSE utility — shared with S3 project.
 * @version 1.0.0
 */

#ifndef CRC_UTILS_H
#define CRC_UTILS_H

#include <stdint.h>
#include <stddef.h>

uint16_t crc16_calc(const uint8_t *data, size_t len);

#endif /* CRC_UTILS_H */