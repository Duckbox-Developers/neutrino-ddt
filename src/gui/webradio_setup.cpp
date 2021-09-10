/*
	WebRadio Setup

	(C) 2012-2013 by martii
	Mod to WebRadio 2017 TangoCash


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

#define __USE_FILE_OFFSET64 1
#include "filebrowser.h"
#include <stdio.h>
#include <global.h>
#include <libgen.h>
#include <neutrino.h>
#include <driver/screen_max.h>
#include <driver/framebuffer.h>
#include <gui/movieplayer.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/keyboard_input.h>
#include <zapit/zapit.h>
#include <neutrino_menue.h>
#include <thread>
#include "webradio_setup.h"

CWebRadioSetup::CWebRadioSetup()
{
	width = 55;
	selected = -1;
	item_offset = 0;
	changed = false;
}

#define CWebRadioSetupFooterButtonCount 4
static const struct button_label CWebRadioSetupFooterButtons[CWebRadioSetupFooterButtonCount] = {
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_WEBRADIO_XML_DEL },
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_WEBRADIO_XML_ADD },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_WEBTV_XML_ENTER },
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_WEBRADIO_XML_RELOAD }
};

int CWebRadioSetup::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	if(actionKey == "d" /* delete */)
	{
		selected = m->getSelected();
		if (selected >= item_offset)
		{
			m->removeItem(selected);
			m->hide();
			selected = m->getSelected();
			changed = true;
		}
		return res;
	}
	if(actionKey == "c" /* change */)
	{
		selected = m->getSelected();
		CMenuItem* item = m->getItem(selected);
		CMenuForwarder *f = static_cast<CMenuForwarder*>(item);
		std::string dirname(f->getName());
		if (strstr(dirname.c_str(), "://"))
		{
			std::string entry = dirname;

			CKeyboardInput *e = new CKeyboardInput(LOCALE_WEBTV_XML_ENTER, &entry, 50);
			e->exec(this, "");
			delete e;

			if (entry.compare(dirname) != 0)
			{
				f->setName(entry);
				changed = true;
			}
		}
		else
		{
			CFileBrowser fileBrowser;
			CFileFilter fileFilter;
			fileFilter.addFilter("xml");
			fileFilter.addFilter("tv");
			fileFilter.addFilter("m3u");
			fileBrowser.Filter = &fileFilter;

			dirname = dirname.substr(0, dirname.rfind('/'));
			if (fileBrowser.exec(dirname.c_str()))
			{
				f->setName(fileBrowser.getSelectedFile()->Name);
				g_settings.last_webradio_dir = dirname;
				changed = true;
			}
		}
		return res;
	}
	if(actionKey == "a" /* add */)
	{
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("xml");
		fileFilter.addFilter("tv");
		fileFilter.addFilter("m3u");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(g_settings.last_webradio_dir.c_str()) == true)
		{
			std::string s = fileBrowser.getSelectedFile()->Name;
			m->addItem(new CMenuForwarder(s, true, NULL, this, "c"));
			g_settings.last_webradio_dir = s.substr(0, s.rfind('/')).c_str();
			changed = true;
		}
		return res;
	}
	if (actionKey == "e" /* enter */)
	{
		std::string tpl = "http://xxx.xxx.xxx.xxx/control/xmltv.m3u?mode=radio";
		std::string entry = tpl;

		CKeyboardInput *e = new CKeyboardInput(LOCALE_WEBTV_XML_ENTER, &entry, 52);
		e->exec(this, "");
		delete e;

		if (entry.compare(tpl) != 0)
		{
			m->addItem(new CMenuForwarder(entry, true, NULL, this, "c"));
			changed = true;
		}
		return res;
	}
	if(actionKey == "r" /* reload */)
	{
		changed = true;
		return menu_return::RETURN_EXIT_ALL;
	}
	if (actionKey == "script_path")
	{
		const char *action_str = "ScriptPath";
		chooserDir(g_settings.livestreamScriptPath, false, action_str);
		return res;
	}

	if(parent)
		parent->hide();

	res = Show();

	return res;
}

int CWebRadioSetup::Show()
{
	item_offset = 0;

	m = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_AUDIOPLAY, width, MN_WIDGET_ID_WEBTVSETUP);
	m->addKey(CRCInput::RC_red, this, "d");
	m->addKey(CRCInput::RC_green, this, "a");
	m->addKey(CRCInput::RC_yellow, this, "e");
	m->addKey(CRCInput::RC_blue, this, "r");

	m->addIntroItems(LOCALE_WEBRADIO_HEAD, LOCALE_LIVESTREAM_HEAD);

	bool _mode_webradio = (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webradio) &&
				(!CZapit::getInstance()->GetCurrentChannel()->getScriptName().empty());

	CMenuForwarder *mf;
	int shortcut = 1;
	mf = new CMenuForwarder(LOCALE_LIVESTREAM_SCRIPTPATH, !_mode_webradio, g_settings.livestreamScriptPath, this, "script_path", CRCInput::convertDigitToKey(shortcut++));
	m->addItem(mf);

	m->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_WEBRADIO_XML));

	item_offset = m->getItemsCount();
	for (std::list<std::string>::iterator it = g_settings.webradio_xml.begin(); it != g_settings.webradio_xml.end(); ++it)
		m->addItem(new CMenuForwarder(*it, true, NULL, this, "c"));

	m->setFooter(CWebRadioSetupFooterButtons, CWebRadioSetupFooterButtonCount); //Why we need here an extra buttonbar?

	int res = m->exec(NULL, "");
	m->hide();
	if (changed)
	{
		CHintBox hint(LOCALE_MESSAGEBOX_INFO, LOCALE_SERVICEMENU_RELOAD_HINT);
		hint.paint();
		g_settings.webradio_xml.clear();
		for (int i = item_offset; i < m->getItemsCount(); i++)
		{
			CMenuItem *item = m->getItem(i);
			CMenuForwarder *f = static_cast<CMenuForwarder*>(item);
			g_settings.webradio_xml.push_back(f->getName());
		}
		g_Zapit->reinitChannels();
		std::thread t1(&CNeutrinoApp::xmltv_xml_m3u_readepg, CNeutrinoApp::getInstance());
		t1.detach();
		changed = false;
		hint.hide();
	}

	delete m;

	return res;
}
