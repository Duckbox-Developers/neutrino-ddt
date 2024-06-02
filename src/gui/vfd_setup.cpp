/*
	$similar port: lcd_setup.cpp,v 1.1 2010/07/30 20:52:16 tuxbox-cvs Exp $

	vfd setup implementation, similar to lcd_setup.cpp of tuxbox-cvs - Neutrino-GUI

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


#include "vfd_setup.h"
#ifdef ENABLE_GRAPHLCD
#include <gui/glcdsetup.h>
#endif

#ifdef ENABLE_LCD4LINUX
#include "gui/lcd4l_setup.h"
#endif

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>

#include <driver/display.h>
#include <driver/screen_max.h>
#include <driver/display.h>

#include <system/debug.h>
#include <system/helpers.h>


CVfdSetup::CVfdSetup()
{
	width = 40;
}

CVfdSetup::~CVfdSetup()
{
}


int CVfdSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init lcd setup\n");
	if(parent != NULL)
		parent->hide();

	if(actionKey=="def") {
		brightness		= DEFAULT_VFD_BRIGHTNESS;
		brightnessstandby	= DEFAULT_VFD_STANDBYBRIGHTNESS;
		brightnessdeepstandby   = DEFAULT_VFD_STANDBYBRIGHTNESS;
		g_settings.lcd_setting_dim_brightness = 3;
		CVFD::getInstance()->setBrightness(brightness);
		CVFD::getInstance()->setBrightnessStandby(brightnessstandby);
		CVFD::getInstance()->setBrightnessDeepStandby(brightnessdeepstandby);
		return menu_return::RETURN_REPAINT;
	} else if (actionKey == "brightness") {
		return showBrightnessSetup();
	}

	int res = showSetup();

	return res;
}

#define LCDMENU_STATUSLINE_OPTION_COUNT 2
const CMenuOptionChooser::keyval LCDMENU_STATUSLINE_OPTIONS[LCDMENU_STATUSLINE_OPTION_COUNT] =
{
	{ 0, LOCALE_LCDMENU_STATUSLINE_PLAYTIME },
	{ 1, LOCALE_LCDMENU_STATUSLINE_VOLUME   }
	//,{ 2, LOCALE_LCDMENU_STATUSLINE_BOTH     }
};

#define LCD_INFO_OPTION_COUNT 2
const CMenuOptionChooser::keyval LCD_INFO_OPTIONS[LCD_INFO_OPTION_COUNT] =
{
#if BOXMODEL_H7 || BOXMODEL_BRE2ZE4K
	{ 0, LOCALE_LCD_INFO_LINE_CHANNELNUMBER },
#else
	{ 0, LOCALE_LCD_INFO_LINE_CHANNELNAME },
#endif
	{ 1, LOCALE_LCD_INFO_LINE_CLOCK }
};
#if HAVE_SH4_HARDWARE
#define OPTIONS_OFF_ON_OPTION_COUNT 2
const CMenuOptionChooser::keyval OPTIONS_OFF_ON_OPTIONS[OPTIONS_OFF_ON_OPTION_COUNT] =
{
	{ 0, LOCALE_OPTIONS_OFF },
	{ 1, LOCALE_OPTIONS_ON  }
};
#endif

int CVfdSetup::showSetup()
{
	CMenuWidget *vfds = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_LCD, width, MN_WIDGET_ID_VFDSETUP);

#if BOXMODEL_E4HDULTRA|| BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUDUO4KSE || BOXMODEL_VUULTIMO4K || BOXMODEL_VUUNO4KSE
	vfds->addIntroItems(LOCALE_LCDMENU_HEAD, NONEXISTANT_LOCALE, 0, false, false);
#else
	vfds->addIntroItems(LOCALE_LCDMENU_HEAD);
#endif

	CMenuForwarder * mf;

	if (g_info.hw_caps->display_can_set_brightness)
	{
		//vfd brightness menu
		mf = new CMenuForwarder(LOCALE_LCDMENU_LCDCONTROLER, true, NULL, this, "brightness", CRCInput::RC_green);
		mf->setHint("", LOCALE_MENU_HINT_VFD_BRIGHTNESS_SETUP);
		vfds->addItem(mf);
	}

	if (CVFD::getInstance()->has_lcd)
	{

		CMenuOptionChooser* oj;

#if ENABLE_LCD
#if 0
		//option power
		oj = new CMenuOptionChooser("Power LCD"/*LOCALE_LCDMENU_POWER*/, &g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, new CLCDNotifier(), CRCInput::RC_nokey);
		vfds->addItem(oj);
#endif
#if BOXMODEL_DM820 || BOXMODEL_DM7080
		g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE] = 0;
#else
		//option invert
		oj = new CMenuOptionChooser("Invert LCD"/*LOCALE_LCDMENU_INVERSE*/, &g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, new CLCDNotifier(), CRCInput::RC_nokey);
		vfds->addItem(oj);
#endif
#endif

		if (g_info.hw_caps->display_has_statusline)
		{
			//status line options
			oj = new CMenuOptionChooser(LOCALE_LCDMENU_STATUSLINE, &g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME], LCDMENU_STATUSLINE_OPTIONS, LCDMENU_STATUSLINE_OPTION_COUNT, true);
			oj->setHint("", LOCALE_MENU_HINT_VFD_STATUSLINE);
			vfds->addItem(oj);
		}

#if BOXMODEL_DM820 || BOXMODEL_DM7080
		g_settings.lcd_info_line = 0;
		g_settings.lcd_scroll = 0;
#else
		//info line options
		oj = new CMenuOptionChooser(LOCALE_LCD_INFO_LINE, &g_settings.lcd_info_line, LCD_INFO_OPTIONS, LCD_INFO_OPTION_COUNT, true);
		oj->setHint("", LOCALE_MENU_HINT_VFD_INFOLINE);
		vfds->addItem(oj);

#if HAVE_DUCKBOX_HARDWARE
		vfds->addItem(new CMenuOptionNumberChooser(LOCALE_LCDMENU_VFD_SCROLL, &g_settings.lcd_vfd_scroll, true, 0, 999, this, 0, 0, NONEXISTANT_LOCALE, true));
#elif HAVE_SPARK_HARDWARE
		vfds->addItem(new CMenuOptionNumberChooser(LOCALE_LCDMENU_VFD_SCROLL, &g_settings.lcd_vfd_scroll, (g_info.hw_caps->display_type == HW_DISPLAY_LINE_TEXT), 0, 999, this, 0, 0, NONEXISTANT_LOCALE, true));
#else
		//scroll options
		if (file_exists("/proc/stb/lcd/scroll_repeats"))
		{
			// allow to set scroll_repeats
			CMenuOptionNumberChooser * nc = new CMenuOptionNumberChooser(LOCALE_LCDMENU_SCROLL_REPEATS, &g_settings.lcd_scroll, true, 0, 999, this);
			nc->setLocalizedValue(0);
			nc->setLocalizedValueName(LOCALE_OPTIONS_OFF);
			nc->setHint("", LOCALE_MENU_HINT_VFD_SCROLL);
			vfds->addItem(nc);
		}
		else
		{
			// simple on/off chooser
			oj = new CMenuOptionChooser(LOCALE_LCDMENU_SCROLL, &g_settings.lcd_scroll, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
			oj->setHint("", LOCALE_MENU_HINT_VFD_SCROLL);
			vfds->addItem(oj);
		}
#endif
#endif

		//notify rc-lock
		oj = new CMenuOptionChooser(LOCALE_LCDMENU_NOTIFY_RCLOCK, &g_settings.lcd_notify_rclock, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
		oj->setHint("", LOCALE_MENU_HINT_VFD_NOTIFY_RCLOCK);
		vfds->addItem(oj);
	}

	if (g_info.hw_caps->display_type == HW_DISPLAY_LED_NUM)
	{
		//LED NUM info line options
		CMenuOptionChooser* led_num;
		led_num = new CMenuOptionChooser(LOCALE_LCD_INFO_LINE, &g_settings.lcd_info_line, LCD_INFO_OPTIONS, LCD_INFO_OPTION_COUNT, true);
		led_num->setHint("", LOCALE_MENU_HINT_VFD_INFOLINE);
		vfds->addItem(led_num);
	}

#if !defined BOXMODEL_VUSOLO4K && !defined BOXMODEL_VUDUO4K && !defined BOXMODEL_VUDUO4KSE && !defined BOXMODEL_VUULTIMO4K && !defined BOXMODEL_VUUNO4KSE && !defined BOXMODEL_DM820 && !defined BOXMODEL_DM7080 && !defined BOXMODEL_DM900 && !defined BOXMODEL_DM8000 && !defined BOXMODEL_E4HDULTRA && !defined BOXMODEL_VUDUO2
#if ENABLE_LCD4LINUX && ENABLE_GRAPHLCD
	if (g_settings.glcd_enable != 0 && g_settings.lcd4l_support != 0) {
		g_settings.glcd_enable = 0;
		g_settings.lcd4l_support = 0;
	}
#endif
#endif

#ifdef ENABLE_LCD4LINUX
#if !defined BOXMODEL_VUSOLO4K && !defined BOXMODEL_VUDUO4K && !defined BOXMODEL_VUDUO4KSE && !defined BOXMODEL_VUULTIMO4K && !defined BOXMODEL_VUUNO4KSE && !defined BOXMODEL_DM820 && !defined BOXMODEL_DM7080 && !defined BOXMODEL_DM900 && !defined BOXMODEL_DM8000 && !defined BOXMODEL_E4HDULTRA && !defined BOXMODEL_VUDUO2 && defined (ENABLE_GRAPHLCD)
	if (g_settings.glcd_enable == 0)
#endif
	{
		vfds->addItem(GenericMenuSeparatorLine);
		vfds->addItem(new CMenuForwarder(LOCALE_LCD4L_SUPPORT, ((access("/usr/bin/lcd4linux", F_OK) == 0) || (access("/var/bin/lcd4linux", F_OK) == 0)), NULL, new CLCD4lSetup(), NULL, CRCInput::RC_yellow));
	}
#endif

#ifdef ENABLE_GRAPHLCD
	GLCD_Menu glcdMenu;
#ifdef ENABLE_LCD4LINUX
#if !defined BOXMODEL_VUSOLO4K && !defined BOXMODEL_VUDUO4K && !defined BOXMODEL_VUDUO4KSE && !defined BOXMODEL_VUULTIMO4K && !defined BOXMODEL_VUUNO4KSE && !defined BOXMODEL_DM820 && !defined BOXMODEL_DM7080 && !defined BOXMODEL_DM900 && !defined BOXMODEL_DM8000 && !defined BOXMODEL_E4HDULTRA && !defined BOXMODEL_VUDUO2
	if (g_settings.lcd4l_support == 0)
#endif
#endif
	{
		vfds->addItem(GenericMenuSeparatorLine);
		vfds->addItem(new CMenuForwarder(LOCALE_GLCD_HEAD, true, NULL, &glcdMenu, NULL, CRCInput::RC_blue));
	}
#endif

	int res = vfds->exec(NULL, "");

	delete vfds;
	return res;
}

int CVfdSetup::showBrightnessSetup()
{
	CMenuOptionNumberChooser * nc;
	CMenuForwarder * mf;

	CMenuWidget *mn_widget = new CMenuWidget(LOCALE_LCDMENU_HEAD, NEUTRINO_ICON_LCD,width, MN_WIDGET_ID_VFDSETUP_LCD_SLIDERS);

	mn_widget->addIntroItems(LOCALE_LCDMENU_LCDCONTROLER);

	brightness = CVFD::getInstance()->getBrightness();
	brightnessstandby = CVFD::getInstance()->getBrightnessStandby();
	brightnessdeepstandby = CVFD::getInstance()->getBrightnessDeepStandby();

#if defined (ENABLE_LCD)
	nc = new CMenuOptionNumberChooser(LOCALE_LCDCONTROLER_BRIGHTNESS, &brightness, true, 0, 255, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true);
#elif defined (HAVE_ARM_HARDWARE) || defined (HAVE_MIPS_HARDWARE)
	nc = new CMenuOptionNumberChooser(LOCALE_LCDCONTROLER_BRIGHTNESS, &brightness, true, 0, 15, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true);
#else
	nc = new CMenuOptionNumberChooser(LOCALE_LCDCONTROLER_BRIGHTNESS, &brightness, true, 0, 7, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true);
#endif
	nc->setHint("", LOCALE_MENU_HINT_VFD_BRIGHTNESS);
	nc->setActivateObserver(this);
	mn_widget->addItem(nc);

#if defined (ENABLE_LCD)
	nc = new CMenuOptionNumberChooser(LOCALE_LCDCONTROLER_BRIGHTNESSSTANDBY, &brightnessstandby, true, 0, 255, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true);
#elif defined (HAVE_ARM_HARDWARE) || defined (HAVE_MIPS_HARDWARE)
	nc = new CMenuOptionNumberChooser(LOCALE_LCDCONTROLER_BRIGHTNESSSTANDBY, &brightnessstandby, true, 0, 15, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true);
#else
	nc = new CMenuOptionNumberChooser(LOCALE_LCDCONTROLER_BRIGHTNESSSTANDBY, &brightnessstandby, true, 0, 7, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true);
#endif
	nc->setHint("", LOCALE_MENU_HINT_VFD_BRIGHTNESSSTANDBY);
	nc->setActivateObserver(this);
	mn_widget->addItem(nc);

	if (g_info.hw_caps->display_can_deepstandby)
	{
#if defined (ENABLE_LCD)
		nc = new CMenuOptionNumberChooser(LOCALE_LCDCONTROLER_BRIGHTNESSDEEPSTANDBY, &brightnessdeepstandby, true, 0, 255, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true);
#elif defined (HAVE_ARM_HARDWARE) || defined (HAVE_MIPS_HARDWARE)
		nc = new CMenuOptionNumberChooser(LOCALE_LCDCONTROLER_BRIGHTNESSDEEPSTANDBY, &brightnessdeepstandby, true, 0, 15, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true);
#else
		nc = new CMenuOptionNumberChooser(LOCALE_LCDCONTROLER_BRIGHTNESSDEEPSTANDBY, &brightnessdeepstandby, true, 0, 7, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true);
#endif
		nc->setHint("", LOCALE_MENU_HINT_VFD_BRIGHTNESSDEEPSTANDBY);
		nc->setActivateObserver(this);
		mn_widget->addItem(nc);
	}

#if defined (ENABLE_LCD)
	nc = new CMenuOptionNumberChooser(LOCALE_LCDMENU_DIM_BRIGHTNESS, &g_settings.lcd_setting_dim_brightness, true, -1, 255, NULL, CRCInput::RC_nokey, NULL, 0, -1, LOCALE_OPTIONS_OFF, true);
#elif defined (HAVE_ARM_HARDWARE) || defined (HAVE_MIPS_HARDWARE)
	nc = new CMenuOptionNumberChooser(LOCALE_LCDMENU_DIM_BRIGHTNESS, &g_settings.lcd_setting_dim_brightness, true, -1, 15, NULL, CRCInput::RC_nokey, NULL, 0, -1, LOCALE_OPTIONS_OFF, true);
#else
	nc = new CMenuOptionNumberChooser(LOCALE_LCDMENU_DIM_BRIGHTNESS, &g_settings.lcd_setting_dim_brightness, true, -1, 7, NULL, CRCInput::RC_nokey, NULL, 0, -1, LOCALE_OPTIONS_OFF, true);
#endif
	nc->setHint("", LOCALE_MENU_HINT_VFD_BRIGHTNESSDIM);
	nc->setActivateObserver(this);
	mn_widget->addItem(nc);

	mn_widget->addItem(GenericMenuSeparatorLine);
	CStringInput *dim_time = new CStringInput(LOCALE_LCDMENU_DIM_TIME, &g_settings.lcd_setting_dim_time, 3, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE,"0123456789 ");

	mf = new CMenuForwarder(LOCALE_LCDMENU_DIM_TIME, true, g_settings.lcd_setting_dim_time, dim_time);
	mf->setHint("", LOCALE_MENU_HINT_VFD_DIMTIME);
	mf->setActivateObserver(this);
	mn_widget->addItem(mf);

	mn_widget->addItem(GenericMenuSeparatorLine);
	mf = new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, "def", CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_VFD_DEFAULTS);
	mf->setActivateObserver(this);
	mn_widget->addItem(mf);

	int res = mn_widget->exec(this, "");
	delete mn_widget;

	g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = brightness;
	g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = brightnessstandby;
	return res;
}

bool CVfdSetup::changeNotify(const neutrino_locale_t OptionName, void * /* data */)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESS)) {
		CVFD::getInstance()->setBrightness(brightness);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESSSTANDBY)) {
		CVFD::getInstance()->setBrightnessStandby(brightnessstandby);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESSDEEPSTANDBY)) {
		CVFD::getInstance()->setBrightnessStandby(brightnessdeepstandby);
		CVFD::getInstance()->setBrightnessDeepStandby(brightnessdeepstandby);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDMENU_DIM_BRIGHTNESS)) {
		CVFD::getInstance()->setBrightness(g_settings.lcd_setting_dim_brightness);
#if !HAVE_SH4_HARDWARE
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDMENU_SCROLL) || ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDMENU_SCROLL_REPEATS)) {
		CVFD::getInstance()->setScrollMode(g_settings.lcd_scroll);
#endif
	}

	return false;
}

void CVfdSetup::activateNotify(const neutrino_locale_t OptionName)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESSSTANDBY)) {
		g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = brightnessstandby;
		CVFD::getInstance()->setMode(CVFD::MODE_STANDBY);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESSDEEPSTANDBY)) {
		g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = brightnessdeepstandby;
		CVFD::getInstance()->setMode(CVFD::MODE_STANDBY);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESS)) {
		g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = brightness;
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDMENU_DIM_BRIGHTNESS)) {
		g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = g_settings.lcd_setting_dim_brightness;
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	} else {
		g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = brightness;
		CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);
	}
}

#if ENABLE_LCD
// lcd notifier
bool CLCDNotifier::changeNotify(const neutrino_locale_t, void * Data)
{
	int state = *(int *)Data;

	dprintf(DEBUG_NORMAL, "CLCDNotifier: state: %d\n", state);
#if 0
	CVFD::getInstance()->setPower(state);
#else
	CVFD::getInstance()->setPower(1);
#endif
	CVFD::getInstance()->setlcdparameter();

	return true;
}
#endif
