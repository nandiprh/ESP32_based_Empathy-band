#include "max30102.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

#define MAX30102_ADDR       0x57

#define REG_INTR_ENABLE_1   0x02
#define REG_INTR_ENABLE_2   0x03
#define REG_FIFO_WR_PTR     0x04
#define REG_OVF_COUNTER     0x05
#define REG_FIFO_RD_PTR     0x06
#define REG_FIFO_DATA       0x07
#define REG_FIFO_CONFIG     0x08
#define REG_MODE_CONFIG     0x09
#define REG_SPO2_CONFIG     0x0A
#define REG_LED1_PA         0x0C
#define REG_LED2_PA         0x0D
#define REG_PILOT_PA        0x10
#define REG_PART_ID         0xFF

#define FIFO_DEPTH          32
#define RR_HISTORY          8
#define MIN_RR_MS           300
#define MAX_RR_MS           2000
#define DC_ALPHA            0.05f

static const char *TAG = "max30102";

static float   s_dc;
static float   s_prev_ac;
static bool    s_have_prev;
static int64_t s_last_beat_us;

static uint32_t s_rr[RR_HISTORY];
static int      s_rr_count;
static uint32_t s_beats;
static float    s_bpm;
static float    s_rmssd;
static bool     s_valid;

static void write_reg(uint8_t reg, uint8_t val)
{
    i2c_write_reg(MAX30102_ADDR, reg, val);
}

static uint8_t read_reg(uint8_t reg)
{
    uint8_t v = 0;
    i2c_read_regs(MAX30102_ADDR, reg, &v, 1);
    return v;
}

static void add_rr(uint32_t rr)
{
    if (s_rr_count < RR_HISTORY) {
        s_rr[s_rr_count++] = rr;
    } else {
        memmove(&s_rr[0], &s_rr[1], (RR_HISTORY - 1) * sizeof(uint32_t));
        s_rr[RR_HISTORY - 1] = rr;
    }
}

static void compute_stats(void)
{
    if (s_rr_count < 2) {
        s_valid = false;
        return;
    }

    float sum = 0.0f;
    for (int i = 0; i < s_rr_count; i++) {
        sum += (float)s_rr[i];
    }
    float mean = sum / (float)s_rr_count;
    s_bpm = 60000.0f / mean;

    float sq = 0.0f;
    for (int i = 1; i < s_rr_count; i++) {
        float d = (float)s_rr[i] - (float)s_rr[i - 1];
        sq += d * d;
    }
    s_rmssd = sqrtf(sq / (float)(s_rr_count - 1));
    s_valid = s_rr_count >= 4;
}

static void process_ir(int32_t ir)
{
    if (s_dc == 0.0f) {
        s_dc = (float)ir;
        return;
    }

    s_dc += DC_ALPHA * ((float)ir - s_dc);
    float ac = (float)ir - s_dc;

    if (s_have_prev && s_prev_ac <= 0.0f && ac > 0.0f) {
        int64_t now = esp_timer_get_time();
        if (s_last_beat_us == 0) {
            s_last_beat_us = now;
        } else {
            uint32_t rr = (uint32_t)((now - s_last_beat_us) / 1000);
            if (rr >= MIN_RR_MS && rr <= MAX_RR_MS) {
                add_rr(rr);
                s_beats++;
                s_last_beat_us = now;
                compute_stats();
            } else if (rr > MAX_RR_MS) {
                s_last_beat_us = now;
            }
        }
    }

    s_prev_ac = ac;
    s_have_prev = true;
}

void max30102_init(void)
{
    write_reg(REG_MODE_CONFIG, 0x40);
    vTaskDelay(pdMS_TO_TICKS(100));

    for (int i = 0; i < 50 && (read_reg(REG_MODE_CONFIG) & 0x40); i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    write_reg(REG_INTR_ENABLE_1, 0xC0);
    write_reg(REG_INTR_ENABLE_2, 0x00);
    write_reg(REG_FIFO_WR_PTR, 0x00);
    write_reg(REG_OVF_COUNTER, 0x00);
    write_reg(REG_FIFO_RD_PTR, 0x00);
    write_reg(REG_FIFO_CONFIG, 0x4F);
    write_reg(REG_MODE_CONFIG, 0x03);
    write_reg(REG_SPO2_CONFIG, 0x27);
    write_reg(REG_LED1_PA, 0x24);
    write_reg(REG_LED2_PA, 0x24);
    write_reg(REG_PILOT_PA, 0x7F);

    s_dc = 0.0f;
    s_prev_ac = 0.0f;
    s_have_prev = false;
    s_last_beat_us = 0;
    s_rr_count = 0;
    s_beats = 0;
    s_bpm = 0.0f;
    s_rmssd = 0.0f;
    s_valid = false;

    ESP_LOGI(TAG, "part id 0x%02x", read_reg(REG_PART_ID));
}

void max30102_poll(void)
{
    uint8_t wr = read_reg(REG_FIFO_WR_PTR) & 0x1F;
    uint8_t rd = read_reg(REG_FIFO_RD_PTR) & 0x1F;
    int n = (wr - rd) & 0x1F;
    if (n > FIFO_DEPTH) {
        n = FIFO_DEPTH;
    }

    for (int i = 0; i < n; i++) {
        uint8_t d[6];
        if (i2c_read_regs(MAX30102_ADDR, REG_FIFO_DATA, d, sizeof(d)) != ESP_OK) {
            break;
        }
        int32_t ir = ((int32_t)(d[3] & 0x03) << 16) |
                     ((int32_t)d[4] << 8) | (int32_t)d[5];
        process_ir(ir);
    }
}

void max30102_get_stats(max30102_stats_t *out)
{
    out->bpm = s_bpm;
    out->rmssd_ms = s_rmssd;
    out->beat_valid = s_valid;
    out->beats = s_beats;
}
