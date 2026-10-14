#pragma once

#include <stdint.h>

typedef struct {
    uint16_t peak;
    float    norm;
} sound_sample_t;

void sound_init(void);
void sound_poll(sound_sample_t *out);
