#include "classify.h"
#include "band_config.h"

band_state_t classify(const band_inputs_t *in)
{
    int points = 0;

    if (in->gsr_level >= GSR_LEVEL_THRESH) {
        points++;
    }
    if (in->gsr_slope >= GSR_SLOPE_THRESH) {
        points++;
    }
    if (in->beat_valid && in->rmssd_ms < RMSSD_LOW_THRESH_MS) {
        points++;
    }
    if (in->sound_sustained) {
        points++;
    }

    bool moving = in->motion_magnitude >= MOTION_STILL_THRESH;

    if (points >= DISTRESS_MIN_POINTS && !moving) {
        return STATE_DISTRESS;
    }
    if (points >= 1) {
        return STATE_ANXIOUS;
    }
    return STATE_CALM;
}

const char *band_state_name(band_state_t state)
{
    switch (state) {
    case STATE_CALM:     return "CALM";
    case STATE_ANXIOUS:  return "ANXIOUS";
    case STATE_DISTRESS: return "DISTRESS";
    default:             return "?";
    }
}
