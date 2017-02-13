/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
                      2003 thegoodguy
	Copyright (C) 2007-2013 Stefan Seyfried
	Copyright (C) 2017 TangoCash

	Framebuffer acceleration hardware abstraction functions.
	The various hardware dependent framebuffer acceleration functions
	are represented in this class.

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
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <driver/fb_generic_sti_ddt.h>
#include <driver/fb_accel_sti_ddt.h>
#ifdef ENABLE_GRAPHLCD
#include <driver/nglcd.h>
#endif

#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <memory.h>
#include <math.h>

#include <linux/kd.h>

#include <stdlib.h>

#include <driver/abstime.h>

/* note that it is *not* enough to just change those values */
#define DEFAULT_XRES 1280
#define DEFAULT_YRES 720
#define DEFAULT_BPP  32

static int bpafd = -1;
static size_t lbb_sz = 1920 * 1080;	/* offset from fb start in 'pixels' */
static size_t lbb_off = lbb_sz * sizeof(fb_pixel_t);	/* same in bytes */
static int backbuf_sz = 0;

void CFbAccel::waitForIdle(void)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	ioctl(fb->fd, STMFBIO_SYNC_BLITTER);
}

CFbAccel::CFbAccel(CFrameBuffer *_fb)
{
	fb = _fb;
	init();
	lastcol = 0xffffffff;
	lbb = fb->lfb;	/* the memory area to draw to... */
	if (fb->available < 12*1024*1024)
	{
		/* for old installations that did not upgrade their module config
		 * it will still work good enough to display the message below */
		fprintf(stderr, "[neutrino] WARNING: not enough framebuffer memory available!\n");
		fprintf(stderr, "[neutrino]          I need at least 12MB.\n");
		FILE *f = fopen("/tmp/infobar.txt", "w");
		if (f) {
			fprintf(f, "NOT ENOUGH FRAMEBUFFER MEMORY!");
			fclose(f);
		}
		lbb_sz = 0;
		lbb_off = 0;
	}
	lbb = fb->lfb + lbb_sz;
	bpafd = open("/dev/bpamem0", O_RDWR | O_CLOEXEC);
	if (bpafd < 0)
	{
		fprintf(stderr, "[neutrino] FB: cannot open /dev/bpamem0: %m\n");
		return;
	}
	backbuf_sz = DEFAULT_XRES * DEFAULT_YRES * sizeof(fb_pixel_t);
	BPAMemAllocMemData bpa_data;
#if BOXMODEL_OCTAGON1008 || BOXMODEL_FORTIS_HDBOX || BOXMODEL_CUBEREVO || BOXMODEL_CUBEREVO_MINI || BOXMODEL_CUBEREVO_MINI2 || BOXMODEL_CUBEREVO_250HD || BOXMODEL_CUBEREVO_2000HD || BOXMODEL_CUBEREVO_3000HD || BOXMODEL_IPBOX9900 || BOXMODEL_IPBOX99 || BOXMODEL_IPBOX55 || BOXMODEL_TF7700 || BOXMODEL_HL101
	bpa_data.bpa_part = (char *)"LMI_SYS";
#else
	bpa_data.bpa_part = (char *)"LMI_VID";
#endif
	bpa_data.mem_size = backbuf_sz;
	int res;
	res = ioctl(bpafd, BPAMEMIO_ALLOCMEM, &bpa_data);
	if (res)
	{
		fprintf(stderr, "[neutrino] FB: cannot allocate from bpamem: %m\n");
		fprintf(stderr, "backbuf_sz: %d\n", backbuf_sz);
		close(bpafd);
		bpafd = -1;
		return;
	}
	close(bpafd);

	char bpa_mem_device[30];
	sprintf(bpa_mem_device, "/dev/bpamem%d", bpa_data.device_num);
	bpafd = open(bpa_mem_device, O_RDWR | O_CLOEXEC);
	if (bpafd < 0)
	{
		fprintf(stderr, "[neutrino] FB: cannot open secondary %s: %m\n", bpa_mem_device);
		return;
	}

	backbuffer = (fb_pixel_t *)mmap(0, bpa_data.mem_size, PROT_WRITE|PROT_READ, MAP_SHARED, bpafd, 0);
	if (backbuffer == MAP_FAILED)
	{
		fprintf(stderr, "[neutrino] FB: cannot map from bpamem: %m\n");
		ioctl(bpafd, BPAMEMIO_FREEMEM);
		close(bpafd);
		bpafd = -1;
		return;
	}
	startX = 0;
	startY = 0;
	endX = DEFAULT_XRES - 1;
	endY = DEFAULT_YRES - 1;
	borderColor = 0;
	borderColorOld = 0x01010101;
	resChange();
};

CFbAccel::~CFbAccel()
{
	if (backbuffer)
	{
		fprintf(stderr, "CFbAccel: unmap backbuffer\n");
		munmap(backbuffer, backbuf_sz);
	}
	if (bpafd != -1)
	{
		fprintf(stderr, "CFbAccel: BPAMEMIO_FREEMEM\n");
		ioctl(bpafd, BPAMEMIO_FREEMEM);
		close(bpafd);
	}
	if (fb->lfb)
		munmap(fb->lfb, fb->available);
	if (fb->fd > -1)
		close(fb->fd);
}

void CFbAccel::update()
{
}

void CFbAccel::setColor(fb_pixel_t)
{
}

void CFbAccel::paintRect(const int x, const int y, const int dx, const int dy, const fb_pixel_t col)
{
	if (dx <= 0 || dy <= 0)
		return;

	// The STM blitter introduces considerable overhead probably worth for small areas.  --martii
	if (dx * dy < DEFAULT_XRES * 4) {
		waitForIdle();
		fb_pixel_t *fbs = fb->getFrameBufferPointer() + (DEFAULT_XRES * y) + x;
		fb_pixel_t *fbe = fbs + DEFAULT_XRES * (dy - 1) + dx;
		int off = DEFAULT_XRES - dx;
		while (fbs < fbe) {
			fb_pixel_t *ex = fbs + dx;
			while (fbs < ex)
				*fbs++ = col;
			fbs += off;
		}
		return;
	}

	/* function has const parameters, so copy them here... */
	int width = dx;
	int height = dy;
	int xx = x;
	int yy = y;
	/* maybe we should just return instead of fixing this up... */
	if (x < 0) {
		fprintf(stderr, "[neutrino] fb::%s: x < 0 (%d)\n", __func__, x);
		width += x;
		if (width <= 0)
			return;
		xx = 0;
	}

	if (y < 0) {
		fprintf(stderr, "[neutrino] fb::%s: y < 0 (%d)\n", __func__, y);
		height += y;
		if (height <= 0)
			return;
		yy = 0;
	}

	int right = xx + width;
	int bottom = yy + height;

	if (right > (int)fb->xRes) {
		if (xx >= (int)fb->xRes) {
			fprintf(stderr, "[neutrino] fb::%s: x >= xRes (%d > %d)\n", __func__, xx, fb->xRes);
			return;
		}
		fprintf(stderr, "[neutrino] fb::%s: x+w > xRes! (%d+%d > %d)\n", __func__, xx, width, fb->xRes);
		right = fb->xRes;
	}
	if (bottom > (int)fb->yRes) {
		if (yy >= (int)fb->yRes) {
			fprintf(stderr, "[neutrino] fb::%s: y >= yRes (%d > %d)\n", __func__, yy, fb->yRes);
			return;
		}
		fprintf(stderr, "[neutrino] fb::%s: y+h > yRes! (%d+%d > %d)\n", __func__, yy, height, fb->yRes);
		bottom = fb->yRes;
	}

	STMFBIO_BLT_DATA bltData;
	memset(&bltData, 0, sizeof(STMFBIO_BLT_DATA));

	bltData.operation  = BLT_OP_FILL;
	bltData.dstOffset  = lbb_off;
	bltData.dstPitch   = fb->stride;

	bltData.dst_left   = xx;
	bltData.dst_top    = yy;
	bltData.dst_right  = right;
	bltData.dst_bottom = bottom;

	bltData.dstFormat  = SURF_ARGB8888;
	bltData.srcFormat  = SURF_ARGB8888;
	bltData.dstMemBase = STMFBGP_FRAMEBUFFER;
	bltData.srcMemBase = STMFBGP_FRAMEBUFFER;
	bltData.colour     = col;

	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if (ioctl(fb->fd, STMFBIO_BLT, &bltData ) < 0)
		fprintf(stderr, "blitRect FBIO_BLIT: %m x:%d y:%d w:%d h:%d s:%d\n", xx,yy,width,height,fb->stride);
}

void CFbAccel::paintPixel(const int x, const int y, const fb_pixel_t col)
{
	fb_pixel_t *pos = fb->getFrameBufferPointer();
	pos += (fb->stride / sizeof(fb_pixel_t)) * y;
	pos += x;
	*pos = col;
}

void CFbAccel::paintLine(int xa, int ya, int xb, int yb, const fb_pixel_t col)
{
	int dx = abs (xa - xb);
	int dy = abs (ya - yb);
	if (dy == 0) /* horizontal line */
	{
		/* paintRect actually is 1 pixel short to the right,
		 * but that's bug-compatibility with the GXA code */
		paintRect(xa, ya, xb - xa, 1, col);
		return;
	}
	if (dx == 0) /* vertical line */
	{
		paintRect(xa, ya, 1, yb - ya, col);
		return;
	}
	int x;
	int y;
	int End;
	int step;

	if (dx > dy)
	{
		int p = 2 * dy - dx;
		int twoDy = 2 * dy;
		int twoDyDx = 2 * (dy-dx);

		if (xa > xb)
		{
			x = xb;
			y = yb;
			End = xa;
			step = ya < yb ? -1 : 1;
		}
		else
		{
			x = xa;
			y = ya;
			End = xb;
			step = yb < ya ? -1 : 1;
		}

		paintPixel(x, y, col);

		while (x < End)
		{
			x++;
			if (p < 0)
				p += twoDy;
			else
			{
				y += step;
				p += twoDyDx;
			}
			paintPixel(x, y, col);
		}
	}
	else
	{
		int p = 2 * dx - dy;
		int twoDx = 2 * dx;
		int twoDxDy = 2 * (dx-dy);

		if (ya > yb)
		{
			x = xb;
			y = yb;
			End = ya;
			step = xa < xb ? -1 : 1;
		}
		else
		{
			x = xa;
			y = ya;
			End = yb;
			step = xb < xa ? -1 : 1;
		}

		paintPixel(x, y, col);

		while (y < End)
		{
			y++;
			if (p < 0)
				p += twoDx;
			else
			{
				x += step;
				p += twoDxDy;
			}
			paintPixel(x, y, col);
		}
	}
}

void CFbAccel::blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp, uint32_t yp, bool transp)
{
	int x, y, dw, dh;
	x = xoff;
	y = yoff;
	dw = width - xp;
	dh = height - yp;

	size_t mem_sz = width * height * sizeof(fb_pixel_t);
	unsigned long ulFlags = 0;
	if (!transp) /* transp == false (default): use transparency from source alphachannel */
		ulFlags = BLT_OP_FLAGS_BLEND_SRC_ALPHA|BLT_OP_FLAGS_BLEND_DST_MEMORY; // we need alpha blending

	STMFBIO_BLT_EXTERN_DATA blt_data;
	memset(&blt_data, 0, sizeof(STMFBIO_BLT_EXTERN_DATA));
	blt_data.operation  = BLT_OP_COPY;
	blt_data.ulFlags    = ulFlags;
	blt_data.srcOffset  = 0;
	blt_data.srcPitch   = width * 4;
	blt_data.dstOffset  = lbb_off;
	blt_data.dstPitch   = fb->stride;
	blt_data.src_left   = xp;
	blt_data.src_top    = yp;
	blt_data.src_right  = width;
	blt_data.src_bottom = height;
	blt_data.dst_left   = x;
	blt_data.dst_top    = y;
	blt_data.dst_right  = x + dw;
	blt_data.dst_bottom = y + dh;
	blt_data.srcFormat  = SURF_ARGB8888;
	blt_data.dstFormat  = SURF_ARGB8888;
	blt_data.srcMemBase = (char *)backbuffer;
	blt_data.dstMemBase = (char *)fb->lfb;
	blt_data.srcMemSize = mem_sz;
	blt_data.dstMemSize = fb->stride * fb->yRes + lbb_off;

	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
#if 0
	ioctl(fb->fd, STMFBIO_SYNC_BLITTER);
#endif
	if (fbbuff != backbuffer)
		memmove(backbuffer, fbbuff, mem_sz);
	// icons are so small that they will still be in cache
	msync(backbuffer, backbuf_sz, MS_SYNC);

	if (ioctl(fb->fd, STMFBIO_BLT_EXTERN, &blt_data) < 0)
		perror("CFbAccel blit2FB STMFBIO_BLT_EXTERN");
	return;
}

void CFbAccel::blitBB2FB(int fx0, int fy0, int fx1, int fy1, int tx0, int ty0, int tx1, int ty1)
{
	STMFBIO_BLT_DATA  bltData;
	memset(&bltData, 0, sizeof(STMFBIO_BLT_DATA));
	bltData.operation  = BLT_OP_COPY;
	bltData.srcOffset  = lbb_off;
	bltData.srcPitch   = fb->stride;
	bltData.src_left   = fx0;
	bltData.src_top    = fy0;
	bltData.src_right  = fx1;
	bltData.src_bottom = fy1;
	bltData.srcFormat = SURF_BGRA8888;
	bltData.srcMemBase = STMFBGP_FRAMEBUFFER;
	bltData.dstPitch   = s.xres * 4;
	bltData.dstFormat = SURF_BGRA8888;
	bltData.dstMemBase = STMFBGP_FRAMEBUFFER;
	bltData.dst_left   = tx0;
	bltData.dst_top    = ty0;
	bltData.dst_right  = tx1;
	bltData.dst_bottom = ty1;
	if (ioctl(fb->fd, STMFBIO_BLT, &bltData ) < 0)
		perror("STMFBIO_BLT");
}

void CFbAccel::blitFB2FB(int fx0, int fy0, int fx1, int fy1, int tx0, int ty0, int tx1, int ty1)
{
	STMFBIO_BLT_DATA  bltData;
	memset(&bltData, 0, sizeof(STMFBIO_BLT_DATA));
	bltData.operation  = BLT_OP_COPY;
	bltData.srcPitch   = s.xres * 4;
	bltData.src_left   = fx0;
	bltData.src_top    = fy0;
	bltData.src_right  = fx1;
	bltData.src_bottom = fy1;
	bltData.srcFormat = SURF_BGRA8888;
	bltData.srcMemBase = STMFBGP_FRAMEBUFFER;
	bltData.dstPitch   = bltData.srcPitch;
	bltData.dstFormat = SURF_BGRA8888;
	bltData.dstMemBase = STMFBGP_FRAMEBUFFER;
	bltData.dst_left   = tx0;
	bltData.dst_top    = ty0;
	bltData.dst_right  = tx1;
	bltData.dst_bottom = ty1;
	if (ioctl(fb->fd, STMFBIO_BLT, &bltData ) < 0)
		perror("STMFBIO_BLT");
}

void CFbAccel::blitBoxFB(int x0, int y0, int x1, int y1, fb_pixel_t color)
{
	if (x0 > -1 && y0 > -1 && x0 < x1 && y0 < y1) {
		STMFBIO_BLT_DATA  bltData;
		memset(&bltData, 0, sizeof(STMFBIO_BLT_DATA));
		bltData.operation  = BLT_OP_FILL;
		bltData.dstPitch   = s.xres * 4;
		bltData.dstFormat  = SURF_ARGB8888;
		bltData.srcFormat  = SURF_ARGB8888;
		bltData.dstMemBase = STMFBGP_FRAMEBUFFER;
		bltData.srcMemBase = STMFBGP_FRAMEBUFFER;
		bltData.colour     = color;
		bltData.dst_left   = x0;
		bltData.dst_top    = y0;
		bltData.dst_right  = x1;
		bltData.dst_bottom = y1;
		if (ioctl(fb->fd, STMFBIO_BLT, &bltData ) < 0)
			perror("STMFBIO_BLT");
	}
}

void CFbAccel::blit()
{
#ifdef ENABLE_GRAPHLCD
	nGLCD::Blit();
#endif
	msync(lbb, DEFAULT_XRES * 4 * DEFAULT_YRES, MS_SYNC);

	if (borderColor != borderColorOld || (borderColor != 0x00000000 && borderColor != 0xFF000000)) {
		borderColorOld = borderColor;
		switch(fb->mode3D) {
		case CFrameBuffer::Mode3D_off:
		default:
			blitBoxFB(0, 0, s.xres, sY, borderColor);		// top
			blitBoxFB(0, 0, sX, s.yres, borderColor);	// left
			blitBoxFB(eX, 0, s.xres, s.yres, borderColor);	// right
			blitBoxFB(0, eY, s.xres, s.yres, borderColor);	// bottom
			break;
		case CFrameBuffer::Mode3D_SideBySide:
			blitBoxFB(0, 0, s.xres, sY, borderColor);			// top
			blitBoxFB(0, 0, sX/2, s.yres, borderColor);			// left
			blitBoxFB(eX/2 + 1, 0, s.xres/2 + sX/2, s.yres, borderColor);	// middle
			blitBoxFB(s.xres/2 + eX/2 + 1, 0, s.xres, s.yres, borderColor);	// right
			blitBoxFB(0, eY, s.xres, s.yres, borderColor);			// bottom
			break;
		case CFrameBuffer::Mode3D_TopAndBottom:
			blitBoxFB(0, 0, s.xres, sY/2, borderColor); 			// top
			blitBoxFB(0, eY/2 + 1, s.xres, s.yres/2 + sY/2, borderColor); 	// middle
			blitBoxFB(0, s.yres/2 + eY/2 + 1, s.xres, s.yres, borderColor); // bottom
			blitBoxFB(0, 0, sX, s.yres, borderColor);			// left
			blitBoxFB(eX, 0, s.xres, s.yres, borderColor);			// right
			break;
		case CFrameBuffer::Mode3D_Tile:
			blitBoxFB(0, 0, (s.xres * 2)/3, (sY * 2)/3, borderColor);		// top
			blitBoxFB(0, 0, (sX * 2)/3, (s.yres * 2)/3, borderColor);		// left
			blitBoxFB((eX * 2)/3, 0, (s.xres * 2)/3, (s.yres * 2)/3, borderColor);	// right
			blitBoxFB(0, (eY * 2)/3, (s.xres * 2)/3, (s.yres * 2)/3, borderColor);	// bottom
			break;
		}
	}
	switch(fb->mode3D) {
	case CFrameBuffer::Mode3D_off:
	default:
		blitBB2FB(0, 0, DEFAULT_XRES, DEFAULT_YRES, sX, sY, eX, eY);
		break;
	case CFrameBuffer::Mode3D_SideBySide:
		blitBB2FB(0, 0, DEFAULT_XRES, DEFAULT_YRES, sX/2, sY, eX/2, eY);
		blitBB2FB(0, 0, DEFAULT_XRES, DEFAULT_YRES, s.xres/2 + sX/2, sY, s.xres/2 + eX/2, eY);
		break;
	case CFrameBuffer::Mode3D_TopAndBottom:
		blitBB2FB(0, 0, DEFAULT_XRES, DEFAULT_YRES, sX, sY/2, eX, eY/2);
		blitBB2FB(0, 0, DEFAULT_XRES, DEFAULT_YRES, sX, s.yres/2 + sY/2, eX, s.yres/2 + eY/2);
		break;
	case CFrameBuffer::Mode3D_Tile:
		blitBB2FB(0, 0, DEFAULT_XRES, DEFAULT_YRES, (sX * 2)/3, (sY * 2)/3, (eX * 2)/3, (eY * 2)/3);
		blitFB2FB(0, 0, s.xres/3, (s.yres * 2)/3, (s.xres * 2)/3, 0, s.xres, (s.yres * 2)/3);
		blitFB2FB(s.xres/3, 0, (s.xres * 2)/3, s.yres/3, 0, (s.yres * 2)/3, s.xres/3, s.yres);
		blitFB2FB(s.xres/3, s.yres/3, (s.xres * 2)/3, (s.yres * 2)/3, s.xres/3, (s.yres * 2)/3, (s.xres * 2)/3, s.yres);
		break;
	}
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if(ioctl(fb->fd, STMFBIO_SYNC_BLITTER) < 0)
		perror("CFrameBuffer::blit ioctl STMFBIO_SYNC_BLITTER 2");
}

void CFbAccel::mark(int, int, int, int)
{
}

void CFbAccel::blitBPA2FB(unsigned char *mem, SURF_FMT fmt, int w, int h, int x, int y, int pan_x, int pan_y, int fb_x, int fb_y, int fb_w, int fb_h, bool transp)
{
	if (w < 1 || h < 1)
		return;
	if (fb_x < 0)
		fb_x = x;
	if (fb_y < 0)
		fb_y = y;
	if (pan_x < 0 || pan_x > w - x)
		pan_x = w - x;
	if (pan_y < 0 || pan_y > h - y)
		pan_y = h - y;
	if (fb_w < 0)
		fb_w = pan_x;
	if (fb_h < 0)
		fb_h = pan_y;

	STMFBIO_BLT_EXTERN_DATA blt_data;
	memset(&blt_data, 0, sizeof(STMFBIO_BLT_EXTERN_DATA));
	blt_data.operation  = BLT_OP_COPY;
	if (!transp) /* transp == false (default): use transparency from source alphachannel */
		blt_data.ulFlags = BLT_OP_FLAGS_BLEND_SRC_ALPHA|BLT_OP_FLAGS_BLEND_DST_MEMORY; // we need alpha blending
//	blt_data.srcOffset  = 0;
	switch (fmt) {
	case SURF_RGB888:
	case SURF_BGR888:
		blt_data.srcPitch   = w * 3;
		break;
	default: // FIXME, this is wrong for quite a couple of formats which are currently not in use
		blt_data.srcPitch   = w * 4;
	}
	blt_data.dstOffset  = lbb_off;
	blt_data.dstPitch   = fb->stride;
	blt_data.src_left   = x;
	blt_data.src_top    = y;
	blt_data.src_right  = x + pan_x;
	blt_data.src_bottom = y + pan_y;
	blt_data.dst_left   = fb_x;
	blt_data.dst_top    = fb_y;
	blt_data.dst_right  = fb_x + fb_w;
	blt_data.dst_bottom = fb_y + fb_h;
	blt_data.srcFormat  = fmt;
	blt_data.dstFormat  = SURF_ARGB8888;
	blt_data.srcMemBase = (char *)mem;
	blt_data.dstMemBase = (char *)fb->lfb;
	blt_data.srcMemSize = blt_data.srcPitch * h;
	blt_data.dstMemSize = fb->stride * DEFAULT_YRES + lbb_off;

	msync(mem, blt_data.srcPitch * h, MS_SYNC);

	if(ioctl(fb->fd, STMFBIO_BLT_EXTERN, &blt_data) < 0)
		perror("blitBPA2FB FBIO_BLIT");
}

void CFbAccel::blitArea(int src_width, int src_height, int fb_x, int fb_y, int width, int height)
{
	if (!src_width || !src_height)
		return;
	STMFBIO_BLT_EXTERN_DATA blt_data;
	memset(&blt_data, 0, sizeof(STMFBIO_BLT_EXTERN_DATA));
	blt_data.operation  = BLT_OP_COPY;
	blt_data.ulFlags    = BLT_OP_FLAGS_BLEND_SRC_ALPHA | BLT_OP_FLAGS_BLEND_DST_MEMORY;	// we need alpha blending
//	blt_data.srcOffset  = 0;
	blt_data.srcPitch   = src_width * 4;
	blt_data.dstOffset  = lbb_off;
	blt_data.dstPitch   = fb->stride;
//	blt_data.src_top    = 0;
//	blt_data.src_left   = 0;
	blt_data.src_right  = src_width;
	blt_data.src_bottom = src_height;
	blt_data.dst_left   = fb_x;
	blt_data.dst_top    = fb_y;
	blt_data.dst_right  = fb_x + width;
	blt_data.dst_bottom = fb_y + height;
	blt_data.srcFormat  = SURF_ARGB8888;
	blt_data.dstFormat  = SURF_ARGB8888;
	blt_data.srcMemBase = (char *)backbuffer;
	blt_data.dstMemBase = (char *)fb->lfb;
	blt_data.srcMemSize = backbuf_sz;
	blt_data.dstMemSize = fb->stride * DEFAULT_YRES + lbb_off;

	msync(backbuffer, blt_data.srcPitch * src_height, MS_SYNC);

	if(ioctl(fb->fd, STMFBIO_BLT_EXTERN, &blt_data) < 0)
		perror("blitArea FBIO_BLIT");
}

void CFbAccel::resChange(void)
{
	if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &s) == -1)
		perror("frameBuffer <FBIOGET_VSCREENINFO>");

	sX = (startX * s.xres)/DEFAULT_XRES;
	sY = (startY * s.yres)/DEFAULT_YRES;
	eX = (endX * s.xres)/DEFAULT_XRES;
	eY = (endY * s.yres)/DEFAULT_YRES;
	borderColorOld = 0x01010101;
}

void CFbAccel::setBorder(int sx, int sy, int ex, int ey)
{
	startX = sx;
	startY = sy;
	endX = ex;
	endY = ey;
	sX = (startX * s.xres)/DEFAULT_XRES;
	sY = (startY * s.yres)/DEFAULT_YRES;
	eX = (endX * s.xres)/DEFAULT_XRES;
	eY = (endY * s.yres)/DEFAULT_YRES;
	borderColorOld = 0x01010101;
}

void CFbAccel::setBorderColor(fb_pixel_t col)
{
	if (!col && borderColor)
		blitBoxFB(0, 0, s.xres, s.yres, 0);
	borderColor = col;
}

void CFbAccel::ClearFB(void)
{
	blitBoxFB(0, 0, s.xres, s.yres, 0);
}

bool CFbAccel::init(void)
{
	fb_pixel_t *lfb;
	fb->lfb = NULL;
	fb->fd = -1;
	int fd;
	fd = open("/dev/fb0", O_RDWR|O_CLOEXEC);
	if (fd < 0) {
		perror("open /dev/fb0");
		return false;
	}
	fb->fd = fd;

	if (ioctl(fd, FBIOGET_VSCREENINFO, &fb->screeninfo) < 0) {
		perror("FBIOGET_VSCREENINFO");
		return false;
	}

	if (ioctl(fd, FBIOGET_FSCREENINFO, &fb->fix) < 0) {
		perror("FBIOGET_FSCREENINFO");
		return false;
	}

	fb->available = fb->fix.smem_len;
	printf("%dk video mem\n", fb->available / 1024);
	lfb = (fb_pixel_t *)mmap(0, fb->available, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);

	if (lfb == MAP_FAILED) {
		perror("mmap");
		return false;;
	}

	memset(lfb, 0, fb->available);
	fb->lfb = lfb;
	return true;
}

/* wrong name... */
int CFbAccel::setMode(void)
{
	int fd = fb->fd;
	t_fb_var_screeninfo *si = &fb->screeninfo;
	/* it's all fake... :-) */
	si->xres = si->xres_virtual = DEFAULT_XRES;
	si->yres = si->yres_virtual = DEFAULT_YRES;
	si->bits_per_pixel = DEFAULT_BPP;
	fb->stride = si->xres * si->bits_per_pixel / 8;
	/* avoid compiler warnings on various platforms */
	(void) fd;
	(void) si;
	return 0;
}
