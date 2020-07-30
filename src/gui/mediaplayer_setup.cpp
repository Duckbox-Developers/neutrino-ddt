/*
	$port: mediaplayer_setup.cpp,v 1.5 2010/12/06 21:00:15 tuxbox-cvs Exp $

	Neutrino-GUI  -   DBoxII-Project

	media player setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011 T. Graf 'dbt'
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

#include "mediaplayer_setup.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>

#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>

#include <gui/audioplayer_setup.h>
#include <gui/pictureviewer_setup.h>
#include <gui/webtv_setup.h>
#include <gui/xmltv_setup.h>
#include <gui/moviebrowser/mb.h>

#include <driver/screen_max.h>

#include <system/debug.h>

CMediaPlayerSetup::CMediaPlayerSetup()
{
	width = 40;
	selected = -1;
}

CMediaPlayerSetup::~CMediaPlayerSetup()
{
}

int CMediaPlayerSetup::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	dprintf(DEBUG_DEBUG, "init mediaplayer setup menu\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();


	res = showMediaPlayerSetup();
	
	return res;
}

/*shows media setup menue entries*/
int CMediaPlayerSetup::showMediaPlayerSetup()
{
	CMenuWidget* mediaSetup = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width);
	mediaSetup->setSelected(selected);

	// intros
	mediaSetup->addIntroItems(LOCALE_MAINMENU_MEDIA);

	CMenuForwarder *mf;

	CAudioPlayerSetup asetup;
	mf = new CMenuForwarder(LOCALE_AUDIOPLAYER_NAME, true, NULL, &asetup, "", CRCInput::RC_red);
	mf->setHint(NEUTRINO_ICON_HINT_APLAY, LOCALE_MENU_HINT_APLAY_SETUP);
	mediaSetup->addItem(mf);

	CPictureViewerSetup psetup;
	mf = new CMenuForwarder(LOCALE_PICTUREVIEWER_HEAD, true, NULL, &psetup, "", CRCInput::RC_green);
	mf->setHint(NEUTRINO_ICON_HINT_PICVIEW, LOCALE_MENU_HINT_PICTUREVIEWER_SETUP);
	mediaSetup->addItem(mf);

	mediaSetup->addItem(new CMenuSeparator(CMenuSeparator::LINE));

	CWebTVSetup wsetup;
	mf = new CMenuForwarder(LOCALE_WEBTV_HEAD, true, NULL, &wsetup, "show_menu", CRCInput::RC_yellow);
	mf->setHint(NEUTRINO_ICON_HINT_TVMODE /* FIXME */, LOCALE_MENU_HINT_WEBTV_SETUP);
	mediaSetup->addItem(mf);

	CXMLTVSetup xmltvsetup;
	mf = new CMenuForwarder(LOCALE_XMLTV_HEAD, true, NULL, &xmltvsetup, "show_menu", CRCInput::RC_blue);
	mf->setHint(NEUTRINO_ICON_HINT_TVMODE /* FIXME */, LOCALE_MENU_HINT_XMLTV_SETUP);
	mediaSetup->addItem(mf);

	mediaSetup->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MAINMENU_MOVIEPLAYER));

	CMovieBrowser msetup;
	int shortcut = 1;
	mf = new CMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, &msetup, "show_menu", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_MB, LOCALE_MENU_HINT_MOVIEBROWSER_SETUP);
	mediaSetup->addItem(mf);

	mediaSetup->addItem(GenericMenuSeparator);

	CMenuOptionChooser *mc;
	mc = new CMenuOptionChooser(LOCALE_MOVIEPLAYER_DISPLAY_PLAYTIME, &g_settings.movieplayer_display_playtime, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_info.hw_caps->display_xres >= 8);
	mc->setHint("", LOCALE_MENU_HINT_MOVIEPLAYER_DISPLAY_PLAYTIME);
	mediaSetup->addItem(mc);

	int res = mediaSetup->exec (NULL, "");
	selected = mediaSetup->getSelected();
	delete mediaSetup;
	return res;
}
