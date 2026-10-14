#include "gsr.h"
#include "band_config.h"
#include "driver/adc.h"
#include "esp_log.h"

static const char *TAG = "gsr";

static uint32_t s_cal_sum;
static uint32_t s_cal_count;
static double   s_baseline;
static bool     s_calibrated;

void gsr_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(GSR_ADC_CHANNEL, ADC_ATTEN_DB_11);
    gsr_calibration_reset();
}

void gsr_calibration_reset(void)
{
    s_cal_sum = 0;
    s_cal_count = 0;
    s_baseline = 0.0;
    s_calibrated = false;
}

bool gsr_calibrated(void)
{
    return s_calibrated;
}

float gsr_calibration_progress(void)
{
    float p = (float)s_cal_count / (float)GSR_CAL_SAMPLES;
    return p > 1.0f ? 1.0f : p;
}

void gsr_poll(gsr_sample_t *out)
{
    uint16_t raw = (uint16_t)adc1_get_raw(GSR_ADC_CHANNEL);
    out->raw = raw;

    if (!s_calibrated) {
        s_cal_sum += raw;
        s_cal_count++;
        if (s_cal_count >= GSR_CAL_SAMPLES) {
            s_baseline = (double)s_cal_sum / (double)s_cal_count;
            s_calibrated = true;
            ESP_LOGI(TAG, "rest baseline calibrated: %.1f counts", s_baseline);
        }
        out->norm = 0.0f;
        return;
    }

    float delta = (float)((double)raw - s_baseline);
    float norm = GSR_STRESS_RAISES_OUTPUT ? (delta / GSR_SPAN_COUNTS)
                                          : (-delta / GSR_SPAN_COUNTS);
    if (norm < 0.0f) {
        norm = 0.0f;
    } else if (norm > 1.0f) {
        norm = 1.0f;
    }
    out->norm = norm;
}
