#include "classify.h"
#include "config.h"

BandState classify(const BandInputs *in)
{
    int points = 0;

    if (in->gsrLevel >= GSR_LEVEL_THRESH) {
        points++;
    }
    if (in->gsrSlope >= GSR_SLOPE_THRESH) {
        points++;
    }
    if (in->beatValid && in->rmssdMs < RMSSD_LOW_THRESH_MS) {
        points++;
    }
    if (in->soundSustained) {
        points++;
    }

    bool moving = in->motionMagnitude >= MOTION_STILL_THRESH;

    if (points >= DISTRESS_MIN_POINTS && !moving) {
        return STATE_DISTRESS;
    }
    if (points >= 1) {
        return STATE_ANXIOUS;
    }
    return STATE_CALM;
}

const char *bandStateName(BandState state)
{
    switch (state) {
    case STATE_CALM:     return "CALM";
    case STATE_ANXIOUS:  return "ANXIOUS";
    case STATE_DISTRESS: return "DISTRESS";
    default:             return "?";
    }
}
