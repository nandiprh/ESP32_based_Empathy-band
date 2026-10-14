#pragma once

#include <Arduino.h>

struct GsrSample {
    uint16_t raw;
    float    norm;
};

void  gsrInit();
void  gsrCalibrationReset();
bool  gsrCalibrated();
float gsrCalibrationProgress();
void  gsrPoll(GsrSample *out);
