#include "max30102.h"
#include "config.h"
#include "i2c_bus.h"
#include <math.h>
#include <string.h>

#define REG_INTR_ENABLE_1 0x02
#define REG_INTR_ENABLE_2 0x03
#define REG_FIFO_WR_PTR   0x04
#define REG_OVF_COUNTER   0x05
#define REG_FIFO_RD_PTR   0x06
#define REG_FIFO_DATA     0x07
#define REG_FIFO_CONFIG   0x08
#define REG_MODE_CONFIG   0x09
#define REG_SPO2_CONFIG   0x0A
#define REG_LED1_PA       0x0C
#define REG_LED2_PA       0x0D
#define REG_PILOT_PA      0x10
#define REG_PART_ID       0xFF

#define FIFO_DEPTH   32
#define RR_HISTORY   8
#define MIN_RR_MS    300
#define MAX_RR_MS    2000
#define DC_ALPHA     0.05f

static float   s_dc;
static float   s_prevAc;
static bool    s_havePrev;
static uint32_t s_lastBeatUs;

static uint32_t s_rr[RR_HISTORY];
static int      s_rrCount;
static uint32_t s_beats;
static float    s_bpm;
static float    s_rmssd;
static bool     s_valid;

static void writeReg(uint8_t reg, uint8_t val)
{
    i2cWriteReg(I2C_ADDR_MAX30102, reg, val);
}

static uint8_t readReg(uint8_t reg)
{
    uint8_t v = 0;
    i2cReadRegs(I2C_ADDR_MAX30102, reg, &v, 1);
    return v;
}

static void addRr(uint32_t rr)
{
    if (s_rrCount < RR_HISTORY) {
        s_rr[s_rrCount++] = rr;
    } else {
        memmove(&s_rr[0], &s_rr[1], (RR_HISTORY - 1) * sizeof(uint32_t));
        s_rr[RR_HISTORY - 1] = rr;
    }
}

static void computeStats()
{
    if (s_rrCount < 2) {
        s_valid = false;
        return;
    }

    float sum = 0.0f;
    for (int i = 0; i < s_rrCount; i++) {
        sum += (float)s_rr[i];
    }
    float mean = sum / (float)s_rrCount;
    s_bpm = 60000.0f / mean;

    float sq = 0.0f;
    for (int i = 1; i < s_rrCount; i++) {
        float d = (float)s_rr[i] - (float)s_rr[i - 1];
        sq += d * d;
    }
    s_rmssd = sqrtf(sq / (float)(s_rrCount - 1));
    s_valid = s_rrCount >= 4;
}

static void processIr(int32_t ir)
{
    if (s_dc == 0.0f) {
        s_dc = (float)ir;
        return;
    }

    s_dc += DC_ALPHA * ((float)ir - s_dc);
    float ac = (float)ir - s_dc;

    if (s_havePrev && s_prevAc <= 0.0f && ac > 0.0f) {
        uint32_t now = micros();
        if (s_lastBeatUs == 0) {
            s_lastBeatUs = now;
        } else {
            uint32_t rr = (now - s_lastBeatUs) / 1000;
            if (rr >= MIN_RR_MS && rr <= MAX_RR_MS) {
                addRr(rr);
                s_beats++;
                s_lastBeatUs = now;
                computeStats();
            } else if (rr > MAX_RR_MS) {
                s_lastBeatUs = now;
            }
        }
    }

    s_prevAc = ac;
    s_havePrev = true;
}

void max30102Init()
{
    writeReg(REG_MODE_CONFIG, 0x40);
    delay(100);

    for (int i = 0; i < 50 && (readReg(REG_MODE_CONFIG) & 0x40); i++) {
        delay(10);
    }

    writeReg(REG_INTR_ENABLE_1, 0xC0);
    writeReg(REG_INTR_ENABLE_2, 0x00);
    writeReg(REG_FIFO_WR_PTR, 0x00);
    writeReg(REG_OVF_COUNTER, 0x00);
    writeReg(REG_FIFO_RD_PTR, 0x00);
    writeReg(REG_FIFO_CONFIG, 0x4F);
    writeReg(REG_MODE_CONFIG, 0x03);
    writeReg(REG_SPO2_CONFIG, 0x27);
    writeReg(REG_LED1_PA, 0x24);
    writeReg(REG_LED2_PA, 0x24);
    writeReg(REG_PILOT_PA, 0x7F);

    s_dc = 0.0f;
    s_prevAc = 0.0f;
    s_havePrev = false;
    s_lastBeatUs = 0;
    s_rrCount = 0;
    s_beats = 0;
    s_bpm = 0.0f;
    s_rmssd = 0.0f;
    s_valid = false;

    Serial.printf("[max30102] part id 0x%02x\n", readReg(REG_PART_ID));
}

void max30102Poll()
{
    uint8_t wr = readReg(REG_FIFO_WR_PTR) & 0x1F;
    uint8_t rd = readReg(REG_FIFO_RD_PTR) & 0x1F;
    int n = (wr - rd) & 0x1F;
    if (n > FIFO_DEPTH) {
        n = FIFO_DEPTH;
    }

    for (int i = 0; i < n; i++) {
        uint8_t d[6];
        if (!i2cReadRegs(I2C_ADDR_MAX30102, REG_FIFO_DATA, d, sizeof(d))) {
            break;
        }
        int32_t ir = ((int32_t)(d[3] & 0x03) << 16) |
                     ((int32_t)d[4] << 8) | (int32_t)d[5];
        processIr(ir);
    }
}

void max30102GetStats(Max30102Stats *out)
{
    out->bpm = s_bpm;
    out->rmssdMs = s_rmssd;
    out->beatValid = s_valid;
    out->beats = s_beats;
}
