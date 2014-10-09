/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

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
#include <driver/volume.h>

#include <zapit/satconfig.h>
#include <zapit/femanager.h>
#include <zapit/zapit.h>
#include <eitd/sectionsd.h>
#include <video.h>

extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */
extern CBouquetList * bouquetList;       /* neutrino.cpp */
extern CPictureViewer * g_PicViewer;
extern cVideo * videoDecoder;
extern CInfoClock *InfoClock;

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
	sigscale = NULL;
	snrscale = NULL;
	timescale = NULL;
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
	time_left_width = 0;
	time_dot_width = 0;
	time_width = 0;
	time_height = 0;
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
}

CInfoViewer::~CInfoViewer()
{
	delete sigscale;
	delete snrscale;
	delete timescale;
	delete infoViewerBB;
	delete infobar_txt;
	delete clock;
}

void CInfoViewer::Init()
{
	BoxStartX = BoxStartY = BoxEndX = BoxEndY = 0;
	recordModeActive = false;
	is_visible = false;
	showButtonBar = false;
	//gotTime = g_Sectionsd->getIsTimeSet ();
	gotTime = timeset;
	virtual_zap_mode = false;
	newfreq = true;
	chanready = 1;
	fileplay = 0;
	SDT_freq_update = false;

	/* maybe we should not tie this to the blinkenlights settings? */
	infoViewerBB->setBBOffset();
	/* after font size changes, Init() might be called multiple times */
	changePB();

	casysChange = g_settings.casystem_display;
	channellogoChange = g_settings.infobar_show_channellogo;

	channel_id = CZapit::getInstance()->GetCurrentChannelID();;
	lcdUpdateTimer = 0;
	rt_x = rt_y = rt_h = rt_w = 0;

	infobar_txt = NULL;
}

/*
 * This nice ASCII art should hopefully explain how all the variables play together ;)
 *

              ___BoxStartX
             |-ChanWidth-|
             |           |  _recording icon                 _progress bar
 BoxStartY---+-----------+ |                               |
     |       |           | *                              #######____
     |       |           |-------------------------------------------+--+-ChanNameY
     |       |           | Channelname                               |  |
 ChanHeight--+-----------+                                           |  |
                |                                                    |  InfoHeightY
                |01:23     Current Event                             |  |
                |02:34     Next Event                                |  |
                |                                                    |  |
     BoxEndY----+----------------------------------------------------+--+
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

	if ( g_settings.infobar_show_channellogo != 3 && g_settings.infobar_show_channellogo != 5  && g_settings.infobar_show_channellogo != 6) /* 3 & 5 & 6 is "default" with sigscales etc. */
	{
		ChanWidth = 4 * g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getMaxDigitWidth() + 10;
		ChanHeight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getHeight() * 9 / 8;
	}
	else
	{	/* default mode, with signal bars etc. */
		ChanWidth = 122;
		ChanHeight = 74;
		int test = g_SignalFont->getWidth() * 14;
		if (test > ChanWidth) {
			ChanWidth = test;
		}
		test = (g_SignalFont->getHeight() * 2) + (36 * g_settings.screen_yres / 100);
		if (test > ChanHeight) {
			ChanHeight = test;
		}
	}

	BoxStartX = g_settings.screen_StartX + 10;
	BoxEndX = g_settings.screen_EndX - 10;
	BoxEndY = g_settings.screen_EndY - 10 - infoViewerBB->InfoHeightY_Info - infoViewerBB->bottom_bar_offset;
	BoxStartY = BoxEndY - InfoHeightY - ChanHeight / 2;

	ChanNameY = BoxStartY + (ChanHeight / 2) + SHADOW_OFFSET;	//oberkante schatten?
	ChanInfoX = BoxStartX + (ChanWidth / 3);

	time_height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getHeight();
	time_left_width = 2 * g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getWidth(); /* still a kludge */
	time_dot_width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(":");
	time_width = time_left_width* 2+ time_dot_width;

	if (clock) {
		delete clock;
		clock = NULL;
	}
}

void CInfoViewer::changePB()
{
	if (sigscale)
		delete sigscale;
	sigscale = new CProgressBar();
	
	if (snrscale)
		delete snrscale;
	snrscale = new CProgressBar();
	
	if (timescale)
		delete timescale;
	timescale = new CProgressBar();
	timescale->setType(CProgressBar::PB_TIMESCALE);
}

void CInfoViewer::paintTime (bool show_dot)
{
	if (!gotTime)
		gotTime = timeset;

	if (!gotTime)
		return;

	int clock_x = BoxEndX - time_width - LEFT_OFFSET;
	int clock_y = ChanNameY;
	int clock_w = time_width + LEFT_OFFSET;
	int clock_h = time_height;

	if (clock == NULL){
		clock = new CComponentsFrmClock();
		clock->doPaintBg(false);
	}

	clock->setColorBody(COL_INFOBAR_PLUS_0);
	clock->setCorner(RADIUS_LARGE, CORNER_TOP_RIGHT);
	clock->setDimensionsAll(clock_x, clock_y, clock_w, clock_h);
	clock->setClockFont(SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME);
	clock->setClockFormat(show_dot ? "%H:%M" : "%H %M");
	clock->setTextColor(COL_INFOBAR_TEXT);

	clock->paint(CC_SAVE_SCREEN_NO);
}

void CInfoViewer::showRecordIcon (const bool show)
{
	CRecordManager * crm		= CRecordManager::getInstance();
	
	recordModeActive		= crm->RecordingStatus();
	/* FIXME if record or timeshift stopped while infobar visible, artifacts */
	if (recordModeActive)
	{
		std::string Icon_Rec = NEUTRINO_ICON_REC_GRAY, Icon_Ts = NEUTRINO_ICON_AUTO_SHIFT_GRAY;
		t_channel_id cci	= g_RemoteControl->current_channel_id;

		/* global record mode */
		int rec_mode 		= crm->GetRecordMode();
		/* channel record mode */
		int ccrec_mode 		= crm->GetRecordMode(cci);

		/* set 'active' icons for current channel */
		if (ccrec_mode & CRecordManager::RECMODE_TSHIFT)
			Icon_Ts		= NEUTRINO_ICON_AUTO_SHIFT;

		if (ccrec_mode & CRecordManager::RECMODE_REC)
			Icon_Rec	= NEUTRINO_ICON_REC;

		int records		= crm->GetRecordCount();
		
		const int radius = RADIUS_MIN;
		const int ChanName_X = BoxStartX + ChanWidth + SHADOW_OFFSET;
		const int icon_space = 3, box_posY = 12;
		int box_len = 0, rec_icon_posX = 0, ts_icon_posX = 0;
		
		int rec_icon_w = 0, rec_icon_h = 0, ts_icon_w = 0, ts_icon_h = 0;
		frameBuffer->getIconSize(Icon_Rec.c_str(), &rec_icon_w, &rec_icon_h);
		frameBuffer->getIconSize(Icon_Ts.c_str(), &ts_icon_w, &ts_icon_h);
		
		int chanH = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight () * (g_settings.screen_yres / 100);
		if (chanH < rec_icon_h)
			chanH = rec_icon_h;
		const int box_posX = ChanName_X + SHADOW_OFFSET;
		
		char records_msg[8];
		snprintf(records_msg, sizeof(records_msg)-1, "%d%s", records, "x");
		int TextWidth = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(records_msg) 
					* (g_settings.screen_xres / 100);
					
		if (rec_mode == CRecordManager::RECMODE_REC)
		{
			box_len = rec_icon_w + TextWidth + icon_space*5;
			rec_icon_posX = box_posX + icon_space*2;
		}
		else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
		{
			box_len = ts_icon_w + icon_space*4;
			ts_icon_posX = box_posX + icon_space*2;
		}
		else if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
		{
			box_len = ts_icon_w + rec_icon_w + TextWidth + icon_space*7;
			ts_icon_posX = box_posX + icon_space*2;
			rec_icon_posX = ts_icon_posX + ts_icon_w + icon_space*2;
			records--;
			snprintf(records_msg, sizeof(records_msg)-1, "%d%s", records, "x");
		}
		
		if (show)
		{
			frameBuffer->paintBoxRel(box_posX + SHADOW_OFFSET, BoxStartY + box_posY + SHADOW_OFFSET, box_len, chanH, COL_INFOBAR_SHADOW_PLUS_0, radius);
			frameBuffer->paintBoxRel(box_posX, BoxStartY + box_posY , box_len, chanH, COL_INFOBAR_PLUS_0, radius);
			
			if (rec_mode != CRecordManager::RECMODE_TSHIFT)
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString (rec_icon_posX + rec_icon_w + icon_space, BoxStartY + box_posY + chanH, box_len, records_msg, COL_INFOBAR_TEXT);
			
			if (rec_mode == CRecordManager::RECMODE_REC)
			{
				frameBuffer->paintIcon(Icon_Rec, rec_icon_posX, BoxStartY + box_posY + (chanH - rec_icon_h)/2);
			}
			else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
			{
				frameBuffer->paintIcon(Icon_Ts, ts_icon_posX, BoxStartY + box_posY + (chanH - ts_icon_h)/2);
			}
			else if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
			{
				frameBuffer->paintIcon(Icon_Rec, rec_icon_posX, BoxStartY + box_posY + (chanH - rec_icon_h)/2);
				frameBuffer->paintIcon(Icon_Ts, ts_icon_posX, BoxStartY + box_posY + (chanH - ts_icon_h)/2);
			}
		}
		else
		{
			if (rec_mode == CRecordManager::RECMODE_REC)
				frameBuffer->paintBoxRel(rec_icon_posX, BoxStartY + box_posY + (chanH - rec_icon_h)/2, rec_icon_w, rec_icon_h, COL_INFOBAR_PLUS_0);
			else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
				frameBuffer->paintBoxRel(ts_icon_posX, BoxStartY + box_posY + (chanH - ts_icon_h)/2, ts_icon_w, ts_icon_h, COL_INFOBAR_PLUS_0);
			else if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
				frameBuffer->paintBoxRel(ts_icon_posX, BoxStartY + box_posY + (chanH - rec_icon_h)/2, ts_icon_w + rec_icon_w + icon_space*2, rec_icon_h, COL_INFOBAR_PLUS_0);
		}
	}
}

void CInfoViewer::paintBackground(int col_NumBox)
{
	int c_rad_large = RADIUS_LARGE;
	int c_shadow_width = (c_rad_large * 2) + 1;
	int c_rad_mid = RADIUS_MID;
	int BoxEndInfoY = BoxEndY;
	if (showButtonBar) // add button bar and blinkenlights
		BoxEndInfoY += infoViewerBB->InfoHeightY_Info + infoViewerBB->bottom_bar_offset;
	// kill left side
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
			      BoxEndX - c_shadow_width, BoxEndInfoY + SHADOW_OFFSET,
			      COL_INFOBAR_SHADOW_PLUS_0, c_rad_large, CORNER_BOTTOM_LEFT);

	// background for channel name, epg data
	frameBuffer->paintBox(ChanInfoX, ChanNameY, BoxEndX, BoxEndY,
			      COL_INFOBAR_PLUS_0, c_rad_large,
			      CORNER_TOP_RIGHT | (showButtonBar ? 0 : CORNER_BOTTOM));

	// number box
	frameBuffer->paintBoxRel(BoxStartX + SHADOW_OFFSET, BoxStartY + SHADOW_OFFSET,
				 ChanWidth, ChanHeight,
				 COL_INFOBAR_SHADOW_PLUS_0, c_rad_mid);
	frameBuffer->paintBoxRel(BoxStartX, BoxStartY,
				 ChanWidth, ChanHeight,
				 col_NumBox, c_rad_mid);
}

void CInfoViewer::show_current_next(bool new_chan, int  epgpos)
{
	CEitManager::getInstance()->getCurrentNextServiceKey(channel_id, info_CurrentNext);
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
		display_Info(g_Locale->getText(loc), NULL);
	} else {
		show_Data ();
	}
}

void CInfoViewer::showMovieTitle(const int playState, const t_channel_id &Channel_Id, const std::string &Channel,
				 const std::string &g_file_epg, const std::string &g_file_epg1,
				 const int duration, const int curr_pos,
				 const int repeat_mode)
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
	t_channel_id old_channel_id = channel_id;
	channel_id = Channel_Id;

	/* showChannelLogo() changes this, so better reset it every time... */
	ChanNameX = BoxStartX + ChanWidth + SHADOW_OFFSET;

	paintBackground(COL_INFOBAR_PLUS_0);

	bool show_dot = true;
	paintTime (show_dot);
	showRecordIcon (show_dot);
	show_dot = !show_dot;

	infoViewerBB->paintshowButtonBar();

	int ChannelLogoMode = 0;
	if (g_settings.infobar_show_channellogo > 1)
		ChannelLogoMode = showChannelLogo(channel_id, 0);
	if (ChannelLogoMode == 0 || ChannelLogoMode == 3 || ChannelLogoMode == 4)
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(ChanNameX + 10 , ChanNameY + time_height,BoxEndX - (ChanNameX + 20) - time_width - LEFT_OFFSET - 5 ,ChannelName, COL_INFOBAR_TEXT);

	// show_Data
	if (CMoviePlayerGui::getInstance().file_prozent > 100)
		CMoviePlayerGui::getInstance().file_prozent = 100;

	const char *unit_short_minute = g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);
 	char runningRest[32]; // %d can be 10 digits max...
	snprintf(runningRest, sizeof(runningRest), "%d / %d %s", (curr_pos + 30000) / 60000, (duration - curr_pos + 30000) / 60000, unit_short_minute);
	display_Info(g_file_epg.c_str(), g_file_epg1.c_str(), true, false, CMoviePlayerGui::getInstance().file_prozent, NULL, runningRest);

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
	channel_id = old_channel_id;
}

void CInfoViewer::reset_allScala()
{
	sigscale->reset();
	snrscale->reset();
	timescale->reset();
	lastsig = lastsnr = -1;
	infoViewerBB->reset_allScala();
}

void CInfoViewer::check_channellogo_ca_SettingsChange()
{
	if (casysChange != g_settings.casystem_display || channellogoChange != g_settings.infobar_show_channellogo) {
		casysChange = g_settings.casystem_display;
		channellogoChange = g_settings.infobar_show_channellogo;
		infoViewerBB->setBBOffset();
		start();
	}
}

void CInfoViewer::showTitle(CZapitChannel * channel, const bool calledFromNumZap, int epgpos)
{
	if(channel) {

		showTitle(channel->number, channel->getName(), channel->getSatellitePosition(),
				channel->getChannelID(), calledFromNumZap, epgpos, channel->pname);
	}
}

void CInfoViewer::showTitle(t_channel_id chid, const bool calledFromNumZap, int epgpos)
{
	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(chid);

	if(channel) {
		showTitle(channel->number, channel->getName(), channel->getSatellitePosition(),
				channel->getChannelID(), calledFromNumZap, epgpos, channel->pname);
	}
}

void CInfoViewer::showTitle (const int ChanNum, const std::string & Channel, const t_satellite_position satellitePosition, const t_channel_id new_channel_id, const bool calledFromNumZap, int epgpos, char *pname)
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

	if (virtual_zap_mode) {
		if (g_RemoteControl->current_channel_id != new_channel_id) {
			col_NumBoxText = COL_MENUHEAD_TEXT;
		}
		if ((channel_id != new_channel_id) || (evtlist.empty())) {
			CEitManager::getInstance()->getEventsServiceKey(new_channel_id, evtlist);
			if (!evtlist.empty())
				sort(evtlist.begin(),evtlist.end(), sortByDateTime);
			new_chan = true;
		}
	}
	if (! calledFromNumZap && !(g_RemoteControl->subChannels.empty()) && (g_RemoteControl->selected_subchannel > 0))
	{
		channel_id = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].getChannelID();
		ChannelName = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].subservice_name;
	} else {
		channel_id = new_channel_id;
	}

	/* showChannelLogo() changes this, so better reset it every time... */
	ChanNameX = BoxStartX + ChanWidth + SHADOW_OFFSET;


	paintBackground(col_NumBox);

	bool show_dot = true;
	paintTime (show_dot);
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
		ChannelLogoMode = showChannelLogo(channel_id,channel_number_width); // get logo mode, paint channel logo if adjusted
		logo_ok = ( g_settings.infobar_show_channellogo != 0 && ChannelLogoMode != 0);
		fprintf(stderr, "after showchannellogo, mode = %d ret = %d logo_ok = %d\n",g_settings.infobar_show_channellogo, ChannelLogoMode, logo_ok);

		int ChanNumYPos = BoxStartY + ChanHeight;
		if (g_settings.infobar_sat_display) {
			std::string name = (IS_WEBTV(channel_id))? "WebTV" : CServiceManager::getInstance()->GetSatelliteName(satellitePosition);
			int satNameWidth = g_SignalFont->getRenderWidth (name);
			std::string satname_tmp = name;
			if (satNameWidth > (ChanWidth - 4)) {
				satNameWidth = ChanWidth - 4;
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
					if (satNameWidth > (ChanWidth - 4)) 
						satNameWidth = ChanWidth - 4;
				}
			}
			int chanH = g_SignalFont->getHeight ();
			g_SignalFont->RenderString (3 + BoxStartX + ((ChanWidth - satNameWidth) / 2) , BoxStartY + chanH, satNameWidth, satname_tmp, COL_INFOBAR_TEXT);
			ChanNumYPos += 10;
		}

		/* TODO: the logic will get much easier once we decouple channellogo and signal bars */
		if ((!logo_ok && g_settings.infobar_show_channellogo < 2) || g_settings.infobar_show_channellogo == 2 || g_settings.infobar_show_channellogo == 4) // no logo in numberbox
		{
			// show number in numberbox
			int tmpwidth = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getRenderWidth(strChanNum);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->RenderString(
				BoxStartX + (ChanWidth - tmpwidth) / 2, ChanNumYPos,
				ChanWidth, strChanNum, col_NumBoxText);
		}
		if (ChannelLogoMode == 1 || ( g_settings.infobar_show_channellogo == 3 && !logo_ok) || g_settings.infobar_show_channellogo == 6 ) /* channel number besides channel name */
		{
			ChanNumWidth = 5 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth (strChanNum);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(
				ChanNameX + 5, ChanNameY + time_height,
				ChanNumWidth, strChanNum, col_NumBoxText);
		}
	}

	if (g_settings.infobar_show_channellogo < 5 || !logo_ok) {
		if (ChannelLogoMode != 2) {
			//FIXME good color to display inactive for zap ?
			//fb_pixel_t color = CNeutrinoApp::getInstance ()->channelList->SameTP(new_channel_id) ? COL_INFOBAR_TEXT : COL_INFOBAR_SHADOW_TEXT;
			fb_pixel_t color = COL_INFOBAR_TEXT;
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(
				ChanNameX + 10 + ChanNumWidth, ChanNameY + time_height,
				BoxEndX - (ChanNameX + 20) - time_width - LEFT_OFFSET - 5 - ChanNumWidth,
				ChannelName, color /*COL_INFOBAR_TEXT*/);
			//provider name
			if(g_settings.infobar_show_channeldesc && pname){
				std::string prov_name = pname;
				prov_name=prov_name.substr(prov_name.find_first_of("]")+1);

				int chname_width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth (ChannelName);
				unsigned int chann_size = ChannelName.size();
				if(ChannelName.empty())
					chann_size = 1;
				chname_width += (chname_width/chann_size/2);

				int tmpY = ((ChanNameY + time_height) - g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getDigitOffset() 
						+ g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getDigitOffset());
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(
					ChanNameX + 10 + ChanNumWidth + chname_width, tmpY,
					BoxEndX - (ChanNameX + 20) - time_width - LEFT_OFFSET - 5 - ChanNumWidth - chname_width,
					prov_name, color /*COL_INFOBAR_TEXT*/);
			}

		}
	}

	if (fileplay) {
		show_Data ();
	} else if (IS_WEBTV(new_channel_id)) {
		CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(new_channel_id);
		if (channel) {
			const char *current = channel->getDesc().c_str();
			const char *next = channel->getUrl().c_str();
			if (!current) {
				current = next;
				next = "";
			}
			display_Info(current, next, true, false, 0, NULL, NULL, NULL, NULL, true, true);
		}
	} else {
		show_current_next(new_chan,epgpos);
	}

	showLcdPercentOver ();
	showInfoFile();

#if 0
	if ((g_RemoteControl->current_channel_id == channel_id) && !(((info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) && (info_CurrentNext.flags & (CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::has_no_current))) || (info_CurrentNext.flags & CSectionsdClient::epgflags::not_broadcast))) {
		g_Sectionsd->setServiceChanged (channel_id, true);
	}
#endif

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
void CInfoViewer::loop(bool show_dot)
{
	bool hideIt = true;
	virtual_zap_mode = false;
	//bool fadeOut = false;
	timeoutEnd=0;;
	setInfobarTimeout();

	int res = messages_return::none;
	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	if (isVolscale)
		CVolume::getInstance()->showVolscale();

	while (!(res & (messages_return::cancel_info | messages_return::cancel_all))) {
		g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd);

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
		}
		else if (msg == CRCInput::RC_help || msg == CRCInput::RC_info) {
			g_RCInput->postMsg (NeutrinoMessages::SHOW_EPG, 0);
			res = messages_return::cancel_info;
		} else if ((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetFadeTimer())) {
			if(fader.FadeDone())
				res = messages_return::cancel_info;
		} else if ((msg == CRCInput::RC_ok) || (msg == CRCInput::RC_home) || (msg == CRCInput::RC_timeout)) {
			if(fader.StartFadeOut())
				timeoutEnd = CRCInput::calcTimeoutEnd (1);
			else
				res = messages_return::cancel_info;
		} else if ((msg == NeutrinoMessages::EVT_TIMER) && (data == sec_timer_id)) {
			showSNR ();
			paintTime (show_dot);
			showRecordIcon (show_dot);
			show_dot = !show_dot;
			showInfoFile();
			if ((g_settings.radiotext_enable) && (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_radio))
				showRadiotext();

			infoViewerBB->showIcon_16_9();
			//infoViewerBB->showIcon_CA_Status(0);
			infoViewerBB->showIcon_Resolution();
		} else if ((g_settings.mode_left_right_key_tv == SNeutrinoSettings::VZAP) && ((msg == CRCInput::RC_right) || (msg == CRCInput::RC_left ))) {
			virtual_zap_mode = true;
			res = messages_return::cancel_all;
			hideIt = true;
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
			printf("%s:%d msg:%08lx, data: %08lx\n", __func__, __LINE__, (long)msg, (long)data);
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
	if (virtual_zap_mode) {
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
	InfoClock->enableInfoClock(true);
}

void CInfoViewer::showRadiotext()
{
	char stext[3][100];
	bool RTisIsUTF = false;

	if (g_Radiotext == NULL) return;
	infoViewerBB->showIcon_RadioText(g_Radiotext->haveRadiotext());

	if (g_Radiotext->S_RtOsd) {
		InfoClock->enableInfoClock(false);
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

}

int CInfoViewer::handleMsg (const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if ((msg == NeutrinoMessages::EVT_CURRENTNEXT_EPG) || (msg == NeutrinoMessages::EVT_NEXTPROGRAM)) {
//printf("CInfoViewer::handleMsg: NeutrinoMessages::EVT_CURRENTNEXT_EPG data %llx current %llx\n", *(t_channel_id *) data, channel_id & 0xFFFFFFFFFFFFULL);
		if ((*(t_channel_id *) data) == (channel_id & 0xFFFFFFFFFFFFULL)) {
			getEPG (*(t_channel_id *) data, info_CurrentNext);
			if (is_visible)
				show_Data (true);
			showLcdPercentOver ();
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_GOTPIDS) {
		if ((*(t_channel_id *) data) == channel_id) {
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
		channel_id = (*(t_channel_id *)data);
		killInfobarText();
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
					display_Info(NULL, NULL, true, false, CMoviePlayerGui::getInstance().file_prozent, NULL, runningRest);
				} else if (!IS_WEBTV(channel_id)) {
					show_Data( true );
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
		if ((*(t_channel_id *) data) == channel_id) {
			if (is_visible && showButtonBar)
				infoViewerBB->showBBButtons(CInfoViewerBB::BUTTON_AUDIO);
			if (g_settings.radiotext_enable && g_Radiotext && ((CNeutrinoApp::getInstance()->getMode()) == NeutrinoMessages::mode_radio))
				g_Radiotext->setPid(g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].pid);
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES) {
		if ((*(t_channel_id *) data) == channel_id) {
			if (is_visible && showButtonBar)
				infoViewerBB->showBBButtons(CInfoViewerBB::BUTTON_SUBS);
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_SUB_COMPLETE) {
		//chanready = 1;
		showSNR ();
		//if ((*(t_channel_id *)data) == channel_id)
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
		CVFD::getInstance ()->showServicename ("(" + g_RemoteControl->getCurrentChannelName () + ')');
		printf ("zap failed!\n");
		showFailure ();
		CVFD::getInstance ()->showPercentOver (255);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_FAILED) {
		//chanready = 1;
		showSNR ();
		if ((*(t_channel_id *) data) == channel_id) {
			// show failure..!
			CVFD::getInstance ()->showServicename ("(" + g_RemoteControl->getCurrentChannelName () + ')');
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
		aspectRatio = data;
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
	if (!virtual_zap_mode) {
		char *p = new char[sizeof(t_channel_id)];
		memcpy(p, &for_channel_id, sizeof(t_channel_id));
		g_RCInput->postMsg (NeutrinoMessages::EVT_NOEPG_YET, (const neutrino_msg_data_t) p, false);
	}
}

CSectionsdClient::CurrentNextInfo CInfoViewer::getEPG (const t_channel_id for_channel_id, CSectionsdClient::CurrentNextInfo &info)
{
	/* to clear the oldinfo for channels without epg, call getEPG() with for_channel_id = 0 */
	if (for_channel_id == 0 || IS_WEBTV(for_channel_id))
	{
		oldinfo.current_uniqueKey = 0;
		return info;
	}


	CEitManager::getInstance()->getCurrentNextServiceKey(for_channel_id, info);

//printf("CInfoViewer::getEPG: old uniqueKey %llx new %llx\n", oldinfo.current_uniqueKey, info.current_uniqueKey);

	/* of there is no EPG, send an event so that parental lock can work */
	if (info.current_uniqueKey == 0 && info.next_uniqueKey == 0) {
		sendNoEpg(for_channel_id);
		oldinfo = info;
		return info;
	}

	if (info.current_uniqueKey != oldinfo.current_uniqueKey || info.next_uniqueKey != oldinfo.next_uniqueKey) {
		if (info.flags & (CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::has_next)) {
			char *_info = new char[sizeof(CSectionsdClient::CurrentNextInfo)];
			memcpy(_info, &info, sizeof(CSectionsdClient::CurrentNextInfo));
			neutrino_msg_t msg;
			if (info.flags & CSectionsdClient::epgflags::has_current)
				msg = NeutrinoMessages::EVT_CURRENTEPG;
			else
				msg = NeutrinoMessages::EVT_NEXTEPG;
			g_RCInput->postMsg(msg, (unsigned) _info, false );
		} else {
			sendNoEpg(for_channel_id);
		}
		oldinfo = info;
	}

	return info;
}

void CInfoViewer::showSNR ()
{
	if (! is_visible)
		return;
	/* right now, infobar_show_channellogo == 3 is the trigger for signal bars etc.
	   TODO: decouple this  */
	if (!fileplay && !IS_WEBTV(channel_id) && ( g_settings.infobar_show_channellogo == 3 || g_settings.infobar_show_channellogo == 5 || g_settings.infobar_show_channellogo == 6 )) {
		int chanH = g_SignalFont->getHeight();
		int freqStartY = BoxStartY + 2 * chanH - 3;
		if ((newfreq && chanready) || SDT_freq_update) {
			char freq[20];
			newfreq = false;

			std::string polarisation = "";
			
			if (CFrontend::isSat(CFEManager::getInstance()->getLiveFE()->getCurrentDeliverySystem()))
				polarisation = transponder::pol(CFEManager::getInstance()->getLiveFE()->getPolarization());

			int frequency = CFEManager::getInstance()->getLiveFE()->getFrequency();
			snprintf (freq, sizeof(freq), "%d.%d MHz %s", frequency / 1000, frequency % 1000, polarisation.c_str());

			int satNameWidth = g_SignalFont->getRenderWidth (freq);
			g_SignalFont->RenderString (3 + BoxStartX + ((ChanWidth - satNameWidth) / 2), BoxStartY + 2 * chanH - 3, satNameWidth, freq, SDT_freq_update ? COL_COLORED_EVENTS_TEXT:COL_INFOBAR_TEXT);
			SDT_freq_update = false;
		}

		char percent[10];
		uint16_t ssig, ssnr;
		int sw, snr, sig, posx, posy;

		int height;
		ssig = CFEManager::getInstance()->getLiveFE()->getSignalStrength();
		ssnr = CFEManager::getInstance()->getLiveFE()->getSignalNoiseRatio();

		sig = (ssig & 0xFFFF) * 100 / 65535;
		snr = (ssnr & 0xFFFF) * 100 / 65535;
		height = g_SignalFont->getHeight () - 1;

		if (lastsig != sig) {
			lastsig = sig;
			posx = BoxStartX + (ChanWidth - (bar_width + 2 + (g_SignalFont->getWidth() * 4))) / 2;
			posy = freqStartY;
			sigscale->setDimensionsAll(posx, posy+4, bar_width, 10 * g_settings.screen_yres / 100);
			sigscale->setColorBody(COL_INFOBAR_PLUS_0);
			sigscale->setValues(sig, 100);
			sigscale->paint();
			snprintf (percent, sizeof(percent), "%d%%S", sig);
			posx = posx + bar_width + 2;
			sw = BoxStartX + ChanWidth - posx;
			frameBuffer->paintBoxRel (posx, posy, sw, height, COL_INFOBAR_PLUS_0);
			g_SignalFont->RenderString (posx, posy + height, sw, percent, COL_INFOBAR_TEXT);
		}
		if (lastsnr != snr) {
			lastsnr = snr;
			posx = BoxStartX + (ChanWidth - (bar_width + 2 + (g_SignalFont->getWidth() * 4))) / 2;
			posy = freqStartY + height - (2 * g_settings.screen_yres / 100);
			snrscale->setDimensionsAll(posx, posy+4, bar_width, 10 * g_settings.screen_yres / 100);
			snrscale->setColorBody(COL_INFOBAR_PLUS_0);
			snrscale->setValues(snr, 100);
			snrscale->paint();
			snprintf (percent, sizeof(percent), "%d%%Q", snr);
			posx = posx + bar_width + 2;
			sw = BoxStartX + ChanWidth - posx -4;
			frameBuffer->paintBoxRel (posx, posy, sw, height-2, COL_INFOBAR_PLUS_0);
			g_SignalFont->RenderString (posx, posy + height, sw, percent, COL_INFOBAR_TEXT);
		}
	}
	if(showButtonBar)
		infoViewerBB->showSysfsHdd();
}

void CInfoViewer::display_Info(const char *current, const char *next,
			       bool UTF8, bool starttimes, const int pb_pos,
			       const char *runningStart, const char *runningRest,
			       const char *nextStart, const char *nextDuration,
			       bool update_current, bool update_next)
{
	/* dimensions of the two-line current-next "box":
	   top of box    == ChanNameY + time_height (bottom of channel name)
	   bottom of box == BoxEndY
	   height of box == BoxEndY - (ChanNameY + time_height)
	   middle of box == top + height / 2
			 == ChanNameY + time_height + (BoxEndY - (ChanNameY + time_height))/2
			 == ChanNameY + time_height + (BoxEndY - ChanNameY - time_height)/2
			 == ChanNameY / 2 + time_height / 2 + BoxEndY / 2
			 == (BoxEndY + ChanNameY + time_height)/2
	   The bottom of current info and the top of next info is == middle of box.
	 */

	int height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight();
	int CurrInfoY = (BoxEndY + ChanNameY + time_height) / 2;
	int NextInfoY = CurrInfoY + height;	// lower end of next info box
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
		timescale->setShadowOnOff(true);
		int pb_color = (g_settings.progressbar_design == CProgressBar::PB_MONO) ? COL_INFOBAR_PLUS_0 : COL_INFOBAR_SHADOW_PLUS_0;
		if(g_settings.infobar_progressbar){
			pb_startx = xStart;
			pb_w = BoxEndX - 10 - xStart;
			pb_shadow = 0;
			timescale->setShadowOnOff(false);
		}
		int tmpY = CurrInfoY - height - ChanNameY + time_height - 
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getDigitOffset()/3;
		switch(g_settings.infobar_progressbar) //set progressbar position
		{
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
		timescale->paint();
		//printf("paintProgressBar(%d, %d, %d, %d)\n", BoxEndX - pb_w - SHADOW_OFFSET, ChanNameY - (pb_h + 10) , pb_w, pb_h);
	}

	int currTimeW = 0;
	int nextTimeW = 0;
	if (runningRest != NULL)
		currTimeW = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(runningRest, UTF8);
	if (nextDuration != NULL)
		nextTimeW = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(nextDuration, UTF8);
	int currTimeX = BoxEndX - currTimeW - 10;
	int nextTimeX = BoxEndX - nextTimeW - 10;
	static int oldCurrTimeX = currTimeX; // remember the last pos. of remaining time, in case we change from 20/100min to 21/99min

	//colored_events init
	bool colored_event_C = false;
	bool colored_event_N = false;
	if (g_settings.colored_events_infobar == 1)
		colored_event_C = true;
	if (g_settings.colored_events_infobar == 2)
		colored_event_N = true;

	if (current != NULL && update_current)
	{
		frameBuffer->paintBox(InfoX, CurrInfoY - height, currTimeX, CurrInfoY, COL_INFOBAR_PLUS_0);
		if (runningStart != NULL)
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(InfoX, CurrInfoY, info_time_width, runningStart, colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT, 0, UTF8);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(xStart, CurrInfoY, currTimeX - xStart - 5, current, colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT, 0, UTF8);
		oldCurrTimeX = currTimeX;
	}

	if (currTimeX < oldCurrTimeX)
		oldCurrTimeX = currTimeX;
	frameBuffer->paintBox(oldCurrTimeX, CurrInfoY-height, BoxEndX, CurrInfoY, COL_INFOBAR_PLUS_0);
	oldCurrTimeX = currTimeX;
	if (currTimeW != 0)
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(currTimeX, CurrInfoY, currTimeW, runningRest, colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT, 0, UTF8);

	if (next != NULL && update_next)
	{
		frameBuffer->paintBox(InfoX, NextInfoY-height, BoxEndX, NextInfoY, COL_INFOBAR_PLUS_0);
		if (nextStart != NULL)
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(InfoX, NextInfoY, info_time_width, nextStart, colored_event_N ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT, 0, UTF8);
		if (starttimes)
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(xStart, NextInfoY, nextTimeX - xStart - 5, next, colored_event_N ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT, 0, UTF8);
		else
			g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->RenderString(xStart, NextInfoY, nextTimeX - xStart - 5, next, colored_event_N ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT, 0, UTF8);
		if (nextTimeW != 0)
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(nextTimeX, NextInfoY, nextTimeW, nextDuration, colored_event_N ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT, 0, UTF8);
	}
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

	if ((g_RemoteControl->current_channel_id == channel_id) && (!g_RemoteControl->subChannels.empty()) && (!g_RemoteControl->are_subchannels)) {
		is_nvod = true;
		info_CurrentNext.current_zeit.startzeit = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].startzeit;
		info_CurrentNext.current_zeit.dauer = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].dauer;
	} else {
		if ((info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) && (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) && (showButtonBar)) {
			if ((uint) info_CurrentNext.next_zeit.startzeit < (info_CurrentNext.current_zeit.startzeit + info_CurrentNext.current_zeit.dauer)) {
				is_nvod = true;
			}
		}
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
		infoViewerBB->showBBButtons(CInfoViewerBB::BUTTON_EPG);
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
		sendNoEpg(channel_id);
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
		if (info_CurrentNext.current_uniqueKey != last_curr_id)
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
	display_Info(current, next, true, true, runningPercent,
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
			infobar_txt->hide();
		delete infobar_txt;
	}
	infobar_txt = NULL;
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
	if (infobar_txt == NULL)
		infobar_txt = new CComponentsInfoBox();

	//get text from file and set it to info object, exit and delete object if failed
	if (!infobar_txt->setTextFromFile(infobar_file, CTextBox::CENTER, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO])){
		killInfobarText();
		return;
	}

	//set some properties for info object
	infobar_txt->setDimensionsAll(xStart, yStart, width, height);
	infobar_txt->setCorner(RADIUS_SMALL);
	infobar_txt->setShadowOnOff(true);
	infobar_txt->setTextColor(COL_INFOBAR_TEXT);
	infobar_txt->setColorBody(COL_INFOBAR_PLUS_0);
	infobar_txt->doPaintTextBoxBg(false);

	//paint info, don't save background, if already painted, global hide is also done by killTitle()
	bool save_bg = !infobar_txt->isPainted();
	if (infobar_txt->textChanged() || virtual_zap_mode)
		infobar_txt->paint(save_bg);

}

void CInfoViewer::killTitle()
{
	if (is_visible)
	{
		is_visible = false;
		infoViewerBB->is_visible = false;
		int bottom = BoxEndY + SHADOW_OFFSET + infoViewerBB->bottom_bar_offset;
		if (showButtonBar)
			bottom += infoViewerBB->InfoHeightY_Info;
		//printf("killTitle(%d, %d, %d, %d)\n", BoxStartX, BoxStartY, BoxEndX+ SHADOW_OFFSET-BoxStartX, bottom-BoxStartY);
		frameBuffer->paintBackgroundBox(BoxStartX, BoxStartY, BoxEndX+ SHADOW_OFFSET, bottom);
		if (g_settings.radiotext_enable && g_Radiotext) {
			g_Radiotext->S_RtOsd = g_Radiotext->haveRadiotext() ? 1 : 0;
			killRadiotext();
		}

		killInfobarText();
	}
	showButtonBar = false;
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
	int chan_w = BoxEndX- (start_x+ 20)- time_width- 15;

	bool logo_available = g_PicViewer->GetLogoName(logo_channel_id, ChannelName, strAbsIconPath, &logo_w, &logo_h);

	fprintf(stderr, "%s: logo_available: %d file: %s\n", __FUNCTION__, logo_available, strAbsIconPath.c_str());

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
		y_mid = BoxStartY + (satNameHeight + ChanHeight) / 2;

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
		g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, chan_w, time_height);
		// hide channel name
// this is too ugly...		ChannelName = "";
		// calculate logo position
		y_mid = ChanNameY + time_height / 2;
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
		g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, Logo_max_width, time_height);
		// calculate logo position
		y_mid = ChanNameY + time_height / 2;
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
			info_CurrentNext = getEPG (channel_id);
		}
#endif
		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) {
			if (jetzt < info_CurrentNext.current_zeit.startzeit)
				runningPercent = 0;
			else
				runningPercent = MIN ((unsigned) ((float) (jetzt - info_CurrentNext.current_zeit.startzeit) / (float) info_CurrentNext.current_zeit.dauer * 100.), 100);
		}
		CVFD::getInstance ()->showPercentOver (runningPercent);
	}
}

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
