#include "actuators.h"
#include "band_config.h"
#include "band_types.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#define RMT_TX_CHANNEL RMT_CHANNEL_0

#define WS2812_T0H 16
#define WS2812_T0L 32
#define WS2812_T1H 32
#define WS2812_T1L 16

#define RGB_CALM_R     0
#define RGB_CALM_G     0
#define RGB_CALM_B     40
#define RGB_ANXIOUS_R  80
#define RGB_ANXIOUS_G  30
#define RGB_ANXIOUS_B  0
#define RGB_DISTRESS_R 80
#define RGB_DISTRESS_G 0
#define RGB_DISTRESS_B 0

static void ws2812_init(void)
{
    rmt_config_t cfg = RMT_DEFAULT_CONFIG_TX(PIN_RGB_LED, RMT_TX_CHANNEL);
    cfg.clk_div = 2;
    cfg.mem_block_num = 1;
    rmt_config(&cfg);
    rmt_driver_install(RMT_TX_CHANNEL, 0, 0);
}

static void ws2812_send(uint8_t r, uint8_t g, uint8_t b)
{
    rmt_item32_t items[25];
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;

    for (int i = 0; i < 24; i++) {
        if (grb & (1u << (23 - i))) {
            items[i].level0 = 1;
            items[i].duration0 = WS2812_T1H;
            items[i].level1 = 0;
            items[i].duration1 = WS2812_T1L;
        } else {
            items[i].level0 = 1;
            items[i].duration0 = WS2812_T0H;
            items[i].level1 = 0;
            items[i].duration1 = WS2812_T0L;
        }
    }

    items[24].level0 = 0;
    items[24].duration0 = 2000;
    items[24].level1 = 0;
    items[24].duration1 = 0;

    rmt_write_items(RMT_TX_CHANNEL, items, 25, true);
}

static void set_state_colour(band_state_t state)
{
    switch (state) {
    case STATE_CALM:
        ws2812_send(RGB_CALM_R, RGB_CALM_G, RGB_CALM_B);
        break;
    case STATE_ANXIOUS:
        ws2812_send(RGB_ANXIOUS_R, RGB_ANXIOUS_G, RGB_ANXIOUS_B);
        break;
    case STATE_DISTRESS:
        ws2812_send(RGB_DISTRESS_R, RGB_DISTRESS_G, RGB_DISTRESS_B);
        break;
    default:
        ws2812_send(0, 0, 0);
        break;
    }
}

static void buzzer_set(bool on)
{
#if BUZZER_ACTIVE_LOW
    gpio_set_level(PIN_BUZZER, on ? 0 : 1);
#else
    gpio_set_level(PIN_BUZZER, on ? 1 : 0);
#endif
}

static void motor_set(bool on)
{
    gpio_set_level(PIN_VIBRATION, on ? 1 : 0);
}

void actuators_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_BUZZER) | (1ULL << PIN_VIBRATION),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    buzzer_set(false);
    motor_set(false);

    ws2812_init();
    set_state_colour(STATE_CALM);
}

void actuator_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    int64_t distress_since_us = 0;

    for (;;) {
        band_state_t state = g_state;
        set_state_colour(state);

        int64_t now = esp_timer_get_time();
        if (state == STATE_DISTRESS) {
            if (distress_since_us == 0) {
                distress_since_us = now;
            } else if (now - distress_since_us >= (int64_t)ALERT_DELAY_MS * 1000) {
                buzzer_set(true);
                motor_set(true);
            }
        } else {
            distress_since_us = 0;
            buzzer_set(false);
            motor_set(false);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(ACTUATOR_PERIOD_MS));
    }
}
