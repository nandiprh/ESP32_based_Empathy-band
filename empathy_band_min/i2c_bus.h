#pragma once

#include <Arduino.h>

void i2cBegin();
bool i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t val);
bool i2cReadRegs(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len);
