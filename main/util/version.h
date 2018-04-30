#pragma once

#if defined(VERSION) && defined(GIT_REVISION)
#define SOFTWARE_VERSION VERSION " (" GIT_REVISION ")"
#elif defined(VERSION)
#define SOFTWARE_VERSION VERSION
#elif defined(GIT_REVISION)
#define SOFTWARE_VERSION GIT_REVISION
#else
#define SOFTWARE_VERSION "Unknown"
#endif

#define FIRMWARE_NAME "Raven"