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

#include <system/setting_helpers.h>
#include "configure_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <libnet.h>
#include <linux/if.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#include <config.h>

#include <global.h>
#include <neutrino.h>
#include <gui/widget/stringinput.h>
#include <gui/infoclock.h>
#include <gui/infoviewer.h>
#include <gui/movieplayer.h>
#include <driver/display.h>
#include <driver/volume.h>
#include <system/helpers.h>

#include <gui/widget/msgbox.h>
#include <gui/widget/hintbox.h>

#include <gui/plugins.h>
#include <daemonc/remotecontrol.h>
#include <xmlinterface.h>
#include <hardware/audio.h>
#include <hardware/video.h>
#include <hardware/dmx.h>
#include <pwrmngr.h>
#include <libdvbsub/dvbsub.h>
#include <libtuxtxt/teletext.h>
#include <zapit/satconfig.h>
#include <zapit/zapit.h>

extern CPlugins        *g_Plugins;    /* neutrino.cpp */
extern CRemoteControl *g_RemoteControl;  /* neutrino.cpp */
extern cVideo *videoDecoder;
extern cAudio *audioDecoder;

extern cDemux *videoDemux;
extern cDemux *audioDemux;
extern cDemux *pcrDemux;

extern "C" int pinghost(const char *hostname);

COnOffNotifier::COnOffNotifier(int OffValue)
{
	offValue = OffValue;
}

bool COnOffNotifier::changeNotify(const neutrino_locale_t, void *Data)
{
	bool active = (*(int *)(Data) != offValue);

	for (std::vector<CMenuItem *>::iterator it = toDisable.begin(); it != toDisable.end(); it++)
		(*it)->setActive(active);

	return false;
}

void COnOffNotifier::addItem(CMenuItem *menuItem)
{
	toDisable.push_back(menuItem);
}

bool CSectionsdConfigNotifier::changeNotify(const neutrino_locale_t locale, void *data)
{
	char *str = (char *) data;
	if (locale == LOCALE_MISCSETTINGS_EPG_CACHE)
		g_settings.epg_cache = atoi(str);
	else if (locale == LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE)
		g_settings.epg_extendedcache = atoi(str);
	else if (locale == LOCALE_MISCSETTINGS_EPG_OLD_EVENTS)
		g_settings.epg_old_events = atoi(str);
	else if (locale == LOCALE_MISCSETTINGS_EPG_MAX_EVENTS)
		g_settings.epg_max_events = atoi(str);

	CNeutrinoApp::getInstance()->SendSectionsdConfig();
	return false;
}

bool CTouchFileNotifier::changeNotify(const neutrino_locale_t, void *data)
{
	if ((*(int *)data) != 0)
	{
		FILE *fd = fopen(filename, "w");
		if (fd)
			fclose(fd);
		else
			return false;
	}
	else
		remove(filename);
	return true;
}

void CColorSetupNotifier::setPalette()
{
	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();
	SNeutrinoTheme &t = g_settings.theme;
	//setting colors-..
	frameBuffer->paletteGenFade(COL_MENUHEAD,
		convertSetupColor2RGB(t.menu_Head_red, t.menu_Head_green, t.menu_Head_blue),
		convertSetupColor2RGB(t.menu_Head_Text_red, t.menu_Head_Text_green, t.menu_Head_Text_blue),
		8, convertSetupAlpha2Alpha(t.menu_Head_alpha));

	frameBuffer->paletteGenFade(COL_MENUCONTENT,
		convertSetupColor2RGB(t.menu_Content_red, t.menu_Content_green, t.menu_Content_blue),
		convertSetupColor2RGB(t.menu_Content_Text_red, t.menu_Content_Text_green, t.menu_Content_Text_blue),
		8, convertSetupAlpha2Alpha(t.menu_Content_alpha));


	frameBuffer->paletteGenFade(COL_MENUCONTENTDARK,
		convertSetupColor2RGB(int(t.menu_Content_red * 0.6), int(t.menu_Content_green * 0.6), int(t.menu_Content_blue * 0.6)),
		convertSetupColor2RGB(t.menu_Content_Text_red, t.menu_Content_Text_green, t.menu_Content_Text_blue),
		8, convertSetupAlpha2Alpha(t.menu_Content_alpha));

	frameBuffer->paletteGenFade(COL_MENUCONTENTSELECTED,
		convertSetupColor2RGB(t.menu_Content_Selected_red, t.menu_Content_Selected_green, t.menu_Content_Selected_blue),
		convertSetupColor2RGB(t.menu_Content_Selected_Text_red, t.menu_Content_Selected_Text_green, t.menu_Content_Selected_Text_blue),
		8, convertSetupAlpha2Alpha(t.menu_Content_Selected_alpha));

	frameBuffer->paletteGenFade(COL_MENUCONTENTINACTIVE,
		convertSetupColor2RGB(t.menu_Content_inactive_red, t.menu_Content_inactive_green, t.menu_Content_inactive_blue),
		convertSetupColor2RGB(t.menu_Content_inactive_Text_red, t.menu_Content_inactive_Text_green, t.menu_Content_inactive_Text_blue),
		8, convertSetupAlpha2Alpha(t.menu_Content_inactive_alpha));

	frameBuffer->paletteGenFade(COL_MENUFOOT,
		convertSetupColor2RGB(t.menu_Foot_red, t.menu_Foot_green, t.menu_Foot_blue),
		convertSetupColor2RGB(t.menu_Foot_Text_red, t.menu_Foot_Text_green, t.menu_Foot_Text_blue),
		8, convertSetupAlpha2Alpha(t.menu_Foot_alpha));

	frameBuffer->paletteGenFade(COL_INFOBAR,
		convertSetupColor2RGB(t.infobar_red, t.infobar_green, t.infobar_blue),
		convertSetupColor2RGB(t.infobar_Text_red, t.infobar_Text_green, t.infobar_Text_blue),
		8, convertSetupAlpha2Alpha(t.infobar_alpha));

	frameBuffer->paletteGenFade(COL_SHADOW,
		convertSetupColor2RGB(int(t.shadow_red), int(t.shadow_green), int(t.shadow_blue)),
		convertSetupColor2RGB(t.shadow_red, t.shadow_green, t.shadow_blue),
		8, convertSetupAlpha2Alpha(t.shadow_alpha));

	frameBuffer->paletteGenFade(COL_INFOBAR_CASYSTEM,
		convertSetupColor2RGB(t.infobar_casystem_red, t.infobar_casystem_green, t.infobar_casystem_blue),
		convertSetupColor2RGB(t.infobar_Text_red, t.infobar_Text_green, t.infobar_Text_blue),
		8, convertSetupAlpha2Alpha(t.infobar_casystem_alpha));

	frameBuffer->paletteGenFade(COL_COLORED_EVENTS_INFOBAR,
		convertSetupColor2RGB(t.infobar_red, t.infobar_green, t.infobar_blue),
		convertSetupColor2RGB(t.colored_events_red, t.colored_events_green, t.colored_events_blue),
		8, convertSetupAlpha2Alpha(t.infobar_alpha));

	frameBuffer->paletteGenFade(COL_COLORED_EVENTS_CHANNELLIST,
		convertSetupColor2RGB(int(t.menu_Content_red * 0.6), int(t.menu_Content_green * 0.6), int(t.menu_Content_blue * 0.6)),
		convertSetupColor2RGB(t.colored_events_red, t.colored_events_green, t.colored_events_blue),
		8, convertSetupAlpha2Alpha(t.infobar_alpha));
	//NI
	frameBuffer->paletteGenFade(COL_PROGRESSBAR,
		convertSetupColor2RGB(t.progressbar_passive_red, t.progressbar_passive_green, t.progressbar_passive_blue),
		convertSetupColor2RGB(t.progressbar_active_red, t.progressbar_active_green, t.progressbar_active_blue),
		8, convertSetupAlpha2Alpha(t.menu_Content_alpha));

	// ##### TEXT COLORS #####
	// COL_COLORED_EVENTS_TEXT
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 0,
		convertSetupColor2RGB(t.colored_events_red, t.colored_events_green, t.colored_events_blue),
		convertSetupAlpha2Alpha(t.menu_Content_alpha));

	// COL_INFOBAR_TEXT
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 1,
		convertSetupColor2RGB(t.infobar_Text_red, t.infobar_Text_green, t.infobar_Text_blue),
		convertSetupAlpha2Alpha(t.infobar_alpha));

	// COL_MENUFOOT_TEXT
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 2,
		convertSetupColor2RGB(t.menu_Foot_Text_red, t.menu_Foot_Text_green, t.menu_Foot_Text_blue),
		convertSetupAlpha2Alpha(t.menu_Foot_alpha));

	// COL_MENUHEAD_TEXT
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 3,
		convertSetupColor2RGB(t.menu_Head_Text_red, t.menu_Head_Text_green, t.menu_Head_Text_blue),
		convertSetupAlpha2Alpha(t.menu_Head_alpha));

	// COL_MENUCONTENT_TEXT
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 4,
		convertSetupColor2RGB(t.menu_Content_Text_red, t.menu_Content_Text_green, t.menu_Content_Text_blue),
		convertSetupAlpha2Alpha(t.menu_Content_alpha));

	// COL_MENUCONTENT_TEXT_PLUS_1
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 5,
		changeBrightnessRGBRel(convertSetupColor2RGB(t.menu_Content_Text_red, t.menu_Content_Text_green, t.menu_Content_Text_blue), -16),
		convertSetupAlpha2Alpha(t.menu_Content_alpha));

	// COL_MENUCONTENT_TEXT_PLUS_2
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 6,
		changeBrightnessRGBRel(convertSetupColor2RGB(t.menu_Content_Text_red, t.menu_Content_Text_green, t.menu_Content_Text_blue), -32),
		convertSetupAlpha2Alpha(t.menu_Content_alpha));

	// COL_MENUCONTENT_TEXT_PLUS_3
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 7,
		changeBrightnessRGBRel(convertSetupColor2RGB(t.menu_Content_Text_red, t.menu_Content_Text_green, t.menu_Content_Text_blue), -48),
		convertSetupAlpha2Alpha(t.menu_Content_alpha));

	// COL_MENUCONTENTDARK_TEXT
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 8,
		convertSetupColor2RGB(t.menu_Content_Text_red, t.menu_Content_Text_green, t.menu_Content_Text_blue),
		convertSetupAlpha2Alpha(t.menu_Content_alpha));

	// COL_MENUCONTENTDARK_TEXT_PLUS_1
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 9,
		changeBrightnessRGBRel(convertSetupColor2RGB(t.menu_Content_Text_red, t.menu_Content_Text_green, t.menu_Content_Text_blue), -52),
		convertSetupAlpha2Alpha(t.menu_Content_alpha));

	// COL_MENUCONTENTDARK_TEXT_PLUS_2
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 10,
		changeBrightnessRGBRel(convertSetupColor2RGB(t.menu_Content_Text_red, t.menu_Content_Text_green, t.menu_Content_Text_blue), -60),
		convertSetupAlpha2Alpha(t.menu_Content_alpha));

	// COL_MENUCONTENTSELECTED_TEXT
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 11,
		convertSetupColor2RGB(t.menu_Content_Selected_Text_red, t.menu_Content_Selected_Text_green, t.menu_Content_Selected_Text_blue),
		convertSetupAlpha2Alpha(t.menu_Content_Selected_alpha));

	// COL_MENUCONTENTSELECTED_TEXT_PLUS_1
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 12,
		changeBrightnessRGBRel(convertSetupColor2RGB(t.menu_Content_Selected_Text_red, t.menu_Content_Selected_Text_green, t.menu_Content_Selected_Text_blue), -16),
		convertSetupAlpha2Alpha(t.menu_Content_Selected_alpha));

	// COL_MENUCONTENTSELECTED_TEXT_PLUS_2
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 13,
		changeBrightnessRGBRel(convertSetupColor2RGB(t.menu_Content_Selected_Text_red, t.menu_Content_Selected_Text_green, t.menu_Content_Selected_Text_blue), -32),
		convertSetupAlpha2Alpha(t.menu_Content_Selected_alpha));

	// COL_MENUCONTENTINACTIVE_TEXT
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 14,
		convertSetupColor2RGB(t.menu_Content_inactive_Text_red, t.menu_Content_inactive_Text_green, t.menu_Content_inactive_Text_blue),
		convertSetupAlpha2Alpha(t.menu_Content_inactive_alpha));

	// COL_INFOCLOCK_TEXT
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 15,
		convertSetupColor2RGB(t.clock_Digit_red, t.clock_Digit_green, t.clock_Digit_blue),
		convertSetupAlpha2Alpha(t.clock_Digit_alpha));
	//NI
	// COL_PROGRESSBAR_ACTIVE
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 16,
		convertSetupColor2RGB(t.progressbar_active_red, t.progressbar_active_green, t.progressbar_active_blue),
		convertSetupAlpha2Alpha(t.menu_Content_alpha));

	// COL_CHANNELLIST_DESCRIPTION_TEXT
	frameBuffer->paletteSetColor(COL_NEUTRINO_TEXT + 22,
		convertSetupColor2RGB(t.channellist_Description_Text_red, t.channellist_Description_Text_green, t.channellist_Description_Text_blue),
		convertSetupAlpha2Alpha(t.channellist_Description_Text_alpha));

	frameBuffer->paletteSet();
}

bool CColorSetupNotifier::changeNotify(const neutrino_locale_t, void *)
{
	setPalette();
	return false;
}

#if HAVE_SH4_HARDWARE
bool CAudioSetupNotifier::changeNotify(const neutrino_locale_t OptionName, void *data)
#else
bool CAudioSetupNotifier::changeNotify(const neutrino_locale_t OptionName, void *)
#endif
{
	//printf("notify: %d\n", OptionName);
#if HAVE_SH4_HARDWARE || BOXMODEL_DM7020HD || BOXMODEL_DM8000 || BOXMODEL_VUUNO || BOXMODEL_VUDUO || BOXMODEL_VUDUO2 || BOXMODEL_VUULTIMO || BOXMODEL_VUULTIMO4K || BOXMODEL_VUUNO4K || BOXMODEL_HD51 || BOXMODEL_DCUBE
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_ANALOG_MODE))
	{
		g_Zapit->setAudioMode(g_settings.audio_AnalogMode);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_ANALOG_OUT))
	{
		audioDecoder->EnableAnalogOut(g_settings.analog_out ? true : false);
	}
#endif
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_AC3))
	{
		audioDecoder->SetHdmiDD(g_settings.ac3_pass ? true : false);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_DTS))
	{
		audioDecoder->SetSpdifDD(g_settings.dts_pass ? true : false);
	}
	else
#endif
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_AVSYNC))
	{
		videoDecoder->SetSyncMode((AVSYNC_TYPE)g_settings.avsync);
		audioDecoder->SetSyncMode((AVSYNC_TYPE)g_settings.avsync);
		videoDemux->SetSyncMode((AVSYNC_TYPE)g_settings.avsync);
		audioDemux->SetSyncMode((AVSYNC_TYPE)g_settings.avsync);
		pcrDemux->SetSyncMode((AVSYNC_TYPE)g_settings.avsync);
	}
#if HAVE_SH4_HARDWARE
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_HDMI_DD))
	{
		audioDecoder->SetHdmiDD((HDMI_ENCODED_MODE) g_settings.hdmi_dd);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_SPDIF_DD))
	{
		audioDecoder->SetSpdifDD(g_settings.spdif_dd ? true : false);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_MIXER_VOLUME_ANALOG))
	{
		audioDecoder->setMixerVolume("Analog", (long)(*((int *)(data))));
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_MIXER_VOLUME_HDMI))
	{
		audioDecoder->setMixerVolume("HDMI", (long)(*((int *)(data))));
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_MIXER_VOLUME_SPDIF))
	{
		audioDecoder->setMixerVolume("SPDIF", (long)(*((int *)(data))));
	}
#endif

	return false;
}

// used in ./gui/osd_setup.cpp:
bool CFontSizeNotifier::changeNotify(const neutrino_locale_t, void *)
{
	CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FONTSIZE_HINT)); // UTF-8
	hintBox.paint();

	CNeutrinoApp::getInstance()->SetupFonts(CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT);

	hintBox.hide();
	/* recalculate infoclock/muteicon/volumebar */
	CVolumeHelper::getInstance()->refresh();
	return true;
}

int CSubtitleChangeExec::exec(CMenuTarget * /*parent*/, const std::string &actionKey)
{
	printf("CSubtitleChangeExec::exec: action %s\n", actionKey.c_str());

	CMoviePlayerGui *mp = &CMoviePlayerGui::getInstance();
	bool is_mp = mp->Playing();

	if (actionKey == "off")
	{
		tuxtx_stop_subtitle();
		if (!is_mp && dvbsub_getpid() > 0)
			dvbsub_stop();
		if (is_mp && playback)
		{
			playback->SetSubtitlePid(0);
			playback->SetTeletextPid(0);
			dvbsub_stop();
			mp->setCurrentTTXSub("");
		}
		return menu_return::RETURN_EXIT;
	}
	if (!strncmp(actionKey.c_str(), "DVB", 3))
	{
		char const *pidptr = strchr(actionKey.c_str(), ':');
		int pid = atoi(pidptr + 1);
		tuxtx_stop_subtitle();
		dvbsub_pause();
		dvbsub_start(pid);
	}
	else if (!strncmp(actionKey.c_str(), "TTX", 3))
	{
		char const *ptr = strchr(actionKey.c_str(), ':');
		ptr++;
		int pid = atoi(ptr);
		ptr = strchr(ptr, ':');
		ptr++;
		int page = strtol(ptr, NULL, 16);
		ptr = strchr(ptr, ':');
		ptr++;
		printf("CSubtitleChangeExec::exec: TTX, pid %x page %x lang %s\n", pid, page, ptr);
		tuxtx_stop_subtitle();
		tuxtx_set_pid(pid, page, ptr);
		dvbsub_stop();
		if (is_mp)
		{
			playback->SetSubtitlePid(0);
			playback->SetTeletextPid(pid);
			tuxtx_set_pid(pid, page, ptr);
#if HAVE_SH4_HARDWARE
			tuxtx_main(pid, page, 0, true);
#else
			tuxtx_main(pid, page, 0);
#endif
			mp->setCurrentTTXSub(actionKey.c_str());
		}
		else
		{
			tuxtx_set_pid(pid, page, ptr);
			tuxtx_main(pid, page);
		}
	}
	else if (is_mp && !strncmp(actionKey.c_str(), "SUB", 3))
	{
		tuxtx_stop_subtitle();
		dvbsub_stop();
		playback->SetSubtitlePid(0);
		playback->SetTeletextPid(0);
		mp->setCurrentTTXSub("");
		char const *pidptr = strchr(actionKey.c_str(), ':');
		int pid = atoi(pidptr + 1);
		playback->SetSubtitlePid(pid);
	}
	return menu_return::RETURN_EXIT;
}

int CNVODChangeExec::exec(CMenuTarget *parent, const std::string &actionKey)
{
	//    printf("CNVODChangeExec exec: %s\n", actionKey.c_str());
	unsigned sel = atoi(actionKey.c_str());
	g_RemoteControl->setSubChannel(sel);

	parent->hide();
	g_InfoViewer->showSubchan();
	return menu_return::RETURN_EXIT;
}

long CNetAdapter::mac_addr_sys(u_char *addr)   //only for function getMacAddr()
{
	struct ifreq ifr;
	struct ifreq *IFR;
	struct ifconf ifc;
	char buf[1024];
	int s, i;
	int ok = 0;
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
	{
		return -1;
	}

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	ioctl(s, SIOCGIFCONF, &ifc);
	IFR = ifc.ifc_req;
	for (i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; IFR++)
	{
		strcpy(ifr.ifr_name, IFR->ifr_name);
		if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0)
		{
			if (!(ifr.ifr_flags & IFF_LOOPBACK))
			{
				if (ioctl(s, SIOCGIFHWADDR, &ifr) == 0)
				{
					ok = 1;
					break;
				}
			}
		}
	}
	close(s);
	if (ok)
	{
		memmove(addr, ifr.ifr_hwaddr.sa_data, 6);
	}
	else
	{
		return -1;
	}
	return 0;
}

std::string CNetAdapter::getMacAddr(void)
{
	long stat;
	u_char addr[6];
	stat = mac_addr_sys(addr);
	if (stat == 0)
	{
		std::stringstream mac_tmp;
		for (int i = 0; i < 6; ++i)
			mac_tmp << std::hex << std::setfill('0') << std::setw(2) << (int)addr[i] << ':';
		return mac_tmp.str().substr(0, 17);
	}
	else
	{
		printf("[neutrino] eth-id not detected...\n");
		return "";
	}
}

bool CTZChangeNotifier::changeNotify(const neutrino_locale_t, void *Data)
{
	bool found = false;
	std::string name, zone;
	printf("CTZChangeNotifier::changeNotify: %s\n", (char *) Data);

	xmlDocPtr parser = parseXmlFile("/etc/timezone.xml");
	if (parser != NULL)
	{
		xmlNodePtr search = xmlDocGetRootElement(parser);
		search = xmlChildrenNode(search);
		while (search)
		{
			if (!strcmp(xmlGetName(search), "zone"))
			{
				const char *nptr = xmlGetAttribute(search, "name");
				if (nptr)
					name = nptr;

				if (g_settings.timezone == name)
				{
					const char *zptr = xmlGetAttribute(search, "zone");
					if (zptr)
						zone = zptr;
					if (!access("/usr/share/zoneinfo/" + zone, R_OK))
						found = true;
					break;
				}
			}
			search = xmlNextNode(search);
		}
		xmlFreeDoc(parser);
	}
	if (found)
	{
		printf("Timezone: %s -> %s\n", name.c_str(), zone.c_str());
		std::string cmd = "/usr/share/zoneinfo/" + zone;
		printf("symlink %s to /etc/localtime\n", cmd.c_str());
		if (unlink("/etc/localtime"))
			perror("unlink failed");
		if (symlink(cmd.c_str(), "/etc/localtime"))
			perror("symlink failed");
		/* for yocto tzdata compatibility */
		FILE *f = fopen("/etc/timezone", "w");
		if (f)
		{
			fprintf(f, "%s\n", zone.c_str());
			fclose(f);
		}
#if 0
		cmd = ":" + zone;
		setenv("TZ", cmd.c_str(), 1);
#endif
		tzset();
	}

	return false;
}

extern Zapit_config zapitCfg;

int CDataResetNotifier::exec(CMenuTarget * /*parent*/, const std::string &actionKey)
{
	bool delete_all = (actionKey == "all");
	bool delete_chan = (actionKey == "channels") || delete_all;
	bool delete_set = (actionKey == "settings") || delete_all;
	bool delete_removed = (actionKey == "delete_removed");
	neutrino_locale_t msg = delete_all ? LOCALE_RESET_ALL : delete_chan ? LOCALE_RESET_CHANNELS : LOCALE_RESET_SETTINGS;
	int ret = menu_return::RETURN_REPAINT;

	/* no need to confirm if we only remove deleted channels */
	if (!delete_removed)
	{
		int result = ShowMsg(msg, g_Locale->getText(LOCALE_RESET_CONFIRM), CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo);
		if (result != CMsgBox::mbrYes)
			return true;
	}

	if (delete_all)
	{
		my_system(3, "/bin/sh", "-c", "rm -f " CONFIGDIR "/zapit/*.conf");
		CServiceManager::getInstance()->SatelliteList().clear();
		CZapit::getInstance()->LoadSettings();
		CZapit::getInstance()->GetConfig(zapitCfg);
		g_RCInput->postMsg(NeutrinoMessages::REBOOT, 0);
		ret = menu_return::RETURN_EXIT_ALL;
	}
	if (delete_set)
	{
		unlink(NEUTRINO_SETTINGS_FILE);
		//unlink(NEUTRINO_SCAN_SETTINGS_FILE);
		CNeutrinoApp::getInstance()->loadSetup(NEUTRINO_SETTINGS_FILE);
		CNeutrinoApp::getInstance()->saveSetup(NEUTRINO_SETTINGS_FILE);
		//CNeutrinoApp::getInstance()->loadColors(NEUTRINO_SETTINGS_FILE);
		CNeutrinoApp::getInstance()->SetupFonts();
		CColorSetupNotifier::setPalette();
		CVFD::getInstance()->setlcdparameter();
		CFrameBuffer::getInstance()->Clear();
	}
	if (delete_chan)
	{
		my_system(3, "/bin/sh", "-c", "rm -f " CONFIGDIR "/zapit/*.xml");
		g_Zapit->reinitChannels();
	}
	if (delete_removed)
	{
		CHintBox chb(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_SERVICEMENU_RELOAD_HINT));
		chb.paint();
		CServiceManager::getInstance()->SaveServices(true, false, true);
		g_Zapit->reinitChannels();
		chb.hide();
	}
	return ret;
}

#if BOXMODEL_DM8000 || BOXMODEL_DM800SE || BOXMODEL_DM800SEV2 || BOXMODEL_DM820 || BOXMODEL_DM7080 || BOXMODEL_UFS922 || BOXMODEL_CUBEREVO || BOXMODEL_CUBEREVO_MINI2 || BOXMODEL_CUBEREVO_250HD || BOXMODEL_CUBEREVO_3000HD || BOXMODEL_IPBOX9900 || BOXMODEL_IPBOX99 || BOXMODEL_VUDUO2 || BOXMODEL_VUULTIMO || BOXMODEL_VUUNO
#if HAVE_DUCKBOX_HARDWARE
void CFanControlNotifier::setSpeed(unsigned int speed)
{
	int cfd;

	printf("FAN Speed %d\n", speed);
#if defined (BOXMODEL_IPBOX9900) || defined (BOXMODEL_IPBOX99)
	cfd = open("/proc/stb/misc/fan", O_WRONLY);
	if (cfd < 0)
	{
		perror("Cannot open /proc/stb/misc/fan");
#else
	cfd = open("/proc/stb/fan/fan_ctrl", O_WRONLY);
	if (cfd < 0)
	{
		perror("Cannot open /proc/stb/fan/fan_ctrl");
#endif
		return;
	}

	switch (speed)

#if defined (BOXMODEL_IPBOX9900) || defined (BOXMODEL_IPBOX99)
	{
		case 0:
			write(cfd, "0", 1);
			break;
		case 1:
			write(cfd, "1", 1);
			break;
	}
#else
	{
		case 1:
			write(cfd, "115", 3);
			break;
		case 2:
			write(cfd, "130", 3);
			break;
		case 3:
			write(cfd, "145", 3);
			break;
		case 4:
			write(cfd, "160", 3);
			break;
		case 5:
			write(cfd, "170", 3);
	}
#endif
	close(cfd);
}
#else
void CFanControlNotifier::setSpeed(unsigned int __attribute__((unused)) speed)
{
#if defined (BOXMODEL_DM8000) || defined (BOXMODEL_DM800SE) || defined (BOXMODEL_DM800SEV2) || defined (BOXMODEL_DM820) || defined (BOXMODEL_DM7080)
	int cfd;
	cfd = open("/proc/stb/fp/fan_vlt", O_WRONLY);
	if (cfd < 0)
	{
		perror("Cannot open /proc/stb/fp/fan_vlt");
		return;
	}
	switch (speed)
	{
		case 0:
			write(cfd, "00000000", 8);
			break;
		case 1:
			write(cfd, "FFFFFFFF", 8);
			break;
	}
	close(cfd);
#endif
#if defined (BOXMODEL_VUDUO2) || defined (BOXMODEL_VUULTIMO) || defined (BOXMODEL_VUUNO)
	int cfd;
	cfd = open("/proc/stb/fp/fan_pwm", O_WRONLY);
	if (cfd < 0)
	{
		perror("Cannot open /proc/stb/fp/fan_pwm");
		return;
	}
	switch (speed)
	{
		case 1:
			write(cfd, "10", 2);
			break;
		case 2:
			write(cfd, "20", 2);
			break;
		case 3:
			write(cfd, "30", 2);
			break;
		case 4:
			write(cfd, "40", 2);
			break;
		case 5:
			write(cfd, "50", 2);
			break;
		case 6:
			write(cfd, "60", 2);
			break;
		case 7:
			write(cfd, "70", 2);
			break;
		case 8:
			write(cfd, "80", 2);
			break;
		case 9:
			write(cfd, "90", 2);
			break;
		case 10:
			write(cfd, "100", 3);
	}
	close(cfd);
#endif
}
#endif

bool CFanControlNotifier::changeNotify(const neutrino_locale_t, void *data)
{
	unsigned int speed = * (int *) data;
	setSpeed(speed);
	return false;
}
#endif
