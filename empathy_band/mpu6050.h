#pragma once

#include <Arduino.h>

struct MpuSample {
    float ax;
    float ay;
    float az;
    float motion;
};

void mpu6050Init();
bool mpu6050Present();
void mpu6050Poll(MpuSample *out);
