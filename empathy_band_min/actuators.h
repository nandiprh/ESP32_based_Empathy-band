#pragma once

#include <Arduino.h>
#include "band_types.h"

void actuatorsInit();
void actuatorsUpdate(BandState state, uint32_t nowMs);
