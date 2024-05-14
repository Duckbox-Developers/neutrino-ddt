/* helper for different display CVFD implementations */
#if HAVE_DUCKBOX_HARDWARE
#include <driver/vfd.h>
#endif
#if HAVE_SPARK_HARDWARE || HAVE_GENERIC_HARDWARE  || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#if BOXMODEL_DM8000 || BOXMODEL_DM820
#include <driver/lcdd.h>
#define CVFD CLCD
#else
#include <driver/simple_display.h>
#endif
#endif
#ifdef ENABLE_GRAPHLCD
#include <driver/nglcd.h>
#endif
