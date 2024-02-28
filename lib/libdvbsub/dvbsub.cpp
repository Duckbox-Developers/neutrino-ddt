#include <config.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>

#include <cerrno>
#include <map>

#include <hardware/dmx.h>
#include <driver/framebuffer.h>

#include <poll.h>

#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
extern "C" {
#include <ass/ass.h>
}
#endif

#include <OpenThreads/ScopedLock>
#include <OpenThreads/Thread>
#include <OpenThreads/Condition>

#include "Debug.hpp"
#include "PacketQueue.hpp"
#include "helpers.hpp"
#include "dvbsubtitle.h"
#include <system/set_threadname.h>

Debug sub_debug;
static PacketQueue packet_queue;
static PacketQueue bitmap_queue;
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
static PacketQueue ass_queue;
static sem_t ass_sem;
#endif

static pthread_t threadReader;
static pthread_t threadDvbsub;
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
static pthread_t threadAss = 0;
#endif

static pthread_cond_t readerCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t readerMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t packetCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t packetMutex = PTHREAD_MUTEX_INITIALIZER;

#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
static OpenThreads::Mutex ass_mutex;
static std::map<int, ASS_Track *> ass_map;
static ASS_Library *ass_library = NULL;
static ASS_Renderer *ass_renderer = NULL;
static ASS_Track *ass_track = NULL;
#endif

static int reader_running;
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
static int ass_reader_running;
#endif
static int dvbsub_running;
static int dvbsub_paused = true;
static int dvbsub_pid;
static int dvbsub_stopped;
static int pid_change_req;
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
static bool isEplayer = false;
#endif

cDvbSubtitleConverter *dvbSubtitleConverter;
static void *reader_thread(void *arg);
static void *dvbsub_thread(void *arg);
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
static void *ass_reader_thread(void *arg);
#endif
static void clear_queue();

int dvbsub_init()
{
	int trc;

	sub_debug.set_level(0);

	reader_running = true;
	dvbsub_stopped = 1;
	pid_change_req = 1;
	// reader-Thread starten
	trc = pthread_create(&threadReader, 0, reader_thread, NULL);
	if (trc)
	{
		fprintf(stderr, "[dvb-sub] failed to create reader-thread (rc=%d)\n", trc);
		reader_running = false;
		return -1;
	}

	dvbsub_running = true;
	// subtitle decoder-Thread starten
	trc = pthread_create(&threadDvbsub, 0, dvbsub_thread, NULL);
	if (trc)
	{
		fprintf(stderr, "[dvb-sub] failed to create dvbsub-thread (rc=%d)\n", trc);
		dvbsub_running = false;
		return -1;
	}

#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	ass_reader_running = true;
	sem_init(&ass_sem, 0, 0);
	trc = pthread_create(&threadAss, 0, ass_reader_thread, NULL);
	if (trc)
	{
		fprintf(stderr, "[dvb-sub] failed to create %s-thread (rc=%d)\n", "ass-reader", trc);
		ass_reader_running = false;
		return -1;
	}
	pthread_detach(threadAss);
#endif
	return (0);
}

int dvbsub_pause()
{
	if (reader_running)
	{
		dvbsub_paused = true;
		if (dvbSubtitleConverter)
			dvbSubtitleConverter->Pause(true);

		printf("[dvb-sub] paused\n");
	}
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(ass_mutex);
	ass_track = NULL;
#endif

	return 0;
}

#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
int dvbsub_start(int pid, bool _isEplayer)
#else
int dvbsub_start(int pid)
#endif
{
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	isEplayer = _isEplayer;
	if (isEplayer && !dvbsub_paused)
		return 0;
	if (!isEplayer && !pid)
#else
	if (!pid)
#endif
		pid = -1;
	if (!dvbsub_paused && (pid < 0))
		return 0;

	if (pid > -1 && pid != dvbsub_pid)
	{
		dvbsub_pause();
		if (dvbSubtitleConverter)
			dvbSubtitleConverter->Reset();
		dvbsub_pid = pid;
		pid_change_req = 1;
	}
	printf("[dvb-sub] start, stopped %d pid %x\n", dvbsub_stopped, dvbsub_pid);
	if (dvbsub_pid > -1)
	{
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
		if (isEplayer)
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(ass_mutex);
			std::map<int, ASS_Track *>::iterator it = ass_map.find(dvbsub_pid);
			if (it != ass_map.end())
				ass_track = it->second;
			else
				ass_track = NULL; //FIXME
		}
#endif
		dvbsub_stopped = 0;
		dvbsub_paused = false;
		if (dvbSubtitleConverter)
			dvbSubtitleConverter->Pause(false);
		pthread_mutex_lock(&readerMutex);
		pthread_cond_broadcast(&readerCond);
		pthread_mutex_unlock(&readerMutex);
		printf("[dvb-sub] started with pid 0x%x\n", pid);
	}

	return 1;
}

static int flagFd = -1;

int dvbsub_stop()
{
	dvbsub_pid = -1;
	if (reader_running)
	{
		dvbsub_stopped = 1;
		dvbsub_pause();
		pid_change_req = 1;
		write(flagFd, "", 1);
	}

	return 0;
}

int dvbsub_getpid()
{
	return dvbsub_pid;
}

void dvbsub_setpid(int pid)
{
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	if (!isEplayer && !pid)
#else
	if (!pid)
#endif
		pid = -1;
	dvbsub_pid = pid;

	if (dvbsub_pid < 0)
		return;

	clear_queue();

	if (dvbSubtitleConverter)
		dvbSubtitleConverter->Reset();

	pid_change_req = 1;
	dvbsub_stopped = 0;

#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	if (isEplayer == false)
	{
		pthread_mutex_lock(&readerMutex);
		pthread_cond_broadcast(&readerCond);
		pthread_mutex_unlock(&readerMutex);
	}
#else
	pthread_mutex_lock(&readerMutex);
	pthread_cond_broadcast(&readerCond);
	pthread_mutex_unlock(&readerMutex);
#endif
}

int dvbsub_close()
{
	if (threadReader)
	{
		dvbsub_pause();
		reader_running = false;
		dvbsub_stopped = 1;
		write(flagFd, "", 1);

		pthread_mutex_lock(&readerMutex);
		pthread_cond_broadcast(&readerCond);
		pthread_mutex_unlock(&readerMutex);

// dvbsub_close() is called at shutdown time only. Instead of waiting for
// the threads to terminate just detach. --martii
		pthread_detach(threadReader);
		threadReader = 0;
	}
	if (threadDvbsub)
	{
		dvbsub_running = false;

		pthread_mutex_lock(&packetMutex);
		pthread_cond_broadcast(&packetCond);
		pthread_mutex_unlock(&packetMutex);

		pthread_detach(threadDvbsub);
		threadDvbsub = 0;
	}
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	if (ass_reader_running)
	{
		ass_reader_running = false;
		sem_post(&ass_sem);
	}
#endif
	printf("[dvb-sub] stopped\n");

	return 0;
}

static cDemux *dmx = NULL;

extern void getPlayerPts(int64_t *);

void dvbsub_get_stc(int64_t *STC)
{
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE // requires libeplayer3
	if (isEplayer)
	{
		getPlayerPts(STC);
		return;
	}
#endif
	if (dmx)
	{
		dmx->getSTC(STC);
		return;
	}
	*STC = 0;
}

static int64_t get_pts(unsigned char *packet)
{
	int64_t pts;
	int pts_dts_flag;

	pts_dts_flag = getbits(packet, 7 * 8, 2);
	if ((pts_dts_flag == 2) || (pts_dts_flag == 3))
	{
		pts = (uint64_t)getbits(packet, 9 * 8 + 4, 3) << 30; /* PTS[32..30] */
		pts |= getbits(packet, 10 * 8, 15) << 15;         /* PTS[29..15] */
		pts |= getbits(packet, 12 * 8, 15);               /* PTS[14..0] */
	}
	else
	{
		pts = 0;
	}
	return pts;
}

#define LimitTo32Bit(n) (n & 0x00000000FFFFFFFFL)

static int64_t get_pts_stc_delta(int64_t pts)
{
	int64_t stc, delta;

	dvbsub_get_stc(&stc);
	delta = LimitTo32Bit(pts) - LimitTo32Bit(stc);
	delta /= 90;
	return delta;
}

static void clear_queue()
{
	uint8_t *packet;
	cDvbSubtitleBitmaps *Bitmaps;

	pthread_mutex_lock(&packetMutex);
	while (packet_queue.size())
	{
		packet = packet_queue.pop();
		free(packet);
	}
	while (bitmap_queue.size())
	{
		Bitmaps = (cDvbSubtitleBitmaps *) bitmap_queue.pop();
		delete Bitmaps;
	}
	pthread_mutex_unlock(&packetMutex);
}

#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
extern "C" void dvbsub_ass_clear(void);
extern "C" void dvbsub_ass_write(AVCodecContext *c, AVSubtitle *sub, int pid);
extern "C" void dvbsub_write(AVSubtitle *sub, int64_t pts);
#endif
struct ass_data
{
	AVCodecContext *c;
	AVSubtitle sub;
	int pid;
	ass_data(AVCodecContext *_c, AVSubtitle *_sub, int _pid) : c(_c), pid(_pid)
	{
		memcpy(&sub, _sub, sizeof(sub));
	}
};

void dvbsub_ass_clear(void)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(ass_mutex);

	while (ass_queue.size())
	{
		ass_data *a = (ass_data *) ass_queue.pop();
		if (a)
		{
			avsubtitle_free(&a->sub);
			delete a;
		}
	}
	while (!sem_trywait(&ass_sem));

	ass_track = NULL;
	for (std::map<int, ASS_Track *>::iterator it = ass_map.begin(); it != ass_map.end(); ++it)
		ass_free_track(it->second);
	ass_map.clear();
	if (ass_renderer)
	{
		ass_renderer_done(ass_renderer);
		ass_renderer = NULL;
	}
	if (ass_library)
	{
		ass_library_done(ass_library);
		ass_library = NULL;
	}
	clear_queue();
}

// from neutrino.cpp
extern std::string *sub_font_file;
extern int sub_font_size;
static std::string ass_font;
static int ass_size;

#if HAVE_SH4_HARDWARE
// Thes functions below are based on ffmpeg-2.1.4/libavcodec/ass.c,
// Copyright (c) 2010 Aurelien Jacobs <aurel@gnuage.org>

// These are the FFMPEG defaults:
#define ASS_DEFAULT_FONT		"Arial"
#define ASS_DEFAULT_FONT_SIZE		16
#define ASS_DEFAULT_COLOR		0xffffff
#define ASS_DEFAULT_OUTLINE_COLOR	0
#define ASS_DEFAULT_BACK_COLOR		0
#define ASS_DEFAULT_BOLD		0
#define ASS_DEFAULT_ITALIC		0
#define ASS_DEFAULT_UNDERLINE		0
#define ASS_DEFAULT_ALIGNMENT		2

// And this is what we're going to use:
#define ASS_CUSTOM_FONT			"Arial"
#define ASS_CUSTOM_FONT_SIZE		36
#define ASS_CUSTOM_COLOR		0xffffff
#define ASS_CUSTOM_OUTLINE_COLOR	0
#define ASS_CUSTOM_BACK_COLOR		0
#define ASS_CUSTOM_BOLD			0
#define ASS_CUSTOM_ITALIC		0
#define ASS_CUSTOM_UNDERLINE		0
#define ASS_CUSTOM_ALIGNMENT		2

static std::string ass_subtitle_header(const char *font, int font_size,
	int color, int outline_color, int back_color, int bold, int italic, int underline, int alignment)
{
	char buf[8192];
	snprintf(buf, sizeof(buf),
		"[Script Info]\r\n"
		"ScriptType: v4.00+\r\n"
		"\r\n"
		"[V4+ Styles]\r\n"
		"Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding\r\n"
		"Style: Default,%s,%d,&H%x,&H%x,&H%x,&H%x,%d,%d,%d,1,1,0,%d,10,10,10,0,0\r\n"
		"\r\n"
		"[Events]\r\n"
		"Format: Layer, Start, End, Style, Text\r\n",
		font, font_size, color, color, outline_color, back_color, -bold, -italic, -underline, alignment);
	return std::string(buf);
}

static std::string ass_subtitle_header_default(void)
{
	return ass_subtitle_header(
			ASS_DEFAULT_FONT,
			ASS_DEFAULT_FONT_SIZE,
			ASS_DEFAULT_COLOR,
			ASS_DEFAULT_OUTLINE_COLOR,
			ASS_DEFAULT_BACK_COLOR,
			ASS_DEFAULT_BOLD,
			ASS_DEFAULT_ITALIC,
			ASS_DEFAULT_UNDERLINE,
			ASS_DEFAULT_ALIGNMENT);
}

static std::string ass_subtitle_header_custom(void)
{
	return ass_subtitle_header(
			ASS_CUSTOM_FONT,
			ASS_CUSTOM_FONT_SIZE,
			ASS_CUSTOM_COLOR,
			ASS_CUSTOM_OUTLINE_COLOR,
			ASS_CUSTOM_BACK_COLOR,
			ASS_CUSTOM_BOLD,
			ASS_CUSTOM_ITALIC,
			ASS_CUSTOM_UNDERLINE,
			ASS_CUSTOM_ALIGNMENT);
}
// The functions above are based on ffmpeg-2.1.4/libavcodec/ass.c,
// Copyright (c) 2010 Aurelien Jacobs <aurel@gnuage.org>
#endif

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
// Thes functions below are based on ffmpeg-3.0.7/libavcodec/ass.c,
// Copyright (c) 2010 Aurelien Jacobs <aurel@gnuage.org>

#define ASS_DEFAULT_PLAYRESX 384
#define ASS_DEFAULT_PLAYRESY 288

// These are the FFMPEG defaults:
#define ASS_DEFAULT_FONT                "Arial"
#define ASS_DEFAULT_FONT_SIZE           16
#define ASS_DEFAULT_COLOR               0xffffff
#define ASS_DEFAULT_BACK_COLOR          0
#define ASS_DEFAULT_BOLD                0
#define ASS_DEFAULT_ITALIC              0
#define ASS_DEFAULT_UNDERLINE           0
#define ASS_DEFAULT_BORDERSTYLE         0
#define ASS_DEFAULT_ALIGNMENT           2

// And this is what we're going to use:
#define ASS_CUSTOM_FONT                "Arial"
#define ASS_CUSTOM_FONT_SIZE           36
#define ASS_CUSTOM_COLOR               0xffffff
#define ASS_CUSTOM_BACK_COLOR          0
#define ASS_CUSTOM_BOLD                0
#define ASS_CUSTOM_ITALIC              0
#define ASS_CUSTOM_UNDERLINE           0
#define ASS_CUSTOM_BORDERSTYLE         0
#define ASS_CUSTOM_ALIGNMENT           2

static std::string ass_subtitle_header(const char *font, int font_size,
	int color, int back_color, int bold, int italic, int underline, int border_style, int alignment)
{
	char buf[8192];
	snprintf(buf, sizeof(buf),
		"[Script Info]\r\n"
		"; Script generated by FFmpeg/Lavc%s\r\n"
		"ScriptType: v4.00+\r\n"
		"PlayResX: %d\r\n"
		"PlayResY: %d\r\n"
		"\r\n"
		"[V4+ Styles]\r\n"
		/* ASSv4 header */
		"Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\r\n"
		"Style: Default,%s,%d,&H%x,&H%x,&H%x,&H%x,%d,%d,%d,0,100,100,0,0,%d,1,0,%d,10,10,10,0\r\n"
		"\r\n"
		"[Events]\r\n"
		"Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\r\n",
		AV_STRINGIFY(LIBAVCODEC_VERSION),
		ASS_DEFAULT_PLAYRESX, ASS_DEFAULT_PLAYRESY,
		font, font_size, color, color, back_color, back_color, -bold, -italic, -underline, border_style, alignment);
	return std::string(buf);
}

static std::string ass_subtitle_header_default(void)
{
	return ass_subtitle_header(
			ASS_DEFAULT_FONT,
			ASS_DEFAULT_FONT_SIZE,
			ASS_DEFAULT_COLOR,
			ASS_DEFAULT_BACK_COLOR,
			ASS_DEFAULT_BOLD,
			ASS_DEFAULT_ITALIC,
			ASS_DEFAULT_UNDERLINE,
			ASS_DEFAULT_BORDERSTYLE,
			ASS_DEFAULT_ALIGNMENT);
}

static std::string ass_subtitle_header_custom(void)
{
	return ass_subtitle_header(
			ASS_CUSTOM_FONT,
			ASS_CUSTOM_FONT_SIZE,
			ASS_CUSTOM_COLOR,
			ASS_CUSTOM_BACK_COLOR,
			ASS_CUSTOM_BOLD,
			ASS_CUSTOM_ITALIC,
			ASS_CUSTOM_UNDERLINE,
			ASS_CUSTOM_BORDERSTYLE,
			ASS_CUSTOM_ALIGNMENT);
}
// The functions above are based on ffmpeg-3.0.7/libavcodec/ass.c,
// Copyright (c) 2010 Aurelien Jacobs <aurel@gnuage.org>

#endif

static void *ass_reader_thread(void *)
{
	set_threadname("ass_reader_thread");
	while (!sem_wait(&ass_sem))
	{
		if (!ass_reader_running)
			break;

		ass_data *a = (ass_data *) ass_queue.pop();
		if (!a)
		{
			if (!ass_reader_running)
				break;
			continue;
		}

		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(ass_mutex);
		std::map<int, ASS_Track *>::iterator it = ass_map.find(a->pid);
		ASS_Track *track;
		if (it == ass_map.end())
		{
			CFrameBuffer *fb = CFrameBuffer::getInstance();

			int xres = fb->getScreenWidth(true);
			int yres = fb->getScreenHeight(true);

			if (!ass_library)
			{
				ass_library = ass_library_init();
				ass_set_extract_fonts(ass_library, 1);
				ass_set_style_overrides(ass_library, NULL);
				ass_renderer = ass_renderer_init(ass_library);
				ass_set_frame_size(ass_renderer, xres, yres);
				ass_set_margins(ass_renderer, 3 * yres / 100, 3 * yres / 100, 3 * xres / 100, 3 * xres / 100);
				ass_set_use_margins(ass_renderer, 1);
				ass_set_hinting(ass_renderer, ASS_HINTING_LIGHT);
				ass_set_aspect_ratio(ass_renderer, 1.0, 1.0);
				ass_font = *sub_font_file;
				ass_set_fonts(ass_renderer, ass_font.c_str(), "Arial", 0, NULL, 1);
			}
			track = ass_new_track(ass_library);
			track->PlayResX = xres;
			track->PlayResY = yres;
			ass_size = sub_font_size;
			ass_set_font_scale(ass_renderer, ((double) ass_size) / ASS_CUSTOM_FONT_SIZE);
			if (a->c->subtitle_header)
			{
				std::string ass_hdr = ass_subtitle_header_default();
				if (ass_hdr.compare((char *) a->c->subtitle_header))
				{
					ass_process_codec_private(track, (char *) a->c->subtitle_header, a->c->subtitle_header_size);
				}
				else
				{
					// This is the FFMPEG default ASS header. Use something more suitable instead:
					ass_hdr = ass_subtitle_header_custom();
					ass_process_codec_private(track, (char *) ass_hdr.c_str(), ass_hdr.length());
				}
			}
			ass_map[a->pid] = track;
			if (a->pid == dvbsub_pid)
				ass_track = track;
//fprintf(stderr, "### got subtitle track %d, subtitle header: \n---\n%s\n---\n", pid, c->subtitle_header);
		}
		else
			track = it->second;
		for (unsigned int i = 0; i < a->sub.num_rects; i++)
			if (a->sub.rects[i]->ass)
				ass_process_data(track, a->sub.rects[i]->ass, strlen(a->sub.rects[i]->ass));
		avsubtitle_free(&a->sub);
		delete a;
	}
	ass_reader_running = false;
	pthread_exit(NULL);
}

void dvbsub_ass_write(AVCodecContext *c, AVSubtitle *sub, int pid)
{
	if (ass_reader_running)
	{
		ass_queue.push((uint8_t *)new ass_data(c, sub, pid));
		sem_post(&ass_sem);
		memset(sub, 0, sizeof(AVSubtitle));
	}
	else
		avsubtitle_free(sub);
}

#endif

void dvbsub_write(AVSubtitle *sub, int64_t pts)
{
	pthread_mutex_lock(&packetMutex);
	cDvbSubtitleBitmaps *Bitmaps = new cDvbSubtitleBitmaps(pts);
	Bitmaps->SetSub(sub); // Note: this will copy sub, including all references. DON'T call avsubtitle_free() from the caller.
	memset(sub, 0, sizeof(AVSubtitle));
	bitmap_queue.push((unsigned char *) Bitmaps);
	pthread_cond_broadcast(&packetCond);
	pthread_mutex_unlock(&packetMutex);
}

static void *reader_thread(void * /*arg*/)
{
	int fds[2];
	pipe(fds);
	fcntl(fds[0], F_SETFD, FD_CLOEXEC);
	fcntl(fds[0], F_SETFL, O_NONBLOCK);
	fcntl(fds[1], F_SETFD, FD_CLOEXEC);
	fcntl(fds[1], F_SETFL, O_NONBLOCK);
	flagFd = fds[1];
	uint8_t tmp[16];  /* actually 6 should be enough */
	int count;
	int len;
	uint16_t packlen;
	uint8_t *buf;
	bool bad_startcode = false;
	set_threadname("dvbsub:reader");

	dmx = new cDemux(0);
	dmx->Open(DMX_PES_CHANNEL, NULL, 64 * 1024);

	while (reader_running)
	{
		if (dvbsub_stopped /*dvbsub_paused*/)
		{
			sub_debug.print(Debug::VERBOSE, "%s stopped\n", __FUNCTION__);
			dmx->Stop();

			pthread_mutex_lock(&packetMutex);
			pthread_cond_broadcast(&packetCond);
			pthread_mutex_unlock(&packetMutex);

			pthread_mutex_lock(&readerMutex);
			int ret = pthread_cond_wait(&readerCond, &readerMutex);
			pthread_mutex_unlock(&readerMutex);
			if (ret)
			{
				sub_debug.print(Debug::VERBOSE, "pthread_cond_timedwait fails with %d\n", ret);
			}
			if (!reader_running)
				break;
			dvbsub_stopped = 0;
			sub_debug.print(Debug::VERBOSE, "%s (re)started with pid 0x%x\n", __FUNCTION__, dvbsub_pid);
		}
		if (pid_change_req)
		{
			pid_change_req = 0;
			clear_queue();
			dmx->Stop();
			dmx->pesFilter(dvbsub_pid);
			dmx->Start();
			sub_debug.print(Debug::VERBOSE, "%s changed to pid 0x%x\n", __FUNCTION__, dvbsub_pid);
		}

		struct pollfd pfds[2];
		pfds[0].fd = fds[1];
		pfds[0].events = POLLIN;
		char _tmp[64];

#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
		if (isEplayer)
		{
			usleep(1000000);
/*			// ?? thread freezes with this...
			poll(pfds, 1, -1);
			while (0 > read(pfds[0].fd, _tmp, sizeof(tmp)));
*/
			continue;
		}
#endif

		pfds[1].fd = dmx->getFD();
		pfds[1].events = POLLIN;
		switch (poll(pfds, 2, -1))
		{
			case 0:
			case -1:
				if (pfds[0].revents & POLLIN)
					while (0 > read(pfds[0].fd, _tmp, sizeof(tmp)));
				continue;
			default:
				if (pfds[0].revents & POLLIN)
					while (0 > read(pfds[0].fd, _tmp, sizeof(tmp)));
				if (!(pfds[1].revents & POLLIN))
					continue;
		}

		len = dmx->Read(tmp, 6, 0);
		if (len <= 0)
			continue;

		if (!memcmp(tmp, "\x00\x00\x01\xbe", 4))  // padding stream
		{
			packlen =  getbits(tmp, 4 * 8, 16) + 6;
			count = 6;
			buf = (uint8_t *) malloc(packlen);

			// actually, we're doing slightly too much here ...
			memmove(buf, tmp, 6);
			/* read rest of the packet */
			while ((count < packlen) && !dvbsub_stopped)
			{
				len = dmx->Read(buf + count, packlen - count, 1000);
				if (len < 0)
				{
					break;
				}
				else
				{
					count += len;
				}
			}
			free(buf);
			buf = NULL;
			continue;
		}

		if (memcmp(tmp, "\x00\x00\x01\xbd", 4))
		{
			if (!bad_startcode)
			{
				sub_debug.print(Debug::VERBOSE, "[subtitles] bad start code: %02x%02x%02x%02x\n", tmp[0], tmp[1], tmp[2], tmp[3]);
				bad_startcode = true;
			}
			continue;
		}
		bad_startcode = false;
		count = 6;

		packlen =  getbits(tmp, 4 * 8, 16) + 6;

		buf = (uint8_t *) malloc(packlen);

		memmove(buf, tmp, 6);
		/* read rest of the packet */
		while ((count < packlen) && !dvbsub_stopped)
		{
			len = dmx->Read(buf + count, packlen - count, 1000);
			if (len < 0)
			{
				break;
			}
			else
			{
				count += len;
			}
		}

		if (!dvbsub_stopped /*!dvbsub_paused*/)
		{
			sub_debug.print(Debug::VERBOSE, "[subtitles] *** new packet, len %d buf 0x%x pts-stc diff %lld ***\n", count, buf, get_pts_stc_delta(get_pts(buf)));
			/* Packet now in memory */
			pthread_mutex_lock(&packetMutex);
			packet_queue.push(buf);
			/* TODO: allocation exception */
			// wake up dvb thread
			pthread_cond_broadcast(&packetCond);
			pthread_mutex_unlock(&packetMutex);
		}
		else
		{
			free(buf);
			buf = NULL;
		}
	}

	dmx->Stop();
	delete dmx;
	dmx = NULL;

	close(fds[0]);
	close(fds[1]);
	flagFd = -1;

	sub_debug.print(Debug::VERBOSE, "%s shutdown\n", __FUNCTION__);
	pthread_exit(NULL);
}

static void *dvbsub_thread(void * /*arg*/)
{
	struct timespec restartWait;
	struct timeval now;
	set_threadname("dvbsub:main");

	sub_debug.print(Debug::VERBOSE, "%s started\n", __FUNCTION__);
	if (!dvbSubtitleConverter)
		dvbSubtitleConverter = new cDvbSubtitleConverter;

	int timeout = 1000000;
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	CFrameBuffer *fb = CFrameBuffer::getInstance();
#if HAVE_SH4_HARDWARE
	int xres = fb->getScreenWidth(true);
	int yres = fb->getScreenHeight(true);
	int clr_x0 = xres, clr_y0 = yres, clr_x1 = 0, clr_y1 = 0;
#else
	int stride = fb->getStride() / 4;
	int xres = stride;
	int yres = fb->getScreenHeight(true);
	int clr_x0 = stride - yres / 4, clr_y0 = yres, clr_x1 = 0, clr_y1 = 0;
#endif
	uint32_t colortable[256];
	memset(colortable, 0, sizeof(colortable));
	uint32_t last_color = 0;
#endif

	while (dvbsub_running)
	{
#if HAVE_SH4_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
		if (ass_track)
		{
			usleep(100000); // FIXME ... should poll instead

			OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(ass_mutex);

			if (!ass_track)
				continue;

			if (ass_size != sub_font_size)
			{
				ass_size = sub_font_size;
				ass_set_font_scale(ass_renderer, ((double) ass_size) / ASS_CUSTOM_FONT_SIZE);
			}

			int detect_change = 0;
			int64_t pts;
			getPlayerPts(&pts);
			ASS_Image *image = ass_render_frame(ass_renderer, ass_track, pts / 90, &detect_change);

			if (detect_change)
			{
				if (clr_x1 && clr_y1)
				{
					fb->paintBox(clr_x0, clr_y0, clr_x1 + 1, clr_y1 + 1, 0);
					clr_x0 = xres;
					clr_y0 = yres;
					clr_x1 = clr_y1 = 0;
				}

				while (image)
				{
					if (last_color != image->color)
					{
						last_color = image->color;
						uint32_t c = last_color >> 8, a = 255 - (last_color & 0xff);
						for (int i = 0; i < 256; i++)
						{
							uint32_t k = (a * i) >> 8;
							colortable[i] = k ? (c | (k << 24)) : 0;
						}
					}
					if (image->w && image->h && image->dst_x > -1 && image->dst_x + image->w < xres && image->dst_y > -1 && image->dst_y + image->h < yres)
					{
						if (image->dst_x < clr_x0)
							clr_x0 = image->dst_x;
						if (image->dst_y < clr_y0)
							clr_y0 = image->dst_y;
						if (image->dst_x + image->w > clr_x1)
							clr_x1 = image->dst_x + image->w;
						if (image->dst_y + image->h > clr_y1)
							clr_y1 = image->dst_y + image->h;

						uint32_t *lfb = fb->getFrameBufferPointer() + image->dst_x + xres * image->dst_y;
						unsigned char *bm = image->bitmap;
						int bm_add = image->stride - image->w;
						int lfb_add = xres - image->w;
						for (int y = 0; y < image->h; y++)
						{
							for (int x = 0; x < image->w; x++)
							{
								if (*bm)
									*lfb = colortable[*bm];
								lfb++, bm++;
							}
							lfb += lfb_add;
							bm += bm_add;
						}
					}
					image = image->next;
				}
				fb->getInstance()->blit();
			}
			continue;
		}
		else
		{
			if (clr_x1 && clr_y1)
			{
				fb->paintBox(clr_x0, clr_y0, clr_x1 + 1, clr_y1 + 1, 0);
				clr_x0 = xres;
				clr_y0 = yres;
				clr_x1 = clr_y1 = 0;
				fb->getInstance()->blit();
			}
		}
#endif

		uint8_t *packet;
		int64_t pts;
		int dataoffset;
		int packlen;

		gettimeofday(&now, NULL);

		now.tv_usec += (timeout == 0) ? 1000000 : timeout;   // add the timeout
		while (now.tv_usec >= 1000000)     // take care of an overflow
		{
			now.tv_sec++;
			now.tv_usec -= 1000000;
		}
		restartWait.tv_sec = now.tv_sec;          // seconds
		restartWait.tv_nsec = now.tv_usec * 1000; // nano seconds

		pthread_mutex_lock(&packetMutex);
		pthread_cond_timedwait(&packetCond, &packetMutex, &restartWait);
		pthread_mutex_unlock(&packetMutex);

		timeout = dvbSubtitleConverter->Action();

		pthread_mutex_lock(&packetMutex);
		if (packet_queue.size() == 0 && bitmap_queue.size() == 0)
		{
			pthread_mutex_unlock(&packetMutex);
			continue;
		}
		sub_debug.print(Debug::VERBOSE, "PES: Wakeup, packet queue size %u, bitmap queue size %u\n", packet_queue.size(), bitmap_queue.size());
		if (dvbsub_stopped /*dvbsub_paused*/)
		{
			pthread_mutex_unlock(&packetMutex);
			clear_queue();
			continue;
		}
		if (packet_queue.size())
		{
			sub_debug.print(Debug::INFO, "*DVB packet*\n");
			packet = packet_queue.pop();
			pthread_mutex_unlock(&packetMutex);

			if (!packet)
			{
				sub_debug.print(Debug::VERBOSE, "Error no packet found\n");
				continue;
			}
			packlen = (packet[4] << 8 | packet[5]) + 6;

			pts = get_pts(packet);

			dataoffset = packet[8] + 8 + 1;
			if (packet[dataoffset] != 0x20)
			{
				sub_debug.print(Debug::VERBOSE, "Not a dvb subtitle packet, discard it (len %d)\n", packlen);
				goto next_round;
			}

			sub_debug.print(Debug::VERBOSE, "PES packet: len %d data len %d PTS=%Ld (%02d:%02d:%02d.%d) diff %lld\n",
				packlen, packlen - (dataoffset + 2), pts, (int)(pts / 324000000), (int)((pts / 5400000) % 60),
				(int)((pts / 90000) % 60), (int)(pts % 90000), get_pts_stc_delta(pts));

			if (packlen <= dataoffset + 3)
			{
				sub_debug.print(Debug::INFO, "Packet too short, discard\n");
				goto next_round;
			}

			if (packet[dataoffset + 2] == 0x0f)
			{
				dvbSubtitleConverter->Convert(&packet[dataoffset + 2],
					packlen - (dataoffset + 2), pts);
			}
			else
			{
				sub_debug.print(Debug::INFO, "End_of_PES is missing\n");
			}
next_round:
			if (packet)
				free(packet);
		}
		else
		{
			sub_debug.print(Debug::INFO, "*eplayer write*\n");
			cDvbSubtitleBitmaps *Bitmaps = (cDvbSubtitleBitmaps *) bitmap_queue.pop();
			pthread_mutex_unlock(&packetMutex);
			dvbSubtitleConverter->Convert(Bitmaps->GetSub(), Bitmaps->Pts());
		}
		timeout = dvbSubtitleConverter->Action();
	}

	delete dvbSubtitleConverter;

	sub_debug.print(Debug::VERBOSE, "%s shutdown\n", __FUNCTION__);
	pthread_exit(NULL);
}
