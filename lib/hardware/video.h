#if USE_STB_HAL
#include <init.h>
#include <video_hal.h>
#else
#error neither USE_STB_HAL defined.
#error do you need to include config.h?
#endif
