#pragma once

#include <stdbool.h>

typedef enum {
    STATE_CALM = 0,
    STATE_ANXIOUS,
    STATE_DISTRESS,
} band_state_t;

typedef struct {
    float gsr_level;
    float gsr_slope;
    float sound_level;
    bool  sound_sustained;
    float motion_magnitude;
    float bpm;
    float rmssd_ms;
    bool  beat_valid;
} band_inputs_t;

extern volatile band_state_t g_state;
