#pragma once

#include <Arduino.h>

struct SoundSample {
    uint16_t peak;
    float    norm;
};

void soundInit();
void soundPoll(SoundSample *out);
