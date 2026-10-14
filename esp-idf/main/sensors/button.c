#include "button.h"
#include "band_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "button";
static QueueHandle_t s_queue;

static void IRAM_ATTR button_isr(void *arg)
{
    (void)arg;
    int64_t ts = esp_timer_get_time();
    BaseType_t hp_task_woken = pdFALSE;
    xQueueSendFromISR(s_queue, &ts, &hp_task_woken);
    if (hp_task_woken) {
        portYIELD_FROM_ISR(hp_task_woken);
    }
}

static void button_task(void *arg)
{
    (void)arg;
    int64_t ts;
    int64_t last_us = 0;

    for (;;) {
        if (xQueueReceive(s_queue, &ts, portMAX_DELAY) == pdPASS) {
            if (ts - last_us > (int64_t)BUTTON_DEBOUNCE_MS * 1000) {
                last_us = ts;
                ESP_LOGI(TAG, "caregiver event mark @ %lld us", (long long)ts);
            }
        }
    }
}

void button_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io);

    s_queue = xQueueCreate(8, sizeof(int64_t));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BUTTON, button_isr, NULL);

    xTaskCreate(button_task, "button", 2560, NULL, 4, NULL);
}
