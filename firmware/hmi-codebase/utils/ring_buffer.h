/**
 * @file    ring_buffer.h
 * @brief   Generic fixed-size ring buffer for telemetry history.
 * @version 1.0.0
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief  Ring buffer descriptor.  Caller provides the backing array.
 */
typedef struct {
    uint8_t *buf;           /**< Pointer to backing byte array          */
    size_t   item_size;     /**< Size of one element in bytes           */
    size_t   capacity;      /**< Maximum number of elements             */
    size_t   count;         /**< Current number of stored elements      */
    size_t   head;          /**< Next write position                    */
    size_t   tail;          /**< Next read position                     */
} ring_buffer_t;

/**
 * @brief  Initialise ring buffer.
 * @param  rb         Ring buffer descriptor.
 * @param  backing    Caller-allocated byte array.
 * @param  item_size  Size of one element.
 * @param  capacity   Max elements (backing must be item_size × capacity).
 */
void ring_buffer_init(ring_buffer_t *rb, uint8_t *backing,
                      size_t item_size, size_t capacity);

/** @brief  Push one item.  Overwrites oldest if full. */
void ring_buffer_push(ring_buffer_t *rb, const void *item);

/** @brief  Read item at index (0 = oldest).  Returns false if out of range. */
bool ring_buffer_peek(const ring_buffer_t *rb, size_t index, void *out);

/** @brief  Get newest item.  Returns false if empty. */
bool ring_buffer_peek_latest(const ring_buffer_t *rb, void *out);

/** @brief  Current item count. */
size_t ring_buffer_count(const ring_buffer_t *rb);

/** @brief  Reset to empty. */
void ring_buffer_clear(ring_buffer_t *rb);

#endif /* RING_BUFFER_H */