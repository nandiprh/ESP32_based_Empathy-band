#include "sound.h"
#include "band_config.h"
#include "driver/adc.h"

void sound_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(SOUND_ADC_CHANNEL, ADC_ATTEN_DB_11);
}

void sound_poll(sound_sample_t *out)
{
    uint16_t mn = 4095;
    uint16_t mx = 0;

    for (int i = 0; i < SOUND_BURST_SAMPLES; i++) {
        uint16_t v = (uint16_t)adc1_get_raw(SOUND_ADC_CHANNEL);
        if (v < mn) {
            mn = v;
        }
        if (v > mx) {
            mx = v;
        }
    }

    uint16_t pp = mx - mn;
    out->peak = pp;

    float norm = (float)((int)pp - SOUND_FLOOR_COUNTS) /
                 (float)(SOUND_CEIL_COUNTS - SOUND_FLOOR_COUNTS);
    if (norm < 0.0f) {
        norm = 0.0f;
    } else if (norm > 1.0f) {
        norm = 1.0f;
    }
    out->norm = norm;
}
