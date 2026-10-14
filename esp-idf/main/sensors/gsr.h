#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t raw;
    float    norm;
} gsr_sample_t;

void gsr_init(void);
void gsr_calibration_reset(void);
bool gsr_calibrated(void);
float gsr_calibration_progress(void);
void gsr_poll(gsr_sample_t *out);
