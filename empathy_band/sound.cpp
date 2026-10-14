#include "sound.h"
#include "config.h"

void soundInit()
{
    pinMode(PIN_SOUND, INPUT);
}

void soundPoll(SoundSample *out)
{
    uint16_t mn = 4095;
    uint16_t mx = 0;

    for (int i = 0; i < SOUND_BURST_SAMPLES; i++) {
        uint16_t v = (uint16_t)analogRead(PIN_SOUND);
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
