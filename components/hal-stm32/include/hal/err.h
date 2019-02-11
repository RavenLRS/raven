#pragma once

typedef int hal_err_t;

#define HAL_ERR_NONE 0
#define HAL_ERR_BUSY -1
#define HAL_ERR_INVALID_ARG -2
// TODO: Crash if non-zero
#define HAL_ERR_ASSERT_OK(e) e