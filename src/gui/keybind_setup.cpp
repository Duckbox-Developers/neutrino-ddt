/*
	$port: keybind_setup.cpp,v 1.4 2010/09/20 10:24:12 tuxbox-cvs Exp $

	keybindings setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2010 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/


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

#include <unistd.h>

#include "keybind_setup.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/keyboard_input.h>

#include <gui/filebrowser.h>

#include <driver/screen_max.h>
#include <driver/screenshot.h>
#include <driver/display.h>

#include <system/debug.h>
#include <system/helpers.h>
#include <sys/socket.h>
#include <sys/un.h>

CKeybindSetup::CKeybindSetup()
{
	changeNotify(LOCALE_KEYBINDINGMENU_REPEATBLOCKGENERIC, NULL);

	width = 40;
}

CKeybindSetup::~CKeybindSetup()
{
}

int CKeybindSetup::exec(CMenuTarget *parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init keybindings setup\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	if (actionKey == "loadkeys")
	{
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("conf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(g_settings.backup_dir.c_str()) == true)
		{
			CNeutrinoApp::getInstance()->loadKeys(fileBrowser.getSelectedFile()->Name.c_str());
			printf("[neutrino keybind_setup] new keys: %s\n", fileBrowser.getSelectedFile()->Name.c_str());
			for (int i = 0; i < KEYBINDS_COUNT; i++)
			{
				keychooser[i]->reinitName();
			}
		}
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "savekeys")
	{
		CFileBrowser fileBrowser;
		fileBrowser.Dir_Mode = true;
		if (fileBrowser.exec(g_settings.backup_dir.c_str()) == true)
		{
			std::string fname = "keys.conf";
			CKeyboardInput *sms = new CKeyboardInput(LOCALE_EXTRA_SAVEKEYS, &fname);
			sms->exec(NULL, "");
			std::string sname = fileBrowser.getSelectedFile()->Name + "/" + fname;
			printf("[neutrino keybind_setup] save keys: %s\n", sname.c_str());
			CNeutrinoApp::getInstance()->saveKeys(sname.c_str());
			delete sms;
		}
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "savecode")
	{
		if (remote_code != remote_code_old)
		{
			if (setRemoteCode(remote_code))
			{
				neutrino_msg_t      msg = 0;
				neutrino_msg_data_t data = 0;
				CVFD::getInstance()->Clear();
				remote_code_old = remote_code;
				CHintBox *Hint;
				std::string Text = g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_CODE);
				Text += " > " + to_string(remote_code) + "\n";
				Text += g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_CODE_MSG1);
				Text += " " + to_string(remote_code) + "\n";
				Text += g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_CODE_MSG2);
				Hint = new CHintBox(LOCALE_KEYBINDINGMENU_REMOTECONTROL_CODE_SAVE, Text.c_str());
				Hint->paint();
				for (int i = 0; i < 6; i++)
				{
					g_RCInput->getMsg(&msg, &data, 1);
					g_RCInput->clearRCMsg();
				}
				system("killall evremote2");
				usleep(300000);
				system("/bin/evremote2 10 120 > /dev/null &");
				std::string vfd_msg = "RC Code: " + to_string(remote_code);
				sleep(1);
				CVFD::getInstance()->repaintIcons();
				CVFD::getInstance()->ShowText(vfd_msg.c_str());
				while (true)
				{
					g_RCInput->getMsg(&msg, &data, 1);
					if (msg == CRCInput::RC_ok)
					{
						//printf("MSG: 0x%lx\n", msg);
						g_RCInput->clearRCMsg();
						break;
					}
					g_RCInput->clearRCMsg();
				}
				Hint->hide();
			}
		}
		else
		{
			ShowHint(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_CODE_NOT_SAVE), 450, 5);
		}
		return menu_return::RETURN_REPAINT;
	}

	res = showKeySetup();

	return res;
}

#define KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_COUNT 4
const CMenuOptionChooser::keyval KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_OPTIONS[KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_COUNT] =
{
	{ SNeutrinoSettings::ZAP,     LOCALE_KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_ZAP     },
	{ SNeutrinoSettings::VZAP,    LOCALE_KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_VZAP    },
	{ SNeutrinoSettings::VOLUME,  LOCALE_KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_VOLUME  },
	{ SNeutrinoSettings::INFOBAR, LOCALE_KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_INFOBAR }
};

#define KEYBINDINGMENU_PLAYBUTTON_OPTIONS_COUNT 4
const CMenuOptionChooser::keyval KEYBINDINGMENU_PLAYBUTTON_OPTIONS[KEYBINDINGMENU_PLAYBUTTON_OPTIONS_COUNT] =
{
	{ 0, LOCALE_MOVIEPLAYER_TSPLAYBACK         },
	{ 1, LOCALE_MOVIEPLAYER_FILEPLAYBACK_VIDEO },
	{ 2, LOCALE_AUDIOPLAYER_NAME               },
	{ 3, LOCALE_INETRADIO_NAME                 }
};

typedef struct key_settings_t
{
	const neutrino_locale_t keydescription;
	int *keyvalue_p;
	const neutrino_locale_t hint;

} key_settings_struct_t;

const key_settings_struct_t key_settings[CKeybindSetup::KEYBINDS_COUNT] =
{
	{LOCALE_KEYBINDINGMENU_TVRADIOMODE,   	&g_settings.key_tvradio_mode,		LOCALE_MENU_HINT_KEY_TVRADIOMODE },
	{LOCALE_KEYBINDINGMENU_POWEROFF,      	&g_settings.key_power_off,		LOCALE_MENU_HINT_KEY_POWEROFF },
	{LOCALE_KEYBINDINGMENU_PAGEUP, 		&g_settings.key_pageup,			LOCALE_MENU_HINT_KEY_PAGEUP },
	{LOCALE_KEYBINDINGMENU_PAGEDOWN, 	&g_settings.key_pagedown, 		LOCALE_MENU_HINT_KEY_PAGEDOWN },
	{LOCALE_KEYBINDINGMENU_VOLUMEUP, 	&g_settings.key_volumeup,		LOCALE_MENU_HINT_KEY_VOLUMEUP },
	{LOCALE_KEYBINDINGMENU_VOLUMEDOWN,	&g_settings.key_volumedown, 		LOCALE_MENU_HINT_KEY_VOLUMEDOWN },
	{LOCALE_EXTRA_KEY_LIST_START, 		&g_settings.key_list_start, 		LOCALE_MENU_HINT_KEY_LIST_START },
	{LOCALE_EXTRA_KEY_LIST_END,	 	&g_settings.key_list_end,		LOCALE_MENU_HINT_KEY_LIST_END },
	{LOCALE_KEYBINDINGMENU_SORT,		&g_settings.key_channelList_sort,	LOCALE_MENU_HINT_KEY_SORT },
	{LOCALE_KEYBINDINGMENU_ADDRECORD,	&g_settings.key_channelList_addrecord,	LOCALE_MENU_HINT_KEY_ADDRECORD },
	{LOCALE_KEYBINDINGMENU_ADDREMIND,	&g_settings.key_channelList_addremind,	LOCALE_MENU_HINT_KEY_ADDREMIND },
	{LOCALE_KEYBINDINGMENU_BOUQUETUP,	&g_settings.key_bouquet_up, 		LOCALE_MENU_HINT_KEY_BOUQUETUP },
	{LOCALE_KEYBINDINGMENU_BOUQUETDOWN,	&g_settings.key_bouquet_down, 		LOCALE_MENU_HINT_KEY_BOUQUETDOWN },
	{LOCALE_EXTRA_KEY_CURRENT_TRANSPONDER,	&g_settings.key_current_transponder,	LOCALE_MENU_HINT_KEY_TRANSPONDER },
	{LOCALE_KEYBINDINGMENU_CHANNELUP,	&g_settings.key_quickzap_up,		LOCALE_MENU_HINT_KEY_CHANNELUP },
	{LOCALE_KEYBINDINGMENU_CHANNELDOWN,	&g_settings.key_quickzap_down,  	LOCALE_MENU_HINT_KEY_CHANNELDOWN },
	{LOCALE_KEYBINDINGMENU_SUBCHANNELUP,	&g_settings.key_subchannel_up,  	LOCALE_MENU_HINT_KEY_SUBCHANNELUP },
	{LOCALE_KEYBINDINGMENU_SUBCHANNELDOWN,	&g_settings.key_subchannel_down,	LOCALE_MENU_HINT_KEY_SUBCHANNELDOWN },
	{LOCALE_KEYBINDINGMENU_ZAPHISTORY,	&g_settings.key_zaphistory, 		LOCALE_MENU_HINT_KEY_HISTORY },
	{LOCALE_KEYBINDINGMENU_LASTCHANNEL,	&g_settings.key_lastchannel,		LOCALE_MENU_HINT_KEY_LASTCHANNEL },
	{LOCALE_MPKEY_PLAY,			&g_settings.mpkey_play,			LOCALE_MENU_HINT_KEY_MPPLAY },
	{LOCALE_MPKEY_PAUSE,			&g_settings.mpkey_pause, 		LOCALE_MENU_HINT_KEY_MPPAUSE },
	{LOCALE_MPKEY_STOP,			&g_settings.mpkey_stop,			LOCALE_MENU_HINT_KEY_MPSTOP },
	{LOCALE_MPKEY_FORWARD,			&g_settings.mpkey_forward,  		LOCALE_MENU_HINT_KEY_MPFORWARD },
	{LOCALE_MPKEY_REWIND,			&g_settings.mpkey_rewind,		LOCALE_MENU_HINT_KEY_MPREWIND },
	{LOCALE_MPKEY_AUDIO,			&g_settings.mpkey_audio, 		LOCALE_MENU_HINT_KEY_MPAUDIO },
	{LOCALE_MPKEY_SUBTITLE,			&g_settings.mpkey_subtitle,		LOCALE_MENU_HINT_KEY_MPSUBTITLE },
	{LOCALE_MPKEY_TIME,			&g_settings.mpkey_time,			LOCALE_MENU_HINT_KEY_MPTIME },
	{LOCALE_MPKEY_BOOKMARK,			&g_settings.mpkey_bookmark, 		LOCALE_MENU_HINT_KEY_MPBOOKMARK },
	{LOCALE_MPKEY_GOTO,			&g_settings.mpkey_goto,	 		NONEXISTANT_LOCALE},
	{LOCALE_MPKEY_NEXT3DMODE,		&g_settings.mpkey_next3dmode,		NONEXISTANT_LOCALE},
	{LOCALE_MPKEY_NEXT_REPEAT_MODE,		&g_settings.mpkey_next_repeat_mode,	NONEXISTANT_LOCALE},
	{LOCALE_EXTRA_KEY_TIMESHIFT,		&g_settings.key_timeshift,  		LOCALE_MENU_HINT_KEY_TIMESHIFT },
	{LOCALE_EXTRA_KEY_UNLOCK,		&g_settings.key_unlock,			LOCALE_MENU_HINT_KEY_UNLOCK},
	{LOCALE_EXTRA_KEY_HELP,			&g_settings.key_help,			NONEXISTANT_LOCALE},
	{LOCALE_EXTRA_KEY_SCREENSHOT,		&g_settings.key_screenshot,		LOCALE_MENU_HINT_KEY_SCREENSHOT },
#if ENABLE_PIP
	{LOCALE_EXTRA_KEY_PIP_CLOSE,		&g_settings.key_pip_close,		LOCALE_MENU_HINT_KEY_PIP_CLOSE },
	{LOCALE_EXTRA_KEY_PIP_CLOSE_AVINPUT,	&g_settings.key_pip_close_avinput,	NONEXISTANT_LOCALE /*LOCALE_MENU_HINT_KEY_PIP_CLOSE_AVINPUT*/ },
	{LOCALE_EXTRA_KEY_PIP_SETUP,		&g_settings.key_pip_setup,		LOCALE_MENU_HINT_KEY_PIP_SETUP },
	{LOCALE_EXTRA_KEY_PIP_SWAP,		&g_settings.key_pip_swap,		LOCALE_MENU_HINT_KEY_PIP_CLOSE },
#endif
	{LOCALE_EXTRA_KEY_RECORD,		&g_settings.key_record,			LOCALE_MENU_HINT_KEY_RECORD },
	{LOCALE_MBKEY_COPY_ONEFILE,		&g_settings.mbkey_copy_onefile,		NONEXISTANT_LOCALE },
	{LOCALE_MBKEY_COPY_SEVERAL,		&g_settings.mbkey_copy_several,		NONEXISTANT_LOCALE },
	{LOCALE_MBKEY_CUT,			&g_settings.mbkey_cut,			NONEXISTANT_LOCALE },
	{LOCALE_MBKEY_TRUNCATE,			&g_settings.mbkey_truncate,		NONEXISTANT_LOCALE },
	{LOCALE_MBKEY_COVER,			&g_settings.mbkey_cover,		LOCALE_MENU_HINT_MBKEY_COVER },
};

// used by driver/rcinput.cpp
bool checkLongPress(uint32_t key)
{
	if (g_settings.longkeypress_duration == LONGKEYPRESS_OFF)
		return false;
	if (key == CRCInput::RC_standby)
		return true;
	key |= CRCInput::RC_Repeat;
	for (unsigned int i = 0; i < CKeybindSetup::KEYBINDS_COUNT; i++)
		if ((uint32_t)*key_settings[i].keyvalue_p == key)
			return true;
	for (std::vector<SNeutrinoSettings::usermenu_t *>::iterator it = g_settings.usermenu.begin(); it != g_settings.usermenu.end(); ++it)
		if (*it && (uint32_t)((*it)->key) == key)
			return true;
	return false;
}

int CKeybindSetup::showKeySetup()
{
	//keysetup menu
	CMenuWidget *keySettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP);
	keySettings->addIntroItems(LOCALE_MAINSETTINGS_KEYBINDING);

	//keybindings menu
	CMenuWidget bindSettings(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING);

	//keybindings
	for (int i = 0; i < KEYBINDS_COUNT; i++)
		keychooser[i] = new CKeyChooser((unsigned int *) key_settings[i].keyvalue_p, key_settings[i].keydescription/*as head caption*/, NEUTRINO_ICON_SETTINGS);

	showKeyBindSetup(&bindSettings);
	CMenuForwarder *mf;

	mf = new CMenuForwarder(LOCALE_KEYBINDINGMENU_EDIT, true, NULL, &bindSettings, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_KEY_BINDING);
	keySettings->addItem(mf);

	mf = new CMenuForwarder(LOCALE_EXTRA_SAVEKEYS, true, NULL, this, "savekeys", CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_KEY_SAVE);
	keySettings->addItem(mf);

	mf = new CMenuForwarder(LOCALE_EXTRA_LOADKEYS, true, NULL, this, "loadkeys", CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_KEY_LOAD);
	keySettings->addItem(mf);

#if defined (BOXMODEL_UFS912) || defined (BOXMODEL_UFS913)
	remote_code = getRemoteCode();
	remote_code_old = remote_code;
	keySettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_REMOTECONTROL_CODE_CHANGE));
	CMenuOptionNumberChooser *nc = new CMenuOptionNumberChooser(LOCALE_KEYBINDINGMENU_REMOTECONTROL_CODE, &remote_code, true, 1, 4);
	nc->setHint("", LOCALE_MENU_HINT_REMOTE_CODE);
	keySettings->addItem(nc);

	mf = new CMenuForwarder(LOCALE_KEYBINDINGMENU_REMOTECONTROL_CODE_SAVE, true, NULL, this, "savecode", CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_REMOTE_CODE_SAVE);
	keySettings->addItem(mf);
#endif

	keySettings->addItem(GenericMenuSeparatorLine);

	//rc tuning
	std::string ms_number_format("%d ");
	ms_number_format += g_Locale->getText(LOCALE_UNIT_SHORT_MILLISECOND);
	CMenuOptionNumberChooser *cc;

	int shortcut = 1;

	cc = new CMenuOptionNumberChooser(LOCALE_KEYBINDINGMENU_LONGKEYPRESS_DURATION,
		&g_settings.longkeypress_duration, true, LONGKEYPRESS_OFF, 9999, NULL,
		CRCInput::convertDigitToKey(shortcut++), NULL, 0, LONGKEYPRESS_OFF, LOCALE_OPTIONS_OFF);
	cc->setNumberFormat(ms_number_format);
	cc->setNumericInput(true);
	cc->setHint("", LOCALE_MENU_HINT_LONGKEYPRESS_DURATION);
	keySettings->addItem(cc);

#if HAVE_SH4_HARDWARE
	g_settings.accept_other_remotes = access("/etc/lircd_predata_lock", R_OK) ? 1 : 0;
	CMenuOptionChooser *mc = new CMenuOptionChooser(LOCALE_KEYBINDINGMENU_ACCEPT_OTHER_REMOTES,
		&g_settings.accept_other_remotes, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this,
		CRCInput::convertDigitToKey(shortcut++));
	mc->setHint("", LOCALE_MENU_HINT_ACCEPT_OTHER_REMOTES);
	keySettings->addItem(mc);
#endif

	cc = new CMenuOptionNumberChooser(LOCALE_KEYBINDINGMENU_REPEATBLOCK,
		&g_settings.repeat_blocker, true, 0, 999, this,
		CRCInput::convertDigitToKey(shortcut++), NULL, 0, 0, LOCALE_OPTIONS_OFF);
	cc->setNumberFormat(ms_number_format);
	cc->setNumericInput(true);
	cc->setHint("", LOCALE_MENU_HINT_KEY_REPEATBLOCK);
	keySettings->addItem(cc);

	cc = new CMenuOptionNumberChooser(LOCALE_KEYBINDINGMENU_REPEATBLOCKGENERIC,
		&g_settings.repeat_genericblocker, true, 0, 999, this,
		CRCInput::convertDigitToKey(shortcut++), NULL, 0, 0, LOCALE_OPTIONS_OFF);
	cc->setNumberFormat(ms_number_format);
	cc->setNumericInput(true);
	cc->setHint("", LOCALE_MENU_HINT_KEY_REPEATBLOCKGENERIC);
	keySettings->addItem(cc);

	int res = keySettings->exec(NULL, "");

	delete keySettings;
	for (int i = 0; i < KEYBINDS_COUNT; i++)
		delete keychooser[i];
	return res;
}


void CKeybindSetup::showKeyBindSetup(CMenuWidget *bindSettings)
{

	CMenuForwarder *mf;

	bindSettings->addIntroItems(LOCALE_KEYBINDINGMENU_HEAD);

	//modes
	CMenuWidget *bindSettings_modes = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_MODES);
	showKeyBindModeSetup(bindSettings_modes);
	mf = new CMenuDForwarder(LOCALE_KEYBINDINGMENU_MODECHANGE, true, NULL, bindSettings_modes, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_KEY_MODECHANGE);
	bindSettings->addItem(mf);

	// channellist keybindings
	CMenuWidget *bindSettings_chlist = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_CHANNELLIST);
	showKeyBindChannellistSetup(bindSettings_chlist);
	mf = new CMenuDForwarder(LOCALE_KEYBINDINGMENU_CHANNELLIST, true, NULL, bindSettings_chlist, NULL, CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_KEY_CHANNELLIST);
	bindSettings->addItem(mf);

	// Zapping keys quickzap
	CMenuWidget *bindSettings_qzap = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_QUICKZAP);
	showKeyBindQuickzapSetup(bindSettings_qzap);
	mf = new CMenuDForwarder(LOCALE_KEYBINDINGMENU_QUICKZAP, true, NULL, bindSettings_qzap, NULL, CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_KEY_QUICKZAP);
	bindSettings->addItem(mf);

	//movieplayer
	CMenuWidget *bindSettings_mplayer = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_MOVIEPLAYER);
	showKeyBindMovieplayerSetup(bindSettings_mplayer);
	mf = new CMenuDForwarder(LOCALE_MAINMENU_MOVIEPLAYER, true, NULL, bindSettings_mplayer, NULL, CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_KEY_MOVIEPLAYER);
	bindSettings->addItem(mf);

	//moviebrowser
	CMenuWidget *bindSettings_mbrowser = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_MOVIEBROWSER);
	showKeyBindMoviebrowserSetup(bindSettings_mbrowser);
	mf = new CMenuDForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, bindSettings_mbrowser, NULL, CRCInput::RC_nokey);
	mf->setHint("", LOCALE_MENU_HINT_KEY_MOVIEBROWSER);
	bindSettings->addItem(mf);

	//navigation
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_NAVIGATION));
	for (int i = NKEY_PAGE_UP; i <= NKEY_PAGE_DOWN; i++)
	{
		mf = new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]);
		mf->setHint("", key_settings[i].hint);
		bindSettings->addItem(mf);
	}
	CMenuOptionChooser *mc = new CMenuOptionChooser(LOCALE_EXTRA_MENU_LEFT_EXIT, &g_settings.menu_left_exit, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_KEY_LEFT_EXIT);
	bindSettings->addItem(mc);

	//volume
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_VOLUME));
	for (int i = NKEY_VOLUME_UP; i <= NKEY_VOLUME_DOWN; i++)
		bindSettings->addItem(new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]));

	//misc
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_MISC));
	//bindSettings->addItem(new CMenuForwarder(keydescription[NKEY_PLUGIN], true, NULL, keychooser[NKEY_PLUGIN]));

	// timeshift
	mf = new CMenuForwarder(key_settings[NKEY_TIMESHIFT].keydescription, true, keychooser[NKEY_TIMESHIFT]->getKeyName(), keychooser[NKEY_TIMESHIFT]);
	mf->setHint("", key_settings[NKEY_TIMESHIFT].hint);
	bindSettings->addItem(mf);
	// unlock
	mf = new CMenuForwarder(key_settings[NKEY_UNLOCK].keydescription, true, keychooser[NKEY_UNLOCK]->getKeyName(), keychooser[NKEY_UNLOCK]);
	mf->setHint("", key_settings[NKEY_UNLOCK].hint);
	bindSettings->addItem(mf);
	// screenshot
#ifdef SCREENSHOT
	mf = new CMenuForwarder(key_settings[NKEY_SCREENSHOT].keydescription, true, keychooser[NKEY_SCREENSHOT]->getKeyName(), keychooser[NKEY_SCREENSHOT]);
	mf->setHint("", key_settings[NKEY_SCREENSHOT].hint);
	bindSettings->addItem(mf);
#endif
#ifdef ENABLE_PIP
	// pip
	mf = new CMenuForwarder(key_settings[NKEY_PIP_CLOSE].keydescription, true, keychooser[NKEY_PIP_CLOSE]->getKeyName(), keychooser[NKEY_PIP_CLOSE]);
	mf->setHint("", key_settings[NKEY_PIP_CLOSE].hint);
	bindSettings->addItem(mf);
	mf = new CMenuForwarder(key_settings[NKEY_PIP_CLOSE_AVINPUT].keydescription, true, keychooser[NKEY_PIP_CLOSE_AVINPUT]->getKeyName(), keychooser[NKEY_PIP_CLOSE_AVINPUT]);
//	mf->setHint("", key_settings[NKEY_PIP_CLOSE_AVINPUT].hint);
	bindSettings->addItem(mf);
	mf = new CMenuForwarder(key_settings[NKEY_PIP_SETUP].keydescription, true, keychooser[NKEY_PIP_SETUP]->getKeyName(), keychooser[NKEY_PIP_SETUP]);
	mf->setHint("", key_settings[NKEY_PIP_SETUP].hint);
	bindSettings->addItem(mf);
	mf = new CMenuForwarder(key_settings[NKEY_PIP_SWAP].keydescription, true, keychooser[NKEY_PIP_SWAP]->getKeyName(), keychooser[NKEY_PIP_SWAP]);
	mf->setHint("", key_settings[NKEY_PIP_SWAP].hint);
	bindSettings->addItem(mf);
#endif

	bindSettings->addItem(new CMenuForwarder(key_settings[NKEY_HELP].keydescription, true, keychooser[NKEY_HELP]->getKeyName(), keychooser[NKEY_HELP]));
	bindSettings->addItem(new CMenuForwarder(key_settings[NKEY_RECORD].keydescription, true, keychooser[NKEY_RECORD]->getKeyName(), keychooser[NKEY_RECORD]));

	//play button starts....
	mc = new CMenuOptionChooser(LOCALE_MPKEY_PLAY, &g_settings.key_playbutton, KEYBINDINGMENU_PLAYBUTTON_OPTIONS, KEYBINDINGMENU_PLAYBUTTON_OPTIONS_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_KEY_MPPLAY);
	bindSettings->addItem(mc);

	bindSettings->addItem(new CMenuSeparator());
	// left/right keys
	mc = new CMenuOptionChooser(LOCALE_KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV, &g_settings.mode_left_right_key_tv, KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_OPTIONS, KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_KEY_RIGHT);
	bindSettings->addItem(mc);
}

void CKeybindSetup::showKeyBindModeSetup(CMenuWidget *bindSettings_modes)
{
	CMenuForwarder *mf;
	bindSettings_modes->addIntroItems(LOCALE_KEYBINDINGMENU_MODECHANGE);

	// tv/radio
	mf = new CMenuForwarder(key_settings[NKEY_TV_RADIO_MODE].keydescription, true, keychooser[NKEY_TV_RADIO_MODE]->getKeyName(), keychooser[NKEY_TV_RADIO_MODE], NULL, CRCInput::RC_red);
	mf->setHint("", key_settings[NKEY_TV_RADIO_MODE].hint);
	bindSettings_modes->addItem(mf);

	mf = new CMenuForwarder(key_settings[NKEY_POWER_OFF].keydescription, true, keychooser[NKEY_POWER_OFF]->getKeyName(), keychooser[NKEY_POWER_OFF], NULL, CRCInput::RC_green);
	mf->setHint("", key_settings[NKEY_POWER_OFF].hint);
	bindSettings_modes->addItem(mf);
}

void CKeybindSetup::showKeyBindChannellistSetup(CMenuWidget *bindSettings_chlist)
{
	bindSettings_chlist->addIntroItems(LOCALE_KEYBINDINGMENU_CHANNELLIST);

	for (int i = NKEY_LIST_START; i <= NKEY_CURRENT_TRANSPONDER; i++)
	{
		CMenuForwarder *mf = new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]);
		mf->setHint("", key_settings[i].hint);
		bindSettings_chlist->addItem(mf);
	}

	CMenuOptionChooser *mc = new CMenuOptionChooser(LOCALE_EXTRA_SMS_CHANNEL, &g_settings.sms_channel, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SMS_CHANNEL);
	bindSettings_chlist->addItem(mc);
}

void CKeybindSetup::showKeyBindQuickzapSetup(CMenuWidget *bindSettings_qzap)
{
	bindSettings_qzap->addIntroItems(LOCALE_KEYBINDINGMENU_QUICKZAP);

	for (int i = NKEY_CHANNEL_UP; i <= NKEY_LASTCHANNEL; i++)
	{
		CMenuForwarder *mf = new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]);
		mf->setHint("", key_settings[i].hint);
		bindSettings_qzap->addItem(mf);
	}
}

void CKeybindSetup::showKeyBindMovieplayerSetup(CMenuWidget *bindSettings_mplayer)
{
	bindSettings_mplayer->addIntroItems(LOCALE_MAINMENU_MOVIEPLAYER);

	for (int i = MPKEY_PLAY; i <= MPKEY_NEXT_REPEAT_MODE; i++)
	{
		CMenuForwarder *mf = new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]);
		mf->setHint("", key_settings[i].hint);
		bindSettings_mplayer->addItem(mf);
	}
}

void CKeybindSetup::showKeyBindMoviebrowserSetup(CMenuWidget *bindSettings_mbrowser)
{
	bindSettings_mbrowser->addIntroItems(LOCALE_MOVIEBROWSER_HEAD);

	for (int i = MBKEY_COPY_ONEFILE; i <= MBKEY_COVER; i++)
	{
		CMenuForwarder *mf = new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]);
		mf->setHint("", key_settings[i].hint);
		bindSettings_mbrowser->addItem(mf);
	}

	CMenuOptionChooser *mc = new CMenuOptionChooser(LOCALE_EXTRA_SMS_MOVIE, &g_settings.sms_movie, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SMS_MOVIE);
	bindSettings_mbrowser->addItem(mc);
}

bool CKeybindSetup::changeNotify(const neutrino_locale_t OptionName, void * /* data */)
{
#if HAVE_SH4_HARDWARE
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_KEYBINDINGMENU_ACCEPT_OTHER_REMOTES))
	{
		struct sockaddr_un sun;
		memset(&sun, 0, sizeof(sun));
		sun.sun_family = AF_UNIX;
		strcpy(sun.sun_path, "/var/run/lirc/lircd");
		int lircd_sock = socket(AF_UNIX, SOCK_STREAM, 0);
		if (lircd_sock > -1)
		{
			if (!connect(lircd_sock, (struct sockaddr *) &sun, sizeof(sun)))
			{
				if (g_settings.accept_other_remotes)
					write(lircd_sock, "PREDATA_UNLOCK\n", 16);
				else
					write(lircd_sock, "PREDATA_LOCK\n", 14);
			}
			close(lircd_sock);
		}
		// work around junk data sent by vfd controller
		sleep(2);
		g_RCInput->clearRCMsg();
		return false;
	}
#endif
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_KEYBINDINGMENU_REPEATBLOCKGENERIC) ||
		ARE_LOCALES_EQUAL(OptionName, LOCALE_KEYBINDINGMENU_REPEATBLOCK))
	{
		unsigned int fdelay = g_settings.repeat_blocker;
		unsigned int xdelay = g_settings.repeat_genericblocker;

		g_RCInput->repeat_block = fdelay * 1000;
		g_RCInput->repeat_block_generic = xdelay * 1000;
		g_RCInput->setKeyRepeatDelay(fdelay, xdelay);
	}
	return false;
}

const char *CKeybindSetup::getMoviePlayerButtonName(const neutrino_msg_t key, bool &active, bool return_title)
{
	active = false;
	for (unsigned int i = MPKEY_PLAY; i <= MPKEY_NEXT_REPEAT_MODE; i++)
	{
		if ((uint32_t)*key_settings[i].keyvalue_p == (unsigned int)key)
		{
			active = true;
			return g_Locale->getText(key_settings[i].keydescription);
		}
	}
	return "";
}

int CKeybindSetup::getRemoteCode()
{
	int code = 1;
	if (!access("/etc/.rccode", F_OK))
	{
		char buf[10];
		int val;
		FILE *fd;
		fd = fopen("/etc/.rccode", "r");
		if (fd != NULL)
		{
			if (fgets(buf, sizeof(buf), fd) != NULL)
			{
				val = atoi(buf);
				if (val > 0 && val < 5)
					code = val;
			}
			fclose(fd);
		}
	}
	return code;
}

bool CKeybindSetup::setRemoteCode(int code)
{
	char buf[10];
	bool ret = false;
	FILE *fd;
	fd = fopen("/etc/.rccode", "w");
	if (fd != NULL)
	{
		sprintf(buf, "%d\n", code);
		if (fputs(buf, fd) > 0)
			ret = true;
		fclose(fd);
	}
	return ret;
}
