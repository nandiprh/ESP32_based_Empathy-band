#include "actuators.h"
#include "config.h"
#include <esp_arduino_version.h>

#define RGB_PWM_FREQ 5000
#define RGB_PWM_RES  8

#define CH_R 0
#define CH_G 1
#define CH_B 2

static uint32_t s_distressSinceMs = 0;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
#define LEDC_NEW_API 1
#endif

static void pwmAttach(int pin, int channel)
{
#ifdef LEDC_NEW_API
    (void)channel;
    ledcAttach(pin, RGB_PWM_FREQ, RGB_PWM_RES);
#else
    ledcSetup(channel, RGB_PWM_FREQ, RGB_PWM_RES);
    ledcAttachPin(pin, channel);
#endif
}

static void pwmWrite(int pin, int channel, uint8_t duty)
{
#ifdef LEDC_NEW_API
    (void)channel;
    ledcWrite(pin, duty);
#else
    (void)pin;
    ledcWrite(channel, duty);
#endif
}

static void showColour(uint8_t r, uint8_t g, uint8_t b)
{
#if RGB_COMMON_ANODE
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
#endif
    pwmWrite(PIN_RGB_R, CH_R, r);
    pwmWrite(PIN_RGB_G, CH_G, g);
    pwmWrite(PIN_RGB_B, CH_B, b);
}

static void buzzerSet(bool on)
{
#if BUZZER_ACTIVE_LOW
    digitalWrite(PIN_BUZZER, on ? LOW : HIGH);
#else
    digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
#endif
}

static void motorSet(bool on)
{
    digitalWrite(PIN_VIBRATION, on ? HIGH : LOW);
}

void actuatorsInit()
{
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_VIBRATION, OUTPUT);
    buzzerSet(false);
    motorSet(false);

    pwmAttach(PIN_RGB_R, CH_R);
    pwmAttach(PIN_RGB_G, CH_G);
    pwmAttach(PIN_RGB_B, CH_B);
    showColour(RGB_CALM_R, RGB_CALM_G, RGB_CALM_B);
}

void actuatorsUpdate(BandState state, uint32_t nowMs)
{
    switch (state) {
    case STATE_CALM:
        showColour(RGB_CALM_R, RGB_CALM_G, RGB_CALM_B);
        break;
    case STATE_ANXIOUS:
        showColour(RGB_ANXIOUS_R, RGB_ANXIOUS_G, RGB_ANXIOUS_B);
        break;
    case STATE_DISTRESS:
        showColour(RGB_DISTRESS_R, RGB_DISTRESS_G, RGB_DISTRESS_B);
        break;
    default:
        showColour(0, 0, 0);
        break;
    }

    if (state == STATE_DISTRESS) {
        if (s_distressSinceMs == 0) {
            s_distressSinceMs = nowMs;
        } else if (nowMs - s_distressSinceMs >= ALERT_DELAY_MS) {
            buzzerSet(true);
            motorSet(true);
        }
    } else {
        s_distressSinceMs = 0;
        buzzerSet(false);
        motorSet(false);
    }
}
