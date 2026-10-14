#include "band_config.h"
#include "band_types.h"
#include "classify.h"
#include "i2c_bus.h"
#include "act/actuators.h"
#include "sensors/button.h"
#include "sensors/gsr.h"
#include "sensors/max30102.h"
#include "sensors/mpu6050.h"
#include "sensors/sound.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "band";

volatile band_state_t g_state = STATE_CALM;

static void sensor_task(void *arg)
{
    (void)arg;

    gsr_sample_t gsr = { 0 };
    sound_sample_t sound = { 0 };
    mpu_sample_t mpu = { 0 };

    bool have_prev = false;
    float prev_norm = 0.0f;
    int64_t prev_us = esp_timer_get_time();
    int sound_above_ms = 0;

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        gsr_poll(&gsr);
        sound_poll(&sound);
        mpu6050_poll(&mpu);
        max30102_poll();

        int64_t now = esp_timer_get_time();
        float dt = (float)(now - prev_us) / 1000000.0f;
        if (dt <= 0.0f) {
            dt = (float)SENSOR_PERIOD_MS / 1000.0f;
        }

        float slope = 0.0f;
        if (have_prev && gsr_calibrated()) {
            slope = (gsr.norm - prev_norm) / dt;
        }
        prev_norm = gsr.norm;
        prev_us = now;
        have_prev = true;

        if (sound.norm >= SOUND_THRESH) {
            sound_above_ms += SENSOR_PERIOD_MS;
        } else {
            sound_above_ms = 0;
        }

        max30102_stats_t hr;
        max30102_get_stats(&hr);

        band_inputs_t in = {
            .gsr_level = gsr.norm,
            .gsr_slope = slope,
            .sound_level = sound.norm,
            .sound_sustained = sound_above_ms >= SOUND_SUSTAIN_MS,
            .motion_magnitude = mpu.motion,
            .bpm = hr.bpm,
            .rmssd_ms = hr.rmssd_ms,
            .beat_valid = hr.beat_valid,
        };

        band_state_t state = classify(&in);
        g_state = state;

        if (!gsr_calibrated()) {
            ESP_LOGI(TAG, "calibrating GSR rest baseline %.0f%%",
                     gsr_calibration_progress() * 100.0f);
        } else {
            ESP_LOGI(TAG,
                     "state=%s gsr=%.2f slope=%.2f sound=%.2f(%.1fs) mot=%.2f "
                     "bpm=%.0f rmssd=%.0f",
                     band_state_name(state), in.gsr_level, in.gsr_slope,
                     in.sound_level, sound_above_ms / 1000.0f,
                     in.motion_magnitude, in.bpm, in.rmssd_ms);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Empathy Band starting");

    ESP_ERROR_CHECK(i2c_bus_init());
    gsr_init();
    sound_init();
    mpu6050_init();
    max30102_init();
    button_init();
    actuators_init();

    xTaskCreate(sensor_task, "sensor", 4096, NULL, 6, NULL);
    xTaskCreate(actuator_task, "actuator", 3072, NULL, 5, NULL);
}
