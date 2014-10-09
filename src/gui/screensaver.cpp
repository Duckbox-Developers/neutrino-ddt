/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2012-2013 defans@bluepeercrew.us

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
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <global.h>
#include <neutrino.h>
#include <pthread.h>

#include "screensaver.h"

#include <video.h>
extern cVideo * videoDecoder;


CScreensaver::CScreensaver()
{
	thrScreenSaver = 0;
	m_frameBuffer = CFrameBuffer::getInstance();
#if HAVE_DUCKBOX_HARDWARE
	m_viewer = new CPictureViewer();
#endif
	last_pic = 0;
}

CScreensaver::~CScreensaver()
{
	if(thrScreenSaver)
		pthread_cancel(thrScreenSaver);
	thrScreenSaver = 0;
}

void CScreensaver::start()
{
	firstRun = true;
	
	if(!thrScreenSaver)
	{
		//printf("[%s] %s: starting thread\n", __FILE__, __FUNCTION__);
		pthread_create(&thrScreenSaver, NULL, ScreenSaverPrg, (void*) this);
		pthread_detach(thrScreenSaver);	
	}	

}

void CScreensaver::stop()
{
	if(thrScreenSaver)
	{
		pthread_cancel(thrScreenSaver);
		thrScreenSaver = 0;
#if HAVE_DUCKBOX_HARDWARE
		m_frameBuffer->paintBackgroundBoxRel(0,0,m_frameBuffer->getScreenWidth(true),m_frameBuffer->getScreenHeight(true));
#endif
	}
}

void* CScreensaver::ScreenSaverPrg(void* arg)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
	
	CScreensaver * PScreenSaver = static_cast<CScreensaver*>(arg);
	
	while(1)
	{
		PScreenSaver->read_dir();
		sleep(10);
	}
	return 0;	
}

void CScreensaver::read_dir()
{
	char* dir_name = (char *) g_settings.audioplayer_screensaver_dir.c_str();
	struct dirent *dirpointer;
	DIR *dir;
	char curr_ext[5];
	char first_pic[255];
	char fname[255];
	int curr_lenght;
	char* p;
	int pic_cnt;
	bool first_pic_found = false;


	/* open dir */
	if((dir=opendir(dir_name)) == NULL) {
		fprintf(stderr,"[CScreensaver] Error opendir ...\n");
		return;
	}

	pic_cnt = 0;
	/* read complete dir */
	while((dirpointer=readdir(dir)) != NULL)
	{
		curr_lenght = strlen((*dirpointer).d_name);
//		printf("%d\n",curr_lenght);
		if(curr_lenght > 4)
		{
			strncpy(curr_ext,(*dirpointer).d_name+(curr_lenght-4),5);
			for (p = curr_ext; *p; ++p) *p = tolower(*p);
//			printf("%s\n",curr_ext);
			if(
				   (!strcmp(".jpg",curr_ext))
//				|| (!strcmp(".png",curr_ext))
//				|| (!strcmp(".bmp",curr_ext))
			)
			{
				if(!first_pic_found)
				{
					strncpy(first_pic,(*dirpointer).d_name,sizeof(first_pic));
					first_pic_found = true;
					if(firstRun)
					{
						m_frameBuffer->Clear();
						firstRun = false;
					}
				}
//				printf("-->%s\n",curr_ext);
				if(last_pic == pic_cnt)
				{
					printf("[CScreensaver] ShowPicture: %s/%s\n", dir_name, (*dirpointer).d_name);
					sprintf(fname, "%s/%s", dir_name, (*dirpointer).d_name);
#if HAVE_DUCKBOX_HARDWARE
					m_viewer->DisplayImage(fname,0,0,m_frameBuffer->getScreenWidth(true),m_frameBuffer->getScreenHeight(true),CFrameBuffer::TM_NONE);
#else
					videoDecoder->StopPicture();
					videoDecoder->ShowPicture(fname);
#endif
					last_pic ++;
					if(closedir(dir) == -1)
						printf("[CScreensaver] Error no closed %s\n", dir_name);
					return;
				}
				pic_cnt++;
			}
		}
	}
	
	last_pic = 1;
	
	if(first_pic_found)
	{
		printf("[CScreensaver] ShowPicture: %s/%s\n", dir_name, first_pic);
		sprintf(fname, "%s/%s", dir_name, first_pic);
#if HAVE_DUCKBOX_HARDWARE
		m_viewer->DisplayImage(fname,0,0,m_frameBuffer->getScreenWidth(true),m_frameBuffer->getScreenHeight(true),CFrameBuffer::TM_NONE);
#else
		videoDecoder->StopPicture();
		videoDecoder->ShowPicture(fname);
#endif
	}
	else
		printf("[CScreensaver] Error, no picture found\n");

	/* close pointer */
	if(closedir(dir) == -1)
		printf("[CScreensaver] Error no closed %s\n", dir_name);	
}
