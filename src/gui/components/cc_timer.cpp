/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Generic Timer.
	Copyright (C) 2013, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "cc_timer.h"
#include <pthread.h>
#include <errno.h>
#include <system/helpers.h>
#include <system/debug.h>

using namespace std;

CComponentsTimer::CComponentsTimer( const int& interval)
{
	tm_thread 		= 0;
	tm_interval		= interval;

	sl = sigc::mem_fun(*this, &CComponentsTimer::stopTimer);
	if (interval > 0)
		startTimer();
}

CComponentsTimer::~CComponentsTimer()
{
	if (stopTimer())
		dprintf(DEBUG_INFO,"[CComponentsTimer]    [%s]  timer stopped\n", __func__);
}

//thread handle
void* CComponentsTimer::initTimerThread(void *arg)
{
	CComponentsTimer *timer = static_cast<CComponentsTimer*>(arg);

	//start loop
	while(timer) {
		timer->mutex.lock();
		timer->OnTimer();
		mySleep(timer->tm_interval);
		timer->mutex.unlock();
	}

	return 0;
}

//start up running timer with own thread, return true on succses
bool CComponentsTimer::startTimer()
{
	void *ptr = static_cast<void*>(this);

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS,0);

	if(!tm_thread) {
		int res = pthread_create (&tm_thread, NULL, initTimerThread, ptr) ;
		if (res != 0){
			dprintf(DEBUG_NORMAL,"[CComponentsTimer]    [%s]  pthread_create  %s\n", __func__, strerror(errno));
			return false;
		}
		dprintf(DEBUG_INFO,"[CComponentsTimer]    [%s]  timer thread [%lu] created with interval = %d\n", __func__, tm_thread, tm_interval);
	}

	//ensure kill of thread on any restart of neutrino
	CNeutrinoApp::getInstance()->OnBeforeRestart.connect(sl);
	return  true;
}

//stop ticking timer and kill thread, return true on succses
bool CComponentsTimer::stopTimer()
{
	int thres = 0;
	if(tm_thread) {
		thres = pthread_cancel(tm_thread);
		dprintf(DEBUG_INFO,"[CComponentsTimer]    [%s] waiting for timer thread terminate ...\n", __func__);
		if (thres != 0)
			dprintf(DEBUG_NORMAL,"[CComponentsTimer]    [%s] pthread_cancel  %s\n", __func__, strerror(errno));
		thres = pthread_join(tm_thread, NULL);
		if (thres != 0)
			dprintf(DEBUG_NORMAL, "[CComponentsTimer]    [%s] pthread_join  %s\n", __func__, strerror(errno));
	}
	if (thres == 0){
		tm_thread = 0;
		//ensure disconnect of unused slot
		sl.disconnect();
		return true;
	}

	return false;
}
