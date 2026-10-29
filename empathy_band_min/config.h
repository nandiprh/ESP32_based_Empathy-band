#pragma once

#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22
#define PIN_GSR       34
#define PIN_RGB_R     25
#define PIN_RGB_G     26
#define PIN_RGB_B     27
#define PIN_BUZZER    33

#define I2C_ADDR_MAX30102  0x57
#define I2C_FREQ_HZ        400000

#define SENSOR_PERIOD_MS    100
#define ACTUATOR_PERIOD_MS  500

#define GSR_CAL_MS          30000
#define GSR_CAL_SAMPLES     (GSR_CAL_MS / SENSOR_PERIOD_MS)
#define GSR_SPAN_COUNTS     1500.0f
#define GSR_STRESS_RAISES_OUTPUT 1

#define GSR_LEVEL_THRESH    0.55f
#define GSR_SLOPE_THRESH    0.08f
#define RMSSD_LOW_THRESH_MS 30.0f
#define MOTION_STILL_THRESH 0.12f
#define DISTRESS_MIN_POINTS 3

#define ALERT_DELAY_MS      2500

#define BUZZER_ACTIVE_LOW   0

#define RGB_CALM_R     0
#define RGB_CALM_G     0
#define RGB_CALM_B     40
#define RGB_ANXIOUS_R  80
#define RGB_ANXIOUS_G  30
#define RGB_ANXIOUS_B  0
#define RGB_DISTRESS_R 80
#define RGB_DISTRESS_G 0
#define RGB_DISTRESS_B 0


// RGB LED configuration
// Set to 1 if using common anode RGB LED, 0 for common cathode
#define RGB_COMMON_ANODE 1
