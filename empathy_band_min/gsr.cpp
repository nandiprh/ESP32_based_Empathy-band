#include "gsr.h"
#include "config.h"

static uint32_t s_calSum;
static uint32_t s_calCount;
static double   s_baseline;
static bool     s_calibrated;

void gsrInit()
{
    pinMode(PIN_GSR, INPUT);
    gsrCalibrationReset();
}

void gsrCalibrationReset()
{
    s_calSum = 0;
    s_calCount = 0;
    s_baseline = 0.0;
    s_calibrated = false;
}

bool gsrCalibrated()
{
    return s_calibrated;
}

float gsrCalibrationProgress()
{
    float p = (float)s_calCount / (float)GSR_CAL_SAMPLES;
    return p > 1.0f ? 1.0f : p;
}

void gsrPoll(GsrSample *out)
{
    uint16_t raw = (uint16_t)analogRead(PIN_GSR);
    out->raw = raw;

    if (!s_calibrated) {
        s_calSum += raw;
        s_calCount++;
        if (s_calCount >= GSR_CAL_SAMPLES) {
            s_baseline = (double)s_calSum / (double)s_calCount;
            s_calibrated = true;
            Serial.printf("[gsr] rest baseline calibrated: %.1f counts\n", s_baseline);
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
