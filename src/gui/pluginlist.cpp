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

#include <plugin.h>

#include <gui/pluginlist.h>
#include <gui/components/cc.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/icons.h>

#include <sstream>
#include <fstream>
#include <iostream>

#include <dirent.h>
#include <dlfcn.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <global.h>
#include <neutrino.h>
#include <driver/screen_max.h>
#include <driver/fade.h>

#include <zapit/client/zapittools.h>
#include <system/helpers.h>

#include "plugins.h"

extern CPlugins       * g_PluginList;    /* neutrino.cpp */

CPluginList::CPluginList(const neutrino_locale_t Title, const uint32_t listtype)
{
	title = Title;
	pluginlisttype = listtype;
	width = 40;
	selected = -1;
	number = -1;
}

int CPluginList::run()
{
	g_PluginList->startPlugin(number);
	if (!g_PluginList->getScriptOutput().empty()) {
		hide();
		ShowMsg(LOCALE_PLUGINS_RESULT, g_PluginList->getScriptOutput(), CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_SHELL);
	}
	return menu_return::RETURN_REPAINT;
}

int CPluginList::exec(CMenuTarget* parent, const std::string &actionKey)
{
	if (parent)
		parent->hide();

	CColorKeyHelper keyhelper;
	neutrino_msg_t key = CRCInput::RC_nokey;
	const char * dummy = NULL;

	number = -1;
	if (!actionKey.empty())
		number = atoi(actionKey.c_str());

	if (number > -1)
		return run();

	const char *icon = "";
	if (pluginlisttype == CPlugins::P_TYPE_GAME)
		icon = NEUTRINO_ICON_GAMES;
	else
		icon = NEUTRINO_ICON_SHELL;

	CMenuWidget m(title, icon, width);
	m.setSelected(selected);
	m.addIntroItems();

	int nop = g_PluginList->getNumberOfPlugins();

	for(int count = 0; count < nop; count++) {
		if ((g_PluginList->getType(count) & pluginlisttype) && !g_PluginList->isHidden(count) && (g_PluginList->getIntegration(count) == CPlugins::I_TYPE_DISABLED)) {
			neutrino_msg_t d_key = g_PluginList->getKey(count);
			keyhelper.get(&key, &dummy, d_key);
			CMenuForwarder *f = new CMenuForwarder(std::string(g_PluginList->getName(count)), true, NULL, this, to_string(count).c_str(), key);
			f->setHint(g_PluginList->getHintIcon(count), g_PluginList->getDescription(count));
			m.addItem(f);
		}
	}
	int res = m.exec(NULL, "");
	m.hide();
	selected = m.getSelected();

	return res;
}

CPluginChooser::CPluginChooser(const neutrino_locale_t Name, const uint32_t listtype, std::string &selectedFile) : CPluginList(Name, listtype)
{
	selectedFilePtr = &selectedFile;
}

int CPluginChooser::run()
{
	if (number > -1)
		*selectedFilePtr = g_PluginList->getFileName(number);
	return menu_return::RETURN_EXIT;
}

CPluginsExec* CPluginsExec::getInstance()
{
	static CPluginsExec* pluginsExec = NULL;

	if (!pluginsExec)
		pluginsExec = new CPluginsExec();

	return pluginsExec;
}

int CPluginsExec::exec(CMenuTarget* parent, const std::string & actionKey)
{
	if (actionKey.empty())
		return menu_return::RETURN_NONE;

	//printf("CPluginsExec exec: %s\n", actionKey.c_str());
	int sel = atoi(actionKey.c_str());

	if (parent != NULL)
		parent->hide();

	if (actionKey == "teletext") {
		g_RCInput->postMsg(CRCInput::RC_timeout, 0);
		g_RCInput->postMsg(CRCInput::RC_text, 0);
		return menu_return::RETURN_EXIT;
	}
	else if (sel >= 0)
		g_PluginList->startPlugin(sel);

	if (!g_PluginList->getScriptOutput().empty())
		ShowMsg(LOCALE_PLUGINS_RESULT, g_PluginList->getScriptOutput(), CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_SHELL);

	if (g_PluginList->getIntegration(sel) == CPlugins::I_TYPE_DISABLED)
		return menu_return::RETURN_EXIT;

	return menu_return::RETURN_REPAINT;
}
