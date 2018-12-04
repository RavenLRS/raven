#pragma once

#include "target.h"

#if defined(USE_RADIO_FAKE)
#include "air/air_radio_fake.h"
#endif

#if defined(USE_RADIO_SX127X)
#include "air/air_radio_sx127x.h"
#endif