#include "config.h"
#include "band_types.h"
#include "classify.h"
#include "i2c_bus.h"
#include "gsr.h"
#include "sound.h"
#include "mpu6050.h"
#include "max30102.h"
#include "actuators.h"

static BandState s_state = STATE_CALM;

static GsrSample s_gsr;
static SoundSample s_sound;
static MpuSample s_mpu;

static uint32_t s_lastSensorMs = 0;
static uint32_t s_lastActuatorMs = 0;
static uint32_t s_soundAboveMs = 0;

static bool     s_havePrev = false;
static float    s_prevNorm = 0.0f;
static uint32_t s_prevUs = 0;

static volatile bool     s_buttonFlag = false;
static volatile uint32_t s_buttonUs = 0;
static uint32_t s_buttonLastUs = 0;

void IRAM_ATTR buttonISR()
{
    s_buttonUs = micros();
    s_buttonFlag = true;
}

static void sensorStep()
{
    gsrPoll(&s_gsr);
    soundPoll(&s_sound);
    mpu6050Poll(&s_mpu);
    max30102Poll();

    uint32_t nowUs = micros();
    float dt = (float)(nowUs - s_prevUs) / 1000000.0f;
    if (dt <= 0.0f) {
        dt = (float)SENSOR_PERIOD_MS / 1000.0f;
    }

    float slope = 0.0f;
    if (s_havePrev && gsrCalibrated()) {
        slope = (s_gsr.norm - s_prevNorm) / dt;
    }
    s_prevNorm = s_gsr.norm;
    s_prevUs = nowUs;
    s_havePrev = true;

    if (s_sound.norm >= SOUND_THRESH) {
        s_soundAboveMs += SENSOR_PERIOD_MS;
    } else {
        s_soundAboveMs = 0;
    }

    Max30102Stats hr;
    max30102GetStats(&hr);

    BandInputs in;
    in.gsrLevel = s_gsr.norm;
    in.gsrSlope = slope;
    in.soundLevel = s_sound.norm;
    in.soundSustained = s_soundAboveMs >= SOUND_SUSTAIN_MS;
    in.motionMagnitude = s_mpu.motion;
    in.bpm = hr.bpm;
    in.rmssdMs = hr.rmssdMs;
    in.beatValid = hr.beatValid;

    s_state = classify(&in);

    if (!gsrCalibrated()) {
        Serial.printf("[band] calibrating GSR rest baseline %.0f%%\n",
                      gsrCalibrationProgress() * 100.0f);
    } else {
        Serial.printf("[band] state=%s gsr=%.2f slope=%.2f sound=%.2f(%.1fs) "
                      "mot=%.2f bpm=%.0f rmssd=%.0f\n",
                      bandStateName(s_state), in.gsrLevel, in.gsrSlope,
                      in.soundLevel, s_soundAboveMs / 1000.0f,
                      in.motionMagnitude, in.bpm, in.rmssdMs);
    }
}

static void handleButton()
{
    if (!s_buttonFlag) {
        return;
    }
    s_buttonFlag = false;
    uint32_t ts = s_buttonUs;
    if ((ts - s_buttonLastUs) > (uint32_t)BUTTON_DEBOUNCE_MS * 1000) {
        s_buttonLastUs = ts;
        Serial.printf("[button] caregiver event mark @ %lu us\n", (unsigned long)ts);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println("Empathy Band starting");

    analogReadResolution(12);

    i2cBegin();
    gsrInit();
    soundInit();
    mpu6050Init();
    max30102Init();

    pinMode(PIN_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), buttonISR, FALLING);

    actuatorsInit();
}

void loop()
{
    uint32_t now = millis();

    if (now - s_lastSensorMs >= SENSOR_PERIOD_MS]) {
        s_lastSensorMs = now;
        sensorStep();
    }

    if (now - s_lastActuatorMs >= ACTUATOR_PERIOD_MS) {
        s_lastActuatorMs = now;
        actuatorsUpdate(s_state, now);
    }

    handleButton();
}
