#pragma once

#include "driver/adc.h"

#define PIN_I2C_SDA     21
#define PIN_I2C_SCL     22
#define PIN_GSR_ADC     34
#define PIN_SOUND_ADC   35
#define PIN_BUTTON      4
#define PIN_RGB_LED     25
#define PIN_VIBRATION   32
#define PIN_BUZZER      33

#define I2C_PORT        I2C_NUM_0
#define I2C_FREQ_HZ     400000

#define GSR_ADC_CHANNEL     ADC1_CHANNEL_6
#define SOUND_ADC_CHANNEL   ADC1_CHANNEL_7

#define SENSOR_PERIOD_MS    100
#define ACTUATOR_PERIOD_MS  500

#define GSR_CAL_MS          30000
#define GSR_CAL_SAMPLES     (GSR_CAL_MS / SENSOR_PERIOD_MS)
#define GSR_SPAN_COUNTS     1500.0f
#define GSR_STRESS_RAISES_OUTPUT 1

#define SOUND_BURST_SAMPLES 64
#define SOUND_FLOOR_COUNTS  200
#define SOUND_CEIL_COUNTS   1800

#define GSR_LEVEL_THRESH    0.55f
#define GSR_SLOPE_THRESH    0.08f
#define RMSSD_LOW_THRESH_MS 30.0f
#define SOUND_THRESH        0.50f
#define SOUND_SUSTAIN_MS    1000
#define MOTION_STILL_THRESH 0.12f
#define DISTRESS_MIN_POINTS 3

#define ALERT_DELAY_MS      2500
#define BUTTON_DEBOUNCE_MS  50

#define BUZZER_ACTIVE_LOW   0
