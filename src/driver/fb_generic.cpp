/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
		      2003 thegoodguy

	Copyright (C) 2009-2012,2017 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>
	mute icon & info clock handling
	Copyright (C) 2013 M. Liebmann (micha-bbg)

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <driver/fb_generic.h>
#include <driver/fb_accel.h>

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <memory.h>
#include <math.h>

#include <linux/kd.h>

#include <gui/audiomute.h>
#include <gui/color.h>
#include <gui/pictureviewer.h>
#include <system/debug.h>
#include <global.h>
#include <video.h>
#include <cs_api.h>

extern cVideo * videoDecoder;

extern CPictureViewer * g_PicViewer;
#define ICON_CACHE_SIZE 1024*1024*2 // 2mb

#define BACKGROUNDIMAGEWIDTH 720

void CFrameBuffer::waitForIdle(const char *)
{
}

/*******************************************************************************/

static uint8_t * virtual_fb = NULL;
inline unsigned int make16color(uint16_t r, uint16_t g, uint16_t b, uint16_t t,
				  uint32_t  /*rl*/ = 0, uint32_t  /*ro*/ = 0,
				  uint32_t  /*gl*/ = 0, uint32_t  /*go*/ = 0,
				  uint32_t  /*bl*/ = 0, uint32_t  /*bo*/ = 0,
				  uint32_t  /*tl*/ = 0, uint32_t  /*to*/ = 0)
{
	return ((t << 24) & 0xFF000000) | ((r << 8) & 0xFF0000) | ((g << 0) & 0xFF00) | (b >> 8 & 0xFF);
}

CFrameBuffer::CFrameBuffer()
: active ( true )
{
	fb_name = "generic framebuffer";
	iconBasePath = "";
	available  = 0;
	cache_size = 0;
	cmap.start = 0;
	cmap.len = 256;
	cmap.red = red;
	cmap.green = green;
	cmap.blue  = blue;
	cmap.transp = trans;
	backgroundColor = 0;
	useBackgroundPaint = false;
	background = NULL;
	backupBackground = NULL;
	backgroundFilename = "";
	locked = false;
	fd  = 0;
	tty = 0;
	m_transparent_default = CFrameBuffer::TM_BLACK; // TM_BLACK: Transparency when black content ('pseudo' transparency)
							// TM_NONE:  No 'pseudo' transparency
							// TM_INI:   Transparency depends on g_settings.infobar_alpha ???
	m_transparent	 = m_transparent_default;
	q_circle = NULL;
	initQCircle();
	corner_tl = false;
	corner_tr = false;
	corner_bl = false;
	corner_br = false;
//FIXME: test
	memset(red, 0, 256*sizeof(__u16));
	memset(green, 0, 256*sizeof(__u16));
	memset(blue, 0, 256*sizeof(__u16));
	memset(trans, 0, 256*sizeof(__u16));
	fbAreaActiv = false;
	fb_no_check = false;
	do_paint_mute_icon = true;
}

CFrameBuffer* CFrameBuffer::getInstance()
{
	static CFrameBuffer* frameBuffer = NULL;

	if (!frameBuffer) {
#if HAVE_SPARK_HARDWARE
		frameBuffer = new CFbAccelSTi();
#endif
#if HAVE_COOL_HARDWARE
#ifdef BOXMODEL_CS_HD1
		frameBuffer = new CFbAccelCSHD1();
#endif
#ifdef BOXMODEL_CS_HD2
		frameBuffer = new CFbAccelCSHD2();
#endif
#endif
#if HAVE_GENERIC_HARDWARE
		frameBuffer = new CFbAccelGLFB();
#endif
#if HAVE_TRIPLEDRAGON
		frameBuffer = new CFbAccelTD();
#endif
		if (!frameBuffer)
			frameBuffer = new CFrameBuffer();
		printf("[neutrino] %s Instance created\n", frameBuffer->fb_name);
	}
	return frameBuffer;
}

void CFrameBuffer::init(const char * const fbDevice)
{
	int tr = 0xFF;

	fd = open(fbDevice, O_RDWR|O_CLOEXEC);

	if (fd<0) {
		perror(fbDevice);
		goto nolfb;
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &screeninfo)<0) {
		perror("FBIOGET_VSCREENINFO");
		goto nolfb;
	}

	if (ioctl(fd, FBIOGET_FSCREENINFO, &fix)<0) {
		perror("FBIOGET_FSCREENINFO");
		goto nolfb;
	}

	available=fix.smem_len;
	printf("[fb_generic] [%s] framebuffer %dk video mem\n", fix.id, available/1024);
	lbb = lfb = (fb_pixel_t*)mmap(0, available, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
	if (!lfb) {
		perror("mmap");
		goto nolfb;
	}

	/* Windows Colors */
	paletteSetColor(0x1, 0x010101, tr);
	paletteSetColor(0x2, 0x800000, tr);
	paletteSetColor(0x3, 0x008000, tr);
	paletteSetColor(0x4, 0x808000, tr);
	paletteSetColor(0x5, 0x000080, tr);
	paletteSetColor(0x6, 0x800080, tr);
	paletteSetColor(0x7, 0x008080, tr);
	paletteSetColor(0x8, 0xA0A0A0, tr);
	paletteSetColor(0x9, 0x505050, tr);
	paletteSetColor(0xA, 0xFF0000, tr);
	paletteSetColor(0xB, 0x00FF00, tr);
	paletteSetColor(0xC, 0xFFFF00, tr);
	paletteSetColor(0xD, 0x0000FF, tr);
	paletteSetColor(0xE, 0xFF00FF, tr);
	paletteSetColor(0xF, 0x00FFFF, tr);
	paletteSetColor(0x10, 0xFFFFFF, tr);
	paletteSetColor(0x11, 0x000000, tr);
	paletteSetColor(COL_BACKGROUND, 0x000000, 0x0);

	paletteSet();

	useBackground(false);
	m_transparent = m_transparent_default;

	return;

nolfb:
	printf("framebuffer not available.\n");
	lbb = lfb = NULL;
}


CFrameBuffer::~CFrameBuffer()
{
	std::map<std::string, rawIcon>::iterator it;

	for(it = icon_cache.begin(); it != icon_cache.end(); ++it) {
		/* printf("FB: delete cached icon %s: %x\n", it->first.c_str(), (int) it->second.data); */
		cs_free_uncached(it->second.data);
	}
	icon_cache.clear();

	if (background) {
		delete[] background;
		background = NULL;
	}

	if (backupBackground) {
		delete[] backupBackground;
		backupBackground = NULL;
	}

	if (q_circle) {
		delete[] q_circle;
		q_circle = NULL;
	}

	if (lfb)
		munmap(lfb, available);
	lfb = NULL;

	if (virtual_fb){
		delete[] virtual_fb;
		virtual_fb = NULL;
	}
	close(fd);
	fd = -1;

	v_fbarea.clear();
}

int CFrameBuffer::getFileHandle() const
{
	return fd;
}

unsigned int CFrameBuffer::getStride() const
{
	return stride;
}

unsigned int CFrameBuffer::getScreenWidth(bool real)
{
	if(real)
		return xRes;
	else
		return g_settings.screen_EndX - g_settings.screen_StartX;
}

unsigned int CFrameBuffer::getScreenHeight(bool real)
{
	if(real)
		return yRes;
	else
		return g_settings.screen_EndY - g_settings.screen_StartY;
}

unsigned int CFrameBuffer::getScreenWidthRel(bool force_small)
{
	int percent = force_small ? WINDOW_SIZE_MIN_FORCED : g_settings.window_width;
	// always reduce a possible detailline
	return (g_settings.screen_EndX - g_settings.screen_StartX - 2*ConnectLineBox_Width) * percent / 100;
}

unsigned int CFrameBuffer::getScreenHeightRel(bool force_small)
{
	int percent = force_small ? WINDOW_SIZE_MIN_FORCED : g_settings.window_height;
	return (g_settings.screen_EndY - g_settings.screen_StartY) * percent / 100;
}

unsigned int CFrameBuffer::getScreenX()
{
	return g_settings.screen_StartX;
}

unsigned int CFrameBuffer::getScreenY()
{
	return g_settings.screen_StartY;
}

fb_pixel_t * CFrameBuffer::getFrameBufferPointer() const
{
	if (active || (virtual_fb == NULL))
		return lbb;
	else
		return (fb_pixel_t *) virtual_fb;
}

/* dummy if not implemented in CFbAccel */
fb_pixel_t * CFrameBuffer::getBackBufferPointer() const
{
	return getFrameBufferPointer();
}

bool CFrameBuffer::getActive() const
{
	return (active || (virtual_fb != NULL));
}

void CFrameBuffer::setActive(bool enable)
{
	active = enable;
}

t_fb_var_screeninfo *CFrameBuffer::getScreenInfo()
{
	return &screeninfo;
}

int CFrameBuffer::setMode(unsigned int /*nxRes*/, unsigned int /*nyRes*/, unsigned int /*nbpp*/)
{
	if (!available&&!active)
		return -1;

	xRes = screeninfo.xres;
	yRes = screeninfo.yres;
	bpp  = screeninfo.bits_per_pixel;
	fb_fix_screeninfo _fix;

	if (ioctl(fd, FBIOGET_FSCREENINFO, &_fix)<0) {
		perror("FBIOGET_FSCREENINFO");
		return -1;
	}

	stride = _fix.line_length;
	swidth = stride / sizeof(fb_pixel_t);
	printf("FB: %dx%dx%d line length %d. %s accelerator.\n", xRes, yRes, bpp, stride,
		"Not using graphics"
	);

	//memset(getFrameBufferPointer(), 0, stride * yRes);
	paintBackground();
	if (ioctl(fd, FBIOBLANK, FB_BLANK_UNBLANK) < 0) {
		printf("screen unblanking failed\n");
	}
	return 0;
}
#if 0
//never used
void CFrameBuffer::setTransparency( int /*tr*/ )
{
}
#endif
void CFrameBuffer::setBlendMode(uint8_t /*mode*/)
{
}

void CFrameBuffer::setBlendLevel(int /*level*/)
{
}

#if 0
//never used
void CFrameBuffer::setAlphaFade(int in, int num, int tr)
{
	for (int i=0; i<num; i++) {
		cmap.transp[in+i]=tr;
	}
}
#endif
void CFrameBuffer::paletteFade(int i, __u32 rgb1, __u32 rgb2, int level)
{
	__u16 *r = cmap.red+i;
	__u16 *g = cmap.green+i;
	__u16 *b = cmap.blue+i;

	*r= ((rgb2&0xFF0000)>>16)*level;
	*g= ((rgb2&0x00FF00)>>8 )*level;
	*b= ((rgb2&0x0000FF)    )*level;
	*r+=((rgb1&0xFF0000)>>16)*(255-level);
	*g+=((rgb1&0x00FF00)>>8 )*(255-level);
	*b+=((rgb1&0x0000FF)    )*(255-level);
}

void CFrameBuffer::paletteGenFade(int in, __u32 rgb1, __u32 rgb2, int num, int tr)
{
	for (int i=0; i<num; i++) {
		paletteFade(in+i, rgb1, rgb2, i*(255/(num-1)));
		cmap.transp[in+i]=tr;
		tr--; //FIXME
	}
}

void CFrameBuffer::paletteSetColor(int i, __u32 rgb, int tr)
{
	cmap.red[i]	=(rgb&0xFF0000)>>8;
	cmap.green[i]	=(rgb&0x00FF00)   ;
	cmap.blue[i]	=(rgb&0x0000FF)<<8;
	cmap.transp[i]	= tr;
}

void CFrameBuffer::paletteSet(struct fb_cmap *map)
{
	if (!active)
		return;

	if(map == NULL)
		map = &cmap;

	if(bpp == 8) {
		//printf("Set palette for %dbit\n", bpp);
		ioctl(fd, FBIOPUTCMAP, map);
	}

	uint32_t  rl, ro, gl, go, bl, bo, tl, to;

	rl = screeninfo.red.length;
	ro = screeninfo.red.offset;
	gl = screeninfo.green.length;
	go = screeninfo.green.offset;
	bl = screeninfo.blue.length;
	bo = screeninfo.blue.offset;
	tl = screeninfo.transp.length;
	to = screeninfo.transp.offset;
	for (int i = 0; i < 256; i++) {
		realcolor[i] = make16color(cmap.red[i], cmap.green[i], cmap.blue[i], cmap.transp[i],
					   rl, ro, gl, go, bl, bo, tl, to);
	}
	OnAfterSetPallette();
}

void CFrameBuffer::paintHLineRelInternal2Buf(const int& x, const int& dx, const int& y, const int& box_dx, const fb_pixel_t& col, fb_pixel_t* buf)
{
	fb_pixel_t * pos = buf + x + box_dx * y;
	fb_pixel_t * dest = (fb_pixel_t *)pos;
	for (int i = 0; i < dx; i++)
		*(dest++) = col;
}

fb_pixel_t* CFrameBuffer::paintBoxRel2Buf(const int dx, const int dy, const int w_align, const int offs_align, const fb_pixel_t col, fb_pixel_t* buf/* = NULL*/, int radius/* = 0*/, int type/* = CORNER_ALL*/)
{
	if (!getActive())
		return buf;
	if (dx < 1 || dy < 1) {
		dprintf(DEBUG_INFO, "[CFrameBuffer] [%s - %d]: radius %d, dx %d dy %d\n", __func__, __LINE__, radius, dx, dy);
		return buf;
	}

	fb_pixel_t* pixBuf = buf;
	if (pixBuf == NULL) {
		pixBuf = (fb_pixel_t*) cs_malloc_uncached(w_align*dy*sizeof(fb_pixel_t));
		if (pixBuf == NULL) {
			dprintf(DEBUG_NORMAL, "[%s #%d] Error cs_malloc_uncached\n", __func__, __LINE__);
			return NULL;
		}
	}
	memset((void*)pixBuf, '\0', w_align*dy*sizeof(fb_pixel_t));

	if (type && radius) {
		setCornerFlags(type);
		radius = limitRadius(dx, dy, radius);

		int line = 0;
		while (line < dy) {
			int ofl, ofr;
			calcCorners(NULL, &ofl, &ofr, dy, line, radius, type);
			if (dx-ofr-ofl < 1) {
				if (dx-ofr-ofl == 0) {
					dprintf(DEBUG_INFO, "[%s - %d]: radius %d, end x %d y %d\n", __func__, __LINE__, radius, dx-ofr-ofl, line);
				}
				else {
					dprintf(DEBUG_INFO, "[%s - %04d]: Calculated width: %d\n		      (radius %d, dx %d, offsetLeft %d, offsetRight %d).\n		      Width can not be less than 0, abort.\n",
					       __func__, __LINE__, dx-ofr-ofl, radius, dx, ofl, ofr);
				}
				line++;
				continue;
			}
			paintHLineRelInternal2Buf(ofl+offs_align, dx-ofl-ofr, line, w_align, col, pixBuf);
			line++;
		}
	} else {
		fb_pixel_t *bp = pixBuf;
		int line = 0;
		while (line < dy) {
			for (int pos = offs_align; pos < dx+offs_align; pos++)
				*(bp + pos) = col;
			bp += w_align;
			line++;
		}
	}
	return pixBuf;
}

fb_pixel_t* CFrameBuffer::paintBoxRel(const int x, const int y, const int dx, const int dy,
				      const fb_pixel_t /*col*/, gradientData_t *gradientData,
				      int radius, int type)
{
	if (!getActive())
		return NULL;

	checkFbArea(x, y, dx, dy, true);

	fb_pixel_t MASK = 0xFFFFFFFF;
	int _dx = dx;
	int w_align;
	int offs_align;

#ifdef BOXMODEL_CS_HD2
	if (_dx%4 != 0) {
		w_align = getWidth4FB_HW_ACC(x, _dx, true);
		if (w_align < _dx)
			_dx = w_align;
		offs_align = w_align - _dx;
		if ((x - offs_align) < 0)
			offs_align = 0;
	}
	else {
		w_align    = _dx;
		offs_align = 0;
	}
#else
	w_align    = _dx;
	offs_align = 0;
#endif

	fb_pixel_t* boxBuf    = paintBoxRel2Buf(_dx, dy, w_align, offs_align, MASK, NULL, radius, type);
	if (boxBuf == NULL) {
		checkFbArea(x, y, dx, dy, false);
		return NULL;
	}
	fb_pixel_t *bp        = boxBuf;
	fb_pixel_t *gra       = gradientData->gradientBuf;
	gradientData->boxBuf  = boxBuf;
	gradientData->x       = x - offs_align;
	gradientData->dx      = w_align;

	if (gradientData->direction == gradientVertical) {
		// vertical
		for (int pos = offs_align; pos < _dx+offs_align; pos++) {
			for(int count = 0; count < dy; count++) {
				if (*(bp + pos) == MASK)
					*(bp + pos) = (fb_pixel_t)(*(gra + count));
				bp += w_align;
			}
			bp = boxBuf;
		}
	} else {
		// horizontal
		for (int line = 0; line < dy; line++) {
			int gra_pos = 0;
			for (int pos = 0; pos < w_align; pos++) {
				if ((*(bp + pos) == MASK) && (pos >= offs_align) && (gra_pos < _dx)) {
					*(bp + pos) = (fb_pixel_t)(*(gra + gra_pos));
					gra_pos++;
				}
			}
			bp += w_align;
		}
	}

	if ((gradientData->mode & pbrg_noPaint) == pbrg_noPaint) {
		checkFbArea(x, y, dx, dy, false);
		return boxBuf;
	}

	blitBox2FB(boxBuf, w_align, dy, x-offs_align, y);

	if ((gradientData->mode & pbrg_noFree) == pbrg_noFree) {
		checkFbArea(x, y, dx, dy, false);
		return boxBuf;
	}

	cs_free_uncached(boxBuf);

	checkFbArea(x, y, dx, dy, false);
	return NULL;
}

void CFrameBuffer::paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col, int radius, int type)
{
	/* draw a filled rectangle (with additional round corners) */

	if (!getActive())
		return;

	if (dx == 0 || dy == 0) {
		dprintf(DEBUG_DEBUG, "[CFrameBuffer] [%s - %d]: radius %d, start x %d y %d end x %d y %d\n", __func__, __LINE__, radius, x, y, x+dx, y+dy);
		return;
	}
	if (radius < 0)
		dprintf(DEBUG_NORMAL, "[CFrameBuffer] [%s - %d]: WARNING! radius < 0 [%d] FIXME\n", __func__, __LINE__, radius);

	checkFbArea(x, y, dx, dy, true);

	if (type && radius) {
		setCornerFlags(type);
		radius = limitRadius(dx, dy, radius);

		int line = 0;
		while (line < dy) {
			int ofl, ofr;
			if (calcCorners(NULL, &ofl, &ofr, dy, line, radius, type)) {
				//printf("3: x %d y %d dx %d dy %d rad %d line %d\n", x, y, dx, dy, radius, line);
			}

			if (dx-ofr-ofl < 1) {
				if (dx-ofr-ofl == 0){
					dprintf(DEBUG_INFO, "[CFrameBuffer] [%s - %d]: radius %d, start x %d y %d end x %d y %d\n", __func__, __LINE__, radius, x, y, x+dx-ofr-ofl, y+line);
				}else{
					dprintf(DEBUG_INFO, "[CFrameBuffer] [%s - %04d]: Calculated width: %d\n                      (radius %d, dx %d, offsetLeft %d, offsetRight %d).\n                      Width can not be less than 0, abort.\n",
						__func__, __LINE__, dx-ofr-ofl, radius, dx, ofl, ofr);
				}
				line++;
				continue;
			}
			paintHLineRelInternal(x+ofl, dx-ofl-ofr, y+line, col);
			line++;
		}
	} else {
		fb_pixel_t *fbp = getFrameBufferPointer() + (swidth * y);
		int line = 0;
		while (line < dy) {
			for (int pos = x; pos < x + dx; pos++)
				*(fbp + pos) = col;

			fbp += swidth;
			line++;
		}
	}
	checkFbArea(x, y, dx, dy, false);
}

void CFrameBuffer::paintVLineRelInternal(int x, int y, int dy, const fb_pixel_t col)
{
	fb_pixel_t *pos = getFrameBufferPointer() + x + swidth * y;

	for(int count=0;count<dy;count++) {
		*(fb_pixel_t *)pos = col;
		pos += swidth;
	}
}

void CFrameBuffer::paintVLineRel(int x, int y, int dy, const fb_pixel_t col)
{
	if (!getActive())
		return;

	paintVLineRelInternal(x, y, dy, col);
	mark(x, y, x, y + dy);
}

void CFrameBuffer::paintHLineRelInternal(int x, int dx, int y, const fb_pixel_t col)
{
	fb_pixel_t * dest = getFrameBufferPointer() + x + swidth * y;
	for (int i = 0; i < dx; i++)
		*(dest++) = col;
}

void CFrameBuffer::paintHLineRel(int x, int dx, int y, const fb_pixel_t col)
{
	if (!getActive())
		return;

	paintHLineRelInternal(x, dx, y, col);
	mark(x, y, x + dx, y);
}

void CFrameBuffer::setIconBasePath(const std::string & iconPath)
{
	iconBasePath = iconPath;
}

std::string CFrameBuffer::getIconPath(std::string icon_name, std::string file_type)
{
	std::string path, filetype;
	filetype = "." + file_type;
	path = std::string(ICONSDIR_VAR) + "/" + icon_name + filetype;
	if (access(path.c_str(), F_OK))
		path = iconBasePath + "/" + icon_name + filetype;
	if (icon_name.find("/", 0) != std::string::npos)
		path = icon_name;
	return path;
}

void CFrameBuffer::getIconSize(const char * const filename, int* width, int *height)
{
	*width = 0;
	*height = 0;

	if(filename == NULL)
		return;
	//check for full path, icon don't have full path, or ?
	if (filename[0]== '/'){
		return;
	}

	std::map<std::string, rawIcon>::iterator it;


	/* if code ask for size, lets cache it. assume we have enough ram for cache */
	/* FIXME offset seems never used in code, always default = 1 ? */

	it = icon_cache.find(filename);
	if(it == icon_cache.end()) {
		if(paintIcon(filename, 0, 0, 0, 1, false)) {
			it = icon_cache.find(filename);
		}
	}
	if(it != icon_cache.end()) {
		*width = it->second.width;
		*height = it->second.height;
	}
}

bool CFrameBuffer::paintIcon8(const std::string & filename, const int x, const int y, const unsigned char offset)
{
	if (!getActive())
		return false;

//printf("%s(file, %d, %d, %d)\n", __FUNCTION__, x, y, offset);

	struct rawHeader header;
	uint16_t	 width, height;
	int	      lfd;

	lfd = open((iconBasePath + "/" + filename).c_str(), O_RDONLY);

	if (lfd == -1) {
		printf("paintIcon8: error while loading icon: %s/%s\n", iconBasePath.c_str(), filename.c_str());
		return false;
	}

	read(lfd, &header, sizeof(struct rawHeader));

	width  = (header.width_hi  << 8) | header.width_lo;
	height = (header.height_hi << 8) | header.height_lo;

	if (width > 768) {
		/* this is not going to happen, but check anyway */
		printf("%s: icon %s too wide (%d)\n", __func__, filename.c_str(), (int)width);
		close(lfd);
		return false;
	}
	unsigned char pixbuf[768];

	fb_pixel_t *d = getFrameBufferPointer() + x + swidth * y;
	fb_pixel_t * d2;
	for (int count=0; count<height; count ++ ) {
		read(lfd, &pixbuf[0], width );
		unsigned char *pixpos = &pixbuf[0];
		d2 = d;
		for (int count2=0; count2<width; count2 ++ ) {
			unsigned char color = *pixpos;
			if (color != header.transp) {
//printf("icon8: col %d transp %d real %08X\n", color+offset, header.transp, realcolor[color+offset]);
				paintPixel(d2, color + offset);
			}
			d2++;
			pixpos++;
		}
		d += swidth;
	}
	close(lfd);
	mark(x, y, x + width, y + height);
	return true;
}

/* paint icon at position x/y,
   if height h is given, center vertically between y and y+h
   offset is a color offset (probably only useful with palette) */
bool CFrameBuffer::paintIcon(const std::string & filename, const int x, const int y,
			     const int h, const unsigned char offset, bool paint, bool paintBg, const fb_pixel_t colBg)
{
	struct rawHeader header;
	int	 width, height;
	fb_pixel_t * data;
	struct rawIcon tmpIcon;
	std::map<std::string, rawIcon>::iterator it;

	if (!getActive())
		return false;

	int  yy = y;
	//printf("CFrameBuffer::paintIcon: load %s\n", filename.c_str());fflush(stdout);

	/* we cache and check original name */
	it = icon_cache.find(filename);
	if(it == icon_cache.end()) {
		std::string newname = getIconPath(filename);
		//printf("CFrameBuffer::paintIcon: check for %s\n", newname.c_str());fflush(stdout);

		data = g_PicViewer->getIcon(newname, &width, &height);

		if(data) { //TODO: intercepting of possible full icon cache, that could cause strange behavior while painting of uncached icons
			int dsize = width*height*sizeof(fb_pixel_t);
			//printf("CFrameBuffer::paintIcon: %s found, data %x size %d x %d\n", newname.c_str(), data, width, height);fflush(stdout);
			if(cache_size+dsize < ICON_CACHE_SIZE) {
				cache_size += dsize;
				tmpIcon.width = width;
				tmpIcon.height = height;
				tmpIcon.data = data;
				icon_cache.insert(std::pair <std::string, rawIcon> (filename, tmpIcon));
				//printf("Cached %s, cache size %d\n", newname.c_str(), cache_size);
			}
			goto _display;
		}

		newname = getIconPath(filename, "raw");

		int lfd = open(newname.c_str(), O_RDONLY);

		if (lfd == -1) {
			//printf("paintIcon: error while loading icon: %s\n", newname.c_str());
			return false;
		}

		ssize_t s = read(lfd, &header, sizeof(struct rawHeader));
		if (s < 0) {
			perror("read");
			return false;
		}

		if (s < (ssize_t) sizeof(rawHeader)){
			printf("paintIcon: error while loading icon: %s, header too small\n", newname.c_str());
			return false;
		}


		tmpIcon.width = width  = (header.width_hi  << 8) | header.width_lo;
		tmpIcon.height = height = (header.height_hi << 8) | header.height_lo;
		if (!width || !height) {
			printf("paintIcon: error while loading icon: %s, wrong dimensions (%dHx%dW)\n", newname.c_str(), height, width);
			return false;
		}

		int dsize = width*height*sizeof(fb_pixel_t);

		tmpIcon.data = (fb_pixel_t*) cs_malloc_uncached(dsize);
		data = tmpIcon.data;

		unsigned char pixbuf[768];
		for (int count = 0; count < height; count ++ ) {
			read(lfd, &pixbuf[0], width >> 1 );
			unsigned char *pixpos = &pixbuf[0];
			for (int count2 = 0; count2 < width >> 1; count2 ++ ) {
				unsigned char compressed = *pixpos;
				unsigned char pix1 = (compressed & 0xf0) >> 4;
				unsigned char pix2 = (compressed & 0x0f);
				if (pix1 != header.transp)
					*data++ = realcolor[pix1+offset];
				else
					*data++ = 0;
				if (pix2 != header.transp)
					*data++ = realcolor[pix2+offset];
				else
					*data++ = 0;
				pixpos++;
			}
		}
		close(lfd);

		data = tmpIcon.data;

		if(cache_size+dsize < ICON_CACHE_SIZE) {
			cache_size += dsize;
			icon_cache.insert(std::pair <std::string, rawIcon> (filename, tmpIcon));
			//printf("Cached %s, cache size %d\n", newname.c_str(), cache_size);
		}
	} else {
		data = it->second.data;
		width = it->second.width;
		height = it->second.height;
		//printf("paintIcon: already cached %s %d x %d\n", newname.c_str(), width, height);
	}
_display:
	if(!paint)
		return true;

	if (h != 0)
		yy += (h - height) / 2;

	checkFbArea(x, yy, width, height, true);
	if (paintBg)
		paintBoxRel(x, yy, width, height, colBg);
	blit2FB(data, width, height, x, yy);
	checkFbArea(x, yy, width, height, false);
	return true;
}

void CFrameBuffer::loadPal(const std::string & filename, const unsigned char offset, const unsigned char endidx)
{
	if (!getActive())
		return;

//printf("%s()\n", __FUNCTION__);

	struct rgbData rgbdata;
	int	    lfd;

	lfd = open((iconBasePath + "/" + filename).c_str(), O_RDONLY);

	if (lfd == -1) {
		printf("error while loading palette: %s/%s\n", iconBasePath.c_str(), filename.c_str());
		return;
	}

	int pos = 0;
	int readb = read(lfd, &rgbdata,  sizeof(rgbdata) );
	while(readb) {
		__u32 rgb = (rgbdata.r<<16) | (rgbdata.g<<8) | (rgbdata.b);
		int colpos = offset+pos;
		if( colpos>endidx)
			break;

		paletteSetColor(colpos, rgb, 0xFF);
		readb = read(lfd, &rgbdata,  sizeof(rgbdata) );
		pos++;
	}
	paletteSet(&cmap);
	close(lfd);
}

void CFrameBuffer::paintPixel(const int x, const int y, const fb_pixel_t col)
{
	if (!getActive())
		return;

	fb_pixel_t * pos = getFrameBufferPointer();
	pos += swidth * y;
	pos += x;

	*pos = col;
}

void CFrameBuffer::paintShortHLineRelInternal(const int& x, const int& dx, const int& y, const fb_pixel_t& col)
{
	fb_pixel_t *dest = getFrameBufferPointer() + x + swidth * y;
	for (int i = 0; i < dx; i++)
		*(dest++) = col;
}

int CFrameBuffer::limitRadius(const int& dx, const int& dy, int& radius)
{
	if (radius > dx)
		return dx;
	if (radius > dy)
		return dy;
	if (radius > 540)
		return 540;
	return radius;
}

void CFrameBuffer::setCornerFlags(const int& type)
{
	corner_tl = (type & CORNER_TOP_LEFT)     == CORNER_TOP_LEFT;
	corner_tr = (type & CORNER_TOP_RIGHT)    == CORNER_TOP_RIGHT;
	corner_bl = (type & CORNER_BOTTOM_LEFT)  == CORNER_BOTTOM_LEFT;
	corner_br = (type & CORNER_BOTTOM_RIGHT) == CORNER_BOTTOM_RIGHT;
}

void CFrameBuffer::initQCircle()
{
	/* this table contains the x coordinates for a quarter circle (the bottom right quarter) with fixed
	   radius of 540 px which is the half of the max HD graphics size of 1080 px. So with that table we
	   ca draw boxes with round corners and als circles by just setting dx = dy = radius (max 540). */
	static const int _q_circle[541] = {
		540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540,
		540, 540, 540, 540, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539,
		539, 538, 538, 538, 538, 538, 538, 538, 538, 538, 538, 538, 538, 537, 537, 537, 537, 537, 537, 537,
		537, 537, 536, 536, 536, 536, 536, 536, 536, 536, 535, 535, 535, 535, 535, 535, 535, 535, 534, 534,
		534, 534, 534, 534, 533, 533, 533, 533, 533, 533, 532, 532, 532, 532, 532, 532, 531, 531, 531, 531,
		531, 531, 530, 530, 530, 530, 529, 529, 529, 529, 529, 529, 528, 528, 528, 528, 527, 527, 527, 527,
		527, 526, 526, 526, 526, 525, 525, 525, 525, 524, 524, 524, 524, 523, 523, 523, 523, 522, 522, 522,
		522, 521, 521, 521, 521, 520, 520, 520, 519, 519, 519, 518, 518, 518, 518, 517, 517, 517, 516, 516,
		516, 515, 515, 515, 515, 514, 514, 514, 513, 513, 513, 512, 512, 512, 511, 511, 511, 510, 510, 510,
		509, 509, 508, 508, 508, 507, 507, 507, 506, 506, 506, 505, 505, 504, 504, 504, 503, 503, 502, 502,
		502, 501, 501, 500, 500, 499, 499, 499, 498, 498, 498, 497, 497, 496, 496, 496, 495, 495, 494, 494,
		493, 493, 492, 492, 491, 491, 490, 490, 490, 489, 489, 488, 488, 487, 487, 486, 486, 485, 485, 484,
		484, 483, 483, 482, 482, 481, 481, 480, 480, 479, 479, 478, 478, 477, 477, 476, 476, 475, 475, 474,
		473, 473, 472, 472, 471, 471, 470, 470, 469, 468, 468, 467, 466, 466, 465, 465, 464, 464, 463, 462,
		462, 461, 460, 460, 459, 459, 458, 458, 457, 456, 455, 455, 454, 454, 453, 452, 452, 451, 450, 450,
		449, 449, 448, 447, 446, 446, 445, 445, 444, 443, 442, 441, 441, 440, 440, 439, 438, 437, 436, 436,
		435, 435, 434, 433, 432, 431, 431, 430, 429, 428, 427, 427, 426, 425, 425, 424, 423, 422, 421, 421,
		420, 419, 418, 417, 416, 416, 415, 414, 413, 412, 412, 411, 410, 409, 408, 407, 406, 405, 404, 403,
		403, 402, 401, 400, 399, 398, 397, 397, 395, 394, 393, 393, 392, 391, 390, 389, 388, 387, 386, 385,
		384, 383, 382, 381, 380, 379, 378, 377, 376, 375, 374, 373, 372, 371, 369, 368, 367, 367, 365, 364,
		363, 362, 361, 360, 358, 357, 356, 355, 354, 353, 352, 351, 350, 348, 347, 346, 345, 343, 342, 341,
		340, 339, 337, 336, 335, 334, 332, 331, 329, 328, 327, 326, 324, 323, 322, 321, 319, 317, 316, 315,
		314, 312, 310, 309, 308, 307, 305, 303, 302, 301, 299, 297, 296, 294, 293, 291, 289, 288, 287, 285,
		283, 281, 280, 278, 277, 275, 273, 271, 270, 268, 267, 265, 263, 261, 259, 258, 256, 254, 252, 250,
		248, 246, 244, 242, 240, 238, 236, 234, 232, 230, 228, 225, 223, 221, 219, 217, 215, 212, 210, 207,
		204, 202, 200, 197, 195, 192, 190, 187, 184, 181, 179, 176, 173, 170, 167, 164, 160, 157, 154, 150,
		147, 144, 140, 136, 132, 128, 124, 120, 115, 111, 105, 101,  95,  89,  83,  77,  69,  61,  52,  40,
		 23};
	if (q_circle == NULL)
		q_circle = new int[sizeof(_q_circle) / sizeof(int)];
	memcpy(q_circle, _q_circle, sizeof(_q_circle));
}

bool CFrameBuffer::calcCorners(int *ofs, int *ofl, int *ofr, const int& dy, const int& line, const int& radius, const int& type)
{
/* just an multiplicator for all math to reduce rounding errors */
#define MUL 32768
	int scl, _ofs = 0;
	bool ret = false;
	if (ofl != NULL) *ofl = 0;
	if (ofr != NULL) *ofr = 0;
	int scf = (540 * MUL) / ((radius < 1) ? 1 : radius);
	/* one of the top corners */
	if (line < radius && (type & CORNER_TOP)) {
		/* uper round corners */
		scl = scf * (radius - line) / MUL;
		if ((scf * (radius - line) % MUL) >= (MUL / 2)) /* round up */
			scl++;
		_ofs =  radius - (q_circle[scl] * MUL / scf);
		if (ofl != NULL) *ofl = corner_tl ? _ofs : 0;
		if (ofr != NULL) *ofr = corner_tr ? _ofs : 0;
	}
	/* one of the bottom corners */
	else if ((line >= dy - radius) && (type & CORNER_BOTTOM)) {
		/* lower round corners */
		scl = scf * (radius - (dy - (line + 1))) / MUL;
		if ((scf * (radius - (dy - (line + 1))) % MUL) >= (MUL / 2)) /* round up */
			scl++;
		_ofs =  radius - (q_circle[scl] * MUL / scf);
		if (ofl != NULL) *ofl = corner_bl ? _ofs : 0;
		if (ofr != NULL) *ofr = corner_br ? _ofs : 0;
	}
	else
		ret = true;
	if (ofs != NULL) *ofs = _ofs;
	return ret;
}

void CFrameBuffer::paintBoxFrame(const int x, const int y, const int dx, const int dy, const int px, const fb_pixel_t col, int radius, int type)
{
	if (!getActive())
		return;

	if (dx == 0 || dy == 0) {
		dprintf(DEBUG_NORMAL, "[CFrameBuffer] [%s - %d]: radius %d, start x %d y %d end x %d y %d\n",  __func__, __LINE__, radius, x, y, x+dx, y+dy);
		return;
	}
	if (radius < 0)
		dprintf(DEBUG_NORMAL, "[CFrameBuffer] [%s - %d]: WARNING! radius < 0 [%d] FIXIT\n", __func__, __LINE__, radius);

	setCornerFlags(type);
	int rad_tl = 0, rad_tr = 0, rad_bl = 0, rad_br = 0;
	if (type && radius) {
		int x_rad = radius - 1;
		if (corner_tl) rad_tl = x_rad;
		if (corner_tr) rad_tr = x_rad;
		if (corner_bl) rad_bl = x_rad;
		if (corner_br) rad_br = x_rad;
	}
	paintBoxRel(x + rad_tl , y          , dx - rad_tl - rad_tr, px                  , col); // top horizontal
	paintBoxRel(x + rad_bl , y + dy - px, dx - rad_bl - rad_br, px                  , col); // bottom horizontal
	paintBoxRel(x          , y + rad_tl , px                  , dy - rad_tl - rad_bl, col); // left vertical
	paintBoxRel(x + dx - px, y + rad_tr , px                  , dy - rad_tr - rad_br, col); // right vertical

	if (type && radius) {
		radius = limitRadius(dx, dy, radius);
		int line = 0;
		waitForIdle("CFrameBuffer::paintBoxFrame");
		while (line < dy) {
			int ofs = 0, ofs_i = 0;
			// inner box
			if ((line >= px) && (line < (dy - px)))
				ofs_i = calcCornersOffset(dy - 2*px, line-px, radius-px, type);
			// outer box
			ofs = calcCornersOffset(dy, line, radius, type);

			int _x     = x + ofs;
			int _x_end = x + dx;
			int _y     = y + line;
			if ((line < px) || (line >= (dy - px))) {
				// left
				if (((corner_tl) && (line < radius)) || ((corner_bl) && (line >= dy - radius)))
					paintShortHLineRelInternal(_x, radius - ofs, _y, col);
				// right
				if (((corner_tr) && (line < radius)) || ((corner_br) && (line >= dy - radius)))
					paintShortHLineRelInternal(_x_end - radius, radius - ofs, _y, col);
			}
			else if (line < (dy - px)) {
				int _dx = (ofs_i-ofs) + px;
				// left
				if (((corner_tl) && (line < radius)) || ((corner_bl) && (line >= dy - radius)))
					paintShortHLineRelInternal(_x, _dx, _y, col);
				// right
				if (((corner_tr) && (line < radius)) || ((corner_br) && (line >= dy - radius)))
					paintShortHLineRelInternal(_x_end - ofs_i - px, _dx, _y, col);
			}
			if ((line == radius) && (dy > 2*radius))
				// line outside the rounded corners
				line = dy - radius;
			else
				line++;
		}
	}
}

void CFrameBuffer::paintLine(int xa, int ya, int xb, int yb, const fb_pixel_t col)
{
	if (!getActive())
		return;

	int dx = abs (xa - xb);
	int dy = abs (ya - yb);
	int x;
	int y;
	int End;
	int step;

	if ( dx > dy )
	{
		int	p = 2 * dy - dx;
		int	twoDy = 2 * dy;
		int	twoDyDx = 2 * (dy-dx);

		if ( xa > xb )
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

		paintPixel (x, y, col);

		while( x < End )
		{
			x++;
			if ( p < 0 )
				p += twoDy;
			else
			{
				y += step;
				p += twoDyDx;
			}
			paintPixel (x, y, col);
		}
	}
	else
	{
		int	p = 2 * dx - dy;
		int	twoDx = 2 * dx;
		int	twoDxDy = 2 * (dx-dy);

		if ( ya > yb )
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

		paintPixel (x, y, col);

		while( y < End )
		{
			y++;
			if ( p < 0 )
				p += twoDx;
			else
			{
				x += step;
				p += twoDxDy;
			}
			paintPixel (x, y, col);
		}
	}
	mark(xa, ya, xb, yb);
}
#if 0
//never used
void CFrameBuffer::setBackgroundColor(const fb_pixel_t color)
{
	backgroundColor = color;
}

bool CFrameBuffer::loadPictureToMem(const std::string & filename, const uint16_t width, const uint16_t height, const uint16_t pstride, fb_pixel_t * memp)
{
	struct rawHeader header;
	int	      lfd;

//printf("%s(%d, %d, memp)\n", __FUNCTION__, width, height);

	lfd = open((iconBasePath + "/" + filename).c_str(), O_RDONLY );

	if (lfd == -1)
	{
		printf("error while loading icon: %s/%s\n", iconBasePath.c_str(), filename.c_str());
		return false;
	}

	read(lfd, &header, sizeof(struct rawHeader));

	if ((width  != ((header.width_hi  << 8) | header.width_lo)) ||
	    (height != ((header.height_hi << 8) | header.height_lo)))
	{
		printf("error while loading icon: %s - invalid resolution = %hux%hu\n", filename.c_str(), width, height);
		close(lfd);
		return false;
	}

	if ((pstride == 0) || (pstride == width * sizeof(fb_pixel_t)))
		read(lfd, memp, height * width * sizeof(fb_pixel_t));
	else
		for (int i = 0; i < height; i++)
			read(lfd, ((uint8_t *)memp) + i * pstride, width * sizeof(fb_pixel_t));

	close(lfd);
	return true;
}

bool CFrameBuffer::loadPicture2Mem(const std::string & filename, fb_pixel_t * memp)
{
	return loadPictureToMem(filename, BACKGROUNDIMAGEWIDTH, 576, 0, memp);
}

bool CFrameBuffer::loadPicture2FrameBuffer(const std::string & filename)
{
	if (!getActive())
		return false;

	return loadPictureToMem(filename, BACKGROUNDIMAGEWIDTH, 576, getStride(), getFrameBufferPointer());
}

bool CFrameBuffer::savePictureFromMem(const std::string & filename, const fb_pixel_t * const memp)
{
	struct rawHeader header;
	uint16_t	 width, height;
	int	      lfd;

	width = BACKGROUNDIMAGEWIDTH;
	height = 576;

	header.width_lo  = width  &  0xFF;
	header.width_hi  = width  >>    8;
	header.height_lo = height &  0xFF;
	header.height_hi = height >>    8;
	header.transp    =	      0;

	lfd = open((iconBasePath + "/" + filename).c_str(), O_WRONLY | O_CREAT, 0644);

	if (lfd==-1)
	{
		printf("error while saving icon: %s/%s", iconBasePath.c_str(), filename.c_str() );
		return false;
	}

	write(lfd, &header, sizeof(struct rawHeader));

	write(lfd, memp, width * height * sizeof(fb_pixel_t));

	close(lfd);
	return true;
}

bool CFrameBuffer::loadBackground(const std::string & filename, const unsigned char offset)
{
	if ((backgroundFilename == filename) && (background))
		return true;

	if (background)
		delete[] background;

	background = new fb_pixel_t[BACKGROUNDIMAGEWIDTH * 576];

	if (!loadPictureToMem(filename, BACKGROUNDIMAGEWIDTH, 576, 0, background))
	{
		delete[] background;
		background=0;
		return false;
	}

	if (offset != 0)//pic-offset
	{
		fb_pixel_t * bpos = background;
		int pos = BACKGROUNDIMAGEWIDTH * 576;
		while (pos > 0)
		{
			*bpos += offset;
			bpos++;
			pos--;
		}
	}

	fb_pixel_t * dest = background + BACKGROUNDIMAGEWIDTH * 576;
	uint8_t    * src  = ((uint8_t * )background)+ BACKGROUNDIMAGEWIDTH * 576;
	for (int i = 576 - 1; i >= 0; i--)
		for (int j = BACKGROUNDIMAGEWIDTH - 1; j >= 0; j--)
		{
			dest--;
			src--;
			paintPixel(dest, *src);
		}
	backgroundFilename = filename;

	return true;
}

bool CFrameBuffer::loadBackgroundPic(const std::string & filename, bool show)
{
	if ((backgroundFilename == filename) && (background))
		return true;

//printf("loadBackgroundPic: %s\n", filename.c_str());
	if (background){
		delete[] background;
		background = NULL;
	}
	background = g_PicViewer->getImage(iconBasePath + "/" + filename, BACKGROUNDIMAGEWIDTH, 576);

	if (background == NULL) {
		background=0;
		return false;
	}

	backgroundFilename = filename;
	if(show) {
		useBackgroundPaint = true;
		paintBackground();
	}
	return true;
}
#endif
void CFrameBuffer::useBackground(bool ub)
{
	useBackgroundPaint = ub;
	if(!useBackgroundPaint) {
		delete[] background;
		background=0;
	}
}

bool CFrameBuffer::getuseBackground(void)
{
	return useBackgroundPaint;
}

void CFrameBuffer::saveBackgroundImage(void)
{
	if (backupBackground != NULL){
		delete[] backupBackground;
		backupBackground = NULL;
	}
	backupBackground = background;
	//useBackground(false); // <- necessary since no background is available
	useBackgroundPaint = false;
	background = NULL;
}

void CFrameBuffer::restoreBackgroundImage(void)
{
	fb_pixel_t * tmp = background;

	if (backupBackground != NULL)
	{
		background = backupBackground;
		backupBackground = NULL;
	}
	else
		useBackground(false); // <- necessary since no background is available

	if (tmp != NULL){
		delete[] tmp;
		tmp = NULL;
	}
}

void CFrameBuffer::paintBackgroundBoxRel(int x, int y, int dx, int dy)
{
	if (!getActive())
		return;

	checkFbArea(x, y, dx, dy, true);
	if(!useBackgroundPaint)
	{
		paintBoxRel(x, y, dx, dy, backgroundColor);
	}
	else
	{
		fb_pixel_t * fbpos = getFrameBufferPointer() + x + swidth * y;
		fb_pixel_t * bkpos = background + x + BACKGROUNDIMAGEWIDTH * y;
		for(int count = 0;count < dy; count++)
		{
			memmove(fbpos, bkpos, dx * sizeof(fb_pixel_t));
			fbpos += swidth;
			bkpos += BACKGROUNDIMAGEWIDTH;
		}
	}
	checkFbArea(x, y, dx, dy, false);
}

void CFrameBuffer::paintBackground()
{
	if (!getActive())
		return;

	checkFbArea(0, 0, xRes, yRes, true);
	if (useBackgroundPaint && (background != NULL))
	{
		for (int i = 0; i < 576; i++)
			memmove(getFrameBufferPointer() + i * swidth, (background + i * BACKGROUNDIMAGEWIDTH), BACKGROUNDIMAGEWIDTH * sizeof(fb_pixel_t));
	}
	else
	{
		paintBoxRel(0, 0, xRes, yRes, backgroundColor);
	}
	checkFbArea(0, 0, xRes, yRes, false);
}

void CFrameBuffer::SaveScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp)
{
	if (!getActive())
		return;

	checkFbArea(x, y, dx, dy, true);
	fb_pixel_t * pos = getFrameBufferPointer() + x + swidth * y;
	fb_pixel_t * bkpos = memp;
	for (int count = 0; count < dy; count++) {
		fb_pixel_t * dest = (fb_pixel_t *)pos;
		for (int i = 0; i < dx; i++)
			//*(dest++) = col;
			*(bkpos++) = *(dest++);
		pos += swidth;
	}
#if 0 //FIXME test to flush cache
	if (ioctl(fd, 1, FB_BLANK_UNBLANK) < 0);
#endif
	//RestoreScreen(x, y, dx, dy, memp); //FIXME
#if 0
	fb_pixel_t * fbpos = getFrameBufferPointer() + x + swidth * y;
	fb_pixel_t * bkpos = memp;
	for (int count = 0; count < dy; count++)
	{
		memmove(bkpos, fbpos, dx * sizeof(fb_pixel_t));
		fbpos += swidth;
		bkpos += dx;
	}
#endif
	checkFbArea(x, y, dx, dy, false);

}

void CFrameBuffer::RestoreScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp)
{
	if (!getActive())
		return;

	checkFbArea(x, y, dx, dy, true);
	fb_pixel_t * fbpos = getFrameBufferPointer() + x + swidth * y;
	fb_pixel_t * bkpos = memp;
	for (int count = 0; count < dy; count++)
	{
		memmove(fbpos, bkpos, dx * sizeof(fb_pixel_t));
		fbpos += swidth;
		bkpos += dx;
	}
	mark(x, y, x + dx, y + dy);
	checkFbArea(x, y, dx, dy, false);
}

void CFrameBuffer::Clear()
{
	paintBackground();
	//memset(getFrameBufferPointer(), 0, stride * yRes);
}

void CFrameBuffer::showFrame(const std::string & filename)
{
	std::string picture = std::string(ICONSDIR_VAR) + "/" + filename;
	if (access(picture.c_str(), F_OK))
		picture = iconBasePath + "/" + filename;
	if (filename.find("/", 0) != std::string::npos)
		picture = filename;

	videoDecoder->ShowPicture(picture.c_str());
}

void CFrameBuffer::stopFrame()
{
	videoDecoder->StopPicture();
}

bool CFrameBuffer::Lock()
{
	if(locked)
		return false;
	locked = true;
	return true;
}

void CFrameBuffer::Unlock()
{
	locked = false;
}

void * CFrameBuffer::int_convertRGB2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y, int transp, bool alpha)
{
	unsigned long i;
	unsigned int *fbbuff;
	unsigned long count;

	if (!x || !y) {
		printf("convertRGB2FB%s: Error: invalid dimensions (%luX x %luY)\n",
		       ((alpha) ? " (Alpha)" : ""), x, y);
		return NULL;
	}

	count = x * y;

	fbbuff = (unsigned int *) cs_malloc_uncached(count * sizeof(unsigned int));
	if(fbbuff == NULL) {
		printf("convertRGB2FB%s: Error: cs_malloc_uncached\n", ((alpha) ? " (Alpha)" : ""));
		return NULL;
	}

	if (alpha) {
		for(i = 0; i < count ; i++)
			fbbuff[i] = ((rgbbuff[i*4+3] << 24) & 0xFF000000) |
				    ((rgbbuff[i*4]   << 16) & 0x00FF0000) |
				    ((rgbbuff[i*4+1] <<  8) & 0x0000FF00) |
				    ((rgbbuff[i*4+2])       & 0x000000FF);
	} else {
		switch (m_transparent) {
			case CFrameBuffer::TM_BLACK:
				for(i = 0; i < count ; i++) {
					transp = 0;
					if(rgbbuff[i*3] || rgbbuff[i*3+1] || rgbbuff[i*3+2])
						transp = 0xFF;
					fbbuff[i] = (transp << 24) | ((rgbbuff[i*3] << 16) & 0xFF0000) | ((rgbbuff[i*3+1] << 8) & 0xFF00) | (rgbbuff[i*3+2] & 0xFF);
				}
				break;
			case CFrameBuffer::TM_INI:
				for(i = 0; i < count ; i++)
					fbbuff[i] = (transp << 24) | ((rgbbuff[i*3] << 16) & 0xFF0000) | ((rgbbuff[i*3+1] << 8) & 0xFF00) | (rgbbuff[i*3+2] & 0xFF);
				break;
			case CFrameBuffer::TM_NONE:
			default:
				for(i = 0; i < count ; i++)
					fbbuff[i] = 0xFF000000 | ((rgbbuff[i*3] << 16) & 0xFF0000) | ((rgbbuff[i*3+1] << 8) & 0xFF00) | (rgbbuff[i*3+2] & 0xFF);
				break;
		}
	}
	return (void *) fbbuff;
}

void * CFrameBuffer::convertRGB2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y, int transp)
{
	return int_convertRGB2FB(rgbbuff, x, y, transp, false);
}

void * CFrameBuffer::convertRGBA2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y)
{
	return int_convertRGB2FB(rgbbuff, x, y, 0, true);
}

void CFrameBuffer::fbCopyArea(uint32_t width, uint32_t height, uint32_t dst_x, uint32_t dst_y, uint32_t src_x, uint32_t src_y)
{
	uint32_t  w_, h_, i;
	fb_pixel_t *fromBuf = NULL, *toBuf = NULL;
	fb_pixel_t *dst_p, *src_p;
	fb_pixel_t * fbp = getFrameBufferPointer();
	fb_pixel_t * bbp = getBackBufferPointer();
	w_ = (width > xRes) ? xRes : width;
	h_ = (height > yRes) ? yRes : height;

	if ((src_y < yRes) && (dst_y < yRes)) {		/* copy within framebuffer */
		fromBuf = fbp;
		toBuf   = fbp;
	}
	else if ((src_y >= yRes) && (dst_y >= yRes)) {	/* copy within backbuffer */
		fromBuf = bbp;
		toBuf   = bbp;
		dst_y  -= yRes;
		src_y  -= yRes;
	}
	else if (src_y >= yRes) {			/* copy backbuffer => framebuffer */
		fromBuf = bbp;
		toBuf   = fbp;
		src_y  -= yRes;
	}
	else if (dst_y >= yRes) {			/* copy framebuffer => backbuffer */
		fromBuf = fbp;
		toBuf   = bbp;
		dst_y  -= yRes;
	}
	if ((fromBuf == NULL) || (toBuf == NULL)) {
	//printf(">>>>> [%s:%d] buff = NULL\n", __func__, __LINE__);
		return;
	}
	if ((src_x == dst_x) && (src_y == dst_y) && (fromBuf == toBuf)) {	/* self copy? */
	//printf(">>>>> [%s:%d] self copy?\n", __func__, __LINE__);
		return;
	}

	dst_p = toBuf + dst_y*swidth;
	src_p = fromBuf + src_y*swidth;
	if ((w_ == xRes) && (swidth == xRes)) {		/* copy full width */
	//printf(">>>>> [%s:%d] copy full width - dst_p: %p, src_p: %p\n", __func__, __LINE__, dst_p, src_p);
		memcpy(dst_p, src_p, w_*h_*sizeof(fb_pixel_t));
	}
	else {						/* copy all other */
	//printf(">>>>> [%s:%d] copy all other - dst_p: %p, src_p: %p\n", __func__, __LINE__, dst_p, src_p);
		uint32_t wMem = w_*sizeof(fb_pixel_t);
		for (i = 0; i < h_; i++) {
			memcpy(dst_p+dst_x, src_p+src_x, wMem);
			dst_p += swidth;
			src_p += swidth;
		}
	}
}

void CFrameBuffer::blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp, uint32_t yp, bool /*transp*/)
{
	int  xc, yc;

	xc = (width > xRes) ? xRes : width;
	yc = (height > yRes) ? yRes : height;

	fb_pixel_t*  data = (fb_pixel_t *) fbbuff;

	fb_pixel_t * d = getFrameBufferPointer() + xoff + swidth * yoff;
	fb_pixel_t * d2;

	for (int count = 0; count < yc; count++ ) {
		fb_pixel_t *pixpos = &data[(count + yp) * width];
		d2 = (fb_pixel_t *) d;
		for (int count2 = 0; count2 < xc; count2++ ) {
			fb_pixel_t pix = *(pixpos + xp);
			if ((pix & 0xff000000) == 0xff000000)
				*d2 = pix;
			else {
				uint8_t *in = (uint8_t *)(pixpos + xp);
				uint8_t *out = (uint8_t *)d2;
				int a = in[3];	/* TODO: big/little endian */
				*out = (*out + ((*in - *out) * a) / 256);
				in++; out++;
				*out = (*out + ((*in - *out) * a) / 256);
				in++; out++;
				*out = (*out + ((*in - *out) * a) / 256);
			}
			d2++;
			pixpos++;
		}
		d += swidth;
	}
}

void CFrameBuffer::blitBox2FB(const fb_pixel_t* boxBuf, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff)
{
	if(width <1 || height <1 || !boxBuf )
		return;

	uint32_t xc = (width > xRes) ? (uint32_t)xRes : width;
	uint32_t yc = (height > yRes) ? (uint32_t)yRes : height;

	fb_pixel_t *fbp = getFrameBufferPointer() + (swidth * yoff);
	fb_pixel_t* data = (fb_pixel_t*)boxBuf;

	uint32_t line = 0;
	while (line < yc) {
		fb_pixel_t *pixpos = &data[line * xc];
		for (uint32_t pos = xoff; pos < xoff + xc; pos++) {
			//don't paint backgroundcolor (*pixpos = 0x00000000)
			if (*pixpos)
				*(fbp + pos) = *pixpos;
			pixpos++;
		}
		fbp += swidth;
		line++;
	}
}

void CFrameBuffer::displayRGB(unsigned char *rgbbuff, int x_size, int y_size, int x_pan, int y_pan, int x_offs, int y_offs, bool clearfb, int transp)
{
	void *fbbuff = NULL;

	if(rgbbuff == NULL)
		return;

	/* correct panning */
	if(x_pan > x_size - (int)xRes) x_pan = 0;
	if(y_pan > y_size - (int)yRes) y_pan = 0;

	/* correct offset */
	if(x_offs + x_size > (int)xRes) x_offs = 0;
	if(y_offs + y_size > (int)yRes) y_offs = 0;

	/* blit buffer 2 fb */
	fbbuff = convertRGB2FB(rgbbuff, x_size, y_size, transp);
	if(fbbuff==NULL)
		return;

	/* ClearFB if image is smaller */
	/* if(x_size < (int)xRes || y_size < (int)yRes) */
	if(clearfb)
		CFrameBuffer::getInstance()->Clear();

	blit2FB(fbbuff, x_size, y_size, x_offs, y_offs, x_pan, y_pan);
	cs_free_uncached(fbbuff);
}

// ## AudioMute / Clock ######################################

void CFrameBuffer::setFbArea(int element, int _x, int _y, int _dx, int _dy)
{
	if (_x == 0 && _y == 0 && _dx == 0 && _dy == 0) {
		// delete area
		for (fbarea_iterator_t it = v_fbarea.begin(); it != v_fbarea.end(); ++it) {
			if (it->element == element) {
				v_fbarea.erase(it);
				break;
			}
		}
		if (v_fbarea.empty()) {
			fbAreaActiv = false;
		}
	}
	else {
		// change area
		bool found = false;
		for (unsigned int i = 0; i < v_fbarea.size(); i++) {
			if (v_fbarea[i].element == element) {
				v_fbarea[i].x = _x;
				v_fbarea[i].y = _y;
				v_fbarea[i].dx = _dx;
				v_fbarea[i].dy = _dy;
				found = true;
				break;
			}
		}
		// set new area
		if (!found) {
			fb_area_t area;
			area.x = _x;
			area.y = _y;
			area.dx = _dx;
			area.dy = _dy;
			area.element = element;
			v_fbarea.push_back(area);
		}
		fbAreaActiv = true;
	}
}

int CFrameBuffer::checkFbAreaElement(int _x, int _y, int _dx, int _dy, fb_area_t *area)
{
	if (fb_no_check)
		return FB_PAINTAREA_MATCH_NO;

	if (_y > area->y + area->dy)
		return FB_PAINTAREA_MATCH_NO;
	if (_x + _dx < area->x)
		return FB_PAINTAREA_MATCH_NO;
	if (_x > area->x + area->dx)
		return FB_PAINTAREA_MATCH_NO;
	if (_y + _dy < area->y)
		return FB_PAINTAREA_MATCH_NO;
	return FB_PAINTAREA_MATCH_OK;
}

bool CFrameBuffer::_checkFbArea(int _x, int _y, int _dx, int _dy, bool prev)
{
	if (v_fbarea.empty())
		return true;

	static bool firstMutePaint = true;

	for (unsigned int i = 0; i < v_fbarea.size(); i++) {
		int ret = checkFbAreaElement(_x, _y, _dx, _dy, &v_fbarea[i]);
		if (ret == FB_PAINTAREA_MATCH_OK) {
			switch (v_fbarea[i].element) {
				case FB_PAINTAREA_MUTEICON1:
					if (!do_paint_mute_icon)
						break;
//					waitForIdle();
					fb_no_check = true;
					if (prev) {
						firstMutePaint = false;
						CAudioMute::getInstance()->hide();
					}
					else {
						if (!firstMutePaint)
							CAudioMute::getInstance()->paint();
					}
					fb_no_check = false;
					break;
				default:
					break;
			}
		}
	}

	return true;
}

/* dummy, can be implemented in CFbAccel */
void CFrameBuffer::mark(int , int , int , int )
{
}

uint32_t CFrameBuffer::getWidth4FB_HW_ACC(const uint32_t /*x*/, const uint32_t w, const bool /*max*/)
{
	return w;
}
