#include "ring_buffer.h"
#include <string.h>

void ring_buffer_init(ring_buffer_t *rb, void *storage,
                      size_t item_size, size_t capacity)
{
    if (!rb || !storage) { return; }
    rb->storage   = (uint8_t *)storage;
    rb->item_size = item_size;
    rb->capacity  = capacity;
    rb->head      = 0;
    rb->count     = 0;
}

void ring_buffer_push(ring_buffer_t *rb, const void *item)
{
    if (!rb || !item) { return; }
    uint8_t *dest = rb->storage + (rb->head * rb->item_size);
    memcpy(dest, item, rb->item_size);
    rb->head = (rb->head + 1) % rb->capacity;
    if (rb->count < rb->capacity) { rb->count++; }
}

bool ring_buffer_peek(const ring_buffer_t *rb, size_t index, void *out)
{
    if (!rb || !out || index >= rb->count) { return false; }
    /* index 0 = oldest */
    size_t actual;
    if (rb->count < rb->capacity) {
        actual = index;
    } else {
        actual = (rb->head + index) % rb->capacity;
    }
    const uint8_t *src = rb->storage + (actual * rb->item_size);
    memcpy(out, src, rb->item_size);
    return true;
}

size_t ring_buffer_count(const ring_buffer_t *rb)
{
    return rb ? rb->count : 0;
}

void ring_buffer_clear(ring_buffer_t *rb)
{
    if (!rb) { return; }
    rb->head  = 0;
    rb->count = 0;
}