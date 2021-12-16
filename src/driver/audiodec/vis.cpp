#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <math.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "int_fft.c"

typedef signed short gint16;
typedef int gint;
typedef float gfloat;
typedef double gdouble;

#define NUM_BANDS 16
#define WIDTH (120/NUM_BANDS)
#define HEIGHT 64
#define FALL 2
#define FALLOFF 1

#if 0
static int bar_heights[NUM_BANDS];
static int falloffs[NUM_BANDS];
static gdouble scale = 0;

static gint xscale128[] = { 0, 1, 2, 3, 4, 6, 8, 9, 10, 14, 20, 27, 37, 50, 67, 94, 127 };
static gint xscale256[] = { 0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255 };
static gint xscale512[] = { 0, 2, 4, 6, 10, 14, 20, 28, 40, 56, 80, 108, 148, 202, 274, 374, 510 };
#endif
//#define DEBUG
#if HAVE_DBOX2
#define SAMPLES 256
#define LOG 8
#define xscale xscale128
#else
#define SAMPLES 512
#define LOG 9
#define xscale xscale256
#endif

#if 0
static void do_fft(gint16 out_data[], gint16 in_data[])
{
	gint16 im[SAMPLES];

	memset(im, 0, sizeof(im));
	window(in_data, SAMPLES);
	fix_fft(in_data, im, LOG, 0);
	fix_loud(out_data, in_data, im, SAMPLES / 2, 0);
}
#endif
