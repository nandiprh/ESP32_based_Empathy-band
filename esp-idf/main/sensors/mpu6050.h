#pragma once

#include <stdbool.h>

typedef struct {
    float ax;
    float ay;
    float az;
    float motion;
} mpu_sample_t;

void mpu6050_init(void);
bool mpu6050_present(void);
void mpu6050_poll(mpu_sample_t *out);
