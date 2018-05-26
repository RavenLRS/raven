#pragma once

#include "util/time.h"

typedef void (*dispatch_fn)(void *);

void dispatch(dispatch_fn fn, void *data);
void dispatch_after(dispatch_fn fn, void *data, time_millis_t delay);