#pragma once

#include <stdbool.h>

typedef struct {
    float bpm;
    float rmssd_ms;
    bool  beat_valid;
    uint32_t beats;
} max30102_stats_t;

void max30102_init(void);
void max30102_poll(void);
void max30102_get_stats(max30102_stats_t *out);
