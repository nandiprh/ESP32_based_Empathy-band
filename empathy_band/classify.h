#pragma once

#include "band_types.h"

BandState classify(const BandInputs *in);
const char *bandStateName(BandState state);
