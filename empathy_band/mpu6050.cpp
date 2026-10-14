#include "mpu6050.h"
#include "config.h"
#include "i2c_bus.h"
#include <math.h>

#define REG_CONFIG       0x1A
#define REG_ACCEL_CONFIG 0x1C
#define REG_ACCEL_XOUT_H 0x3B
#define REG_PWR_MGMT_1   0x6B
#define REG_WHO_AM_I     0x75

static bool s_present;

void mpu6050Init()
{
    uint8_t who = 0;
    if (!i2cReadRegs(I2C_ADDR_MPU6050, REG_WHO_AM_I, &who, 1) || who != 0x68) {
        Serial.printf("[mpu6050] not responding (who_am_i=0x%02x)\n", who);
        s_present = false;
        return;
    }

    i2cWriteReg(I2C_ADDR_MPU6050, REG_PWR_MGMT_1, 0x00);
    i2cWriteReg(I2C_ADDR_MPU6050, REG_CONFIG, 0x03);
    i2cWriteReg(I2C_ADDR_MPU6050, REG_ACCEL_CONFIG, 0x00);
    s_present = true;
    Serial.println("[mpu6050] ready");
}

bool mpu6050Present()
{
    return s_present;
}

void mpu6050Poll(MpuSample *out)
{
    out->ax = 0.0f;
    out->ay = 0.0f;
    out->az = 0.0f;
    out->motion = 0.0f;

    if (!s_present) {
        return;
    }

    uint8_t b[6];
    if (!i2cReadRegs(I2C_ADDR_MPU6050, REG_ACCEL_XOUT_H, b, sizeof(b))) {
        return;
    }

    int16_t ax = (int16_t)((b[0] << 8) | b[1]);
    int16_t ay = (int16_t)((b[2] << 8) | b[3]);
    int16_t az = (int16_t)((b[4] << 8) | b[5]);

    out->ax = ax / 16384.0f;
    out->ay = ay / 16384.0f;
    out->az = az / 16384.0f;

    float mag = sqrtf(out->ax * out->ax + out->ay * out->ay + out->az * out->az);
    out->motion = fabsf(mag - 1.0f);
}
