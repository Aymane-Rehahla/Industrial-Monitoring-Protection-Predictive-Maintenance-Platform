#include "filter.h"
#include <string.h>
#include <stdlib.h>

/* ── Moving average ──────────────────────────────────────────────────── */

void filter_avg_init(filter_avg_t *f, int32_t *buf, size_t capacity)
{
    if (!f || !buf || capacity == 0) { return; }
    f->buffer   = buf;
    f->capacity = capacity;
    f->count    = 0;
    f->index    = 0;
    f->sum      = 0;
    memset(buf, 0, capacity * sizeof(int32_t));
}

int32_t filter_avg_add(filter_avg_t *f, int32_t value)
{
    if (!f || !f->buffer) { return value; }
    if (f->count >= f->capacity) {
        f->sum -= f->buffer[f->index];  /* subtract oldest */
    } else {
        f->count++;
    }
    f->buffer[f->index] = value;
    f->sum += value;
    f->index = (f->index + 1) % f->capacity;
    return (int32_t)(f->sum / (int64_t)f->count);
}

int32_t filter_avg_get(const filter_avg_t *f)
{
    if (!f || f->count == 0) { return 0; }
    return (int32_t)(f->sum / (int64_t)f->count);
}

void filter_avg_reset(filter_avg_t *f)
{
    if (!f) { return; }
    f->count = 0;
    f->index = 0;
    f->sum   = 0;
}

/* ── Median ──────────────────────────────────────────────────────────── */

static int cmp_int32(const void *a, const void *b)
{
    int32_t va = *(const int32_t *)a;
    int32_t vb = *(const int32_t *)b;
    return (va > vb) - (va < vb);
}

int32_t filter_median(const int32_t *values, size_t count)
{
    if (!values || count == 0) { return 0; }
    int32_t tmp[32]; /* max 32 samples */
    if (count > 32) { count = 32; }
    memcpy(tmp, values, count * sizeof(int32_t));
    qsort(tmp, count, sizeof(int32_t), cmp_int32);
    if (count % 2 == 0) {
        return (tmp[count / 2 - 1] + tmp[count / 2]) / 2;
    }
    return tmp[count / 2];
}