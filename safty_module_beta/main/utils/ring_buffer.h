#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint8_t *storage;
    size_t   item_size;
    size_t   capacity;
    size_t   head;
    size_t   count;
} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *rb, void *storage,
                      size_t item_size, size_t capacity);
void ring_buffer_push(ring_buffer_t *rb, const void *item);
bool ring_buffer_peek(const ring_buffer_t *rb, size_t index, void *out);
size_t ring_buffer_count(const ring_buffer_t *rb);
void ring_buffer_clear(ring_buffer_t *rb);

#endif /* RING_BUFFER_H */