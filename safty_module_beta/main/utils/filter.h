#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>
#include <stddef.h>

/* ── Moving-average filter ───────────────────────────────────────────── */
typedef struct {
    int32_t *buffer;
    size_t   capacity;
    size_t   count;
    size_t   index;
    int64_t  sum;
} filter_avg_t;

void    filter_avg_init(filter_avg_t *f, int32_t *buf, size_t capacity);
int32_t filter_avg_add(filter_avg_t *f, int32_t value);
int32_t filter_avg_get(const filter_avg_t *f);
void    filter_avg_reset(filter_avg_t *f);

/* ── Median (stateless, works on a temp copy) ────────────────────────── */
int32_t filter_median(const int32_t *values, size_t count);

#endif /* FILTER_H */