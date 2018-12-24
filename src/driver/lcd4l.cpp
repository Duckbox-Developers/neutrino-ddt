/*
	lcd4l - Neutrino-GUI

	Copyright (C) 2012 'defans'
	Homepage: http://www.bluepeercrew.us/

	Copyright (C) 2012-2016 'vanhofen'
	Homepage: http://www.neutrino-images.de/

	Modded    (C) 2016 'TangoCash'

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

#include <pthread.h>
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <iomanip>

#include <global.h>
#include <neutrino.h>

#include <timerdclient/timerdclient.h>
#include <system/helpers.h>

#include <driver/record.h>
#include <driver/audioplay.h>
#include <driver/radiotext.h>
#include <zapit/capmt.h>
#include <zapit/zapit.h>
#include <gui/movieplayer.h>
#include <eitd/sectionsd.h>
#include <hardware/video.h>

#include "lcd4l.h"

extern CRemoteControl *g_RemoteControl;
extern cVideo *videoDecoder;

#define LCD_DATADIR		"/tmp/lcd/"

#define LCD_ICONSDIR		DATADIR "/lcd/icons/"
#define ICONSEXT		".png"

#define LOGO_DUMMY		LCD_ICONSDIR "blank.png"

#define BRIGHTNESS		LCD_DATADIR "brightness"
#define BRIGHTNESS_STANDBY	LCD_DATADIR "brightness_standby"
#define RESOLUTION		LCD_DATADIR "resolution"
#define ASPECTRATIO		LCD_DATADIR "aspectratio"
#define VIDEOTEXT		LCD_DATADIR "videotext"
#define RADIOTEXT		LCD_DATADIR "radiotext"
#define DOLBYDIGITAL		LCD_DATADIR "dolbydigital"
#define TUNER			LCD_DATADIR "tuner"
#define VOLUME			LCD_DATADIR "volume"
#define MODE_REC		LCD_DATADIR "mode_rec"
#define MODE_REC_ICON		LCD_DATADIR "mode_rec_icon"
#define MODE_TSHIFT		LCD_DATADIR "mode_tshift"
#define MODE_TIMER		LCD_DATADIR "mode_timer"
#define MODE_ECM		LCD_DATADIR "mode_ecm"

#define SERVICE			LCD_DATADIR "service"
#define CHANNELNR		LCD_DATADIR "channelnr"
#define LOGO			LCD_DATADIR "logo"
#define MODE_LOGO		LCD_DATADIR "mode_logo"
#define LAYOUT			LCD_DATADIR "layout"

#define EVENT			LCD_DATADIR "event"
#define INFO1			LCD_DATADIR "info1"
#define INFO2			LCD_DATADIR "info2"
#define PROGRESS		LCD_DATADIR "progress"
#define DURATION		LCD_DATADIR "duration"
#define START			LCD_DATADIR "start"
#define END			LCD_DATADIR "end"

#define LCD_FONT			LCD_DATADIR "font"
#define FGCOLOR			LCD_DATADIR "fgcolor"
#define BGCOLOR			LCD_DATADIR "bgcolor"

#define FLAG_LCD4LINUX		"/tmp/.lcd4linux"
#define PIDFILE			"/tmp/lcd4linux.pid"

static void lcd4linux(bool run)
{
	const char *buf = "lcd4linux";
	if (run == true)
	{
		if (my_system(2, "/etc/init.d/lcd4linux", "start") != 0)
			printf("[CLCD4l] %s: executing '%s' failed\n", __FUNCTION__, buf);
		sleep(2);
	}
	else
	{
		if (my_system(2, "/etc/init.d/lcd4linux", "stop") != 0)
			printf("[CLCD4l] %s: terminating '%s' failed\n", __FUNCTION__, buf);
	}
}

/* ----------------------------------------------------------------- */

CLCD4l::CLCD4l()
{
	thrLCD4l = 0;
}

CLCD4l::~CLCD4l()
{
	if (thrLCD4l)
		pthread_cancel(thrLCD4l);
	thrLCD4l = 0;
}

/* ----------------------------------------------------------------- */

void CLCD4l::InitLCD4l()
{
	if (thrLCD4l)
	{
		printf("[CLCD4l] %s: initializing\n", __FUNCTION__);
		Init();
	}
}

void CLCD4l::StartLCD4l()
{
	if (!thrLCD4l && (g_settings.lcd4l_support == 1 || g_settings.lcd4l_support == 2))
	{
		printf("[CLCD4l] %s: starting thread\n", __FUNCTION__);
		pthread_create(&thrLCD4l, NULL, LCD4lProc, (void*) this);
		pthread_detach(thrLCD4l);
		lcd4linux(true);
	}
}

void CLCD4l::StopLCD4l()
{
	if (thrLCD4l)
	{
		printf("[CLCD4l] %s: stopping thread\n", __FUNCTION__);
		pthread_cancel(thrLCD4l);
		thrLCD4l = 0;
		lcd4linux(false);
	}
}

void CLCD4l::SwitchLCD4l()
{
	if (thrLCD4l)
		StopLCD4l();
	else
		StartLCD4l();
}

int CLCD4l::CreateFile(const char *file, std::string content, bool convert)
{
	// returns 0 = ok; 1 = can't create file; -1 = thread not found

	int ret = 0;

	if (thrLCD4l)
	{
		if (WriteFile(file, content, convert) == false)
			ret = 1;
	}
	else
		ret = -1;

	return ret;
}

int CLCD4l::RemoveFile(const char *file)
{
	// returns 0 = ok; 1 = can't remove file;

	int ret = 0;

	if (access(file, F_OK) == 0)
	{
		if (unlink(file) != 0)
			ret = 1;
	}

	return ret;
}

/* ----------------------------------------------------------------- */

void CLCD4l::Init()
{
	m_ParseID	= 0;

	m_Brightness	= -1;
	m_Brightness_standby = -1;
	m_Resolution	= "n/a";
	m_AspectRatio	= "n/a";
	m_Videotext	= -1;
	m_Radiotext	= -1;
	m_DolbyDigital	= "n/a";
	m_Tuner		= -1;
	m_Volume	= -1;
	m_ModeRec	= -1;
	m_ModeTshift	= -1;
	m_ModeTimer	= -1;
	m_ModeEcm	= -1;

	m_Service	= "n/a";
	m_ChannelNr	= -1;
	m_Logo		= "n/a";
	m_ModeLogo	= -1;

	m_Layout	= "n/a";

	m_Event		= "n/a";
	m_Info1		= "n/a";
	m_Info2		= "n/a";
	m_Progress	= -1;
	for (int i = 0; i < (int)sizeof(m_Duration); i++)
		m_Duration[i] = ' ';
	m_Start		= "00:00";
	m_End		= "00:00";

	if (!access(LCD_DATADIR, F_OK) == 0)
		mkdir(LCD_DATADIR, 0755);
}

void* CLCD4l::LCD4lProc(void* arg)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	CLCD4l *PLCD4l = static_cast<CLCD4l*>(arg);

	PLCD4l->Init();

	sleep(5); //please wait !

	static bool FirstRun = true;
	uint64_t p_ParseID = 0;
	bool NewParseID = false;

	//printf("[CLCD4l] %s: starting loop\n", __FUNCTION__);
	while(1)
	{
		if ( (!access(PIDFILE, F_OK) == 0) && (!FirstRun) )
		{
			if (g_settings.lcd4l_support == 1) // automatic
			{
				//printf("[CLCD4l] %s: waiting for lcd4linux\n", __FUNCTION__);
				sleep(10);
				continue;
			}
		}

		for (int i = 0; i < 10; i++)
		{
			usleep(5 * 100 * 1000); // 0.5 sec
			NewParseID = PLCD4l->CompareParseID(p_ParseID);
			if (NewParseID || p_ParseID == NeutrinoModes::mode_audio)
				break;
		}

		//printf("[CLCD4l] %s: m_ParseID: %llx (NewParseID: %d)\n", __FUNCTION__, p_ParseID, NewParseID ? 1 : 0);
		PLCD4l->ParseInfo(p_ParseID, NewParseID, FirstRun);

		if (FirstRun)
		{
			PLCD4l->WriteFile(FLAG_LCD4LINUX);
			FirstRun = false;
		}
		if (g_settings.lcd4l_support == 0)
		{
			lcd4linux(false);
		}
		if (g_settings.lcd4l_support == 1 || g_settings.lcd4l_support == 2)
		{
			lcd4linux(true);
		}
	}
	return 0;
}

void CLCD4l::ParseInfo(uint64_t parseID, bool newID, bool firstRun)
{
	SNeutrinoTheme &t = g_settings.theme;

	std::string font = g_settings.font_file;

	if (m_font.compare(font))
	{
		WriteFile(LCD_FONT, font);
		m_font = font;
	}

	/* ----------------------------------------------------------------- */

	std::string fgcolor	= hexStr(&t.infobar_Text_red)
				+ hexStr(&t.infobar_Text_green)
				+ hexStr(&t.infobar_Text_blue)
				+ hexStr(&t.infobar_Text_alpha);

	if (m_fgcolor.compare(fgcolor))
	{
		WriteFile(FGCOLOR, fgcolor);
		m_fgcolor = fgcolor;
	}

	/* ----------------------------------------------------------------- */

	std::string bgcolor	= hexStr(&t.infobar_red)
				+ hexStr(&t.infobar_green)
				+ hexStr(&t.infobar_blue)
				+ hexStr(&t.infobar_alpha);

	if (m_bgcolor.compare(bgcolor))
	{
		WriteFile(BGCOLOR, bgcolor);
		m_bgcolor = bgcolor;
	}

	/* ----------------------------------------------------------------- */

	int Brightness = g_settings.lcd4l_brightness;
	if (m_Brightness != Brightness)
	{
		WriteFile(BRIGHTNESS, to_string(Brightness));
		m_Brightness = Brightness;
		lcd4linux(false);
		lcd4linux(true);
	}

	int Brightness_standby = g_settings.lcd4l_brightness_standby;
	if (m_Brightness_standby != Brightness_standby)
	{
		WriteFile(BRIGHTNESS_STANDBY, to_string(Brightness_standby));
		m_Brightness_standby = Brightness_standby;
		lcd4linux(false);
		lcd4linux(true);
	}

	/* ----------------------------------------------------------------- */

	int x_res, y_res, framerate;
	videoDecoder->getPictureInfo(x_res, y_res, framerate);

	if (y_res == 1088)
		y_res = 1080;

	std::string Resolution = to_string(x_res) + "x" + to_string(y_res);
	//Resolution += "\n" + to_string(framerate); //TODO

	if (m_Resolution.compare(Resolution))
	{
		WriteFile(RESOLUTION, Resolution);
		m_Resolution = Resolution;
	}

	/* ----------------------------------------------------------------- */

	std::string AspectRatio;
	switch (videoDecoder->getAspectRatio())
	{
		case 0:
			AspectRatio = "n/a";
			break;
		case 1:
			AspectRatio = "4:3";
			break;
		case 2:
			AspectRatio = "14:9";
			break;
		case 3:
			AspectRatio = "16:9";
			break;
		case 4:
			AspectRatio = "20:9";
			break;
		default:
			AspectRatio = "n/k";
			break;
	}

	if (m_AspectRatio.compare(AspectRatio))
	{
		WriteFile(ASPECTRATIO, AspectRatio);
		m_AspectRatio = AspectRatio;
	}

	/* ----------------------------------------------------------------- */

	int Videotext = g_RemoteControl->current_PIDs.PIDs.vtxtpid;

	if (m_Videotext != Videotext)
	{
		WriteFile(VIDEOTEXT, Videotext ? "yes" : "no");
		m_Videotext = Videotext;
	}

	/* ----------------------------------------------------------------- */

	int Radiotext = 0;
	if (m_Mode == NeutrinoModes::mode_radio && g_settings.radiotext_enable && g_Radiotext)
		Radiotext = g_Radiotext->haveRadiotext();

	if (m_Radiotext != Radiotext)
	{
		WriteFile(RADIOTEXT, Radiotext ? "yes" : "no");
		m_Radiotext = Radiotext;
	}

	/* ----------------------------------------------------------------- */

	std::string DolbyDigital;
	if ((g_RemoteControl->current_PIDs.PIDs.selected_apid < g_RemoteControl->current_PIDs.APIDs.size()) &&
	    (g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].is_ac3))
		DolbyDigital = "yes";
	else
		DolbyDigital = g_RemoteControl->has_ac3 ? "available" : "no";

	if (m_DolbyDigital.compare(DolbyDigital))
	{
		WriteFile(DOLBYDIGITAL, DolbyDigital);
		m_DolbyDigital = DolbyDigital;
	}

	/* ----------------------------------------------------------------- */

	int Tuner = 1 + CFEManager::getInstance()->getLiveFE()->getNumber();

	if (m_Tuner != Tuner)
	{
		WriteFile(TUNER, to_string(Tuner));
		m_Tuner = Tuner;
	}

	/* ----------------------------------------------------------------- */

	int Volume = g_settings.current_volume;

	if (m_Volume != Volume)
	{
		WriteFile(VOLUME, to_string(Volume));
		m_Volume = Volume;
	}

	/* ----------------------------------------------------------------- */

	int ModeRec = 0;
	int ModeTshift = 0;

	int RecordMode = CRecordManager::getInstance()->GetRecordMode();
	switch (RecordMode)
	{
		case CRecordManager::RECMODE_REC_TSHIFT:
			ModeRec = 1;
			ModeTshift = 1;
			break;
		case CRecordManager::RECMODE_REC:
			ModeRec = 1;
			break;
		case CRecordManager::RECMODE_TSHIFT:
			ModeTshift = 1;
			break;
		default:
			break;
	}

	if (m_ModeRec != ModeRec)
	{
		WriteFile(MODE_REC, ModeRec ? "on" : "off");
		std::string rec_icon ="";
		if (ModeRec)
			rec_icon = ICONSDIR "/" NEUTRINO_ICON_REC ICONSEXT;
		else
			rec_icon = ICONSDIR "/" NEUTRINO_ICON_REC_GRAY ICONSEXT;
		WriteFile(MODE_REC_ICON, rec_icon);
		m_ModeRec = ModeRec;
	}

	if (m_ModeTshift != ModeTshift)
	{
		WriteFile(MODE_TSHIFT, ModeTshift ? "on" : "off");
		m_ModeTshift = ModeTshift;
	}

	/* ----------------------------------------------------------------- */

	int ModeTimer = 0;

	CTimerd::TimerList timerList;
	CTimerdClient TimerdClient;

	timerList.clear();
	TimerdClient.getTimerList(timerList);

	CTimerd::TimerList::iterator timer = timerList.begin();

	for (; timer != timerList.end(); timer++)
	{
		if (timer->alarmTime > time(NULL) && (timer->eventType == CTimerd::TIMER_ZAPTO || timer->eventType == CTimerd::TIMER_RECORD))
		{
			// Nur "true", wenn irgendein timer in der zukunft liegt
			// und dieser vom typ TIMER_ZAPTO oder TIMER_RECORD ist
			ModeTimer = 1;
			break;
		}
	}

	if (m_ModeTimer != ModeTimer)
	{
		WriteFile(MODE_TIMER, ModeTimer ? "on" : "off");
		m_ModeTimer = ModeTimer;
	}

	/* ----------------------------------------------------------------- */

	int ModeEcm = 0;

	if (access("/tmp/ecm.info", F_OK) == 0)
	{
		struct stat buf;
		stat("/tmp/ecm.info", &buf);
		if (buf.st_size > 0)
			ModeEcm = 1;
	}

	if (m_ModeEcm != ModeEcm)
	{
		WriteFile(MODE_ECM, ModeEcm ? "on" : "off");
		m_ModeEcm = ModeEcm;
	}

	/* ----------------------------------------------------------------- */

	if (newID || parseID == NeutrinoModes::mode_audio || parseID == NeutrinoModes::mode_ts)
	{
		std::string Service = "";
		int ChannelNr = 0;
		std::string Logo = LOGO_DUMMY;
		int ModeLogo = 0;

		int ModeStandby	= 0;

		if (m_ModeChannel)
		{
			if (m_ModeChannel > 1)
				Service = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].subservice_name;
			else
				Service = g_RemoteControl->getCurrentChannelName();

			GetLogoName(parseID, Service, Logo);

			ChannelNr = CNeutrinoApp::getInstance()->channelList->getActiveChannelNumber();
		}
		else if (parseID == NeutrinoModes::mode_audio)
		{
			const CAudioMetaData meta = CAudioPlayer::getInstance()->getMetaData();
			if ( (!meta.sc_station.empty()) && (CAudioPlayer::getInstance()->getState() != CBaseDec::STOP))
				Service = meta.sc_station;
			else
			{
				Service = g_Locale->getText(LOCALE_AUDIOPLAYER_NAME);

				switch (CAudioPlayer::getInstance()->getState())
				{
					case CBaseDec::REV:
						Logo = ICONSDIR "/" NEUTRINO_ICON_REW ICONSEXT;
						break;
					case CBaseDec::FF:
						Logo = ICONSDIR "/" NEUTRINO_ICON_FF ICONSEXT;
						break;
					case CBaseDec::PAUSE:
						Logo = ICONSDIR "/" NEUTRINO_ICON_PAUSE ICONSEXT;
						break;
					case CBaseDec::PLAY:
						Logo = ICONSDIR "/" NEUTRINO_ICON_PLAY ICONSEXT;
						break;
					default:
						;
				}
			}
		}
		else if (parseID == NeutrinoModes::mode_pic)
		{
			Service = g_Locale->getText(LOCALE_PICTUREVIEWER_HEAD);
		}
		else if (parseID == NeutrinoModes::mode_ts)
		{
			if (ModeTshift)
				Service = g_Locale->getText(LOCALE_RECORDINGMENU_TIMESHIFT);
			else if (CMoviePlayerGui::getInstance().p_movie_info)
			{
				if (!CMoviePlayerGui::getInstance().p_movie_info->channelName.empty())
					Service = CMoviePlayerGui::getInstance().p_movie_info->channelName;
			}

			if (Service.empty())
				Service = g_Locale->getText(LOCALE_MOVIEPLAYER_HEAD);

			switch (CMoviePlayerGui::getInstance().getState())
			{
				case 6: /* rewind */
					Logo = ICONSDIR "/" NEUTRINO_ICON_REW ICONSEXT;
					break;
				case 5: /* fast forward */
					Logo = ICONSDIR "/" NEUTRINO_ICON_FF ICONSEXT;
					break;
				case 4: /* pause */
					Logo = ICONSDIR "/" NEUTRINO_ICON_PAUSE ICONSEXT;
					break;
				case 3: /* play */
					if (ModeTshift && CMoviePlayerGui::getInstance().p_movie_info) /* show channel-logo */
					{
						if (!GetLogoName(CMoviePlayerGui::getInstance().p_movie_info->channelId,
								 CMoviePlayerGui::getInstance().p_movie_info->channelName,
								 Logo))
							Logo = ICONSDIR "/" NEUTRINO_ICON_PLAY ICONSEXT;
					}
					else /* show play-icon */
						Logo = ICONSDIR "/" NEUTRINO_ICON_PLAY ICONSEXT;
					break;
				default: /* show movieplayer-icon */
					Logo = ICONSDIR "/" NEUTRINO_ICON_MOVIEPLAYER ICONSEXT;
			}
		}
		else if (parseID == NeutrinoModes::mode_upnp)
		{
			Service = g_Locale->getText(LOCALE_UPNPBROWSER_HEAD);
		}
		else if (parseID == NeutrinoModes::mode_standby)
		{
			Service = "STANDBY";
			ModeStandby = 1;
		}

		/* --- */

		if (m_Service.compare(Service))
		{
			WriteFile(SERVICE, Service, true);
			m_Service = Service;
		}

		if (m_ChannelNr != ChannelNr)
		{
			WriteFile(CHANNELNR, to_string(ChannelNr));
			m_ChannelNr = ChannelNr;
		}

		if (m_Logo.compare(Logo))
		{
			WriteFile(LOGO, Logo);
			m_Logo = Logo;
		}

		if (Logo != LOGO_DUMMY)
			ModeLogo = 1;

		if (m_ModeLogo != ModeLogo)
		{
			WriteFile(MODE_LOGO, to_string(ModeLogo));
			m_ModeLogo = ModeLogo;
		}

		/* --- */

		std::string Layout;

		std::string DPF_Type;
		switch (g_settings.lcd4l_dpf_type) {
			case 3:
				DPF_Type = "PNG_";
				break;
#if defined BOXMODEL_VUSOLO4K
			case 2:
				DPF_Type = "VUSolo4K_";
				break;
#endif
			case 1:
				DPF_Type = "Samsung_";
				break;
			case 0:
			default:
				DPF_Type = "Pearl_";
				break;
		}

		if (ModeStandby)
		{
			Layout = DPF_Type + "standby";
		}
		else if ((g_settings.lcd4l_skin_radio) && (m_Mode == NeutrinoModes::mode_radio || m_Mode == NeutrinoModes::mode_webradio))
		{
			Layout = DPF_Type + "radio";
		}
		else
		{
			switch (g_settings.lcd4l_skin)
			{
				case 4:
					Layout = DPF_Type + "user04";
					break;
				case 3:
					Layout = DPF_Type + "user03";
					break;
				case 2:
					Layout = DPF_Type + "user02";
					break;
				case 1:
					Layout = DPF_Type + "user01";
					break;
				default:
					Layout = DPF_Type + "standard";
			}
		}

		if (m_Layout.compare(Layout))
		{
			WriteFile(LAYOUT, Layout);
			m_Layout = Layout;
			if (!firstRun)
			{
				lcd4linux(false);
				lcd4linux(true);
			}
		}
	}

	/* ----------------------------------------------------------------- */

	std::string Event = "";
	std::string Info1 = "";
	std::string Info2 = "";
	int Progress = 0;
	char Duration[sizeof(m_Duration)] = {0};
	char Start[6] = {0};
	char End[6] = {0};

	if (m_ModeChannel)
	{
		t_channel_id channel_id = parseID & 0xFFFFFFFFFFFFULL;

		CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
		if (channel)
			channel_id = channel->getEpgID();

		CSectionsdClient::CurrentNextInfo CurrentNext;
		CEitManager::getInstance()->getCurrentNextServiceKey(channel_id, CurrentNext);

		if (CSectionsdClient::epgflags::has_current)
		{
			if (!CurrentNext.current_name.empty())
				Event = CurrentNext.current_name;

			CShortEPGData shortEpgData;
			if (CEitManager::getInstance()->getEPGidShort(CurrentNext.current_uniqueKey, &shortEpgData))
			{
				Info1 = shortEpgData.info1;
				Info2 = shortEpgData.info2;
			}

			if ((CurrentNext.current_zeit.dauer > 0) && (CurrentNext.current_zeit.dauer < 86400))
			{
				Progress = 100 * (time(NULL) - CurrentNext.current_zeit.startzeit) / CurrentNext.current_zeit.dauer;

				int total = CurrentNext.current_zeit.dauer / 60;
				int done = (abs(time(NULL) - CurrentNext.current_zeit.startzeit) + 30) / 60;
				int todo = total - done;
				if ((time(NULL) < CurrentNext.current_zeit.startzeit) && todo >= 0)
				{
					done = 0;
					todo = CurrentNext.current_zeit.dauer / 60;
				}
				snprintf(Duration, sizeof(Duration), "%d/%d", done, total);
			}

			tm_struct = localtime(&CurrentNext.current_zeit.startzeit);
			snprintf(Start, sizeof(Start), "%02d:%02d", tm_struct->tm_hour, tm_struct->tm_min);
		}

		if (CSectionsdClient::epgflags::has_next)
		{
			Event += "\n" + CurrentNext.next_name;
			tm_struct = localtime(&CurrentNext.next_zeit.startzeit);
			snprintf(End, sizeof(End), "%02d:%02d", tm_struct->tm_hour, tm_struct->tm_min);
		}
	}
	else if (parseID == NeutrinoModes::mode_audio)
	{
		if (CAudioPlayer::getInstance()->getState() == CBaseDec::STOP)
		{
			Event = g_Locale->getText(LOCALE_AUDIOPLAYER_STOP);
			//snprintf(Duration, sizeof(Duration), "-:--");
		}
		else
		{
			const CAudioMetaData meta = CAudioPlayer::getInstance()->getMetaData();
			if ( !meta.artist.empty() )
				Event += meta.artist;
			if ( !meta.artist.empty() && !meta.title.empty() )
				Event += " - ";
			if ( !meta.title.empty() )
				Event += meta.title;

			if (!meta.album.empty())
				Info1 = meta.album;

			if (!meta.genre.empty())
				Info2 = meta.genre;

			time_t total = meta.total_time;
			time_t done = CAudioPlayer::getInstance()->getTimePlayed();

			if ( (total > 0) && (done > 0) )
			{
				Progress = 100 * done / total;
				snprintf(Duration, sizeof(Duration), "%ld:%02ld/%ld:%02ld", done / 60, done % 60, total / 60, total % 60);
			}
		}

		time_t sTime = time(NULL);
		tm_struct = localtime(&sTime);

		snprintf(Start, sizeof(Start), "%02d:%02d", tm_struct->tm_hour, tm_struct->tm_min);
	}
	else if (parseID == NeutrinoModes::mode_ts)
	{
		if (CMoviePlayerGui::getInstance().p_movie_info)
		{
			if (!CMoviePlayerGui::getInstance().p_movie_info->epgTitle.empty())
				Event = CMoviePlayerGui::getInstance().p_movie_info->epgTitle;

			if (!CMoviePlayerGui::getInstance().p_movie_info->epgInfo1.empty())
				Info1 = CMoviePlayerGui::getInstance().p_movie_info->epgInfo1;

			if (!CMoviePlayerGui::getInstance().p_movie_info->epgInfo2.empty())
				Info2 = CMoviePlayerGui::getInstance().p_movie_info->epgInfo2;
		}
		else if (!CMoviePlayerGui::getInstance().GetFile().empty())
			Event = CMoviePlayerGui::getInstance().GetFile();

		if (Event.empty())
			Event = "MOVIE";

		if (!ModeTshift)
		{
			int total = CMoviePlayerGui::getInstance().GetDuration();
			int done = CMoviePlayerGui::getInstance().GetPosition();
			snprintf(Duration, sizeof(Duration), "%d/%d", done / (60 * 1000), total / (60 * 1000));
			Progress = CMoviePlayerGui::getInstance().file_prozent;
		}

		time_t sTime = time(NULL);
		sTime -= (CMoviePlayerGui::getInstance().GetPosition()/1000);
		tm_struct = localtime(&sTime);

		snprintf(Start, sizeof(Start), "%02d:%02d", tm_struct->tm_hour, tm_struct->tm_min);

		time_t eTime = time(NULL);
		eTime +=(CMoviePlayerGui::getInstance().GetDuration()/1000) - (CMoviePlayerGui::getInstance().GetPosition()/1000);
		tm_struct = localtime(&eTime);

		snprintf(End, sizeof(End), "%02d:%02d", tm_struct->tm_hour, tm_struct->tm_min);
	}

	/* ----------------------------------------------------------------- */

	Event += "\n"; // make sure we have at least two lines in event-file

	if (m_Event.compare(Event))
	{
		WriteFile(EVENT, Event, g_settings.lcd4l_convert);
		m_Event = Event;
	}

	if (m_Info1.compare(Info1))
	{
		WriteFile(INFO1, Info1, g_settings.lcd4l_convert);
		m_Info1 = Info1;
	}

	if (m_Info2.compare(Info2))
	{
		WriteFile(INFO2, Info2, g_settings.lcd4l_convert);
		m_Info2 = Info2;
	}

	if (m_Start.compare(Start))
	{
		WriteFile(START, (std::string)Start);
		m_Start = (std::string)Start;
	}

	if (m_End.compare(End))
	{
		WriteFile(END, (std::string)End);
		m_End = (std::string)End;
	}

	if (Progress > 100)
		Progress = 100;

	if (m_Progress != Progress)
	{
		WriteFile(PROGRESS, to_string(Progress));
		m_Progress = Progress;
	}

	if (strcmp(m_Duration, Duration))
	{
		WriteFile(DURATION, (std::string)Duration);
		strcpy(m_Duration, Duration);
	}
}

/* ----------------------------------------------------------------- */

bool CLCD4l::WriteFile(const char *file, std::string content, bool convert)
{
	bool ret = true;

	if (convert) // align to internal lcd4linux font
	{
		strReplace(content, "ä", "\xe4\0");
		strReplace(content, "ö", "\xf6\0");
		strReplace(content, "ü", "\xfc\0");
		strReplace(content, "Ä", "\xc4\0");
		strReplace(content, "Ö", "\xd6\0");
		strReplace(content, "Ü", "\xdc\0");
		if (g_settings.lcd4l_dpf_type == 0) strReplace(content, "ß", "\xe2\0");
		strReplace(content, "é", "e");
	}

	if (FILE *f = fopen(file, "w"))
	{
		//printf("[CLCD4l] %s: %s -> %s\n", __FUNCTION__, content.c_str(), file);
		fprintf(f, "%s\n", content.c_str());
		fclose(f);
	}
	else
	{
		ret = false;
		printf("[CLCD4l] %s: %s failed!\n", __FUNCTION__, file);
	}

	return ret;
}

uint64_t CLCD4l::GetParseID()
{
	uint64_t ID = CNeutrinoApp::getInstance()->getMode();
	m_Mode = (int) ID;
	m_ModeChannel = 0;

	if (ID == NeutrinoModes::mode_tv || ID == NeutrinoModes::mode_webtv || ID == NeutrinoModes::mode_radio || ID == NeutrinoModes::mode_webradio)
	{
		if (!(g_RemoteControl->subChannels.empty()) && (g_RemoteControl->selected_subchannel > 0))
			m_ModeChannel = 2;
		else
			m_ModeChannel = 1;

		if (m_ModeChannel > 1)
			ID = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].getChannelID();
		else
			ID = CZapit::getInstance()->GetCurrentChannelID();
	}
	//printf("[CLCD4l] %s: %llx\n", __FUNCTION__, ID);
	return ID;
}

bool CLCD4l::CompareParseID(uint64_t &i_ParseID)
{
	bool ret = false;

	i_ParseID = GetParseID();
	if (m_ParseID != i_ParseID)
	{
		//printf("[CLCD4l] %s: i_%llx <-> m_%llx\n", __FUNCTION__, i_ParseID, m_ParseID);
		ret = true;
		m_ParseID = i_ParseID;
	}
	return ret;
}

void CLCD4l::strReplace(std::string &orig, const std::string &fstr, const std::string &rstr)
{
	size_t pos = 0;
	while ((pos = orig.find(fstr, pos)) != std::string::npos)
	{
		orig.replace(pos, fstr.length(), rstr);
		pos += rstr.length();
	}
}

std::string CLCD4l::hexStr(unsigned char* data)
{
	std::stringstream ss;
	ss << std::hex;
	for(int i=0; i<1; ++i)
		ss << std::setw(2) << std::setfill('0') << (int)data[i];
	return ss.str();
}

bool CLCD4l::GetLogoName(uint64_t channel_id, std::string channel_name, std::string &logo)
{
	int h, i, j;
	char str_channel_id[16];
	char *upper_name, *lower_name, *p;

	upper_name = strdup(channel_name.c_str());
	for (p = upper_name; *p != '\0'; p++)
		*p = (char) toupper(*p);

	lower_name = strdup(channel_name.c_str());
	for (p = lower_name; *p != '\0'; p++)
		*p = (char) tolower(*p);

	sprintf(str_channel_id, "%llx", channel_id & 0xFFFFFFFFFFFFULL);
	// the directorys to search in
	std::string strLogoDir[4] = { g_settings.lcd4l_logodir, LOGODIR_VAR, LOGODIR, g_settings.logo_hdd_dir };
	// first the channelname, then the upper channelname, then the lower channelname, then the channel-id
	std::string strLogoName[4] = { channel_name, (std::string)upper_name, (std::string)lower_name, (std::string)str_channel_id };
	// first png, then jpg, then gif
	std::string strLogoExt[3] = { ".png", ".jpg", ".gif" };

	//printf("[CLCD4l] %s: ID: %s, Name: %s (u: %s, l: %s)\n", __FUNCTION__, str_channel_id, channel_name.c_str(), upper_name, lower_name);

	for (h = 0; h < 4; h++)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = 0; j < 3; j++)
			{
				std::string tmp(strLogoDir[h] + "/" + strLogoName[i] + strLogoExt[j]);
				if (access(tmp.c_str(), R_OK) != -1)
				{
					logo = tmp;
					return true;
				}
			}
		}
	}

	return false;
}
