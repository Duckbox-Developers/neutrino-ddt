/*
 * dvbsubtitle.c: DVB subtitles
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original author: Marco Schlüßler <marco@lordzodiac.de>
 * With some input from the "subtitle plugin" by Pekka Virtanen <pekka.virtanen@sci.fi>
 *
 * $Id: dvbsubtitle.cpp,v 1.1 2009/02/23 19:46:44 rhabarber1848 Exp $
 * dvbsubtitle for HD1 ported by Coolstream LTD
 */

#include "dvbsubtitle.h"

extern "C" {
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavcodec/version.h>
}
#include <driver/framebuffer.h>
#include "Debug.hpp"

#if LIBAVCODEC_VERSION_INT <= AV_VERSION_INT(57, 1, 99)
	#define CODEC_DVB_SUB CODEC_ID_DVB_SUBTITLE
#else
	#define CODEC_DVB_SUB AV_CODEC_ID_DVB_SUBTITLE
#endif

// Set these to 'true' for debug output:
static bool DebugConverter = true;

#define dbgconverter(a...) if (DebugConverter) sub_debug.print(Debug::VERBOSE, a)

// --- cDvbSubtitleBitmaps ---------------------------------------------------

#if 0
class cDvbSubtitleBitmaps : public cListObject 
{
	private:
		int64_t pts;
		AVSubtitle sub;
	public:
		cDvbSubtitleBitmaps(int64_t Pts);
		~cDvbSubtitleBitmaps();
		int64_t Pts(void) { return pts; }
		int Timeout(void) { return sub.end_display_time; }
		void Draw(int &min_x, int &min_y, int &max_x, int &max_y);
		int Count(void) { return sub.num_rects; };
		AVSubtitle * GetSub(void) { return &sub; };
};
#endif

cDvbSubtitleBitmaps::cDvbSubtitleBitmaps(int64_t pPts)
{
	//dbgconverter("cDvbSubtitleBitmaps::new: PTS: %lld\n", pts);
	pts = pPts;
}

cDvbSubtitleBitmaps::~cDvbSubtitleBitmaps()
{
//    dbgconverter("cDvbSubtitleBitmaps::delete: PTS: %lld rects %d\n", pts, Count());
	avsubtitle_free(&sub);
}

fb_pixel_t * simple_resize32(uint8_t * orgin, uint32_t * colors, int nb_colors, int ox, int oy, int dx, int dy)
{
	fb_pixel_t  *cr,*l;
	int i,j,k,ip;

#if !HAVE_SPARK_HARDWARE && !HAVE_DUCKBOX_HARDWARE
	cr = (fb_pixel_t *) malloc(dx*dy*sizeof(fb_pixel_t));

	if(cr == NULL) {
		printf("Error: malloc\n");
		return NULL;
	}
#else
	cr = CFrameBuffer::getInstance()->getBackBufferPointer();
#endif
	l = cr;

	for(j = 0; j < dy; j++, l += dx)
	{
		uint8_t * p = orgin + (j*oy/dy*ox);
		for(i = 0, k = 0; i < dx; i++, k++) {
			ip = i*ox/dx;
			int idx = p[ip];
			if(idx < nb_colors)
				l[k] = colors[idx];
		}
	}
	return(cr);
}

void cDvbSubtitleBitmaps::Draw(int &min_x, int &min_y, int &max_x, int &max_y)
{
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
#define DEFAULT_XRES 1280	// backbuffer width
#define DEFAULT_YRES 720	// backbuffer height

	if (!Count())
		return;

	dbgconverter("cDvbSubtitleBitmaps::%s: start\n", __func__);

	CFrameBuffer* fb = CFrameBuffer::getInstance();
	fb_pixel_t *b = fb->getBackBufferPointer();

	// HACK. When having just switched channels we may not yet have yet
	// received valid authoring data. This check triggers for the most
	// common HD subtitle format and sets our authoring display format
	// accordingly. Plus, me may not get the authoring data at all, e.g. for
	// blu-ray playback.
	if (max_x == 720 && max_y == 576)
		switch (sub.rects[0]->w) {
		case 1280:
			min_x = min_y = 0, max_x = 1280, max_y = 720;
			break;
		case 1920:
			min_x = min_y = 0, max_x = 1920, max_y = 1080;
			break;
		}

	for (int i = 0; i < Count(); i++) {
		uint32_t * colors = (uint32_t *) sub.rects[i]->pict.data[1];
		int width = sub.rects[i]->w;
		int height = sub.rects[i]->h;
		uint8_t *origin = sub.rects[i]->pict.data[0];
		int nb_colors = sub.rects[i]->nb_colors;

		size_t bs = width * height;
		for (unsigned int j = 0; j < bs; j++)
			if (origin[j] < nb_colors)
				b[j] = colors[origin[j]];

		int width_new = (width * DEFAULT_XRES) / max_x;
		int height_new = (height * DEFAULT_YRES) / max_y;
		int x_new = (sub.rects[i]->x * DEFAULT_XRES) / max_x;
		int y_new = (sub.rects[i]->y * DEFAULT_YRES) / max_y;

		dbgconverter("cDvbSubtitleBitmaps::Draw: original bitmap=%d x=%d y=%d, w=%d, h=%d col=%d\n",
			i, sub.rects[i]->x, sub.rects[i]->y, width, height, sub.rects[i]->nb_colors);
		dbgconverter("cDvbSubtitleBitmaps::Draw: scaled bitmap=%d x_new=%d y_new=%d, w_new=%d, h_new=%d\n",
			i, x_new, y_new, width_new, height_new);
		fb->blitArea(width, height, x_new, y_new, width_new, height_new);
	}
	if (Count())
		fb->blit();

	dbgconverter("cDvbSubtitleBitmaps::%s: done\n", __func__);
#else
	int i;
	int stride = CFrameBuffer::getInstance()->getScreenWidth(true);
	int wd = CFrameBuffer::getInstance()->getScreenWidth();
	int xstart = CFrameBuffer::getInstance()->getScreenX();
	int yend = CFrameBuffer::getInstance()->getScreenY() + CFrameBuffer::getInstance()->getScreenHeight();
	int ystart = CFrameBuffer::getInstance()->getScreenY();
	uint32_t *sublfb = CFrameBuffer::getInstance()->getFrameBufferPointer();

	dbgconverter("cDvbSubtitleBitmaps::Draw: %d bitmaps, x= %d, width= %d yend=%d stride %d\n", Count(), xstart, wd, yend, stride);

	double xc = (double) CFrameBuffer::getInstance()->getScreenWidth(true)/(double) 720;
	double yc = (double) CFrameBuffer::getInstance()->getScreenHeight(true)/(double) 576;
	xc = yc; //FIXME should we scale also to full width ?
	int xf = int(xc * (double) 720);

	for (i = 0; i < Count(); i++) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 5, 0)
		uint32_t * colors = (uint32_t *) sub.rects[i]->pict.data[1];
#else
		uint32_t * colors = (uint32_t *) sub.rects[i]->data[1];
#endif
		int width = sub.rects[i]->w;
		int height = sub.rects[i]->h;
		int xoff, yoff;

		int nw = int(width == 1280 ? ((double) width / xc) : ((double) width * xc));
		int nh = int((double) height * yc);

		int xdiff = (wd > xf) ? ((wd - xf) / 2) : 0;
		xoff = int(sub.rects[i]->x*xc + xstart + xdiff);
		if(sub.rects[i]->y < 576/2) {
			yoff = int(ystart + sub.rects[i]->y*yc);
		} else {
			yoff = int(yend - ((width == 1280 ? 704:576) - (double) (sub.rects[i]->y + height))*yc - nh);
			if(yoff < ystart)
				yoff = ystart;
		}

		dbgconverter("cDvbSubtitleBitmaps::Draw: #%d at %d,%d size %dx%d colors %d (x=%d y=%d w=%d h=%d) \n", i+1, 
				sub.rects[i]->x, sub.rects[i]->y, sub.rects[i]->w, sub.rects[i]->h, sub.rects[i]->nb_colors, xoff, yoff, nw, nh);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 5, 0)
		fb_pixel_t * newdata = simple_resize32 (sub.rects[i]->pict.data[0], colors, sub.rects[i]->nb_colors, width, height, nw, nh);
#else
		fb_pixel_t * newdata = simple_resize32 (sub.rects[i]->data[0], colors, sub.rects[i]->nb_colors, width, height, nw, nh);
#endif
		fb_pixel_t * ptr = newdata;
		for (int y2 = 0; y2 < nh; y2++) {
			int y = (yoff + y2) * stride;
			for (int x2 = 0; x2 < nw; x2++)
				*(sublfb + xoff + x2 + y) = *ptr++;
		}
		free(newdata);

		if(min_x > xoff)
			min_x = xoff;
		if(min_y > yoff)
			min_y = yoff;
		if(max_x < (xoff + nw))
			max_x = xoff + nw;
		if(max_y < (yoff + nh))
			max_y = yoff + nh;
	}
	if(Count())
		dbgconverter("cDvbSubtitleBitmaps::Draw: finish, min/max screen: x=% d y= %d, w= %d, h= %d\n", min_x, min_y, max_x-min_x, max_y-min_y);
	dbgconverter("\n");
#endif
}

static int screen_w, screen_h, screen_x, screen_y;
// --- cDvbSubtitleConverter -------------------------------------------------

cDvbSubtitleConverter::cDvbSubtitleConverter(void)
{
	dbgconverter("cDvbSubtitleConverter: new converter\n");

	bitmaps = new cList<cDvbSubtitleBitmaps>;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&mutex, &attr);
	running = false;
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
	painted = false;
#endif

	avctx = NULL;
	avcodec = NULL;

	avcodec_register_all();
	avcodec = avcodec_find_decoder(CODEC_DVB_SUB);//CODEC_ID_DVB_SUBTITLE or AV_CODEC_ID_DVB_SUBTITLE from 57.1.100
	if (!avcodec) {
		dbgconverter("cDvbSubtitleConverter: unable to get dvb subtitle codec!\n");
		return;
	}
	avctx = avcodec_alloc_context3(avcodec);
	if (avcodec_open2(avctx, avcodec, NULL) < 0)
		dbgconverter("cDvbSubtitleConverter: unable to open codec !\n");

	av_log_set_level(AV_LOG_PANIC);
	//if(DebugConverter)
	//	av_log_set_level(AV_LOG_INFO);

	screen_w = min_x = CFrameBuffer::getInstance()->getScreenWidth();
	screen_h = min_y = CFrameBuffer::getInstance()->getScreenHeight();
	screen_x = max_x = CFrameBuffer::getInstance()->getScreenX();
	screen_y = max_y = CFrameBuffer::getInstance()->getScreenY();
	Timeout.Set(0xFFFF*1000);
}

cDvbSubtitleConverter::~cDvbSubtitleConverter()
{
	if (avctx) {
		avcodec_close(avctx);
		av_free(avctx);
		avctx = NULL;
	}
	delete bitmaps;
}

void cDvbSubtitleConverter::Lock(void)
{
  pthread_mutex_lock(&mutex);
}

void cDvbSubtitleConverter::Unlock(void)
{
  pthread_mutex_unlock(&mutex);
}

void cDvbSubtitleConverter::Pause(bool pause)
{
	dbgconverter("cDvbSubtitleConverter::Pause: %s\n", pause ? "pause" : "resume");
	if(pause) {
		if(!running)
			return;
		Lock();
		Clear();
		running = false;
		Unlock();
		//Reset();
	} else {
		// Assume that we've switched channel. Drop the existing display_definition.
		avctx->width = 0;
		avctx->height = 0;
		//Reset();
		running = true;
	}
}

void cDvbSubtitleConverter::Clear(void)
{
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
	if (running && painted) {
		CFrameBuffer::getInstance()->Clear();
		painted = false;
	}
#else
	dbgconverter("cDvbSubtitleConverter::Clear: x=% d y= %d, w= %d, h= %d\n", min_x, min_y, max_x-min_x, max_y-min_y);
	if(running && (max_x-min_x > 0) && (max_y-min_y > 0)) {
		CFrameBuffer::getInstance()->paintBackgroundBoxRel (min_x, min_y, max_x-min_x, max_y-min_y);
		//CFrameBuffer::getInstance()->paintBackground();
	}
#endif
}

void cDvbSubtitleConverter::Reset(void)
{
	dbgconverter("Converter reset -----------------------\n");
	Lock();
	bitmaps->Clear();
	Unlock();
	Timeout.Set(0xFFFF*1000);
}

int cDvbSubtitleConverter::Convert(AVSubtitle *sub, int64_t pts)
{
	cDvbSubtitleBitmaps *Bitmaps = new cDvbSubtitleBitmaps(pts);
	Bitmaps->SetSub(sub);
	bitmaps->Add(Bitmaps);
	return 0;
}

int cDvbSubtitleConverter::Convert(const uchar *Data, int Length, int64_t pts)
{
	AVPacket avpkt;
	int got_subtitle = 0;

	if(!avctx) {
		dbgconverter("cDvbSubtitleConverter::Convert: no context\n");
		return -1;
	}

	cDvbSubtitleBitmaps *Bitmaps = new cDvbSubtitleBitmaps(pts);

	AVSubtitle * sub = Bitmaps->GetSub();

	av_init_packet(&avpkt);
	avpkt.data = (uint8_t*) Data;
	avpkt.size = Length;

//	dbgconverter("cDvbSubtitleConverter::Convert: sub %x pkt %x pts %lld\n", sub, &avpkt, pts);
	//avctx->sub_id = (anc_page << 16) | comp_page; //FIXME not patched ffmpeg needs this !

	avcodec_decode_subtitle2(avctx, sub, &got_subtitle, &avpkt);
//	dbgconverter("cDvbSubtitleConverter::Convert: pts %lld subs ? %s, %d bitmaps\n", pts, got_subtitle? "yes" : "no", sub->num_rects);

	if(got_subtitle) {
		if(DebugConverter) {
			unsigned int i;
			for(i = 0; i < sub->num_rects; i++) {
//				dbgconverter("cDvbSubtitleConverter::Convert: #%d at %d,%d size %d x %d colors %d\n", i+1, 
//						sub->rects[i]->x, sub->rects[i]->y, sub->rects[i]->w, sub->rects[i]->h, sub->rects[i]->nb_colors);
			}
		}
		bitmaps->Add(Bitmaps);
	} else
		delete Bitmaps;

	return 0;
}

#define LimitTo32Bit(n) (n & 0x00000000FFFFFFFFL)
#define MAXDELTA 40000 // max. reasonable PTS/STC delta in ms
#define MIN_DISPLAY_TIME 1500
#define SHOW_DELTA 20
#define WAITMS 500

void dvbsub_get_stc(int64_t * STC);

int cDvbSubtitleConverter::Action(void)
{
	int WaitMs = WAITMS;

	if (!running)
		return 0;

	if(!avctx) {
		dbgconverter("cDvbSubtitleConverter::Action: no context\n");
		return -1;
	}

	min_x = min_y = 0;
	max_x = 720;
	max_y = 576;

	if (avctx->width && avctx->height) {
		min_x = 0;
		min_y = 0;
		max_x = avctx->width;
		max_y = avctx->height;
		dbgconverter("cDvbSubtitleConverter::Action: Display Definition: min_x=%d min_y=%d max_x=%d max_y=%d\n", min_x, min_y, max_x, max_y);
	}

	Lock();
	if (cDvbSubtitleBitmaps *sb = bitmaps->First()) {
		int64_t STC;
		dvbsub_get_stc(&STC);
		int64_t Delta = 0;

		Delta = LimitTo32Bit(sb->Pts()) - LimitTo32Bit(STC);
		Delta /= 90; // STC and PTS are in 1/90000s
//		dbgconverter("cDvbSubtitleConverter::Action: PTS: %016llx STC: %016llx (%lld) timeout: %d\n", sb->Pts(), STC, Delta, sb->Timeout());

		if (Delta <= MAXDELTA) {
			if (Delta <= SHOW_DELTA) {
				dbgconverter("cDvbSubtitleConverter::Action: PTS: %012llx STC: %012llx (%lld) timeout: %d bmp %d/%d\n", sb->Pts(), STC, Delta, sb->Timeout(), bitmaps->Count(), sb->Index() + 1);
//				dbgconverter("cDvbSubtitleConverter::Action: Got %d bitmaps, showing #%d\n", bitmaps->Count(), sb->Index() + 1);
				if (running) {
					Clear();
					sb->Draw(min_x, min_y, max_x, max_y);
					Timeout.Set(sb->Timeout());
				}
				if(sb->Count()) {
					WaitMs = MIN_DISPLAY_TIME;
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
					painted = true;
#endif
				}
				bitmaps->Del(sb, true);
			}
			else if (Delta < WaitMs)
				WaitMs = int((Delta > SHOW_DELTA) ? Delta - SHOW_DELTA : Delta);
		}
		else
		{
			dbgconverter("deleted because delta (%lld) > MAXDELTA (%d)\n", Delta, MAXDELTA);
			bitmaps->Del(sb, true);
		}
	} else {
		if (Timeout.TimedOut()) {
			dbgconverter("cDvbSubtitleConverter::Action: timeout, elapsed %lld\n", Timeout.Elapsed());
			Clear();
			Timeout.Set(0xFFFF*1000);
		}
	}
	Unlock();
	if(WaitMs != WAITMS)
		dbgconverter("cDvbSubtitleConverter::Action: finish, WaitMs %d\n", WaitMs);

	return WaitMs*1000;
}
