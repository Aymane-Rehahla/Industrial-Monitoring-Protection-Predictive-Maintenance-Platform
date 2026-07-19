/**
 * @file    ring_buffer.c
 * @brief   Generic ring buffer — overwrites oldest on overflow.
 * @version 1.0.0
 */

#include "ring_buffer.h"
#include <string.h>

void ring_buffer_init(ring_buffer_t *rb, uint8_t *backing,
                      size_t item_size, size_t capacity)
{
    if (rb == NULL || backing == NULL) { return; }
    rb->buf       = backing;
    rb->item_size = item_size;
    rb->capacity  = capacity;
    rb->count     = 0;
    rb->head      = 0;
    rb->tail      = 0;
}

void ring_buffer_push(ring_buffer_t *rb, const void *item)
{
    if (rb == NULL || item == NULL) { return; }

    uint8_t *dst = rb->buf + (rb->head * rb->item_size);
    memcpy(dst, item, rb->item_size);

    rb->head = (rb->head + 1) % rb->capacity;

    if (rb->count < rb->capacity) {
        rb->count++;
    } else {
        rb->tail = (rb->tail + 1) % rb->capacity;
    }
}

bool ring_buffer_peek(const ring_buffer_t *rb, size_t index, void *out)
{
    if (rb == NULL || out == NULL) { return false; }
    if (index >= rb->count) { return false; }

    size_t actual = (rb->tail + index) % rb->capacity;
    const uint8_t *src = rb->buf + (actual * rb->item_size);
    memcpy(out, src, rb->item_size);
    return true;
}

bool ring_buffer_peek_latest(const ring_buffer_t *rb, void *out)
{
    if (rb == NULL || out == NULL || rb->count == 0) { return false; }

    size_t latest;
    if (rb->head == 0) {
        latest = rb->capacity - 1;
    } else {
        latest = rb->head - 1;
    }

    const uint8_t *src = rb->buf + (latest * rb->item_size);
    memcpy(out, src, rb->item_size);
    return true;
}

size_t ring_buffer_count(const ring_buffer_t *rb)
{
    if (rb == NULL) { return 0; }
    return rb->count;
}

void ring_buffer_clear(ring_buffer_t *rb)
{
    if (rb == NULL) { return; }
    rb->count = 0;
    rb->head  = 0;
    rb->tail  = 0;
}