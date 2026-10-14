#pragma once

enum BandState {
    STATE_CALM = 0,
    STATE_ANXIOUS,
    STATE_DISTRESS,
};

struct BandInputs {
    float gsrLevel;
    float gsrSlope;
    float soundLevel;
    bool  soundSustained;
    float motionMagnitude;
    float bpm;
    float rmssdMs;
    bool  beatValid;
};
