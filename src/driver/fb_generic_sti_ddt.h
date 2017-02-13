/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2007-2013 Stefan Seyfried
	Copyright (C) 2017 TangoCash

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


#ifndef __framebuffer__
#define __framebuffer__
#include <config.h>

#include <stdint.h>
#include <linux/fb.h>
#include <linux/vt.h>

#include <string>
#include <map>
#include <vector>
#include <sigc++/signal.h>
#include <pthread.h>

#include <linux/stmfb.h>

#define fb_pixel_t uint32_t

typedef struct fb_var_screeninfo t_fb_var_screeninfo;

typedef struct gradientData_t
{
	fb_pixel_t* gradientBuf;
	fb_pixel_t* boxBuf;
	bool direction;
	int mode;
	int x;
	int dx;
} gradientData_struct_t;

#define CORNER_NONE		0x0
#define CORNER_TOP_LEFT		0x1
#define CORNER_TOP_RIGHT	0x2
#define CORNER_TOP		0x3
#define CORNER_BOTTOM_RIGHT	0x4
#define CORNER_RIGHT		0x6
#define CORNER_BOTTOM_LEFT	0x8
#define CORNER_LEFT		0x9
#define CORNER_BOTTOM		0xC
#define CORNER_ALL		0xF

#define FADE_TIME 10000
#define FADE_STEP 5
#define FADE_RESET 0xFFFF

#define WINDOW_SIZE_MAX		100 // %
#define WINDOW_SIZE_MIN		50 // %
#define WINDOW_SIZE_MIN_FORCED	80 // %
#define ConnectLineBox_Width	16 // px

class CFbAccel;
/** Ausfuehrung als Singleton */
class CFrameBuffer : public sigc::trackable
{
	friend class CFbAccel;
	private:
		CFrameBuffer();

		struct rgbData
		{
			uint8_t r;
			uint8_t g;
			uint8_t b;
		} __attribute__ ((packed));

		struct rawHeader
		{
			uint8_t width_lo;
			uint8_t width_hi;
			uint8_t height_lo;
			uint8_t height_hi;
			uint8_t transp;
		} __attribute__ ((packed));

		struct rawIcon
		{
			uint16_t width;
			uint16_t height;
			uint8_t transp;
			fb_pixel_t * data;
		};

		std::string     iconBasePath;

		int             fd, tty;
		fb_pixel_t *    lfb;
		int		available;
		fb_pixel_t *    background;
		fb_pixel_t *    backupBackground;
		fb_pixel_t      backgroundColor;
		std::string     backgroundFilename;
		bool            useBackgroundPaint;
		unsigned int	xRes, yRes, stride, bpp;
		t_fb_var_screeninfo screeninfo, oldscreen;
		fb_cmap cmap;
		__u16 red[256], green[256], blue[256], trans[256];

		void paletteFade(int i, __u32 rgb1, __u32 rgb2, int level);

		int 	kd_mode;
		struct	vt_mode vt_mode;
		bool	active;
		static	void switch_signal (int);
		fb_fix_screeninfo fix;
		bool locked;
		std::map<std::string, rawIcon> icon_cache;
		int cache_size;
		void * int_convertRGB2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y, int transp, bool alpha);
		int m_transparent_default, m_transparent;

		inline void paintHLineRelInternal2Buf(const int& x, const int& dx, const int& y, const int& box_dx, const fb_pixel_t& col, fb_pixel_t* buf);

		CFbAccel *accel_sti_ddt;

	public:
		///gradient direction
		enum {
			gradientHorizontal,
			gradientVertical
		};

		enum {
			pbrg_noOption = 0x00,
			pbrg_noPaint  = 0x01,
			pbrg_noFree   = 0x02
		};

		fb_pixel_t realcolor[256];

		~CFrameBuffer();

		static CFrameBuffer* getInstance();

		void init(const char * const fbDevice = "/dev/fb0");
		int setMode(unsigned int xRes, unsigned int yRes, unsigned int bpp);


		int getFileHandle() const; //only used for plugins (games) !!
		t_fb_var_screeninfo *getScreenInfo();

		fb_pixel_t * getFrameBufferPointer(bool real = false); // pointer to framebuffer
		fb_pixel_t * getBackBufferPointer() const;  // pointer to backbuffer
		unsigned int getStride() const;             // size of a single line in the framebuffer (in bytes)
		unsigned int getScreenWidth(bool real = false);
		unsigned int getScreenHeight(bool real = false); 
		unsigned int getScreenWidthRel(bool force_small = false);
		unsigned int getScreenHeightRel(bool force_small = false);
		unsigned int getScreenX();
		unsigned int getScreenY();
		
		bool getActive() const;                     // is framebuffer active?
		void setActive(bool enable);                     // is framebuffer active?

		void setTransparency( int tr = 0 );
		void setBlendMode(uint8_t mode = 1);
		void setBlendLevel(int level);

		void setMixerColor(uint32_t mixer_background);

		//Palette stuff
		void setAlphaFade(int in, int num, int tr);
		void paletteGenFade(int in, __u32 rgb1, __u32 rgb2, int num, int tr=0);
		void paletteSetColor(int i, __u32 rgb, int tr);
		void paletteSet(struct fb_cmap *map = NULL);

		//paint functions
		inline void paintPixel(fb_pixel_t * const dest, const uint8_t color) const
			{
				*dest = realcolor[color];
			};
		void paintPixel(int x, int y, const fb_pixel_t col);

		fb_pixel_t* paintBoxRel2Buf(const int dx, const int dy, const int w_align, const int offs_align, const fb_pixel_t col, fb_pixel_t* buf = NULL, int radius = 0, int type = CORNER_ALL);
		fb_pixel_t* paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col, gradientData_t *gradientData, int radius = 0, int type = CORNER_ALL);

		void paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col, int radius = 0, int type = CORNER_ALL);
		inline void paintBox(int xa, int ya, int xb, int yb, const fb_pixel_t col) { paintBoxRel(xa, ya, xb - xa, yb - ya, col); }
		inline void paintBox(int xa, int ya, int xb, int yb, const fb_pixel_t col, int radius, int type) { paintBoxRel(xa, ya, xb - xa, yb - ya, col, radius, type); }

		void paintBoxFrame(const int x, const int y, const int dx, const int dy, const int px, const fb_pixel_t col, const int rad = 0, int type = CORNER_ALL);
		void paintLine(int xa, int ya, int xb, int yb, const fb_pixel_t col);

		void paintVLine(int x, int ya, int yb, const fb_pixel_t col);
		void paintVLineRel(int x, int y, int dy, const fb_pixel_t col);

		void paintHLine(int xa, int xb, int y, const fb_pixel_t col);
		void paintHLineRel(int x, int dx, int y, const fb_pixel_t col);

		void setIconBasePath(const std::string & iconPath);
		std::string getIconBasePath(){return iconBasePath;}
		std::string getIconPath(std::string icon_name, std::string file_type = "png");

		void getIconSize(const char * const filename, int* width, int *height);
		/* h is the height of the target "window", if != 0 the icon gets centered in that window */
		bool paintIcon (const std::string & filename, const int x, const int y, 
				const int h = 0, const unsigned char offset = 1, bool paint = true, bool paintBg = false, const fb_pixel_t colBg = 0);
		bool paintIcon8(const std::string & filename, const int x, const int y, const unsigned char offset = 0);
		void loadPal   (const std::string & filename, const unsigned char offset = 0, const unsigned char endidx = 255);

		bool loadPicture2Mem        (const std::string & filename, fb_pixel_t * const memp);
		bool loadPicture2FrameBuffer(const std::string & filename);
		bool loadPictureToMem       (const std::string & filename, const uint16_t width, const uint16_t height, const uint16_t stride, fb_pixel_t * const memp);
		bool savePictureFromMem     (const std::string & filename, const fb_pixel_t * const memp);

		int getBackgroundColor() { return backgroundColor;}
		void setBackgroundColor(const fb_pixel_t color);
		bool loadBackground(const std::string & filename, const unsigned char col = 0);
		void useBackground(bool);
		bool getuseBackground(void);

		void saveBackgroundImage(void);  // <- implies useBackground(false);
		void restoreBackgroundImage(void);

		void paintBackgroundBoxRel(int x, int y, int dx, int dy);
		inline void paintBackgroundBox(int xa, int ya, int xb, int yb) { paintBackgroundBoxRel(xa, ya, xb - xa, yb - ya); }

		void paintBackground();

		void SaveScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp);
		void RestoreScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp);

		void Clear();
		void showFrame(const std::string & filename);
		void stopFrame();
		bool loadBackgroundPic(const std::string & filename, bool show = true);
		bool Lock(void);
		void Unlock(void);
		bool Locked(void) { return locked; };
		void waitForIdle(const char *func = NULL);
		void* convertRGB2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y, int transp = 0xFF);
		void* convertRGBA2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y);
		void displayRGB(unsigned char *rgbbuff, int x_size, int y_size, int x_pan, int y_pan, int x_offs, int y_offs, bool clearfb = true, int transp = 0xFF);
		void blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp = 0, uint32_t yp = 0, bool transp = false);
		void blitBox2FB(const fb_pixel_t* boxBuf, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff);

		void mark(int x, int y, int dx, int dy);
		void blit();

		enum 
			{
				TM_EMPTY  = 0,
				TM_NONE   = 1,
				TM_BLACK  = 2,
				TM_INI    = 3
			};
		void SetTransparent(int t){ m_transparent = t; }
		void SetTransparentDefault(){ m_transparent = m_transparent_default; }
		bool OSDShot(const std::string &name);
		enum Mode3D { Mode3D_off = 0, Mode3D_SideBySide, Mode3D_TopAndBottom, Mode3D_Tile, Mode3D_SIZE };
		void set3DMode(Mode3D);
		Mode3D get3DMode(void);
	private:
		enum Mode3D mode3D;

	public:
		void blitArea(int src_width, int src_height, int fb_x, int fb_y, int width, int height);
		void ClearFB(void);
		void resChange(void);
		void setBorder(int sx, int sy, int ex, int ey);
		void getBorder(int &sx, int &sy, int &ex, int &ey);
		void setBorderColor(fb_pixel_t col = 0);
		fb_pixel_t getBorderColor(void);

	private:
		bool autoBlitStatus;
		pthread_t autoBlitThreadId;
		static void *autoBlitThread(void *arg);
		void autoBlitThread();

	public:
		void autoBlit(bool b = true);
		void blitBPA2FB(unsigned char *mem, SURF_FMT fmt, int w, int h, int x = 0, int y = 0, int pan_x = -1, int pan_y = -1, int fb_x = -1, int fb_y = -1, int fb_w = -1, int fb_h = -1, int transp = false);

// ## AudioMute / Clock ######################################
	private:
		enum {
			FB_PAINTAREA_MATCH_NO,
			FB_PAINTAREA_MATCH_OK
		};

		typedef struct fb_area_t
		{
			int x;
			int y;
			int dx;
			int dy;
			int element;
		} fb_area_struct_t;

		bool fbAreaActiv;
		typedef std::vector<fb_area_t> v_fbarea_t;
		typedef v_fbarea_t::iterator fbarea_iterator_t;
		v_fbarea_t v_fbarea;
		bool fb_no_check;
		bool do_paint_mute_icon;

		bool _checkFbArea(int _x, int _y, int _dx, int _dy, bool prev);
		int checkFbAreaElement(int _x, int _y, int _dx, int _dy, fb_area_t *area);

	public:
		enum {
			FB_PAINTAREA_INFOCLOCK,
			FB_PAINTAREA_MUTEICON1,
			FB_PAINTAREA_MUTEICON2,

			FB_PAINTAREA_MAX
		};

		inline bool checkFbArea(int _x, int _y, int _dx, int _dy, bool prev) { return (fbAreaActiv && !fb_no_check) ? _checkFbArea(_x, _y, _dx, _dy, prev) : true; }
		void setFbArea(int element, int _x=0, int _y=0, int _dx=0, int _dy=0);
		void fbNoCheck(bool noCheck) { fb_no_check = noCheck; }
		void doPaintMuteIcon(bool mode) { do_paint_mute_icon = mode; }
		sigc::signal<void> OnAfterSetPallette;
};

#endif
