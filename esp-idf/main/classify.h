#pragma once

#include "band_types.h"

band_state_t classify(const band_inputs_t *in);
const char *band_state_name(band_state_t state);
