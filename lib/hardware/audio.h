#if USE_STB_HAL
#include <audio_hal.h>
#else
#error neither USE_STB_HAL defined.
#error do you need to include config.h?
#endif
