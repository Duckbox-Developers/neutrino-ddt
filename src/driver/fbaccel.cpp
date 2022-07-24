/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
                      2003 thegoodguy
	Copyright (C) 2007-2013 Stefan Seyfried

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

#include <driver/framebuffer_ng.h>
#include <driver/fbaccel.h>
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

#if HAVE_GENERIC_HARDWARE
#include <glfb.h>
extern GLFramebuffer *glfb;
#endif

#include <driver/abstime.h>

/* note that it is *not* enough to just change those values */
#define DEFAULT_XRES 1280
#define DEFAULT_YRES 720
#define DEFAULT_BPP  32

#if HAVE_SH4_HARDWARE

static int bpafd = -1;
static size_t lbb_sz = 1920 * 1080;	/* offset from fb start in 'pixels' */
static size_t lbb_off = lbb_sz * sizeof(fb_pixel_t);	/* same in bytes */
static int backbuf_sz = 0;

void CFbAccel::waitForIdle(void)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	ioctl(fb->fd, STMFBIO_SYNC_BLITTER);
}
#else
void CFbAccel::waitForIdle(void)
{
}
#endif

CFbAccel::CFbAccel(CFrameBuffer *_fb)
{
	fb = _fb;
	init();
	lastcol = 0xffffffff;
	lbb = fb->lfb;	/* the memory area to draw to... */
#if HAVE_SH4_HARDWARE
	if (fb->available < 12 * 1024 * 1024)
	{
		/* for old installations that did not upgrade their module config
		 * it will still work good enough to display the message below */
		fprintf(stderr, "[neutrino] WARNING: not enough framebuffer memory available!\n");
		fprintf(stderr, "[neutrino]          I need at least 12MB.\n");
		FILE *f = fopen("/tmp/infobar.txt", "w");
		if (f)
		{
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
	backbuf_sz = 1280 * 720 * sizeof(fb_pixel_t);
	BPAMemAllocMemData bpa_data;
#if BOXMODEL_OCTAGON1008 || BOXMODEL_FORTIS_HDBOX || BOXMODEL_CUBEREVO || BOXMODEL_CUBEREVO_MINI || BOXMODEL_CUBEREVO_MINI2 || BOXMODEL_CUBEREVO_250HD || BOXMODEL_CUBEREVO_2000HD || BOXMODEL_CUBEREVO_3000HD || BOXMODEL_IPBOX9900 || BOXMODEL_IPBOX99 || BOXMODEL_IPBOX55 || BOXMODEL_TF7700
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

	backbuffer = (fb_pixel_t *)mmap(0, bpa_data.mem_size, PROT_WRITE | PROT_READ, MAP_SHARED, bpafd, 0);
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
#endif
};

CFbAccel::~CFbAccel()
{
#if HAVE_SH4_HARDWARE
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
#endif
#if !HAVE_GENERIC_HARDWARE
	if (fb->lfb)
		munmap(fb->lfb, fb->available);
	if (fb->fd > -1)
		close(fb->fd);
#endif
}

void CFbAccel::update()
{
#if !HAVE_SH4_HARDWARE
	int needmem = fb->stride * fb->yRes * 2;
	if (fb->available >= needmem)
	{
		backbuffer = fb->lfb + fb->stride / sizeof(fb_pixel_t) * fb->yRes;
		return;
	}
	fprintf(stderr, "CFbAccel: not enough FB memory (have %d, need %d)\n", fb->available, needmem);
	backbuffer = fb->lfb; /* will not work well, but avoid crashes */
#endif
}

void CFbAccel::setColor(fb_pixel_t col)
{
	(void)col; /* avoid "unused parameter" compiler warning */
}

void CFbAccel::paintRect(const int x, const int y, const int dx, const int dy, const fb_pixel_t col)
{
#if HAVE_SH4_HARDWARE
	if (dx <= 0 || dy <= 0)
		return;

	// The STM blitter introduces considerable overhead probably worth for small areas.  --martii
	if (dx * dy < DEFAULT_XRES * 4)
	{
		waitForIdle();
		fb_pixel_t *fbs = fb->getFrameBufferPointer() + (DEFAULT_XRES * y) + x;
		fb_pixel_t *fbe = fbs + DEFAULT_XRES * (dy - 1) + dx;
		int off = DEFAULT_XRES - dx;
		while (fbs < fbe)
		{
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
	if (x < 0)
	{
		fprintf(stderr, "[neutrino] fb::%s: x < 0 (%d)\n", __func__, x);
		width += x;
		if (width <= 0)
			return;
		xx = 0;
	}

	if (y < 0)
	{
		fprintf(stderr, "[neutrino] fb::%s: y < 0 (%d)\n", __func__, y);
		height += y;
		if (height <= 0)
			return;
		yy = 0;
	}

	int right = xx + width;
	int bottom = yy + height;

	if (right > (int)fb->xRes)
	{
		if (xx >= (int)fb->xRes)
		{
			fprintf(stderr, "[neutrino] fb::%s: x >= xRes (%d > %d)\n", __func__, xx, fb->xRes);
			return;
		}
		fprintf(stderr, "[neutrino] fb::%s: x+w > xRes! (%d+%d > %d)\n", __func__, xx, width, fb->xRes);
		right = fb->xRes;
	}
	if (bottom > (int)fb->yRes)
	{
		if (yy >= (int)fb->yRes)
		{
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
	if (ioctl(fb->fd, STMFBIO_BLT, &bltData) < 0)
		fprintf(stderr, "blitRect FBIO_BLIT: %m x:%d y:%d w:%d h:%d s:%d\n", xx, yy, width, height, fb->stride);
#else
	int line = 0;
	int swidth = fb->stride / sizeof(fb_pixel_t);
	fb_pixel_t *fbp = fb->getFrameBufferPointer() + (swidth * y);
	int pos;
	while (line < dy)
	{
		for (pos = x; pos < x + dx; pos++)
			*(fbp + pos) = col;
		fbp += swidth;
		line++;
	}
#endif
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
	int dx = abs(xa - xb);
	int dy = abs(ya - yb);
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
		int twoDyDx = 2 * (dy - dx);

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
		int twoDxDy = 2 * (dx - dy);

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
#if !HAVE_SH4_HARDWARE
	int  xc, yc;
	xc = (width > fb->xRes) ? fb->xRes : width;
	yc = (height > fb->yRes) ? fb->yRes : height;
#endif
#if HAVE_SH4_HARDWARE
	int x, y, dw, dh;
	x = xoff;
	y = yoff;
	dw = width - xp;
	dh = height - yp;

	size_t mem_sz = width * height * sizeof(fb_pixel_t);
	unsigned long ulFlags = 0;
	if (!transp) /* transp == false (default): use transparency from source alphachannel */
		ulFlags = BLT_OP_FLAGS_BLEND_SRC_ALPHA | BLT_OP_FLAGS_BLEND_DST_MEMORY; // we need alpha blending

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

	if (fbbuff != backbuffer)
		memmove(backbuffer, fbbuff, mem_sz);
	// icons are so small that they will still be in cache
	msync(backbuffer, backbuf_sz, MS_SYNC);

	if (ioctl(fb->fd, STMFBIO_BLT_EXTERN, &blt_data) < 0)
		perror("CFbAccel blit2FB STMFBIO_BLT_EXTERN");
	return;
#else
	fb_pixel_t *data = (fb_pixel_t *) fbbuff;

	uint8_t *d = (uint8_t *)lbb + xoff * sizeof(fb_pixel_t) + fb->stride * yoff;
	fb_pixel_t *d2;

	for (int count = 0; count < yc; count++)
	{
		fb_pixel_t *pixpos = &data[(count + yp) * width];
		d2 = (fb_pixel_t *) d;
		for (int count2 = 0; count2 < xc; count2++)
		{
			fb_pixel_t pix = *(pixpos + xp);
			if (transp || (pix & 0xff000000) == 0xff000000)
				*d2 = pix;
			else
			{
				uint8_t *in = (uint8_t *)(pixpos + xp);
				uint8_t *out = (uint8_t *)d2;
				int a = in[3];	/* TODO: big/little endian */
				*out = (*out + ((*in - *out) * a) / 256);
				in++;
				out++;
				*out = (*out + ((*in - *out) * a) / 256);
				in++;
				out++;
				*out = (*out + ((*in - *out) * a) / 256);
			}
			d2++;
			pixpos++;
		}
		d += fb->stride;
	}
#endif
}
#else
void CFbAccel::blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp, uint32_t yp, bool transp)
{
	DFBRectangle src;
	DFBResult err;
	IDirectFBSurface *surf;
	DFBSurfaceDescription dsc;

	src.x = xp;
	src.y = yp;
	src.w = width - xp;
	src.h = height - yp;

	dsc.flags  = (DFBSurfaceDescriptionFlags)(DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PREALLOCATED);
	dsc.caps   = DSCAPS_NONE;
	dsc.width  = width;
	dsc.height = height;
	dsc.preallocated[0].data  = fbbuff;
	dsc.preallocated[0].pitch = width * sizeof(fb_pixel_t);
	err = dfb->CreateSurface(dfb, &dsc, &surf);
	/* TODO: maybe we should not die if this fails? */
	if (err != DFB_OK)
	{
		fprintf(stderr, "CFbAccel::blit2FB: ");
		DirectFBErrorFatal("dfb->CreateSurface(dfb, &dsc, &surf)", err);
	}

	if (transp)
	{
		surf->SetSrcColorKey(surf, 0, 0, 0);
		dfbdest->SetBlittingFlags(dfbdest, DSBLIT_SRC_COLORKEY);
	}
	else
		dfbdest->SetBlittingFlags(dfbdest, DSBLIT_BLEND_ALPHACHANNEL);

	dfbdest->Blit(dfbdest, surf, &src, xoff, yoff);
	surf->Release(surf);
	return;
}
#endif

#if HAVE_SH4_HARDWARE
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
	if (ioctl(fb->fd, STMFBIO_BLT, &bltData) < 0)
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
	if (ioctl(fb->fd, STMFBIO_BLT, &bltData) < 0)
		perror("STMFBIO_BLT");
}

void CFbAccel::blitBoxFB(int x0, int y0, int x1, int y1, fb_pixel_t color)
{
	if (x0 > -1 && y0 > -1 && x0 < x1 && y0 < y1)
	{
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
		if (ioctl(fb->fd, STMFBIO_BLT, &bltData) < 0)
			perror("STMFBIO_BLT");
	}
}

#if HAVE_SH4_HARDWARE
void CFbAccel::blit()
{
#ifdef ENABLE_GRAPHLCD
	nGLCD::Blit();
#endif
	msync(lbb, DEFAULT_XRES * 4 * DEFAULT_YRES, MS_SYNC);

	if (borderColor != borderColorOld || (borderColor != 0x00000000 && borderColor != 0xFF000000))
	{
		borderColorOld = borderColor;
		switch (fb->mode3D)
		{
			case CFrameBuffer::Mode3D_off:
			default:
				blitBoxFB(0, 0, s.xres, sY, borderColor);		// top
				blitBoxFB(0, 0, sX, s.yres, borderColor);	// left
				blitBoxFB(eX, 0, s.xres, s.yres, borderColor);	// right
				blitBoxFB(0, eY, s.xres, s.yres, borderColor);	// bottom
				break;
			case CFrameBuffer::Mode3D_SideBySide:
				blitBoxFB(0, 0, s.xres, sY, borderColor);			// top
				blitBoxFB(0, 0, sX / 2, s.yres, borderColor);			// left
				blitBoxFB(eX / 2 + 1, 0, s.xres / 2 + sX / 2, s.yres, borderColor);	// middle
				blitBoxFB(s.xres / 2 + eX / 2 + 1, 0, s.xres, s.yres, borderColor);	// right
				blitBoxFB(0, eY, s.xres, s.yres, borderColor);			// bottom
				break;
			case CFrameBuffer::Mode3D_TopAndBottom:
				blitBoxFB(0, 0, s.xres, sY / 2, borderColor); 			// top
				blitBoxFB(0, eY / 2 + 1, s.xres, s.yres / 2 + sY / 2, borderColor); 	// middle
				blitBoxFB(0, s.yres / 2 + eY / 2 + 1, s.xres, s.yres, borderColor); // bottom
				blitBoxFB(0, 0, sX, s.yres, borderColor);			// left
				blitBoxFB(eX, 0, s.xres, s.yres, borderColor);			// right
				break;
			case CFrameBuffer::Mode3D_Tile:
				blitBoxFB(0, 0, (s.xres * 2) / 3, (sY * 2) / 3, borderColor);		// top
				blitBoxFB(0, 0, (sX * 2) / 3, (s.yres * 2) / 3, borderColor);		// left
				blitBoxFB((eX * 2) / 3, 0, (s.xres * 2) / 3, (s.yres * 2) / 3, borderColor);	// right
				blitBoxFB(0, (eY * 2) / 3, (s.xres * 2) / 3, (s.yres * 2) / 3, borderColor);	// bottom
				break;
		}
	}
	switch (fb->mode3D)
	{
		case CFrameBuffer::Mode3D_off:
		default:
			blitBB2FB(0, 0, DEFAULT_XRES, DEFAULT_YRES, sX, sY, eX, eY);
			break;
		case CFrameBuffer::Mode3D_SideBySide:
			blitBB2FB(0, 0, DEFAULT_XRES, DEFAULT_YRES, sX / 2, sY, eX / 2, eY);
			blitBB2FB(0, 0, DEFAULT_XRES, DEFAULT_YRES, s.xres / 2 + sX / 2, sY, s.xres / 2 + eX / 2, eY);
			break;
		case CFrameBuffer::Mode3D_TopAndBottom:
			blitBB2FB(0, 0, DEFAULT_XRES, DEFAULT_YRES, sX, sY / 2, eX, eY / 2);
			blitBB2FB(0, 0, DEFAULT_XRES, DEFAULT_YRES, sX, s.yres / 2 + sY / 2, eX, s.yres / 2 + eY / 2);
			break;
		case CFrameBuffer::Mode3D_Tile:
			blitBB2FB(0, 0, DEFAULT_XRES, DEFAULT_YRES, (sX * 2) / 3, (sY * 2) / 3, (eX * 2) / 3, (eY * 2) / 3);
			blitFB2FB(0, 0, s.xres / 3, (s.yres * 2) / 3, (s.xres * 2) / 3, 0, s.xres, (s.yres * 2) / 3);
			blitFB2FB(s.xres / 3, 0, (s.xres * 2) / 3, s.yres / 3, 0, (s.yres * 2) / 3, s.xres / 3, s.yres);
			blitFB2FB(s.xres / 3, s.yres / 3, (s.xres * 2) / 3, (s.yres * 2) / 3, s.xres / 3, (s.yres * 2) / 3, (s.xres * 2) / 3, s.yres);
			break;
	}
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if (ioctl(fb->fd, STMFBIO_SYNC_BLITTER) < 0)
		perror("CFrameBuffer::blit ioctl STMFBIO_SYNC_BLITTER 2");
}
#else
void CFbAccel::blit()
{
#ifdef PARTIAL_BLIT
	if (to_blit.xs == INT_MAX)
		return;

	int srcXa = to_blit.xs;
	int srcYa = to_blit.ys;
	int srcXb = to_blit.xe;
	int srcYb = to_blit.ye;
#else
	const int srcXa = 0;
	const int srcYa = 0;
	int srcXb = fb->xRes;
	int srcYb = fb->yRes;
#endif
	STMFBIO_BLT_DATA  bltData;
	memset(&bltData, 0, sizeof(STMFBIO_BLT_DATA));

	bltData.operation  = BLT_OP_COPY;
	//bltData.ulFlags  = BLT_OP_FLAGS_BLEND_SRC_ALPHA | BLT_OP_FLAGS_BLEND_DST_MEMORY; // we need alpha blending
	// src
	bltData.srcOffset  = lbb_off;
	bltData.srcPitch   = fb->stride;

	bltData.src_left   = srcXa;
	bltData.src_top    = srcYa;
	bltData.src_right  = srcXb;
	bltData.src_bottom = srcYb;

	bltData.srcFormat = SURF_BGRA8888;
	bltData.srcMemBase = STMFBGP_FRAMEBUFFER;

	/* calculate dst/blit factor */
	fb_var_screeninfo s;
	if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &s) == -1)
		perror("CFbAccel <FBIOGET_VSCREENINFO>");

#ifdef PARTIAL_BLIT
	if (s.xres != last_xres) /* fb resolution has changed -> clear artifacts */
	{
		last_xres = s.xres;
		bltData.src_left   = 0;
		bltData.src_top    = 0;
		bltData.src_right  = xRes;
		bltData.src_bottom = yRes;
	}

	double xFactor = (double)s.xres / (double)xRes;
	double yFactor = (double)s.yres / (double)yRes;

	int desXa = xFactor * bltData.src_left;
	int desYa = yFactor * bltData.src_top;
	int desXb = xFactor * bltData.src_right;
	int desYb = yFactor * bltData.src_bottom;
#else
	const int desXa = 0;
	const int desYa = 0;
	int desXb = s.xres;
	int desYb = s.yres;
#endif

	/* dst */
	bltData.dstOffset  = 0;
	bltData.dstPitch   = s.xres * 4;

	bltData.dst_left   = desXa;
	bltData.dst_top    = desYa;
	bltData.dst_right  = desXb;
	bltData.dst_bottom = desYb;

	bltData.dstFormat = SURF_BGRA8888;
	bltData.dstMemBase = STMFBGP_FRAMEBUFFER;

	//printf("CFbAccel::blit: sx:%d sy:%d sxe:%d sye: %d dx:%d dy:%d dxe:%d dye:%d\n", srcXa, srcYa, srcXb, srcYb, desXa, desYa, desXb, desYb);
	if ((bltData.dst_right > s.xres) || (bltData.dst_bottom > s.yres))
		printf("CFbAccel::blit: values out of range desXb:%d desYb:%d\n",
			bltData.dst_right, bltData.dst_bottom);

	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if (ioctl(fb->fd, STMFBIO_SYNC_BLITTER) < 0)
		perror("CFbAccel::blit ioctl STMFBIO_SYNC_BLITTER 1");
	msync(lbb, fb->xRes * 4 * fb->yRes, MS_SYNC);
	if (ioctl(fb->fd, STMFBIO_BLT, &bltData) < 0)
		perror("STMFBIO_BLT");
	if (ioctl(fb->fd, STMFBIO_SYNC_BLITTER) < 0)
		perror("CFbAccel::blit ioctl STMFBIO_SYNC_BLITTER 2");

#ifdef PARTIAL_BLIT
	to_blit.xs = to_blit.ys = INT_MAX;
	to_blit.xe = to_blit.ye = 0;
#endif
}
#endif

#else

void CFbAccel::blit()
{
#if HAVE_GENERIC_HARDWARE
	if (glfb)
		glfb->blit();
#endif
}
#endif

/* not really used yet */
#ifdef PARTIAL_BLIT
void CFbAccel::mark(int xs, int ys, int xe, int ye)
{
	update_dirty(xs, ys, xe, ye);
}
#else
void CFbAccel::mark(int, int, int, int)
{
}
#endif

#if HAVE_SH4_HARDWARE
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
		blt_data.ulFlags = BLT_OP_FLAGS_BLEND_SRC_ALPHA | BLT_OP_FLAGS_BLEND_DST_MEMORY; // we need alpha blending
//	blt_data.srcOffset  = 0;
	switch (fmt)
	{
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

	if (ioctl(fb->fd, STMFBIO_BLT_EXTERN, &blt_data) < 0)
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

	if (ioctl(fb->fd, STMFBIO_BLT_EXTERN, &blt_data) < 0)
		perror("blitArea FBIO_BLIT");
}
#else
void CFbAccel::blitArea(int /*src_width*/, int /*src_height*/, int /*fb_x*/, int /*fb_y*/, int /*width*/, int /*height*/)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}
#endif

#if HAVE_SH4_HARDWARE
void CFbAccel::resChange(void)
{
	if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &s) == -1)
		perror("frameBuffer <FBIOGET_VSCREENINFO>");

	sX = (startX * s.xres) / DEFAULT_XRES;
	sY = (startY * s.yres) / DEFAULT_YRES;
	eX = (endX * s.xres) / DEFAULT_XRES;
	eY = (endY * s.yres) / DEFAULT_YRES;
	borderColorOld = 0x01010101;
}

void CFbAccel::setBorder(int sx, int sy, int ex, int ey)
{
	startX = sx;
	startY = sy;
	endX = ex;
	endY = ey;
	sX = (startX * s.xres) / DEFAULT_XRES;
	sY = (startY * s.yres) / DEFAULT_YRES;
	eX = (endX * s.xres) / DEFAULT_XRES;
	eY = (endY * s.yres) / DEFAULT_YRES;
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
#else
void CFbAccel::resChange(void)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

void CFbAccel::setBorder(int /*sx*/, int /*sy*/, int /*ex*/, int /*ey*/)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

void CFbAccel::setBorderColor(fb_pixel_t /*col*/)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

void CFbAccel::ClearFB(void)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}
#endif

bool CFbAccel::init(void)
{
	fb_pixel_t *lfb;
	fb->lfb = NULL;
	fb->fd = -1;
#if HAVE_GENERIC_HARDWARE
	if (!glfb)
	{
		fprintf(stderr, "CFbAccel::init: GL Framebuffer is not set up? we are doomed...\n");
		return false;
	}
	fb->screeninfo = glfb->getScreenInfo();
	fb->stride = 4 * fb->screeninfo.xres;
	fb->available = glfb->getOSDBuffer()->size(); /* allocated in glfb constructor */
	lfb = reinterpret_cast<fb_pixel_t *>(glfb->getOSDBuffer()->data());
#else
	int fd;
	fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
#endif
	if (fd < 0)
	{
		perror("open /dev/fb0");
		return false;
	}
	fb->fd = fd;

	if (ioctl(fd, FBIOGET_VSCREENINFO, &fb->screeninfo) < 0)
	{
		perror("FBIOGET_VSCREENINFO");
		return false;
	}

	if (ioctl(fd, FBIOGET_FSCREENINFO, &fb->fix) < 0)
	{
		perror("FBIOGET_FSCREENINFO");
		return false;
	}

	fb->available = fb->fix.smem_len;
	printf("%dk video mem\n", fb->available / 1024);
	lfb = (fb_pixel_t *)mmap(0, fb->available, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);

	if (lfb == MAP_FAILED)
	{
		perror("mmap");
		return false;;
	}
#endif /* ! GENERIC_HARDWARE */

	memset(lfb, 0, fb->available);
	fb->lfb = lfb;
	return true;
}

/* wrong name... */
int CFbAccel::setMode(void)
{
	int fd = fb->fd;
	t_fb_var_screeninfo *si = &fb->screeninfo;
#if HAVE_SH4_HARDWARE
	/* it's all fake... :-) */
	si->xres = si->xres_virtual = DEFAULT_XRES;
	si->yres = si->yres_virtual = DEFAULT_YRES;
	si->bits_per_pixel = DEFAULT_BPP;
	fb->stride = si->xres * si->bits_per_pixel / 8;
#else
#if ! HAVE_GENERIC_HARDWARE
	fb_fix_screeninfo _fix;

	if (ioctl(fd, FBIOGET_FSCREENINFO, &_fix) < 0)
	{
		perror("FBIOGET_FSCREENINFO");
		return -1;
	}
	fb->stride = _fix.line_length;
#endif
#endif
	/* avoid compiler warnings on various platforms */
	(void) fd;
	(void) si;
	return 0;
}
