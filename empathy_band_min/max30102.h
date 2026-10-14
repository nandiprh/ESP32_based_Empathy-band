#pragma once

#include <Arduino.h>

struct Max30102Stats {
    float    bpm;
    float    rmssdMs;
    bool     beatValid;
    uint32_t beats;
};

void max30102Init();
void max30102Poll();
void max30102GetStats(Max30102Stats *out);
