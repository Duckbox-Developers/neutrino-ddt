/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2011 CoolStream International Ltd

	parts based on AiO Screengrabber (C) Seddi seddi@ihad.tv

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <png.h>
#include <zlib.h>

#include <global.h>
#include <neutrino.h>
#include <gui/filebrowser.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/messagebox.h>
#include <daemonc/remotecontrol.h>
#include <zapit/debug.h>
#include <zapit/getservices.h>
#include <eitd/sectionsd.h>

#include <video.h>
#include <cs_api.h>
#include <driver/screenshot.h>

extern "C" {
#include <jpeglib.h>
}

extern cVideo *videoDecoder;

/* constructor, defaults is empty fname and CScreenShot::FORMAT_JPG format */
CScreenShot::CScreenShot(const std::string fname, screenshot_format_t fmt)
{
	format = fmt;
	filename = fname;
	pixel_data = NULL;
	fd = NULL;
	xres = 0;
	yres = 0;
	get_video = g_settings.screenshot_video;
	get_osd = g_settings.screenshot_mode;
	scale_to_video = g_settings.screenshot_scale;
}

CScreenShot::~CScreenShot()
{
}

/* try to get video frame data in ARGB format, restore GXA state */
bool CScreenShot::GetData()
{
	static OpenThreads::Mutex mutex;
	bool res = false;

	mutex.lock();
#ifdef USE_NEVIS_GXA
	CFrameBuffer::getInstance()->setActive(false);
#endif
	if (videoDecoder->getBlank()) 
		get_video = false;
#if 1 // to enable after libcs/drivers update
	res = videoDecoder->GetScreenImage(pixel_data, xres, yres, get_video, get_osd, scale_to_video);
#endif

#ifdef USE_NEVIS_GXA
	/* sort of hack. GXA used to transfer/convert live image to RGB,
	 * so setup GXA back */
	CFrameBuffer::getInstance()->setupGXA();
	CFrameBuffer::getInstance()->add_gxa_sync_marker();
	CFrameBuffer::getInstance()->setActive(true);
#endif
	mutex.unlock();
	if (!res) {
		printf("CScreenShot::Start: GetScreenImage failed\n");
		return false;
	}

	printf("CScreenShot::GetData: data: %p %d x %d\n", pixel_data, xres, yres);
	return true;
}

/* start ::run in new thread to save file in selected format */
bool CScreenShot::Start()
{
	bool ret = false;
	if(GetData())
		ret = (start() == 0);
	else
		delete this;
	return ret;
}

/* thread function to save data asynchroniosly. delete itself after saving */
void CScreenShot::run()
{
	printf("CScreenShot::run save to %s format %d\n", filename.c_str(), format);
	detach();
	setCancelModeDisable();
	setSchedulePriority(THREAD_PRIORITY_MIN);
	bool ret = SaveFile();
	printf("CScreenShot::run: %s finished: %d\n", filename.c_str(), ret);
	delete this;
}

/* save file in sync mode, return true if save ok, or false */
bool CScreenShot::StartSync()
{
	bool ret = false;
	printf("CScreenShot::StartSync save to %s format %d\n", filename.c_str(), format);
	if(GetData())
		ret = SaveFile();

	printf("CScreenShot::StartSync: %s finished: %d\n", filename.c_str(), ret);
	return ret;
}

/* save file in selected format, free data received from video decoder */
bool CScreenShot::SaveFile()
{
	bool ret = true;

	switch (format) {
	case FORMAT_PNG:
		ret = SavePng();
		break;
	default:
		printf("CScreenShot::SaveFile unsupported format %d, using jpeg\n", format);
	case FORMAT_JPG:
		ret = SaveJpg();
		break;
	case FORMAT_BMP:
		ret = SaveBmp();
		break;
	}

	cs_free_uncached((void *) pixel_data);
	return ret;
}

/* try to open file, return true if success, or false */
bool CScreenShot::OpenFile()
{
	fd = fopen(filename.c_str(), "w");
	if (!fd) {
		printf("CScreenShot::OpenFile: failed to open %s\n", filename.c_str());
		return false;
	}
	return true;
}

/* save screenshot in png format, return true if success, or false */
bool CScreenShot::SavePng()
{
	png_bytep *row_pointers;
	png_structp png_ptr;
	png_infop info_ptr;

	TIMER_START();
	if(!OpenFile())
		return false;

	row_pointers = (png_bytep *) malloc(sizeof(png_bytep) * yres);
	if (!row_pointers) {
		printf("CScreenShot::SavePng: malloc error\n");
		fclose(fd);
		return false;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, (png_error_ptr)NULL, (png_error_ptr)NULL);
	info_ptr = png_create_info_struct(png_ptr);
#if (PNG_LIBPNG_VER < 10500)
	if (setjmp(png_ptr->jmpbuf))
#else
	if (setjmp(png_jmpbuf(png_ptr)))
#endif
	{
		printf("CScreenShot::SavePng: %s save error\n", filename.c_str());
		png_destroy_write_struct(&png_ptr, &info_ptr);
		free(row_pointers);
		fclose(fd);
		return false;
	}

	png_init_io(png_ptr, fd);

	int y;
	for (y=0; y<yres; y++) {
		row_pointers[y] = pixel_data + (y*xres*4);
	}

	png_set_IHDR(png_ptr, info_ptr, xres, yres, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	//png_set_filter (png_ptr, 0, PNG_FILTER_NONE);

	png_set_compression_level(png_ptr, Z_BEST_SPEED);

	png_set_bgr(png_ptr);
#ifdef BOXMODEL_APOLLO
	png_set_invert_alpha(png_ptr);
#endif
	png_write_info(png_ptr, info_ptr);
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	free(row_pointers);
	fclose(fd);
	TIMER_STOP(filename.c_str());
	return true;
}

#define SWAP(x,y)       { x ^= y; y ^= x; x ^= y; }

/* from libjpg example.c */
struct my_error_mgr {
	struct jpeg_error_mgr pub;    /* "public" fields */
	jmp_buf setjmp_buffer;        /* for return to caller */
};
typedef struct my_error_mgr * my_error_ptr;

void my_error_exit (j_common_ptr cinfo)
{
	/* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
	my_error_ptr myerr = (my_error_ptr) cinfo->err;

	/* Always display the message. */
	/* We could postpone this until after returning, if we chose. */
	(*cinfo->err->output_message) (cinfo);

	/* Return control to the setjmp point */
	longjmp(myerr->setjmp_buffer, 1);
}

/* save screenshot in jpg format, return true if success, or false */
bool CScreenShot::SaveJpg()
{
	int quality = 90;

	TIMER_START();
	if(!OpenFile())
		return false;

	for (int y = 0; y < yres; y++) {
		int xres1 = y*xres*3;
		int xres2 = xres1+2;
		for (int x = 0; x < xres; x++) {
			int x2 = x*3;
			memcpy(pixel_data + x2 + xres1, pixel_data + x*4 + y*xres*4, 3);
			SWAP(pixel_data[x2 + xres1], pixel_data[x2 + xres2]);
		}
	}

	struct jpeg_compress_struct cinfo;
	struct my_error_mgr jerr;
	JSAMPROW row_pointer[1];
	unsigned int row_stride;

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;

	if (setjmp(jerr.setjmp_buffer)) {
		printf("CScreenShot::SaveJpg: %s save error\n", filename.c_str());
		jpeg_destroy_compress(&cinfo);
		fclose(fd);
		return false;
	}

	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, fd);

	cinfo.image_width = xres;
	cinfo.image_height = yres;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	cinfo.dct_method = JDCT_IFAST;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, TRUE);
	row_stride = xres * 3;

	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = & pixel_data[cinfo.next_scanline * row_stride];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	fclose(fd);
	TIMER_STOP(filename.c_str());
	return true;
}

/* save screenshot in bmp format, return true if success, or false */
bool CScreenShot::SaveBmp()
{
	TIMER_START();
	if(!OpenFile())
		return false;

	unsigned char hdr[14 + 40];
	unsigned int i = 0;
#define PUT32(x) hdr[i++] = ((x)&0xFF); hdr[i++] = (((x)>>8)&0xFF); hdr[i++] = (((x)>>16)&0xFF); hdr[i++] = (((x)>>24)&0xFF);
#define PUT16(x) hdr[i++] = ((x)&0xFF); hdr[i++] = (((x)>>8)&0xFF);
#define PUT8(x) hdr[i++] = ((x)&0xFF);
	PUT8('B'); PUT8('M');
	PUT32((((xres * yres) * 3 + 3) &~ 3) + 14 + 40);
	PUT16(0); PUT16(0); PUT32(14 + 40);
	PUT32(40); PUT32(xres); PUT32(yres);
	PUT16(1);
	PUT16(4*8); // bits
	PUT32(0); PUT32(0); PUT32(0); PUT32(0); PUT32(0); PUT32(0);
#undef PUT32
#undef PUT16
#undef PUT8
	fwrite(hdr, 1, i, fd);

	int y;
	for (y=yres-1; y>=0 ; y-=1) {
		fwrite(pixel_data+(y*xres*4),xres*4,1,fd);
	}
	fclose(fd);
	TIMER_STOP(filename.c_str());
	return true;

}

/* 
 * create filename member from channel name and its current EPG data,
 * with added date and time including msecs and suffix for selected format 
 */
void CScreenShot::MakeFileName(const t_channel_id channel_id)
{
	char		fname[512]; // UTF-8
	std::string	channel_name;
	CEPGData	epgData;
	unsigned int	pos = 0;

	snprintf(fname, sizeof(fname), "%s/", g_settings.screenshot_dir.c_str());
	pos = strlen(fname);

	channel_name = CServiceManager::getInstance()->GetServiceName(channel_id);
	if (!(channel_name.empty())) {
		strcpy(&(fname[pos]), UTF8_TO_FILESYSTEM_ENCODING(channel_name.c_str()));
		ZapitTools::replace_char(&fname[pos]);
		strcat(fname, "_");
	}
	pos = strlen(fname);

	if(CEitManager::getInstance()->getActualEPGServiceKey(channel_id, &epgData)) {
		CShortEPGData epgdata;
		if(CEitManager::getInstance()->getEPGidShort(epgData.eventID, &epgdata)) {
			if (!(epgdata.title.empty())) {
				strcpy(&(fname[pos]), epgdata.title.c_str());
				ZapitTools::replace_char(&fname[pos]);
			}
		}
	}
	pos = strlen(fname);

	struct timeval tv;
	gettimeofday(&tv, NULL);	
	strftime(&(fname[pos]), sizeof(fname) - pos - 1, "_%Y%m%d_%H%M%S", localtime(&tv.tv_sec));
	pos = strlen(fname);
	snprintf(&(fname[pos]), sizeof(fname) - pos - 1, "_%03d", (int) tv.tv_usec/1000);

	switch (format) {
	case FORMAT_PNG:
		strcat(fname, ".png");
		break;
	default:
		printf("CScreenShot::MakeFileName unsupported format %d, using jpeg\n", format);
	case FORMAT_JPG:
		strcat(fname, ".jpg");
		break;
	case FORMAT_BMP:
		strcat(fname, ".bmp");
		break;
	}
	printf("CScreenShot::MakeFileName: [%s]\n", fname);
	filename = std::string(fname);
}
