/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Bugfixes/cleanups (C) 2007-2013,2015 Stefan Seyfried
	(C) 2008 Novell, Inc. Author: Stefan Seyfried

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "infoviewer.h"
#include "infoviewer_bb.h"

#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <sys/timeb.h>
#include <sys/param.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include <global.h>
#include <neutrino.h>

#include <gui/bouquetlist.h>
#include <gui/widget/icons.h>
#include <gui/widget/hintbox.h>
#include <gui/customcolor.h>
#include <gui/pictureviewer.h>
#include <gui/movieplayer.h>
#include <gui/infoclock.h>

#include <system/helpers.h>

#include <daemonc/remotecontrol.h>
#include <driver/record.h>
#include <driver/display.h>
#include <driver/volume.h>
#include <driver/radiotext.h>

#include <zapit/satconfig.h>
#include <zapit/femanager.h>
#include <zapit/zapit.h>
#include <eitd/sectionsd.h>
#include <video.h>

extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */
extern CBouquetList * bouquetList;       /* neutrino.cpp */
extern CPictureViewer * g_PicViewer;
extern cVideo * videoDecoder;


#define LEFT_OFFSET 5

event_id_t CInfoViewer::last_curr_id = 0, CInfoViewer::last_next_id = 0;

static bool sortByDateTime (const CChannelEvent& a, const CChannelEvent& b)
{
	return a.startTime < b.startTime;
}

extern bool timeset;

CInfoViewer::CInfoViewer ()
	: fader(g_settings.theme.infobar_alpha)
{
	sigbox = NULL;
	header = numbox = body = rec = NULL;
	txt_cur_start = txt_cur_event = txt_cur_event_rest = txt_next_start = txt_next_event = txt_next_in = NULL;
	timescale = NULL;
	info_CurrentNext.current_zeit.startzeit = 0;
	info_CurrentNext.current_zeit.dauer = 0;
	info_CurrentNext.flags = 0;
	clock = NULL;
	frameBuffer = CFrameBuffer::getInstance();
	infoViewerBB = CInfoViewerBB::getInstance();
	InfoHeightY = 0;
	ButtonWidth = 0;
	rt_dx = 0;
	rt_dy = 0;
	ChanNameX = 0;
	ChanNameY = 0;
	ChanWidth = 0;
	ChanHeight = 0;
	numbox_offset = 0;
	time_width = 0;
	time_height = header_height = 0;
	lastsnr = 0;
	lastsig = 0;
	lasttime = 0;
	aspectRatio = 0;
	ChanInfoX = 0;
	Init();
	infoViewerBB->Init();
	oldinfo.current_uniqueKey = 0;
	oldinfo.next_uniqueKey = 0;
	isVolscale = false;
	info_time_width = 0;
	timeoutEnd = 0;
	sec_timer_id = 0;
}

CInfoViewer::~CInfoViewer()
{
	ResetModules();
}

void CInfoViewer::Init()
{
	BoxStartX = BoxStartY = BoxEndX = BoxEndY = 0;
	initClock();
	recordModeActive = false;
	is_visible = false;
	showButtonBar = false;
	//gotTime = g_Sectionsd->getIsTimeSet ();
	gotTime = timeset;
	zap_mode = IV_MODE_DEFAULT;
	newfreq = true;
	chanready = 1;
	fileplay = 0;
	SDT_freq_update = false;

	/* maybe we should not tie this to the blinkenlights settings? */
	infoViewerBB->initBBOffset();
	/* after font size changes, Init() might be called multiple times */
	changePB();

	casysChange = g_settings.infobar_casystem_display;
	channellogoChange = g_settings.infobar_show_channellogo;

	current_channel_id = CZapit::getInstance()->GetCurrentChannelID();;
	current_epg_id = 0;
	lcdUpdateTimer = 0;
	rt_x = rt_y = rt_h = rt_w = 0;

	infobar_txt = NULL;

	_livestreamInfo1.clear();
	_livestreamInfo2.clear();
}

/*
 * This nice ASCII art should hopefully explain how all the variables play together ;)
 *

              ___BoxStartX
             |-ChanWidth-|
             |           |  _recording icon                 _progress bar
 BoxStartY---+-----------+ |                               |
     |       |           | *                              #######____
     |       |           |-------------------------------------------+--+-ChanNameY-----+
     |       |           | Channelname (header)              | clock |  | header height |
 ChanHeight--+-----------+-------------------------------------------+--+               |
                |                 B---O---D---Y                      |                  |InfoHeightY
                |01:23     Current Event                             |                  |
                |02:34     Next Event                                |                  |
                |                                                    |                  |
     BoxEndY----+----------------------------------------------------+--+---------------+
                |                     optional blinkenlights iconbar |  bottom_bar_offset
     BBarY------+----------------------------------------------------+--+
                | * red   * green  * yellow  * blue ====== [DD][16:9]|  InfoHeightY_Info
                +----------------------------------------------------+--+
                |              asize               |                 |
                                                             BoxEndX-/
*/
void CInfoViewer::start ()
{
	info_time_width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth("22:22") + 10;

	InfoHeightY = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getHeight() * 9/8 +
		      2 * g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight() + 25;
	infoViewerBB->Init();

	ChanWidth = max(125, 4 * g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getMaxDigitWidth() + 10);

	ChanHeight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getHeight()/* * 9/8*/;
	ChanHeight += g_SignalFont->getHeight()/2;
	ChanHeight = max(75, ChanHeight);
	numbox_offset = 3;

	BoxStartX = g_settings.screen_StartX + 10;
	BoxEndX = g_settings.screen_EndX - 10;
	BoxEndY = g_settings.screen_EndY - 10 - infoViewerBB->InfoHeightY_Info - infoViewerBB->bottom_bar_offset;
	BoxStartY = BoxEndY - InfoHeightY - ChanHeight / 2;

	ChanNameY = BoxStartY + (ChanHeight / 2) + SHADOW_OFFSET;
	ChanInfoX = BoxStartX + (ChanWidth / 3);

	initClock();
	time_height = max(ChanHeight / 2, clock->getHeight());
	time_width = clock->getWidth();
}

void CInfoViewer::ResetPB()
{
	if (sigbox){
		delete sigbox;
		sigbox = NULL;
	}

	if (timescale){
		timescale->reset();
	}
}

void CInfoViewer::changePB()
{
	ResetPB();
	if (!timescale){
		timescale = new CProgressBar();
		timescale->setType(CProgressBar::PB_TIMESCALE);
	}
}

void CInfoViewer::initClock()
{

	int gradient_top = g_settings.theme.infobar_gradient_top;

	//basic init for clock object
	if (clock == NULL){
		clock = new CComponentsFrmClock();
		clock->setClockFormat("%H:%M", "%H %M");
		clock->setClockIntervall(1);
	}

	CInfoClock::getInstance()->disableInfoClock();
	clock->enableColBodyGradient(gradient_top, COL_INFOBAR_PLUS_0);
	clock->doPaintBg(!gradient_top);
	clock->enableTboxSaveScreen(gradient_top);
	clock->setColorBody(COL_INFOBAR_PLUS_0);
	clock->setCorner(RADIUS_LARGE, CORNER_TOP_RIGHT);
	clock->setClockFont(g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]);
	clock->setPos(BoxEndX - 10 - clock->getWidth(), ChanNameY);
	clock->setTextColor(COL_INFOBAR_TEXT);
}

void CInfoViewer::showRecordIcon (const bool show)
{
	/* FIXME if record or timeshift stopped while infobar visible, artifacts */

	CRecordManager * crm		= CRecordManager::getInstance();
	
	recordModeActive		= crm->RecordingStatus();
	if (recordModeActive)
	{
		std::string rec_icon = NEUTRINO_ICON_REC_GRAY;
		std::string ts_icon  = NEUTRINO_ICON_AUTO_SHIFT_GRAY;

		t_channel_id cci	= g_RemoteControl->current_channel_id;
		/* global record mode */
		int rec_mode 		= crm->GetRecordMode();
		/* channel record mode */
		int ccrec_mode 		= crm->GetRecordMode(cci);

		/* set 'active' icons for current channel */
		if (ccrec_mode & CRecordManager::RECMODE_REC)
			rec_icon = NEUTRINO_ICON_REC;

		if (ccrec_mode & CRecordManager::RECMODE_TSHIFT)
			ts_icon  = NEUTRINO_ICON_AUTO_SHIFT;

		int records = crm->GetRecordCount();
		
		int txt_h = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();
		int txt_w = 0;

		int box_x = BoxStartX + ChanWidth + 2*SHADOW_OFFSET;
		int box_y = BoxStartY + SHADOW_OFFSET;
		int box_w = 0;
		int box_h = txt_h;

		int icon_space = SHADOW_OFFSET/2;

		int rec_icon_x = 0, rec_icon_w = 0, rec_icon_h = 0;
		int ts_icon_x  = 0, ts_icon_w  = 0, ts_icon_h  = 0;

		frameBuffer->getIconSize(rec_icon.c_str(), &rec_icon_w, &rec_icon_h);
		frameBuffer->getIconSize(ts_icon.c_str(), &ts_icon_w, &ts_icon_h);
		
		int icon_h = std::max(rec_icon_h, ts_icon_h);
		box_h = std::max(box_h, icon_h+icon_space*2);
		
		int icon_y = box_y + (box_h - icon_h)/2;
		int txt_y  = box_y + (box_h + txt_h)/2;

		char records_msg[8];
					
		if (rec_mode == CRecordManager::RECMODE_REC)
		{
			snprintf(records_msg, sizeof(records_msg)-1, "%d%s", records, "x");
			txt_w = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(records_msg);

			box_w = rec_icon_w + txt_w + icon_space*5;
			rec_icon_x = box_x + icon_space*2;
		}
		else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
		{
			box_w = ts_icon_w + icon_space*4;
			ts_icon_x = box_x + icon_space*2;
		}
		else if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
		{
			//subtract ts
			records--;
			snprintf(records_msg, sizeof(records_msg)-1, "%d%s", records, "x");
			txt_w = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(records_msg);

			box_w = ts_icon_w + rec_icon_w + txt_w + icon_space*7;
			ts_icon_x = box_x + icon_space*2;
			rec_icon_x = ts_icon_x + ts_icon_w + icon_space*2;
		}
		
		if (show)
		{
			if (rec == NULL){ //TODO: full refactoring of this icon handler
				rec = new CComponentsShapeSquare(box_x, box_y , box_w, box_h, NULL, CC_SHADOW_ON, COL_RED, COL_INFOBAR_PLUS_0);
				rec->setFrameThickness(2);
				rec->setShadowWidth(SHADOW_OFFSET/2);
				rec->setCorner(RADIUS_MIN, CORNER_ALL);
			}
			if (rec->getWidth() != box_w)
				rec->setWidth(box_w);

			if (!rec->isPainted())
				rec->paint(CC_SAVE_SCREEN_NO);
			
			if (rec_mode != CRecordManager::RECMODE_TSHIFT)
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rec_icon_x + rec_icon_w + icon_space, txt_y, txt_w, records_msg, COL_INFOBAR_TEXT);
			
			if (rec_mode == CRecordManager::RECMODE_REC)
			{
				frameBuffer->paintIcon(rec_icon, rec_icon_x, icon_y);
			}
			else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
			{
				frameBuffer->paintIcon(ts_icon, ts_icon_x, icon_y);
			}
			else if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
			{
				frameBuffer->paintIcon(rec_icon, rec_icon_x, icon_y);
				frameBuffer->paintIcon(ts_icon, ts_icon_x, icon_y);
			}
		}
		else
		{
			if (rec_mode == CRecordManager::RECMODE_REC)
				frameBuffer->paintBoxRel(rec_icon_x, icon_y, rec_icon_w, icon_h, COL_INFOBAR_PLUS_0);
			else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
				frameBuffer->paintBoxRel(ts_icon_x, icon_y, ts_icon_w, icon_h, COL_INFOBAR_PLUS_0);
			else if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
				frameBuffer->paintBoxRel(ts_icon_x, icon_y, ts_icon_w + rec_icon_w + icon_space*2, icon_h, COL_INFOBAR_PLUS_0);
		}
	}
}

void CInfoViewer::paintBackground(int col_NumBox)
{
	int c_rad_mid = RADIUS_MID;
	int BoxEndInfoY = BoxEndY;
	if (showButtonBar) // add button bar and blinkenlights
		BoxEndInfoY += infoViewerBB->InfoHeightY_Info + infoViewerBB->bottom_bar_offset;
#if 0	// kill left side
	frameBuffer->paintBackgroundBox(BoxStartX,
					BoxStartY + ChanHeight - 6,
					BoxStartX + ChanWidth / 3,
					BoxEndInfoY + SHADOW_OFFSET);
	// kill progressbar + info-line
	frameBuffer->paintBackgroundBox(BoxStartX + ChanWidth + 40, // 40 for the recording icon!
					BoxStartY, BoxEndX, BoxStartY + ChanHeight);

	// shadow for channel name, epg data...
	frameBuffer->paintBox(BoxEndX - c_shadow_width, ChanNameY + SHADOW_OFFSET,
			      BoxEndX + SHADOW_OFFSET,  BoxEndInfoY + SHADOW_OFFSET,
			      COL_INFOBAR_SHADOW_PLUS_0, c_rad_large, CORNER_RIGHT);
	frameBuffer->paintBox(ChanInfoX + SHADOW_OFFSET, BoxEndInfoY - c_shadow_width,
			      BoxEndX - c_shadow_width + 2, BoxEndInfoY + SHADOW_OFFSET,
			      COL_INFOBAR_SHADOW_PLUS_0, c_rad_large, CORNER_BOTTOM_LEFT);
#endif
	// background for channel name/logo and clock
	paintHead();

	// background for epg data
	paintBody();

	// number box
	int y_numbox = body->getYPos()-ChanHeight-SHADOW_OFFSET;
	if (numbox == NULL){ //TODO: move into an own member, paintNumBox() or so...
		numbox = new CComponentsShapeSquare(BoxStartX, y_numbox, ChanWidth, ChanHeight);
		numbox->enableShadow(CC_SHADOW_ON, SHADOW_OFFSET, true);
	}else
		numbox->setDimensionsAll(BoxStartX, y_numbox, ChanWidth, ChanHeight);
	numbox->setColorBody(g_settings.theme.infobar_gradient_top ? COL_MENUHEAD_PLUS_0 : col_NumBox);
	numbox->enableColBodyGradient(g_settings.theme.infobar_gradient_top, g_settings.theme.infobar_gradient_top ? COL_INFOBAR_PLUS_0 : col_NumBox, g_settings.theme.infobar_gradient_top_direction);
	numbox->setCorner(c_rad_mid, CORNER_ALL);
	numbox->paint(CC_SAVE_SCREEN_NO);
}

void CInfoViewer::paintHead()
{
	int head_x = BoxStartX+ChanWidth -1; /*Ugly: -1 to avoid background shine through round borders*/
	int head_w = BoxEndX-head_x;
	if (header == NULL){
		header = new CComponentsShapeSquare(head_x, ChanNameY, head_w, time_height, NULL, CC_SHADOW_RIGHT);
		header->setCorner(RADIUS_LARGE, CORNER_TOP_RIGHT);
	}else
		header->setDimensionsAll(head_x, ChanNameY, head_w, time_height);

	header->setColorBody(g_settings.theme.infobar_gradient_top ? COL_MENUHEAD_PLUS_0 : COL_INFOBAR_PLUS_0);
	header->enableColBodyGradient(g_settings.theme.infobar_gradient_top, COL_INFOBAR_PLUS_0, g_settings.theme.infobar_gradient_top_direction);
	clock->setColorBody(header->getColorBody());

	header->paint(CC_SAVE_SCREEN_NO);
	header_height = header->getHeight();
}

void CInfoViewer::paintBody()
{
	int h_body = InfoHeightY - header_height - SHADOW_OFFSET;
	infoViewerBB->initBBOffset();
	if (!zap_mode)
		h_body += infoViewerBB->bottom_bar_offset;

	int y_body = ChanNameY + header_height;

	if (body == NULL){
		body = new CComponentsShapeSquare(ChanInfoX, y_body, BoxEndX-ChanInfoX, h_body);
	} else {
		if (txt_cur_event && txt_cur_start && txt_cur_event_rest &&
				txt_next_event && txt_next_start && txt_next_in) {
			if (h_body != body->getHeight() || y_body != body->getYPos()){
				txt_cur_start->getCTextBoxObject()->clearScreenBuffer();
				txt_cur_event->getCTextBoxObject()->clearScreenBuffer();
				txt_cur_event_rest->getCTextBoxObject()->clearScreenBuffer();
				txt_next_start->getCTextBoxObject()->clearScreenBuffer();
				txt_next_event->getCTextBoxObject()->clearScreenBuffer();
				txt_next_in->getCTextBoxObject()->clearScreenBuffer();
			}
		}
		body->setDimensionsAll(ChanInfoX, y_body, BoxEndX-ChanInfoX, h_body);
	}

	//set corner and shadow modes, consider virtual zap mode
	body->setCorner(RADIUS_LARGE, (zap_mode) ? CORNER_BOTTOM : CORNER_NONE);
	body->enableShadow(zap_mode ? CC_SHADOW_ON : CC_SHADOW_RIGHT);

	body->setColorBody(g_settings.theme.infobar_gradient_body ? COL_MENUHEAD_PLUS_0 : COL_INFOBAR_PLUS_0);
	body->enableColBodyGradient(g_settings.theme.infobar_gradient_body, COL_INFOBAR_PLUS_0, g_settings.theme.infobar_gradient_body_direction);

	body->paint(CC_SAVE_SCREEN_NO);
}

void CInfoViewer::show_current_next(bool new_chan, int  epgpos)
{
	CEitManager::getInstance()->getCurrentNextServiceKey(current_epg_id, info_CurrentNext);
	if (!evtlist.empty()) {
		if (new_chan) {
			for ( eli=evtlist.begin(); eli!=evtlist.end(); ++eli ) {
				if ((uint)eli->startTime >= info_CurrentNext.current_zeit.startzeit + info_CurrentNext.current_zeit.dauer)
					break;
			}
			if (eli == evtlist.end()) // the end is not valid, so go back
				--eli;
		}

		if (epgpos != 0) {
			info_CurrentNext.flags = 0;
			if ((epgpos > 0) && (eli != evtlist.end())) {
				++eli; // next epg
				if (eli == evtlist.end()) // the end is not valid, so go back
					--eli;
			}
			else if ((epgpos < 0) && (eli != evtlist.begin())) {
				--eli; // prev epg
			}
			info_CurrentNext.flags = CSectionsdClient::epgflags::has_current;
			info_CurrentNext.current_uniqueKey      = eli->eventID;
			info_CurrentNext.current_zeit.startzeit = eli->startTime;
			info_CurrentNext.current_zeit.dauer     = eli->duration;
			if (eli->description.empty())
				info_CurrentNext.current_name   = g_Locale->getText(LOCALE_INFOVIEWER_NOEPG);
			else
				info_CurrentNext.current_name   = eli->description;
			info_CurrentNext.current_fsk            = '\0';

			if (eli != evtlist.end()) {
				++eli;
				if (eli != evtlist.end()) {
					info_CurrentNext.flags                  = CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::has_next;
					info_CurrentNext.next_uniqueKey         = eli->eventID;
					info_CurrentNext.next_zeit.startzeit    = eli->startTime;
					info_CurrentNext.next_zeit.dauer        = eli->duration;
					if (eli->description.empty())
						info_CurrentNext.next_name      = g_Locale->getText(LOCALE_INFOVIEWER_NOEPG);
					else
						info_CurrentNext.next_name      = eli->description;
				}
				--eli;
			}
		}
	}

	if (!(info_CurrentNext.flags & (CSectionsdClient::epgflags::has_later | CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::not_broadcast))) {
		neutrino_locale_t loc;
		if (!gotTime)
			loc = LOCALE_INFOVIEWER_WAITTIME;
		else if (showButtonBar)
			loc = LOCALE_INFOVIEWER_EPGWAIT;
		else
			loc = LOCALE_INFOVIEWER_EPGNOTLOAD;

		_livestreamInfo1.clear();
		_livestreamInfo2.clear();
		if (!showLivestreamInfo())
			display_Info(g_Locale->getText(loc), NULL);
	} else {
		show_Data ();
	}
}

void CInfoViewer::showMovieTitle(const int playState, const t_channel_id &Channel_Id, const std::string &Channel,
				 const std::string &g_file_epg, const std::string &g_file_epg1,
				 const int duration, const int curr_pos,
				 const int repeat_mode, const int _zap_mode)
{
	if (g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_BOTTOM_LEFT || 
	    g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_BOTTOM_RIGHT || 
	    g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_BOTTOM_CENTER || 
	    g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_HIGHER_CENTER)
		isVolscale = CVolume::getInstance()->hideVolscale();
	else
		isVolscale = false;

	check_channellogo_ca_SettingsChange();
	aspectRatio = 0;
	last_curr_id = last_next_id = 0;
	showButtonBar = true;
	fileplay = true;
	zap_mode = _zap_mode;
	reset_allScala();
	if (!gotTime)
		gotTime = timeset;

	if (g_settings.radiotext_enable && g_Radiotext) {
		g_Radiotext->RT_MsgShow = true;
	}

	if(!is_visible)
		fader.StartFadeIn();

	is_visible = true;
	infoViewerBB->is_visible = true;

	ChannelName = Channel;
	t_channel_id old_channel_id = current_channel_id;
	current_channel_id = Channel_Id;

	/* showChannelLogo() changes this, so better reset it every time... */
	ChanNameX = BoxStartX + ChanWidth + SHADOW_OFFSET;

	paintBackground(COL_INFOBAR_PLUS_0);

	bool show_dot = true;
	if (timeset)
		clock->paint(CC_SAVE_SCREEN_NO);
	showRecordIcon (show_dot);
	show_dot = !show_dot;

	if (!zap_mode)
		infoViewerBB->paintshowButtonBar();

	int ChannelLogoMode = 0;
	if (g_settings.infobar_show_channellogo > 1)
		ChannelLogoMode = showChannelLogo(current_channel_id, 0);
	if (ChannelLogoMode == 0 || ChannelLogoMode == 3 || ChannelLogoMode == 4)
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(ChanNameX + 10 , ChanNameY + header_height,BoxEndX - (ChanNameX + 20) - time_width - LEFT_OFFSET - 10 ,ChannelName, COL_INFOBAR_TEXT);

	// show_Data
	if (CMoviePlayerGui::getInstance().file_prozent > 100)
		CMoviePlayerGui::getInstance().file_prozent = 100;

	const char *unit_short_minute = g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);
	char runningRest[32]; // %d can be 10 digits max...
	snprintf(runningRest, sizeof(runningRest), "%d / %d %s", (curr_pos + 30000) / 60000, (duration - curr_pos + 30000) / 60000, unit_short_minute);
	display_Info(g_file_epg.c_str(), g_file_epg1.c_str(), false, CMoviePlayerGui::getInstance().file_prozent, NULL, runningRest);

	int speed = CMoviePlayerGui::getInstance().GetSpeed();
	const char *playicon = NULL;
	switch (playState) {
	case CMoviePlayerGui::PLAY:
		switch (repeat_mode) {
		case CMoviePlayerGui::REPEAT_ALL:
			playicon = NEUTRINO_ICON_PLAY_REPEAT_ALL;
			break;
		case CMoviePlayerGui::REPEAT_TRACK:
			playicon = NEUTRINO_ICON_PLAY_REPEAT_TRACK;
			break;
		default:
			playicon = NEUTRINO_ICON_PLAY;
		}
		speed = 0;
		break;
	case CMoviePlayerGui::PAUSE:
		playicon = NEUTRINO_ICON_PAUSE;
		break;
	case CMoviePlayerGui::REW:
		playicon = NEUTRINO_ICON_REW;
		speed = abs(speed);
		break;
	case CMoviePlayerGui::FF:
		playicon = NEUTRINO_ICON_FF;
		speed = abs(speed);
		break;
	default:
		/* NULL crashes in getIconSize, just use something */
		playicon = NEUTRINO_ICON_BUTTON_HELP;
		break;
	}
	int icon_w = 0,icon_h = 0;
	frameBuffer->getIconSize(playicon, &icon_w, &icon_h);
	int speedw = 0;
	if (speed) {
		sprintf(runningRest, "%dx", speed);
		speedw = 5 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(runningRest);
		icon_w += speedw;
	}
	int icon_x = BoxStartX + ChanWidth / 2 - icon_w / 2;
	int icon_y = BoxStartY + ChanHeight / 2 - icon_h / 2;
	if (speed) {
		int sh = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight();
		int sy = BoxStartY + ChanHeight/2 - sh/2 + sh;
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(icon_x, sy, ChanHeight, runningRest, COL_INFOBAR_TEXT);
		icon_x += speedw;
	}
	frameBuffer->paintIcon(playicon, icon_x, icon_y);
	showLcdPercentOver ();
	showInfoFile();
	//loop(fadeValue, show_dot , fadeIn);
	loop(show_dot);
	aspectRatio = 0;
	fileplay = 0;
	current_channel_id = old_channel_id;
}

void CInfoViewer::reset_allScala()
{
	changePB();
	lastsig = lastsnr = -1;
	infoViewerBB->changePB();
	infoViewerBB->reset_allScala();
	if(!clock)
		initClock();
}

void CInfoViewer::check_channellogo_ca_SettingsChange()
{
	if (casysChange != g_settings.infobar_casystem_display || channellogoChange != g_settings.infobar_show_channellogo) {
		casysChange = g_settings.infobar_casystem_display;
		channellogoChange = g_settings.infobar_show_channellogo;
		infoViewerBB->initBBOffset();
		start();
	}
}

void CInfoViewer::showTitle(t_channel_id chid, const bool calledFromNumZap, int epgpos)
{
	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(chid);

	if(channel)
		showTitle(channel, calledFromNumZap, epgpos);
}

void CInfoViewer::showTitle(CZapitChannel * channel, const bool calledFromNumZap, int epgpos)
{
	if(!calledFromNumZap && !(zap_mode & IV_MODE_DEFAULT))
		resetSwitchMode();

	std::string Channel = channel->getName();
	t_satellite_position satellitePosition = channel->getSatellitePosition();
	t_channel_id new_channel_id = channel->getChannelID();
	int ChanNum = channel->number;

	current_epg_id = channel->getEpgID();

	if (g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_BOTTOM_LEFT || 
	    g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_BOTTOM_RIGHT || 
	    g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_BOTTOM_CENTER || 
	    g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_HIGHER_CENTER)
		isVolscale = CVolume::getInstance()->hideVolscale();
	else
		isVolscale = false;

	check_channellogo_ca_SettingsChange();
	aspectRatio = 0;
	last_curr_id = last_next_id = 0;
	showButtonBar = !calledFromNumZap;

	fileplay = (ChanNum == 0);
	newfreq = true;

	reset_allScala();
	if (!gotTime)
		gotTime = timeset;

	if(!is_visible && !calledFromNumZap)
		fader.StartFadeIn();

	is_visible = true;
	infoViewerBB->is_visible = true;

	fb_pixel_t col_NumBoxText = COL_INFOBAR_TEXT;
	fb_pixel_t col_NumBox = COL_INFOBAR_PLUS_0;
	ChannelName = Channel;
	bool new_chan = false;

	if (zap_mode & IV_MODE_VIRTUAL_ZAP) {
		if (g_RemoteControl->current_channel_id != new_channel_id) {
			col_NumBoxText = COL_MENUHEAD_TEXT;
		}
		if ((current_channel_id != new_channel_id) || (evtlist.empty())) {
			CEitManager::getInstance()->getEventsServiceKey(current_epg_id, evtlist);
			if (!evtlist.empty())
				sort(evtlist.begin(),evtlist.end(), sortByDateTime);
			new_chan = true;
		}
	}
	if (! calledFromNumZap && !(g_RemoteControl->subChannels.empty()) && (g_RemoteControl->selected_subchannel > 0))
	{
		current_channel_id = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].getChannelID();
		ChannelName = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].subservice_name;
	} else {
		current_channel_id = new_channel_id;
	}

	/* showChannelLogo() changes this, so better reset it every time... */
	ChanNameX = BoxStartX + ChanWidth + SHADOW_OFFSET;


	paintBackground(col_NumBox);

	bool show_dot = true;
	if (timeset)
		clock->paint(CC_SAVE_SCREEN_NO);
	showRecordIcon (show_dot);
	show_dot = !show_dot;

	if (showButtonBar) {
		infoViewerBB->paintshowButtonBar();
	}

	int ChanNumWidth = 0;
	int ChannelLogoMode = 0;
	bool logo_ok = false;
	if (ChanNum) /* !fileplay */
	{
		char strChanNum[10];
		snprintf (strChanNum, sizeof(strChanNum), "%d", ChanNum);
		const int channel_number_width =(g_settings.infobar_show_channellogo == 6) ? 5 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth (strChanNum) : 0;
		ChannelLogoMode = showChannelLogo(current_channel_id,channel_number_width); // get logo mode, paint channel logo if adjusted
		logo_ok = ( g_settings.infobar_show_channellogo != 0 && ChannelLogoMode != 0);
		//fprintf(stderr, "after showchannellogo, mode = %d ret = %d logo_ok = %d\n",g_settings.infobar_show_channellogo, ChannelLogoMode, logo_ok);

		if (g_settings.infobar_sat_display) {
			std::string name = (IS_WEBTV(current_channel_id))? "WebTV" : CServiceManager::getInstance()->GetSatelliteName(satellitePosition);
			int satNameWidth = g_SignalFont->getRenderWidth (name);
			std::string satname_tmp = name;
			if (satNameWidth > (ChanWidth - numbox_offset*2)) {
				satNameWidth = ChanWidth - numbox_offset*2;
				size_t pos1 = name.find("(") ;
				size_t pos2 = name.find_last_of(")");
				size_t pos0 = name.find(" ") ;
				if ((pos1 != std::string::npos) && (pos2 != std::string::npos) && (pos0 != std::string::npos)) {
					pos1++;
					satname_tmp = name.substr(0, pos0 );

					if(satname_tmp == "Hot")
						satname_tmp = "Hotbird";

					satname_tmp +=" ";
					satname_tmp += name.substr( pos1,pos2-pos1 );
					satNameWidth = g_SignalFont->getRenderWidth (satname_tmp);
					if (satNameWidth > (ChanWidth - numbox_offset*2))
						satNameWidth = ChanWidth - numbox_offset*2;
				}
			}
			int h_sfont = g_SignalFont->getHeight();
			g_SignalFont->RenderString (BoxStartX + numbox_offset + ((ChanWidth - satNameWidth) / 2) , numbox->getYPos() + h_sfont, satNameWidth, satname_tmp, COL_INFOBAR_TEXT);
		}

		/* TODO: the logic will get much easier once we decouple channellogo and signal bars */
		if ((!logo_ok && g_settings.infobar_show_channellogo < 2) || g_settings.infobar_show_channellogo == 2 || g_settings.infobar_show_channellogo == 4) // no logo in numberbox
		{
			// show number in numberbox
			int h_tmp 	= numbox->getHeight();
			int y_tmp 	= numbox->getYPos() + 5*100/h_tmp; //5%
			if (g_settings.infobar_sat_display){
				int h_sfont = g_SignalFont->getHeight();
				h_tmp -= h_sfont;
				y_tmp += h_sfont;
			}
			y_tmp += h_tmp/2 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getHeight()/2;
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->RenderString(BoxStartX + numbox_offset + (ChanWidth-g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getRenderWidth(strChanNum))/2,
											y_tmp,
											ChanWidth - 2*numbox_offset,
											strChanNum,
											col_NumBoxText);
		}
		if (ChannelLogoMode == 1 || ( g_settings.infobar_show_channellogo == 3 && !logo_ok) || g_settings.infobar_show_channellogo == 6 ) /* channel number besides channel name */
		{
			ChanNumWidth = 5 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth (strChanNum);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(
				ChanNameX + 5, ChanNameY + header_height,
				ChanNumWidth, strChanNum, col_NumBoxText);
		}
	}

	if (g_settings.infobar_show_channellogo < 5 || !logo_ok) {
		if (ChannelLogoMode != 2) {
			//FIXME good color to display inactive for zap ?
			//fb_pixel_t color = CNeutrinoApp::getInstance ()->channelList->SameTP(new_channel_id) ? COL_INFOBAR_TEXT : COL_INFOBAR_SHADOW_TEXT;
			fb_pixel_t color = COL_INFOBAR_TEXT;
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(
				ChanNameX + 10 + ChanNumWidth, ChanNameY + header_height,
				BoxEndX - (ChanNameX + 20) - time_width - LEFT_OFFSET - 10 - ChanNumWidth,
				ChannelName, color /*COL_INFOBAR_TEXT*/);
			//provider name
			if(g_settings.infobar_show_channeldesc && channel->pname){
				std::string prov_name = channel->pname;
				prov_name=prov_name.substr(prov_name.find_first_of("]")+1);

				int chname_width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth (ChannelName);
				unsigned int chann_size = ChannelName.size();
				if(ChannelName.empty())
					chann_size = 1;
				chname_width += (chname_width/chann_size/2);

				int tmpY = ((ChanNameY + header_height) - g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getDigitOffset()
						+ g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getDigitOffset());
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(
					ChanNameX + 10 + ChanNumWidth + chname_width, tmpY,
					BoxEndX - (ChanNameX + 20) - time_width - LEFT_OFFSET - 10 - ChanNumWidth - chname_width,
					prov_name, color /*COL_INFOBAR_TEXT*/);
			}

		}
	}

	if (fileplay) {
		show_Data ();
#if 0
	} else if (IS_WEBTV(new_channel_id)) {
		if (channel) {
			const char *current = channel->getDesc().c_str();
			const char *next = channel->getUrl().c_str();
			if (!current) {
				current = next;
				next = "";
			}
			display_Info(current, next, false, 0, NULL, NULL, NULL, NULL, true, true);
		}
#endif
	} else {
		show_current_next(new_chan, epgpos);
	}

	showLcdPercentOver ();
	showInfoFile();

	// Radiotext
	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_radio)
	{
		if ((g_settings.radiotext_enable) && (!recordModeActive) && (!calledFromNumZap))
			showRadiotext();
		else
			infoViewerBB->showIcon_RadioText(false);
	}

	if (!calledFromNumZap) {
		//loop(fadeValue, show_dot , fadeIn);
		loop(show_dot);
	}
	else
		frameBuffer->blit();
	aspectRatio = 0;
	fileplay = 0;
}
void CInfoViewer::setInfobarTimeout(int timeout_ext)
{
	int mode = CNeutrinoApp::getInstance()->getMode();
	//define timeouts
	switch (mode)
	{
		case NeutrinoMessages::mode_tv:
		case NeutrinoMessages::mode_webtv:
				timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] + timeout_ext);
				break;
		case NeutrinoMessages::mode_radio:
				timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR_RADIO] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR_RADIO] + timeout_ext);
				break;
		case NeutrinoMessages::mode_ts:
				timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR_MOVIE] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR_MOVIE] + timeout_ext);
				break;
		default:
				timeoutEnd = CRCInput::calcTimeoutEnd(6 + timeout_ext); 
				break;
	}
}

bool CInfoViewer::showLivestreamInfo()
{
	CZapitChannel * cc = CZapit::getInstance()->GetCurrentChannel();
	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_webtv &&
	    cc->getEpgID() == 0 && !cc->getScriptName().empty()) {
		std::string livestreamInfo1 = "";
		std::string livestreamInfo2 = "";
		std::string tmp1            = "";
		CMoviePlayerGui::getInstance().getLivestreamInfo(&livestreamInfo1, &tmp1);

		if (!(videoDecoder->getBlank())) {
			int xres, yres, framerate;
			std::string tmp2;
			videoDecoder->getPictureInfo(xres, yres, framerate);
			switch (framerate) {
				case 0:
					tmp2 = "23.976fps";
					break;
				case 1:
					tmp2 = "24fps";
					break;
				case 2:
					tmp2 = "25fps";
					break;
				case 3:
					tmp2 = "29,976fps";
					break;
				case 4:
					tmp2 = "30fps";
					break;
				case 5:
					tmp2 = "50fps";
					break;
				case 6:
					tmp2 = "50,94fps";
					break;
				case 7:
					tmp2 = "60fps";
					break;
				default:
					tmp2 = g_Locale->getText(LOCALE_STREAMINFO_FRAMERATE_UNKNOWN);
					break;
			}
			livestreamInfo2 = to_string(xres) + "x" + to_string(yres) + ", " + tmp2;
			if (!tmp1.empty())
				livestreamInfo2 += (std::string)", " + tmp1;
		}

		if (livestreamInfo1 != _livestreamInfo1 || livestreamInfo2 != _livestreamInfo2) {
			display_Info(livestreamInfo1.c_str(), livestreamInfo2.c_str(), false);
			_livestreamInfo1 = livestreamInfo1;
			_livestreamInfo2 = livestreamInfo2;
			infoViewerBB->showBBButtons(true /*paintFooter*/);
		}
		return true;
	}
	return false;
}

void CInfoViewer::loop(bool show_dot)
{
	bool hideIt = true;
	resetSwitchMode(); //no virtual zap
	//bool fadeOut = false;
	timeoutEnd=0;;
	setInfobarTimeout();

	int res = messages_return::none;
	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	if (isVolscale)
		CVolume::getInstance()->showVolscale();

	_livestreamInfo1.clear();
	_livestreamInfo2.clear();

	while (!(res & (messages_return::cancel_info | messages_return::cancel_all))) {
		frameBuffer->blit();
		g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd);

		showLivestreamInfo();

#ifdef ENABLE_PIP
		if ((msg == (neutrino_msg_t) g_settings.key_pip_close) || 
		    (msg == (neutrino_msg_t) g_settings.key_pip_setup) || 
		    (msg == (neutrino_msg_t) g_settings.key_pip_swap)) {
			g_RCInput->postMsg(msg, data);
			res = messages_return::cancel_info;
		} else
#endif
		if (msg == (neutrino_msg_t) g_settings.key_screenshot) {
			res = CNeutrinoApp::getInstance()->handleMsg(msg, data);

		} else if (msg == CRCInput::RC_sat || msg == CRCInput::RC_favorites) {
			g_RCInput->postMsg (msg, 0);
			res = messages_return::cancel_info;
		} else if (msg == CRCInput::RC_help || msg == CRCInput::RC_info) {
			g_RCInput->postMsg (NeutrinoMessages::SHOW_EPG, 0);
			res = messages_return::cancel_info;
		} else if ((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetFadeTimer())) {
			if(fader.FadeDone())
				res = messages_return::cancel_info;
		} else if ((msg == CRCInput::RC_ok) || (msg == CRCInput::RC_home) || (msg == CRCInput::RC_timeout)) {
			if ((g_settings.mode_left_right_key_tv == SNeutrinoSettings::VZAP) && (msg == CRCInput::RC_ok))
			{
				if (fileplay)
				{
					// in movieplayer mode process vzap keys in movieplayer.cpp
					//printf("%s:%d: imitate VZAP; RC_ok\n", __func__, __LINE__);
					CMoviePlayerGui::getInstance().setFromInfoviewer(true);
					g_RCInput->postMsg (msg, data);
					hideIt = true;
				}
			}
			if(fader.StartFadeOut())
				timeoutEnd = CRCInput::calcTimeoutEnd (1);
			else
				res = messages_return::cancel_info;
		} else if ((g_settings.mode_left_right_key_tv == SNeutrinoSettings::VZAP) && ((msg == CRCInput::RC_right) || (msg == CRCInput::RC_left ))) {
			if (fileplay)
			{
				// in movieplayer mode process vzap keys in movieplayer.cpp
				//printf("%s:%d: imitate VZAP; RC_left/right\n", __func__, __LINE__);
				CMoviePlayerGui::getInstance().setFromInfoviewer(true);
				g_RCInput->postMsg (msg, data);
				hideIt = true;
			}
			else
				setSwitchMode(IV_MODE_VIRTUAL_ZAP);
			res = messages_return::cancel_all;
			hideIt = true;
		} else if ((msg == NeutrinoMessages::EVT_TIMER) && (data == sec_timer_id)) {
			// doesn't belong here, but easiest way to check for a change ...
			if (is_visible && showButtonBar)
				infoViewerBB->showIcon_CA_Status(0);
			showSNR ();
			if (timeset)
				clock->paint(CC_SAVE_SCREEN_NO);
			showRecordIcon (show_dot);
			show_dot = !show_dot;
			showInfoFile();
			if ((g_settings.radiotext_enable) && (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_radio))
				showRadiotext();

			infoViewerBB->showIcon_16_9();
			//infoViewerBB->showIcon_CA_Status(0);
			infoViewerBB->showIcon_Resolution();
		} else if ((msg == NeutrinoMessages::EVT_RECORDMODE) && 
			   (CMoviePlayerGui::getInstance().timeshift) && (CRecordManager::getInstance()->GetRecordCount() == 1)) {
			res = CNeutrinoApp::getInstance()->handleMsg(msg, data);
		} else if (!fileplay && !CMoviePlayerGui::getInstance().timeshift) {
			CNeutrinoApp *neutrino = CNeutrinoApp::getInstance ();
			if ((msg == (neutrino_msg_t) g_settings.key_quickzap_up) || (msg == (neutrino_msg_t) g_settings.key_quickzap_down) || (msg == CRCInput::RC_0) || (msg == NeutrinoMessages::SHOW_INFOBAR)) {
				hideIt = false; // default
				if ((g_settings.radiotext_enable) && (neutrino->getMode() == NeutrinoMessages::mode_radio))
					hideIt =  true;

				int rec_mode = CRecordManager::getInstance()->GetRecordMode();
#if 0
				if ((rec_mode == CRecordManager::RECMODE_REC) || (rec_mode == CRecordManager::RECMODE_REC_TSHIFT))
					hideIt = true;
#endif
				/* hide, if record (not timeshift only) is running -> neutrino will show channel list */
				if (rec_mode & CRecordManager::RECMODE_REC)
					hideIt = true;

				g_RCInput->postMsg (msg, data);
				res = messages_return::cancel_info;
			} else if (msg == NeutrinoMessages::EVT_TIMESET) {
				/* handle timeset event in upper layer, ignore here */
				res = neutrino->handleMsg (msg, data);
			} else {
				if (msg == CRCInput::RC_standby) {
					g_RCInput->killTimer (sec_timer_id);
					fader.StopFade();
				}
				res = neutrino->handleMsg (msg, data);
				if (res & messages_return::unhandled) {
					// raus hier und im Hauptfenster behandeln...
					g_RCInput->postMsg (msg, data);
					res = messages_return::cancel_info;
				}
			}
		} else if (fileplay || CMoviePlayerGui::getInstance().timeshift) {

			/* this debug message will only hit in movieplayer mode, where console is
			 * spammed to death anyway... */
			printf("%s:%d msg->MP: %08lx, data: %08lx\n", __func__, __LINE__, (long)msg, (long)data);
			if (msg < CRCInput::RC_Events) /* RC / Keyboard event */
			{
				g_RCInput->postMsg (msg, data);
				res = messages_return::cancel_info;
			}
			else
				res = CNeutrinoApp::getInstance()->handleMsg(msg, data);
		}
#if 0
		else if (CMoviePlayerGui::getInstance().start_timeshift && (msg == NeutrinoMessages::EVT_TIMER)) {
			CMoviePlayerGui::getInstance().start_timeshift = false;
                }
#endif
#if 0
		else if (CMoviePlayerGui::getInstance().timeshift && ((msg == (neutrino_msg_t) g_settings.mpkey_rewind)  || \
									(msg == (neutrino_msg_t) g_settings.mpkey_forward) || \
									(msg == (neutrino_msg_t) g_settings.mpkey_pause)   || \
									(msg == (neutrino_msg_t) g_settings.mpkey_stop)    || \
									(msg == (neutrino_msg_t) g_settings.mpkey_play)    || \
									(msg == (neutrino_msg_t) g_settings.mpkey_time)    || \
									(msg == (neutrino_msg_t) g_settings.key_timeshift))) {
			g_RCInput->postMsg (msg, data);
			res = messages_return::cancel_info;
                }
#endif
	}

	if (hideIt) {
		CVolume::getInstance()->hideVolscale();
		killTitle ();
	}

	g_RCInput->killTimer (sec_timer_id);
	fader.StopFade();
	if (zap_mode & IV_MODE_VIRTUAL_ZAP) {
		/* if bouquet cycle set, do virtual over current bouquet */
		if (/*g_settings.zap_cycle && */ /* (bouquetList != NULL) && */ !(bouquetList->Bouquets.empty()))
			bouquetList->Bouquets[bouquetList->getActiveBouquetNumber()]->channelList->virtual_zap_mode(msg == CRCInput::RC_right);
		else
			CNeutrinoApp::getInstance()->channelList->virtual_zap_mode(msg == CRCInput::RC_right);
	}
}

void CInfoViewer::showSubchan ()
{
	CFrameBuffer *lframeBuffer = CFrameBuffer::getInstance ();
	CNeutrinoApp *neutrino = CNeutrinoApp::getInstance ();

	std::string subChannelName;	// holds the name of the subchannel/audio channel
	int subchannel = 0;				// holds the channel index
	const int borderwidth = 4;

	if (!(g_RemoteControl->subChannels.empty ())) {
		// get info for nvod/subchannel
		subchannel = g_RemoteControl->selected_subchannel;
		if (g_RemoteControl->selected_subchannel >= 0)
			subChannelName = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].subservice_name;
	} else if (g_RemoteControl->current_PIDs.APIDs.size () > 1 && g_settings.audiochannel_up_down_enable) {
		// get info for audio channel
		subchannel = g_RemoteControl->current_PIDs.PIDs.selected_apid;
		subChannelName = g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].desc;
	}

	if (!(subChannelName.empty ())) {
		if ( g_settings.infobar_subchan_disp_pos == 4 ) {
			g_RCInput->postMsg( NeutrinoMessages::SHOW_INFOBAR , 0 );
		} else {
			char text[100];
			snprintf (text, sizeof(text), "%d - %s", subchannel, subChannelName.c_str ());

			int dx = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth (text) + 20;
			int dy = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight(); // 25;

			if (g_RemoteControl->director_mode) {
				int w = 20 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth (g_Locale->getText (LOCALE_NVODSELECTOR_DIRECTORMODE)) + 20;
				int h = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();
				if (w > dx)
					dx = w;
				dy = dy + h + 5; //dy * 2;
			} else
				dy = dy + 5;

			int x = 0, y = 0;
			if (g_settings.infobar_subchan_disp_pos == 0) {
				// Rechts-Oben
				x = g_settings.screen_EndX - dx - 10;
				y = g_settings.screen_StartY + 10;
			} else if (g_settings.infobar_subchan_disp_pos == 1) {
				// Links-Oben
				x = g_settings.screen_StartX + 10;
				y = g_settings.screen_StartY + 10;
			} else if (g_settings.infobar_subchan_disp_pos == 2) {
				// Links-Unten
				x = g_settings.screen_StartX + 10;
				y = g_settings.screen_EndY - dy - 10;
			} else if (g_settings.infobar_subchan_disp_pos == 3) {
				// Rechts-Unten
				x = g_settings.screen_EndX - dx - 10;
				y = g_settings.screen_EndY - dy - 10;
			}

			fb_pixel_t pixbuf[(dx + 2 * borderwidth) * (dy + 2 * borderwidth)];
			lframeBuffer->SaveScreen (x - borderwidth, y - borderwidth, dx + 2 * borderwidth, dy + 2 * borderwidth, pixbuf);

			// clear border
			lframeBuffer->paintBackgroundBoxRel (x - borderwidth, y - borderwidth, dx + 2 * borderwidth, borderwidth);
			lframeBuffer->paintBackgroundBoxRel (x - borderwidth, y + dy, dx + 2 * borderwidth, borderwidth);
			lframeBuffer->paintBackgroundBoxRel (x - borderwidth, y, borderwidth, dy);
			lframeBuffer->paintBackgroundBoxRel (x + dx, y, borderwidth, dy);

			lframeBuffer->paintBoxRel (x, y, dx, dy, COL_MENUCONTENT_PLUS_0);
			//g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (x + 10, y + 30, dx - 20, text, COL_MENUCONTENT_TEXT);

			if (g_RemoteControl->director_mode) {
				lframeBuffer->paintIcon (NEUTRINO_ICON_BUTTON_YELLOW, x + 8, y + dy - 20);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString (x + 30, y + dy - 2, dx - 40, g_Locale->getText (LOCALE_NVODSELECTOR_DIRECTORMODE), COL_MENUCONTENT_TEXT);
				int h = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (x + 10, y + dy - h - 2, dx - 20, text, COL_MENUCONTENT_TEXT);
			} else
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (x + 10, y + dy - 2, dx - 20, text, COL_MENUCONTENT_TEXT);

			uint64_t timeoutEnd_tmp = CRCInput::calcTimeoutEnd (2);
			int res = messages_return::none;

			neutrino_msg_t msg;
			neutrino_msg_data_t data;

			while (!(res & (messages_return::cancel_info | messages_return::cancel_all))) {
				g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd_tmp);

				if (msg == CRCInput::RC_timeout) {
					res = messages_return::cancel_info;
				} else {
					res = neutrino->handleMsg (msg, data);

					if (res & messages_return::unhandled) {
						// raus hier und im Hauptfenster behandeln...
						g_RCInput->postMsg (msg, data);
						res = messages_return::cancel_info;
					}
				}
			}
			lframeBuffer->RestoreScreen (x - borderwidth, y - borderwidth, dx + 2 * borderwidth, dy + 2 * borderwidth, pixbuf);
		}
	} else {
		g_RCInput->postMsg (NeutrinoMessages::SHOW_INFOBAR, 0);
	}
}

void CInfoViewer::showFailure ()
{
	ShowHint (LOCALE_MESSAGEBOX_ERROR, g_Locale->getText (LOCALE_INFOVIEWER_NOTAVAILABLE), 430);	// UTF-8
}

void CInfoViewer::showMotorMoving (int duration)
{
	setInfobarTimeout(duration + 1);

	char text[256];
	snprintf(text, sizeof(text), "%s (%ds)", g_Locale->getText (LOCALE_INFOVIEWER_MOTOR_MOVING), duration);
	ShowHint (LOCALE_MESSAGEBOX_INFO, text, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth (text) + 10, duration);	// UTF-8
}

void CInfoViewer::killRadiotext()
{
	if (g_Radiotext->S_RtOsd)
		frameBuffer->paintBackgroundBox(rt_x, rt_y, rt_w, rt_h);
	rt_x = rt_y = rt_h = rt_w = 0;
	CInfoClock::getInstance()->enableInfoClock(true);
}

void CInfoViewer::showRadiotext()
{
	char stext[3][100];
	bool RTisIsUTF = false;

	if (g_Radiotext == NULL) return;
	infoViewerBB->showIcon_RadioText(g_Radiotext->haveRadiotext());

	bool blit = false;

	if (g_Radiotext->S_RtOsd) {
		CInfoClock::getInstance()->enableInfoClock(false);
		// dimensions of radiotext window
		int /*yoff = 8,*/ ii = 0;
		rt_dx = BoxEndX - BoxStartX;
		rt_dy = 25;
		rt_x = BoxStartX;
		rt_y = g_settings.screen_StartY + 10;
		rt_h = rt_y + 7 + rt_dy*(g_Radiotext->S_RtOsdRows+1)+SHADOW_OFFSET;
		rt_w = rt_x+rt_dx+SHADOW_OFFSET;
		
		int lines = 0;
		for (int i = 0; i < g_Radiotext->S_RtOsdRows; i++) {
			if (g_Radiotext->RT_Text[i][0] != '\0') lines++;
		}
		if (lines == 0)
			frameBuffer->paintBackgroundBox(rt_x, rt_y, rt_w, rt_h);

		if (g_Radiotext->RT_MsgShow) {

			if (g_Radiotext->S_RtOsdTitle == 1) {

		// Title
		//	sprintf(stext[0], g_Radiotext->RT_PTY == 0 ? "%s - %s %s%s" : "%s - %s (%s)%s",
		//	g_Radiotext->RT_Titel, tr("Radiotext"), g_Radiotext->RT_PTY == 0 ? g_Radiotext->RDS_PTYN : g_Radiotext->ptynr2string(g_Radiotext->RT_PTY), g_Radiotext->RT_MsgShow ? ":" : tr("  [waiting ...]"));
				if ((lines) || (g_Radiotext->RT_PTY !=0)) {
					sprintf(stext[0], g_Radiotext->RT_PTY == 0 ? "%s %s%s" : "%s (%s)%s", tr("Radiotext"), g_Radiotext->RT_PTY == 0 ? g_Radiotext->RDS_PTYN : g_Radiotext->ptynr2string(g_Radiotext->RT_PTY), ":");
					
					// shadow
					frameBuffer->paintBoxRel(rt_x+SHADOW_OFFSET, rt_y+SHADOW_OFFSET, rt_dx, rt_dy, COL_INFOBAR_SHADOW_PLUS_0, RADIUS_LARGE, CORNER_TOP);
					frameBuffer->paintBoxRel(rt_x, rt_y, rt_dx, rt_dy, COL_INFOBAR_PLUS_0, RADIUS_LARGE, CORNER_TOP);
					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rt_x+10, rt_y+ 30, rt_dx-20, stext[0], COL_INFOBAR_TEXT, 0, RTisIsUTF);
					blit = true;
				}
//				yoff = 17;
				ii = 1;
#if 0
			// RDS- or Rass-Symbol, ARec-Symbol or Bitrate
			int inloff = (ftitel->Height() + 9 - 20) / 2;
			if (Rass_Flags[0][0]) {
				osd->DrawBitmap(Setup.OSDWidth-51, inloff, rass, bcolor, fcolor);
				if (ARec_Record)
					osd->DrawBitmap(Setup.OSDWidth-107, inloff, arec, bcolor, 0xFFFC1414);	// FG=Red
				else
					inloff = (ftitel->Height() + 9 - ftext->Height()) / 2;
				osd->DrawText(4, inloff, RadioAudio->bitrate, fcolor, clrTransparent, ftext, Setup.OSDWidth-59, ftext->Height(), taRight);
			}
			else {
				osd->DrawBitmap(Setup.OSDWidth-84, inloff, rds, bcolor, fcolor);
				if (ARec_Record)
					osd->DrawBitmap(Setup.OSDWidth-140, inloff, arec, bcolor, 0xFFFC1414);	// FG=Red
				else
					inloff = (ftitel->Height() + 9 - ftext->Height()) / 2;
				osd->DrawText(4, inloff, RadioAudio->bitrate, fcolor, clrTransparent, ftext, Setup.OSDWidth-92, ftext->Height(), taRight);
			}
#endif
			}
			// Body
			if (lines) {
				frameBuffer->paintBoxRel(rt_x+SHADOW_OFFSET, rt_y+rt_dy+SHADOW_OFFSET, rt_dx, 7+rt_dy* g_Radiotext->S_RtOsdRows, COL_INFOBAR_SHADOW_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);
				frameBuffer->paintBoxRel(rt_x, rt_y+rt_dy, rt_dx, 7+rt_dy* g_Radiotext->S_RtOsdRows, COL_INFOBAR_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

				// RT-Text roundloop
				int ind = (g_Radiotext->RT_Index == 0) ? g_Radiotext->S_RtOsdRows - 1 : g_Radiotext->RT_Index - 1;
				int rts_x = rt_x+10;
				int rts_y = rt_y+ 30;
				int rts_dx = rt_dx-20;
				if (g_Radiotext->S_RtOsdLoop == 1) { // latest bottom
					for (int i = ind+1; i < g_Radiotext->S_RtOsdRows; i++)
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rts_x, rts_y + (ii++)*rt_dy, rts_dx, g_Radiotext->RT_Text[i], COL_INFOBAR_TEXT, 0, RTisIsUTF);
					for (int i = 0; i <= ind; i++)
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rts_x, rts_y + (ii++)*rt_dy, rts_dx, g_Radiotext->RT_Text[i], COL_INFOBAR_TEXT, 0, RTisIsUTF);
				}
				else { // latest top
					for (int i = ind; i >= 0; i--)
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rts_x, rts_y + (ii++)*rt_dy, rts_dx, g_Radiotext->RT_Text[i], COL_INFOBAR_TEXT, 0, RTisIsUTF);
					for (int i = g_Radiotext->S_RtOsdRows-1; i > ind; i--)
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rts_x, rts_y + (ii++)*rt_dy, rts_dx, g_Radiotext->RT_Text[i], COL_INFOBAR_TEXT, 0, RTisIsUTF);
				}
				blit = true;
			}
#if 0
			// + RT-Plus or PS-Text = 2 rows
			if ((S_RtOsdTags == 1 && RT_PlusShow) || S_RtOsdTags >= 2) {
				if (!RDS_PSShow || !strstr(RTP_Title, "---") || !strstr(RTP_Artist, "---")) {
					sprintf(stext[1], "> %s  %s", tr("Title  :"), RTP_Title);
					sprintf(stext[2], "> %s  %s", tr("Artist :"), RTP_Artist);
					osd->DrawText(4, 6+yoff+fheight*(ii++), stext[1], fcolor, clrTransparent, ftext, Setup.OSDWidth-4, ftext->Height());
					osd->DrawText(4, 3+yoff+fheight*(ii++), stext[2], fcolor, clrTransparent, ftext, Setup.OSDWidth-4, ftext->Height());
				}
				else {
					char *temp = "";
					int ind = (RDS_PSIndex == 0) ? 11 : RDS_PSIndex - 1;
					for (int i = ind+1; i < 12; i++)
						asprintf(&temp, "%s%s ", temp, RDS_PSText[i]);
					for (int i = 0; i <= ind; i++)
						asprintf(&temp, "%s%s ", temp, RDS_PSText[i]);
					snprintf(stext[1], 6*9, "%s", temp);
					snprintf(stext[2], 6*9, "%s", temp+(6*9));
					free(temp);
					osd->DrawText(6, 6+yoff+fheight*ii, "[", fcolor, clrTransparent, ftext, 12, ftext->Height());
					osd->DrawText(Setup.OSDWidth-12, 6+yoff+fheight*ii, "]", fcolor, clrTransparent, ftext, Setup.OSDWidth-6, ftext->Height());
					osd->DrawText(16, 6+yoff+fheight*(ii++), stext[1], fcolor, clrTransparent, ftext, Setup.OSDWidth-16, ftext->Height(), taCenter);
					osd->DrawText(6, 3+yoff+fheight*ii, "[", fcolor, clrTransparent, ftext, 12, ftext->Height());
					osd->DrawText(Setup.OSDWidth-12, 3+yoff+fheight*ii, "]", fcolor, clrTransparent, ftext, Setup.OSDWidth-6, ftext->Height());
					osd->DrawText(16, 3+yoff+fheight*(ii++), stext[2], fcolor, clrTransparent, ftext, Setup.OSDWidth-16, ftext->Height(), taCenter);
				}
			}
#endif
		}
#if 0
// framebuffer can only display raw images
		// show mpeg-still
		char *image;
		if (g_Radiotext->Rass_Archiv >= 0)
			asprintf(&image, "%s/Rass_%d.mpg", DataDir, g_Radiotext->Rass_Archiv);
		else
			asprintf(&image, "%s/Rass_show.mpg", DataDir);
		frameBuffer->useBackground(frameBuffer->loadBackground(image));// set useBackground true or false
		frameBuffer->paintBackground();
//		RadioAudio->SetBackgroundImage(image);
		free(image);
#endif
	}
	g_Radiotext->RT_MsgShow = false;
	if (blit)
		frameBuffer->blit();

}

int CInfoViewer::handleMsg (const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if ((msg == NeutrinoMessages::EVT_CURRENTNEXT_EPG) || (msg == NeutrinoMessages::EVT_NEXTPROGRAM)) {
//printf("CInfoViewer::handleMsg: NeutrinoMessages::EVT_CURRENTNEXT_EPG data %llx current %llx\n", *(t_channel_id *) data, current_channel_id & 0xFFFFFFFFFFFFULL);
		if ((*(t_channel_id *) data) == (current_channel_id & 0xFFFFFFFFFFFFULL)) {
			getEPG (*(t_channel_id *) data, info_CurrentNext);
			if (is_visible)
				show_Data (true);
			showLcdPercentOver ();
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_GOTPIDS) {
		if ((*(t_channel_id *) data) == current_channel_id) {
			if (is_visible && showButtonBar) {
				infoViewerBB->showIcon_VTXT();
				infoViewerBB->showIcon_SubT();
				//infoViewerBB->showIcon_CA_Status(0);
				infoViewerBB->showIcon_Resolution();
				infoViewerBB->showIcon_Tuner();
			}
		}
		return messages_return::handled;
	} else if ((msg == NeutrinoMessages::EVT_ZAP_COMPLETE) ||
			(msg == NeutrinoMessages::EVT_ZAP_ISNVOD)) {
		current_channel_id = (*(t_channel_id *)data);
		//killInfobarText();
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_CA_ID) {
		//chanready = 1;
		showSNR ();
		if (is_visible && showButtonBar)
			infoViewerBB->showIcon_CA_Status(0);
		//Set_CA_Status (data);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_TIMER) {
		if (data == fader.GetFadeTimer()) {
			// here, the event can only come if there is another window in the foreground!
			fader.StopFade();
			return messages_return::handled;
		} else if (data == lcdUpdateTimer) {
//printf("CInfoViewer::handleMsg: lcdUpdateTimer\n");
			if (is_visible) {
				if (fileplay || CMoviePlayerGui::getInstance().timeshift)
					CMoviePlayerGui::getInstance().UpdatePosition();
				if (fileplay) {
					const char *unit_short_minute = g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);
					char runningRest[64]; // %d can be 10 digits max...
					int curr_pos = CMoviePlayerGui::getInstance().GetPosition(); 
					int duration = CMoviePlayerGui::getInstance().GetDuration();
					snprintf(runningRest, sizeof(runningRest), "%d / %d %s", (curr_pos + 30000) / 60000, (duration - curr_pos + 30000) / 60000, unit_short_minute);
					display_Info(NULL, NULL, false, CMoviePlayerGui::getInstance().file_prozent, NULL, runningRest);
				} else if (!IS_WEBTV(current_channel_id)) {
					show_Data(false);
				}
			}
			showLcdPercentOver ();
			return messages_return::handled;
		} else if (data == sec_timer_id) {
			showSNR ();
			return messages_return::handled;
		}
	} else if (msg == NeutrinoMessages::EVT_RECORDMODE) {
		recordModeActive = data;
		if (is_visible) showRecordIcon(true);
	} else if (msg == NeutrinoMessages::EVT_ZAP_GOTAPIDS) {
		if ((*(t_channel_id *) data) == current_channel_id) {
			if (is_visible && showButtonBar) {
				infoViewerBB->showIcon_DD();
				showLivestreamInfo();
				infoViewerBB->showBBButtons(true /*paintFooter*/); // in case button text has changed
			}
			if (g_settings.radiotext_enable && g_Radiotext && !g_RemoteControl->current_PIDs.APIDs.empty() && ((CNeutrinoApp::getInstance()->getMode()) == NeutrinoMessages::mode_radio))
				g_Radiotext->setPid(g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].pid);
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES) {
		if ((*(t_channel_id *) data) == current_channel_id) {
			if (is_visible && showButtonBar)
				infoViewerBB->showBBButtons(true /*paintFooter*/); // in case button text has changed
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_SUB_COMPLETE) {
		//chanready = 1;
		showSNR ();
		//if ((*(t_channel_id *)data) == current_channel_id)
		{
			if (is_visible && showButtonBar && (!g_RemoteControl->are_subchannels))
				show_Data (true);
		}
		showLcdPercentOver ();
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_SUB_FAILED) {
		//chanready = 1;
		showSNR ();
		// show failure..!
		CVFD::getInstance ()->showServicename ("(" + g_RemoteControl->getCurrentChannelName () + ')', g_RemoteControl->getCurrentChannelNumber());
		printf ("zap failed!\n");
		showFailure ();
		CVFD::getInstance ()->showPercentOver (255);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_FAILED) {
		//chanready = 1;
		showSNR ();
		if ((*(t_channel_id *) data) == current_channel_id) {
			// show failure..!
			CVFD::getInstance ()->showServicename ("(" + g_RemoteControl->getCurrentChannelName () + ')', g_RemoteControl->getCurrentChannelNumber());
			printf ("zap failed!\n");
			showFailure ();
			CVFD::getInstance ()->showPercentOver (255);
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_MOTOR) {
		chanready = 0;
		showMotorMoving (data);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_TUNE_COMPLETE) {
		chanready = 1;
		showSNR ();
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_MODECHANGED) {
		aspectRatio = (int8_t)data;
		if (is_visible && showButtonBar)
			infoViewerBB->showIcon_16_9 ();
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_TIMESET) {
		gotTime = true;
		return messages_return::handled;
	}
#if 0
	else if (msg == NeutrinoMessages::EVT_ZAP_CA_CLEAR) {
		Set_CA_Status (false);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_CA_LOCK) {
		Set_CA_Status (true);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_CA_FTA) {
		Set_CA_Status (false);
		return messages_return::handled;
	}
#endif
	return messages_return::unhandled;
}

void CInfoViewer::sendNoEpg(const t_channel_id for_channel_id)
{
	if (!zap_mode/* & IV_MODE_DEFAULT*/) {
		char *p = new char[sizeof(t_channel_id)];
		memcpy(p, &for_channel_id, sizeof(t_channel_id));
		g_RCInput->postMsg (NeutrinoMessages::EVT_NOEPG_YET, (const neutrino_msg_data_t) p, false);
	}
}

void CInfoViewer::getEPG(const t_channel_id for_channel_id, CSectionsdClient::CurrentNextInfo &info)
{
	/* to clear the oldinfo for channels without epg, call getEPG() with for_channel_id = 0 */
	if (for_channel_id == 0 || IS_WEBTV(for_channel_id))
	{
		oldinfo.current_uniqueKey = 0;
		return;
	}

	CEitManager::getInstance()->getCurrentNextServiceKey(current_epg_id, info);

	/* of there is no EPG, send an event so that parental lock can work */
	if (info.current_uniqueKey == 0 && info.next_uniqueKey == 0) {
		sendNoEpg(for_channel_id);
		oldinfo = info;
		return;
	}

	if (info.current_uniqueKey != oldinfo.current_uniqueKey || info.next_uniqueKey != oldinfo.next_uniqueKey) {
		if (info.flags & (CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::has_next)) {
			CSectionsdClient::CurrentNextInfo *_info = new CSectionsdClient::CurrentNextInfo;
			*_info = info;
			neutrino_msg_t msg;
			if (info.flags & CSectionsdClient::epgflags::has_current)
				msg = NeutrinoMessages::EVT_CURRENTEPG;
			else
				msg = NeutrinoMessages::EVT_NEXTEPG;
			g_RCInput->postMsg(msg, (const neutrino_msg_data_t) _info, false);
		} else {
			sendNoEpg(for_channel_id);
		}
		oldinfo = info;
	}
}

void CInfoViewer::showSNR ()
{
	if (! is_visible)
		return;
	/* right now, infobar_show_channellogo == 3 is the trigger for signal bars etc.
	   TODO: decouple this  */
	if (!fileplay && !IS_WEBTV(current_channel_id) && ( g_settings.infobar_show_channellogo == 3 || g_settings.infobar_show_channellogo == 5 || g_settings.infobar_show_channellogo == 6 )) {
		int y_freq = 2*g_SignalFont->getHeight();
		if (!g_settings.infobar_sat_display)
			y_freq -= g_SignalFont->getHeight()/2; //half line up to center freq vertically
		int y_numbox = numbox->getYPos();
		if ((newfreq && chanready) || SDT_freq_update) {
			char freq[20];
			newfreq = false;

			std::string polarisation = "";
			
			if (CFrontend::isSat(CFEManager::getInstance()->getLiveFE()->getCurrentDeliverySystem()))
				polarisation = transponder::pol(CFEManager::getInstance()->getLiveFE()->getPolarization());

			int frequency = CFEManager::getInstance()->getLiveFE()->getFrequency();
			snprintf (freq, sizeof(freq), "%d.%d MHz %s", frequency / 1000, frequency % 1000, polarisation.c_str());

			int freqWidth = g_SignalFont->getRenderWidth(freq);
			if (freqWidth > (ChanWidth - numbox_offset*2))
				freqWidth = ChanWidth - numbox_offset*2;
			g_SignalFont->RenderString(BoxStartX + numbox_offset + ((ChanWidth - freqWidth) / 2), y_numbox + y_freq - 3, ChanWidth - 2*numbox_offset, freq, SDT_freq_update ? COL_COLORED_EVENTS_TEXT:COL_INFOBAR_TEXT);
			SDT_freq_update = false;
		}
		if (sigbox == NULL){
			int sigbox_offset = ChanWidth *10/100;
			sigbox = new CSignalBox(BoxStartX + sigbox_offset, y_numbox+ChanHeight/2, ChanWidth - 2*sigbox_offset, ChanHeight/2, CFEManager::getInstance()->getLiveFE(), true, NULL, "S", "Q");
			sigbox->setTextColor(COL_INFOBAR_TEXT);
			sigbox->setColorBody(numbox->getColorBody());
			sigbox->doPaintBg(false);
			sigbox->enableTboxSaveScreen(numbox->getColBodyGradientMode());
		}
		sigbox->paint(CC_SAVE_SCREEN_NO);
	}
	if(showButtonBar)
		infoViewerBB->showSysfsHdd();
}

void CInfoViewer::display_Info(const char *current, const char *next,
			       bool starttimes, const int pb_pos,
			       const char *runningStart, const char *runningRest,
			       const char *nextStart, const char *nextDuration,
			       bool update_current, bool update_next)
{
	/* dimensions of the two-line current-next "box":
	   top of box    == ChanNameY + header_height (bottom of channel name)
	   bottom of box == BoxEndY
	   height of box == BoxEndY - (ChanNameY + header_height)
	   middle of box == top + height / 2
			 == ChanNameY + header_height + (BoxEndY - (ChanNameY + header_height))/2
			 == ChanNameY + header_height + (BoxEndY - ChanNameY - header_height)/2
			 == ChanNameY / 2 + header_height / 2 + BoxEndY / 2
			 == (BoxEndY + ChanNameY + header_height)/2
	   The bottom of current info and the top of next info is == middle of box.
	 */

	int height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight();
	int CurrInfoY = (BoxEndY + ChanNameY + header_height) / 2;
	int NextInfoY = CurrInfoY/* + height*/;	// lower end of next info box
	int InfoX = ChanInfoX + 10;

	int xStart = InfoX;
	if (starttimes)
		xStart += info_time_width + 10;

	int pb_h = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight() - 4;
	switch(g_settings.infobar_progressbar)
	{
		case SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME:
		case SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME_SMALL:
			CurrInfoY += (pb_h/3);
			NextInfoY += (pb_h/3);
		break;
		case SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BETWEEN_EVENTS:
			CurrInfoY -= (pb_h/3);
			NextInfoY += (pb_h/3);
		break;
		default:
		break;
	}

	if (pb_pos > -1)
	{
		int pb_w = 112;
		int pb_startx = BoxEndX - pb_w - SHADOW_OFFSET;
		int pb_starty = ChanNameY - (pb_h + 10);
		int pb_shadow = COL_INFOBAR_SHADOW_PLUS_0;
		timescale->enableShadow(!g_settings.infobar_progressbar);
		int pb_color = (g_settings.progressbar_design == CProgressBar::PB_MONO) ? COL_INFOBAR_PLUS_0 : COL_INFOBAR_SHADOW_PLUS_0;
		if(g_settings.infobar_progressbar){
			pb_startx = xStart;
			pb_w = BoxEndX - 10 - xStart;
			pb_shadow = 0;
		}
		int tmpY = CurrInfoY - height - ChanNameY + header_height -
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getDigitOffset()/3+SHADOW_OFFSET;
		switch(g_settings.infobar_progressbar){ //set progressbar position
			case SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME:
				pb_h = (pb_h/3);
				pb_starty = ChanNameY + (tmpY-pb_h)/2;
			break;
			case SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME_SMALL:
				pb_h = (pb_h/5);
				pb_starty = ChanNameY + (tmpY-pb_h)/2;
			break;
			case SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BETWEEN_EVENTS:
				pb_starty = CurrInfoY + ((pb_h / 3)-(pb_h/5)) ;
				pb_h = (pb_h/5);
			break;
			default:
			break;
		}

		int pb_p = pb_pos * pb_w / 100;
		if (pb_p > pb_w)
			pb_p = pb_w;

		timescale->setDimensionsAll(pb_startx, pb_starty, pb_w, pb_h);
		timescale->setColorAll(pb_color, pb_color, pb_shadow);
		timescale->setValues(pb_p, pb_w);

		//printf("paintProgressBar(%d, %d, %d, %d)\n", BoxEndX - pb_w - SHADOW_OFFSET, ChanNameY - (pb_h + 10) , pb_w, pb_h);
	}else{
		if (g_settings.infobar_progressbar == SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_DEFAULT)
			timescale->kill();
	}

	int currTimeW = 0;
	int nextTimeW = 0;
	if (runningRest)
		currTimeW = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(runningRest)*2;
	if (nextDuration)
		nextTimeW = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(nextDuration)*2;
	int currTimeX = BoxEndX - currTimeW - 10;
	int nextTimeX = BoxEndX - nextTimeW - 10;

	//colored_events init
	bool colored_event_C = (g_settings.theme.colored_events_infobar == 1);
	bool colored_event_N = (g_settings.theme.colored_events_infobar == 2);

	//current event
	if (current && update_current){
		if (txt_cur_event == NULL)
			txt_cur_event = new CComponentsTextTransp(NULL, xStart, CurrInfoY - height, currTimeX - xStart - 5, height);
		else
			txt_cur_event->setDimensionsAll(xStart, CurrInfoY - height, currTimeX - xStart - 5, height);

		txt_cur_event->setText(current, CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO], colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT);
		if (txt_cur_event->isPainted())
			txt_cur_event->hide();
		txt_cur_event->paint(CC_SAVE_SCREEN_YES);

		if (runningStart && starttimes){
			if (txt_cur_start == NULL)
				txt_cur_start = new CComponentsTextTransp(NULL, InfoX, CurrInfoY - height, info_time_width, height);
			else
				txt_cur_start->setDimensionsAll(InfoX, CurrInfoY - height, info_time_width, height);
			txt_cur_start->setText(runningStart, CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO], colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT);
			if (txt_cur_event->isPainted())
				txt_cur_event->hide();
			txt_cur_start->paint(CC_SAVE_SCREEN_YES);
		}

		if (runningRest){
			if (txt_cur_event_rest == NULL)
				txt_cur_event_rest = new CComponentsTextTransp(NULL, currTimeX, CurrInfoY - height, currTimeW, height);
			else
				txt_cur_event_rest->setDimensionsAll(currTimeX, CurrInfoY - height, currTimeW, height);
			txt_cur_event_rest->setText(runningRest, CTextBox::RIGHT, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO], colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT);
			if (txt_cur_event_rest->isPainted())
				txt_cur_event_rest->hide();
			txt_cur_event_rest->paint(CC_SAVE_SCREEN_YES);
		}
	}

	//next event
	if (next && update_next)
	{
		if (txt_next_event == NULL)
			txt_next_event = new CComponentsTextTransp(NULL, xStart, NextInfoY, nextTimeX - xStart - 5, height);
		else
			txt_next_event->setDimensionsAll(xStart, NextInfoY, nextTimeX - xStart - 5, height);
		txt_next_event->setText(next, CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO], colored_event_N ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT);
		if (txt_next_event->isPainted())
			txt_next_event->hide();
		txt_next_event->paint(CC_SAVE_SCREEN_YES);

		if (nextStart && starttimes){
			if (txt_next_start == NULL)
				txt_next_start = new CComponentsTextTransp(NULL, InfoX, NextInfoY, info_time_width, height);
			else
				txt_next_start->setDimensionsAll(InfoX, NextInfoY, info_time_width, height);
			txt_next_start->setText(nextStart, CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO], colored_event_N ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT);
			if (txt_next_start->isPainted())
				txt_next_start->hide();
			txt_next_start->paint(CC_SAVE_SCREEN_YES);
		}

		if (nextDuration){
			if (txt_next_in == NULL)
				txt_next_in = new CComponentsTextTransp(NULL, nextTimeX, NextInfoY, nextTimeW, height);
			else
				txt_next_in->setDimensionsAll(nextTimeX, NextInfoY, nextTimeW, height);
			txt_next_in->setText(nextDuration, CTextBox::RIGHT, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO], colored_event_N ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT);
			if (txt_next_in->isPainted())
				txt_next_in->hide();
			txt_next_in->paint(CC_SAVE_SCREEN_YES);
		}
	}

	//finally paint time scale
	if (pb_pos > -1)
		timescale->paint();
}

void CInfoViewer::show_Data (bool calledFromEvent)
{
	if (! is_visible)
		return;

	/* EPG data is not useful in movieplayer mode ;) */
	if (fileplay && !CMoviePlayerGui::getInstance().timeshift)
		return;

	char runningStart[32];
	char runningRest[32];
	char runningPercent = 0;

	char nextStart[32];
	char nextDuration[32];

	int is_nvod = false;

	if ((g_RemoteControl->current_channel_id == current_channel_id) && (!g_RemoteControl->subChannels.empty()) && (!g_RemoteControl->are_subchannels)) {
		is_nvod = true;
		info_CurrentNext.current_zeit.startzeit = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].startzeit;
		info_CurrentNext.current_zeit.dauer = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].dauer;
	} else {
#if 0
/* this triggers false positives on some channels.
 * TODO: test on real NVOD channels, if this was even necessary at all */
		if ((info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) && (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) && (showButtonBar)) {
			if ((uint) info_CurrentNext.next_zeit.startzeit < (info_CurrentNext.current_zeit.startzeit + info_CurrentNext.current_zeit.dauer)) {
				is_nvod = true;
			}
		}
#endif
	}

	time_t jetzt = time (NULL);

	const char *unit_short_minute = g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);

	if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) {
		int seit = (abs(jetzt - info_CurrentNext.current_zeit.startzeit) + 30) / 60;
		int rest = (info_CurrentNext.current_zeit.dauer / 60) - seit;
		runningPercent = 0;
		if (!gotTime)
			snprintf(runningRest, sizeof(runningRest), "%d %s", info_CurrentNext.current_zeit.dauer / 60, unit_short_minute);
		else if (jetzt < info_CurrentNext.current_zeit.startzeit)
			snprintf(runningRest, sizeof(runningRest), "%s %d %s", g_Locale->getText(LOCALE_WORD_IN), seit, unit_short_minute);
		else {
			runningPercent = (jetzt - info_CurrentNext.current_zeit.startzeit) * 100 / info_CurrentNext.current_zeit.dauer;
			if (runningPercent > 100)
				runningPercent = 100;
			if (rest >= 0)
				snprintf(runningRest, sizeof(runningRest), "%d / %d %s", seit, rest, unit_short_minute);
			else
				snprintf(runningRest, sizeof(runningRest), "%d +%d %s", info_CurrentNext.current_zeit.dauer / 60, -rest, unit_short_minute);
		}

		struct tm *pStartZeit = localtime (&info_CurrentNext.current_zeit.startzeit);
		snprintf (runningStart, sizeof(runningStart), "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
	} else
		last_curr_id = 0;

	if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) {
		unsigned dauer = info_CurrentNext.next_zeit.dauer / 60;
		snprintf (nextDuration, sizeof(nextDuration), "%d %s", dauer, unit_short_minute);
		struct tm *pStartZeit = localtime (&info_CurrentNext.next_zeit.startzeit);
		snprintf (nextStart, sizeof(nextStart), "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
	} else
		last_next_id = 0;

//	int ChanInfoY = BoxStartY + ChanHeight + 15;	//+10

	if (showButtonBar) {
#if 0
		int posy = BoxStartY + 16;
		int height2 = 20;
		//percent
		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) {
//printf("CInfoViewer::show_Data: ************************************************* runningPercent %d\n", runningPercent);
			if (!calledFromEvent || (oldrunningPercent != runningPercent)) {
				frameBuffer->paintBoxRel(BoxEndX - 104, posy + 6, 108, 14, COL_INFOBAR_SHADOW_PLUS_0, 1);
				frameBuffer->paintBoxRel(BoxEndX - 108, posy + 2, 108, 14, COL_INFOBAR_PLUS_0, 1);
				oldrunningPercent = runningPercent;
			}
			timescale->paint(BoxEndX - 102, posy + 2, runningPercent);
		} else {
			oldrunningPercent = 255;
			frameBuffer->paintBackgroundBoxRel (BoxEndX - 108, posy, 112, height2);
		}
#endif
		infoViewerBB->showBBButtons(calledFromEvent);
	}

	if ((info_CurrentNext.flags & CSectionsdClient::epgflags::not_broadcast) ||
			(calledFromEvent && !(info_CurrentNext.flags & (CSectionsdClient::epgflags::has_next|CSectionsdClient::epgflags::has_current))))
	{
		// no EPG available
		display_Info(g_Locale->getText(gotTime ? LOCALE_INFOVIEWER_NOEPG : LOCALE_INFOVIEWER_WAITTIME), NULL);
		/* send message. Parental pin check gets triggered on EPG events... */
		/* clear old info in getEPG */
		CSectionsdClient::CurrentNextInfo dummy;
		getEPG(0, dummy);
		sendNoEpg(current_channel_id);
		return;
	}

	// irgendein EPG gefunden
	const char *current   = NULL;
	const char *curr_time = NULL;
	const char *curr_rest = NULL;
	const char *next      = NULL;
	const char *next_time = NULL;
	const char *next_dur  = NULL;
	bool curr_upd = true;
	bool next_upd = true;

	if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current)
	{
		if (!calledFromEvent || info_CurrentNext.current_uniqueKey != last_curr_id)
		{
			last_curr_id = info_CurrentNext.current_uniqueKey;
			curr_time = runningStart;
			current = info_CurrentNext.current_name.c_str();
		}
		else
			curr_upd = false;
		curr_rest = runningRest;
	}
	else
		current = g_Locale->getText(LOCALE_INFOVIEWER_NOCURRENT);

	if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next)
	{
		if (!(is_nvod && (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current))
				&& info_CurrentNext.next_uniqueKey != last_next_id)
		{	/* if current is shown, show next only if !nvod. Why? I don't know */
			//printf("SHOWDATA: last_next_id = 0x%016llx next_id = 0x%016llx\n", last_next_id, info_CurrentNext.next_uniqueKey);
			last_next_id = info_CurrentNext.next_uniqueKey;
			next_time = nextStart;
			next = info_CurrentNext.next_name.c_str();
			next_dur = nextDuration;
		}
		else
			next_upd = false;
	}
	display_Info(current, next, true, runningPercent,
		     curr_time, curr_rest, next_time, next_dur, curr_upd, next_upd);

#if 0
	int height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight ();
	int xStart = BoxStartX + ChanWidth;

	//frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);

	if ((info_CurrentNext.flags & CSectionsdClient::epgflags::not_broadcast) || ((calledFromEvent) && !(info_CurrentNext.flags & (CSectionsdClient::epgflags::has_next | CSectionsdClient::epgflags::has_current)))) {
		// no EPG available
		ChanInfoY += height;
		frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (BoxStartX + ChanWidth + 20, ChanInfoY + height, BoxEndX - (BoxStartX + ChanWidth + 20), g_Locale->getText (gotTime ? LOCALE_INFOVIEWER_NOEPG : LOCALE_INFOVIEWER_WAITTIME), COL_INFOBAR_TEXT);
	} else {
		// irgendein EPG gefunden
		int duration1Width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth (runningRest);
		int duration1TextPos = BoxEndX - duration1Width - LEFT_OFFSET;

		int duration2Width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth (nextDuration);
		int duration2TextPos = BoxEndX - duration2Width - LEFT_OFFSET;

		if ((info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) && (!(info_CurrentNext.flags & CSectionsdClient::epgflags::has_current))) {
			// there are later events available - yet no current
			frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, BoxEndX - xStart, g_Locale->getText (LOCALE_INFOVIEWER_NOCURRENT), COL_INFOBAR_TEXT);

			ChanInfoY += height;

			//info next
			//frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);

			if (last_next_id != info_CurrentNext.next_uniqueKey) {
				frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (ChanInfoX + 10, ChanInfoY + height, 100, nextStart, COL_INFOBAR_TEXT);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, duration2TextPos - xStart - 5, info_CurrentNext.next_name, COL_INFOBAR_TEXT);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (duration2TextPos, ChanInfoY + height, duration2Width, nextDuration, COL_INFOBAR_TEXT);
				last_next_id = info_CurrentNext.next_uniqueKey;
			}
		} else {
			if (last_curr_id != info_CurrentNext.current_uniqueKey) {
				frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (ChanInfoX + 10, ChanInfoY + height, 100, runningStart, COL_INFOBAR_TEXT);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, duration1TextPos - xStart - 5, info_CurrentNext.current_name, COL_INFOBAR_TEXT);

				last_curr_id = info_CurrentNext.current_uniqueKey;
			}
			frameBuffer->paintBox (BoxEndX - 80, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);//FIXME duration1TextPos not really good
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (duration1TextPos, ChanInfoY + height, duration1Width, runningRest, COL_INFOBAR_TEXT);

			ChanInfoY += height;

			//info next
			//frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);

			if ((!is_nvod) && (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next)) {
				if (last_next_id != info_CurrentNext.next_uniqueKey) {
					frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (ChanInfoX + 10, ChanInfoY + height, 100, nextStart, COL_INFOBAR_TEXT);
					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, duration2TextPos - xStart - 5, info_CurrentNext.next_name, COL_INFOBAR_TEXT);
					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (duration2TextPos, ChanInfoY + height, duration2Width, nextDuration, COL_INFOBAR_TEXT);
					last_next_id = info_CurrentNext.next_uniqueKey;
				}
			} //else
			//frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);//why this...
		}
	}
}
#endif
}

void CInfoViewer::killInfobarText()
{
	if (infobar_txt){
		if (infobar_txt->isPainted())
			infobar_txt->kill();
		delete infobar_txt;
		infobar_txt = NULL;
	}
}


void CInfoViewer::showInfoFile()
{
	//read textcontent from this file
	std::string infobar_file = "/tmp/infobar.txt"; 

	//exit if file not found, don't create an info object, delete old instance if required
	if (!file_size(infobar_file.c_str()))	{
		killInfobarText();
		return;
	}

	//get width of progressbar timescale
	int pb_w = 0;
	if ( (timescale != NULL) && (g_settings.infobar_progressbar == 0) ) {
		pb_w = timescale->getWidth();
	}

	//set position of info area
	const int oOffset	= 140; // outer left/right offset
	const int xStart	= BoxStartX + ChanWidth + oOffset;
	const int yStart	= BoxStartY;
	const int width		= BoxEndX - xStart - oOffset - pb_w;
	const int height	= g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight() + 2;

	//create info object
	if (infobar_txt == NULL){
		infobar_txt = new CComponentsInfoBox();
		//set some properties for info object
		infobar_txt->setCorner(RADIUS_SMALL);
		infobar_txt->enableShadow(CC_SHADOW_ON, SHADOW_OFFSET/2);
		infobar_txt->setTextColor(COL_INFOBAR_TEXT);
		infobar_txt->setColorBody(COL_INFOBAR_PLUS_0);
		infobar_txt->doPaintTextBoxBg(false);
		infobar_txt->enableColBodyGradient(g_settings.theme.infobar_gradient_top, g_settings.theme.infobar_gradient_top ? COL_INFOBAR_PLUS_0 : header->getColorBody(), g_settings.theme.infobar_gradient_top_direction);
	}

	//get text from file and set it to info object, exit and delete object if failed
	string old_txt = infobar_txt->getText();
	string new_txt = infobar_txt->getTextFromFile(infobar_file);
	bool has_text = infobar_txt->setText(new_txt, CTextBox::CENTER, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]);
	if (new_txt.empty()){
		killInfobarText();
		return;
	}

	//check dimension, if changed then kill to force reinit
	if (infobar_txt->getWidth() != width)
		infobar_txt->kill();

	//consider possible size change
	infobar_txt->setDimensionsAll(xStart, yStart, width, height);

	//paint info if not painted or text has changed
	if (has_text || (zap_mode & IV_MODE_VIRTUAL_ZAP)){
		if ((old_txt != new_txt) || !infobar_txt->isPainted())
			infobar_txt->paint(CC_SAVE_SCREEN_NO);
	}
}

void CInfoViewer::killTitle()
{
	if (is_visible)
	{
		is_visible = false;
		infoViewerBB->is_visible = false;
#if 0 //unused
		int bottom = BoxEndY + SHADOW_OFFSET + infoViewerBB->bottom_bar_offset;
		if (showButtonBar)
			bottom += infoViewerBB->InfoHeightY_Info;
#endif
		if (infoViewerBB->getFooter())
			infoViewerBB->getFooter()->kill();
		if (infoViewerBB->getCABar())
			infoViewerBB->getCABar()->kill();
		if (rec)
			rec->kill();
		//printf("killTitle(%d, %d, %d, %d)\n", BoxStartX, BoxStartY, BoxEndX+ SHADOW_OFFSET-BoxStartX, bottom-BoxStartY);
		//frameBuffer->paintBackgroundBox(BoxStartX, BoxStartY, BoxEndX+ SHADOW_OFFSET, bottom);
		if (!(zap_mode & IV_MODE_VIRTUAL_ZAP)){
			if (infobar_txt)
				infobar_txt->kill();
			numbox->kill();
		}

#if 0 //not really required to kill sigbox, numbox does this
		if (sigbox)
			sigbox->kill();
#endif
		header->kill();
#if 0 //not really required to kill clock, header does this
		if (clock)
			clock->kill();
#endif
		body->kill();
#if 0 //not really required to kill epg infos, body does this
		if (txt_cur_event)
			txt_cur_event->kill();
		if (txt_cur_event_rest)
			txt_cur_event_rest->kill();
		if (txt_cur_start)
			txt_cur_start->kill();
		if (txt_next_start)
			txt_next_start->kill();
		if (txt_next_event)
			txt_next_event->kill();
		if (txt_next_in)
			txt_next_in->kill();
#endif
		if (timescale)
			if (g_settings.infobar_progressbar == SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_DEFAULT)
				timescale->kill();
		if (g_settings.radiotext_enable && g_Radiotext) {
			g_Radiotext->S_RtOsd = g_Radiotext->haveRadiotext() ? 1 : 0;
			killRadiotext();
		}
		killInfobarText();
		frameBuffer->blit();
	}
	showButtonBar = false;
	CInfoClock::getInstance()->enableInfoClock();
}

#if 0
void CInfoViewer::Set_CA_Status (int /*Status*/)
{
	if (is_visible && showButtonBar)
		infoViewerBB->showIcon_CA_Status(1);
}
#endif
/******************************************************************************
returns mode of painted channel logo,
0 = no logo painted
1 = in number box
2 = in place of channel name
3 = beside channel name
*******************************************************************************/
int CInfoViewer::showChannelLogo(const t_channel_id logo_channel_id, const int channel_number_width)
{
	if (!g_settings.infobar_show_channellogo) // show logo only if configured
		return 0;

	std::string strAbsIconPath;

	int logo_w, logo_h;
	int logo_x = 0, logo_y = 0;
	int res = 0;
	int start_x = ChanNameX;
	int chan_w = BoxEndX- (start_x+ 20)- time_width- LEFT_OFFSET - 10;

	bool logo_available = g_PicViewer->GetLogoName(logo_channel_id, ChannelName, strAbsIconPath, &logo_w, &logo_h);

	//fprintf(stderr, "%s: logo_available: %d file: %s\n", __FUNCTION__, logo_available, strAbsIconPath.c_str());

	if (! logo_available)
		return 0;

	if ((logo_w == 0) || (logo_h == 0)) // corrupt logo size?
	{
		printf("[infoviewer] channel logo: \n"
		       " -> %s (%s) has no size\n"
		       " -> please check logo file!\n", strAbsIconPath.c_str(), ChannelName.c_str());
		return 0;
	}
	int y_mid;

	if (g_settings.infobar_show_channellogo == 1) // paint logo in numberbox
	{
		// calculate mid of numberbox
		int satNameHeight = g_settings.infobar_sat_display ? g_SignalFont->getHeight() : 0;
		int x_mid = BoxStartX + ChanWidth / 2;
		y_mid = numbox->getYPos() + (satNameHeight + ChanHeight) / 2;

		g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, ChanWidth, ChanHeight - satNameHeight);
		// channel name with number
// this is too ugly...		ChannelName = (std::string)strChanNum + ". " + ChannelName;
		// get position of channel logo, must be centered in number box
		logo_x = x_mid - logo_w / 2;
		logo_y = y_mid - logo_h / 2;
		res = 1;
	}
	else if (g_settings.infobar_show_channellogo == 2 || g_settings.infobar_show_channellogo == 5 || g_settings.infobar_show_channellogo == 6) // paint logo in place of channel name
	{
		// check logo dimensions
		g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, chan_w, header_height);
		// hide channel name
// this is too ugly...		ChannelName = "";
		// calculate logo position
		y_mid = ChanNameY + header_height / 2;
		logo_x = start_x + 10 + channel_number_width;;
		logo_y = y_mid - logo_h / 2;
		if (g_settings.infobar_show_channellogo == 2)
			res = 2;
		else
			res = 5;
	}
	else if (g_settings.infobar_show_channellogo == 3 || g_settings.infobar_show_channellogo == 4)  // paint logo beside channel name
	{
		// check logo dimensions
		int Logo_max_width = chan_w - logo_w - 10;
		g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, Logo_max_width, header_height);
		// calculate logo position
		y_mid = ChanNameY + header_height / 2;
		logo_x = start_x + 10;
		logo_y = y_mid - logo_h / 2;
		// set channel name x pos right of the logo
		ChanNameX = start_x + logo_w + 10;
		if (g_settings.infobar_show_channellogo == 3)
			res = 3;
		else
			res = 4;
	}
	else
	{
		res = 0;
	}

	// paint the logo
	if (res != 0) {
		if (!g_PicViewer->DisplayImage(strAbsIconPath, logo_x, logo_y, logo_w, logo_h))
			return 0; // paint logo failed
	}

	return res;
}

#if HAVE_TRIPLEDRAGON
/* the cheap COOLSTREAM display cannot do this, so keep the routines separate */
void CInfoViewer::showLcdPercentOver()
{
	if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] != 1)
	{
		if (fileplay || NeutrinoMessages::mode_ts == CNeutrinoApp::getInstance()->getMode()) {
			CVFD::getInstance()->showPercentOver(CMoviePlayerGui::getInstance().file_prozent);
			return;
		}
		static long long old_interval = 0;
		int runningPercent = -1;
		time_t jetzt = time(NULL);
		long long interval = 60000000; /* 60 seconds default update time */
		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) {
			if (jetzt < info_CurrentNext.current_zeit.startzeit)
				runningPercent = 0;
			else if (jetzt > (int)(info_CurrentNext.current_zeit.startzeit +
					       info_CurrentNext.current_zeit.dauer))
				runningPercent = -2; /* overtime */
			else {
				runningPercent = MIN((jetzt-info_CurrentNext.current_zeit.startzeit) * 100 /
					              info_CurrentNext.current_zeit.dauer, 100);
				interval = info_CurrentNext.current_zeit.dauer * 1000LL * (1000/100); // update every percent
				if (is_visible && interval > 60000000)	// if infobar visible, update at
					interval = 60000000;		// least once per minute (radio mode)
				if (interval < 5000000)
					interval = 5000000;		// but update only every 5 seconds
			}
		}
		if (interval != old_interval) {
			g_RCInput->killTimer(lcdUpdateTimer);
			lcdUpdateTimer = g_RCInput->addTimer(interval, false);
			//printf("lcdUpdateTimer: interval %lld old %lld\n",interval/1000000,old_interval/1000000);
			old_interval = interval;
		}
		CLCD::getInstance()->showPercentOver(runningPercent);
		int mode = CNeutrinoApp::getInstance()->getMode();
		if ((mode == NeutrinoMessages::mode_radio || mode == NeutrinoMessages::mode_tv))
			CVFD::getInstance()->setEPGTitle(info_CurrentNext.current_name);
	}
}
#else
void CInfoViewer::showLcdPercentOver ()
{
	if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] != 1) {
		if (fileplay || (NeutrinoMessages::mode_ts == CNeutrinoApp::getInstance()->getMode())) {
			CVFD::getInstance ()->showPercentOver (CMoviePlayerGui::getInstance().file_prozent);
			return;
		}
		int runningPercent = -1;
		time_t jetzt = time (NULL);
#if 0
		if (!(info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) || jetzt > (int) (info_CurrentNext.current_zeit.startzeit + info_CurrentNext.current_zeit.dauer)) {
			info_CurrentNext = getEPG (current_channel_id);
		}
#endif
		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) {
			if (jetzt < info_CurrentNext.current_zeit.startzeit)
				runningPercent = 0;
			else
				runningPercent = MIN ((jetzt - info_CurrentNext.current_zeit.startzeit) * 100 / info_CurrentNext.current_zeit.dauer, 100);
		}
		CVFD::getInstance ()->showPercentOver (runningPercent);
	}
}
#endif

void CInfoViewer::showEpgInfo()   //message on event change
{
	int mode = CNeutrinoApp::getInstance()->getMode();
	/* show epg info only if we in TV- or Radio mode and current event is not the same like before */
	if ((eventname != info_CurrentNext.current_name) && (mode == 2 /*mode_radio*/ || mode == 1 /*mode_tv*/))
	{
		eventname = info_CurrentNext.current_name;
		if (g_settings.infobar_show)
			g_RCInput->postMsg(NeutrinoMessages::SHOW_INFOBAR , 0);
#if 0
		/* let's check if this is still needed */
		else
			/* don't show anything, but update the LCD
			   TODO: we should not have to update the LCD from the _infoviewer_.
				 they have nothing to do with each other */
			showLcdPercentOver();
#endif
	}
}

void CInfoViewer::setUpdateTimer(uint64_t interval)
{
	g_RCInput->killTimer(lcdUpdateTimer);
	if (interval)
		lcdUpdateTimer = g_RCInput->addTimer(interval, false);
}

#if 0
int CInfoViewerHandler::exec (CMenuTarget * parent, const std::string & /*actionkey*/)
{
	int res = menu_return::RETURN_EXIT_ALL;
	CChannelList *channelList;
	CInfoViewer *i;

	if (parent) {
		parent->hide ();
	}

	i = new CInfoViewer;

	channelList = CNeutrinoApp::getInstance ()->channelList;
	i->start ();
	i->showTitle (channelList->getActiveChannelNumber (), channelList->getActiveChannelName (), channelList->getActiveSatellitePosition (), channelList->getActiveChannel_ChannelID ());	// UTF-8
	delete i;

	return res;
}
#endif

void CInfoViewer::ResetModules(bool kill)
{
	if (kill) {
		if (txt_cur_event)
			txt_cur_event->clearSavedScreen();
		if (txt_cur_event_rest)
			txt_cur_event_rest->clearSavedScreen();
		if (txt_cur_start)
			txt_cur_start->clearSavedScreen();
		if (txt_next_event)
			txt_next_event->clearSavedScreen();
		if (txt_next_in)
			txt_next_in->clearSavedScreen();
		if (txt_next_start)
			txt_next_start->clearSavedScreen();
	}
	delete header;
	header = NULL;
	delete body;
	body = NULL;
	delete infobar_txt;
	infobar_txt = NULL;
	delete clock;
	clock = NULL;
	delete txt_cur_start;
	txt_cur_start = NULL;
	delete txt_cur_event;
	txt_cur_event = NULL;
	delete txt_cur_event_rest;
	txt_cur_event_rest = NULL;
	delete txt_next_start;
	txt_next_start = NULL;
	delete txt_next_event;
	txt_next_event = NULL;
	delete txt_next_in;
	txt_next_in = NULL;
	delete numbox;
	numbox = NULL;
	ResetPB();
	delete rec;
	rec = NULL;
	infoViewerBB->ResetModules();
}
