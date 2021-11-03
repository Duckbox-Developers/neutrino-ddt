/*
	Copyright (C) 2007-2013 Stefan Seyfried

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.

	private functions for the fbaccel class (only used in CFrameBuffer)
*/


#ifndef __fbaccel__
#define __fbaccel__
#include <config.h>

#include <stdint.h>
#include <linux/fb.h>
#include <linux/vt.h>

#include <string>
#include <map>
#include <vector>
#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>
#include <OpenThreads/Thread>
#include <OpenThreads/Condition>
#if HAVE_SH4_HARDWARE
#include <linux/stmfb.h>
#include <bpamem.h>
#endif

class CFrameBuffer;
class CFbAccel
{
	private:
		CFrameBuffer *fb;
		fb_pixel_t lastcol;
		OpenThreads::Mutex mutex;
#ifdef USE_NEVIS_GXA
		int		  devmem_fd;	/* to access the GXA register we use /dev/mem */
		unsigned int	  smem_start;	/* as aquired from the fbdev, the framebuffers physical start address */
		volatile uint8_t *gxa_base;	/* base address for the GXA's register access */
		void add_gxa_sync_marker(void);
#endif /* USE_NEVIS_GXA */
		void setColor(fb_pixel_t col);
		void run(void);
	public:
		fb_pixel_t *backbuffer;
		fb_pixel_t *lbb;
		CFbAccel(CFrameBuffer *fb);
		~CFbAccel();
		bool init(void);
		int setMode(void);
		void paintPixel(int x, int y, const fb_pixel_t col);
		void paintRect(const int x, const int y, const int dx, const int dy, const fb_pixel_t col);
		void paintLine(int xa, int ya, int xb, int yb, const fb_pixel_t col);
		void blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp, uint32_t yp, bool transp);
		void waitForIdle(void);
		void mark(int x, int y, int dx, int dy);
		void blit();
		void update();
#ifdef USE_NEVIS_GXA
		void setupGXA(void);
#endif
		int sX, sY, eX, eY;
		int startX, startY, endX, endY;
		t_fb_var_screeninfo s;
		fb_pixel_t borderColor, borderColorOld;
		void blitArea(int src_width, int src_height, int fb_x, int fb_y, int width, int height);
		void resChange(void);
		void blitBB2FB(int fx0, int fy0, int fx1, int fy1, int tx0, int ty0, int tx1, int ty2);
		void blitFB2FB(int fx0, int fy0, int fx1, int fy1, int tx0, int ty0, int tx1, int ty2);
		void blitBoxFB(int x0, int y0, int x1, int y1, fb_pixel_t color);
		void setBorder(int sx, int sy, int ex, int ey);
		void getBorder(int &sx, int &sy, int &ex, int &ey)
		{
			sx = startX, sy = startY, ex = endX, ey = endY;
		};
		void setBorderColor(fb_pixel_t col = 0);
		fb_pixel_t getBorderColor(void)
		{
			return borderColor;
		};
		void ClearFB(void);
#if HAVE_SH4_HARDWARE
		void blitBPA2FB(unsigned char *mem, SURF_FMT fmt, int w, int h, int x = 0, int y = 0, int pan_x = -1, int pan_y = -1, int fb_x = -1, int fb_y = -1, int fb_w = -1, int fb_h = -1, bool transp = false);
#endif
};

#endif
