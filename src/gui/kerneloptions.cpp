/*
	KernelOptions Menu

	Copyright (C) 2012-2013 martii

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <config.h>
#include <global.h>
#include <neutrino.h>
#include <sys/stat.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <driver/screen_max.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/menue.h>
#include <gui/kerneloptions.h>

#define ONOFF_OPTION_COUNT 2
static const CMenuOptionChooser::keyval ONOFF_OPTIONS[ONOFF_OPTION_COUNT] = {
	{0, LOCALE_OPTIONS_OFF},
	{1, LOCALE_OPTIONS_ON}
};

CKernelOptions::CKernelOptions()
{
	width = 40;
}

void CKernelOptions::loadModule(int i)
{
	for (unsigned int j = 0; j < modules[i].moduleList.size(); j++) {
		if (modules[i].moduleList[j].second.empty())
		{
			//printf(" %s\n", ("command: insmod " + modules[i].moduleList[j].first).c_str());
			my_system(2, "insmod", modules[i].moduleList[j].first.c_str());
		}
		else
		{
			//printf(" %s\n", ("command: insmod " + modules[i].moduleList[j].first + " " + modules[i].moduleList[j].second).c_str());
			system(("insmod " + modules[i].moduleList[j].first + " " + modules[i].moduleList[j].second).c_str());
		}
	}
}

void CKernelOptions::unloadModule(int i)
{
	for (unsigned int j = modules[i].moduleList.size(); j > 0; j--)
		my_system(2, "rmmod", modules[i].moduleList[j - 1].first.c_str());
}

void CKernelOptions::updateStatus(void)
{
	for (unsigned int i = 0; i < modules.size(); i++)
		modules[i].installed = false;
	FILE *f = fopen("/proc/modules", "r");
	if (f) {
		char buf[200];
		while (fgets(buf, sizeof(buf), f)) {
			char name[sizeof(buf)];
			if (1 == sscanf(buf, "%s", name))
				for (unsigned int i = 0; i < modules.size(); i++) {
					if (name == modules[i].moduleList.back().first) {
						modules[i].installed = true;
						break;
					}
				}
		}
		fclose(f);
	}
	for (unsigned int i = 0; i < modules.size(); i++)
		if (modules[i].mc) {
			modules[i].mc->setMarked(modules[i].installed);
		neutrino_locale_t l;
		if (modules[i].active)
			l = modules[i].
				installed ? LOCALE_KERNELOPTIONS_HINT_ENABLED_LOADED : LOCALE_KERNELOPTIONS_HINT_ENABLED_NOT_LOADED;
		else
			l = modules[i].
		installed ? LOCALE_KERNELOPTIONS_HINT_DISABLED_LOADED : LOCALE_KERNELOPTIONS_HINT_DISABLED_NOT_LOADED;
		modules[i].mc->setHint("", l);
		}
}

int CKernelOptions::exec(CMenuTarget * parent, const std::string & actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	if (actionKey == "reset") {
		for (unsigned int i = 0; i < modules.size(); i++)
			modules[i].active = modules[i].active_orig;
		updateStatus();
		return res;
	}

	if (actionKey == "apply" || actionKey == "change") {
		bool needs_save = false;
		for (unsigned int i = 0; i < modules.size(); i++)
			if (modules[i].active != modules[i].active_orig) {
				needs_save = true;
				if (modules[i].active)
					loadModule(i);
				else
					unloadModule(i);
				modules[i].active_orig = modules[i].active;
			}
		if (needs_save)
			save();
		updateStatus();
		return res;
	}

	if (parent)
		parent->hide();

	res = Settings();

	return res;
}

void CKernelOptions::hide()
{
}

bool CKernelOptions::isEnabled(std::string name)
{
	load();
	for (unsigned int i = 0; i < modules.size(); i++)
		if (name == modules[i].moduleList.back().first)
			return modules[i].active;
	return false;
}

bool CKernelOptions::Enable(std::string name, bool active)
{
	load();
	for (unsigned int i = 0; i < modules.size(); i++)
		if (name == modules[i].moduleList.back().first) {
			if (modules[i].active != active) {
				modules[i].active = active;
				exec(NULL, "change");
			}
			return true;
		}
	return false;
}

void CKernelOptions::load()
{
	modules.clear();

	FILE *f = fopen("/etc/modules.available", "r");
	// Syntax:
	//
	// # comment
	// module # description
	// module module module # description
	// module module(arguments) module # description
	//

	if (f) {
		char buf[200];
		while (fgets(buf, sizeof(buf), f)) {
			if (buf[0] == '#')
				continue;
			char *comment = strchr(buf, '#');
			if (!comment)
				continue;
			*comment++ = 0;
			while (*comment == ' ' || *comment == '\t')
				comment++;
			if (strlen(comment) < 1)
				continue;
			module m;
			m.active = m.active_orig = 0;
			m.installed = false;
			char *nl = strchr(comment, '\n');
			if (nl)
				*nl = 0;
			m.comment = std::string(comment);
			char *b = buf;
			while (*b) {
				if (*b == ' ' || *b == '\t') {
					b++;
					continue;
				}
				std::string args = "";
				std::string mod;
				char *e = b;
				char *a = NULL;
				while (*e && ((a && *e != ')') || (!a && *e != ' ' && *e != '\t'))) {
					if (*e == '(')
						a = e;
					e++;
				}
				if (a && *e == ')') {
					*a++ = 0;
					*e++ = 0;
					args = std::string(a);
					*a = 0;
					mod = std::string(b);
					b = e;
				} else if (*e) {
					*e++ = 0;
					mod = std::string(b);
					b = e;
				} else {
					mod = std::string(b);
					b = e;
			}
			m.moduleList.push_back(make_pair(mod, args));
		}
		m.mc = NULL;
		if (m.moduleList.size() > 0)
			modules.push_back(m);
	}
	fclose(f);
	}

	f = fopen("/etc/modules.extra", "r");
	if (f) {
		char buf[200];
		while (fgets(buf, sizeof(buf), f)) {
			char *t = strchr(buf, '#');
			if (t)
				*t = 0;
			char name[200];
			if (1 == sscanf(buf, "%s", name)) {
				for (unsigned int i = 0; i < modules.size(); i++)
					if (modules[i].moduleList.back().first == name) {
						modules[i].active = modules[i].active_orig = 1;
						break;
					}
			}
		}
		fclose(f);
	}
}

void CKernelOptions::save()
{
	FILE *f = fopen("/etc/modules.extra", "w");
	if (f) {
		chmod("/etc/modules.extra", 0644);
		for (unsigned int i = 0; i < modules.size(); i++) {
			if (modules[i].active) {
				for (unsigned int j = 0; j < modules[i].moduleList.size(); j++)
					if (modules[i].moduleList[j].second.length())
						fprintf(f, "%s %s\n", modules[i].moduleList[j].first.c_str(), modules[i].moduleList[j].second.c_str());
					else
						fprintf(f, "%s\n", modules[i].moduleList[j].first.c_str());
			}
		}
		fclose(f);
	}
}

#define KernelOptionsButtonCount 2
static const struct button_label KernelOptionsButtons[KernelOptionsButtonCount] = {
	{NEUTRINO_ICON_BUTTON_RED, LOCALE_KERNELOPTIONS_RESET},
	{NEUTRINO_ICON_BUTTON_GREEN, LOCALE_KERNELOPTIONS_APPLY}
};

bool CKernelOptions::changeNotify(const neutrino_locale_t /*OptionName */ , void * /*Data */ )
{
	updateStatus();
	return true;
}

int CKernelOptions::Settings()
{
	CMenuWidget *menu = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS);
	menu->addKey(CRCInput::RC_red, this, "reset");
	menu->addKey(CRCInput::RC_green, this, "apply");
	menu->setFooter(KernelOptionsButtons, KernelOptionsButtonCount);
	menu->addIntroItems(LOCALE_KERNELOPTIONS_HEAD, LOCALE_KERNELOPTIONS_MODULES);

	load();

	for (unsigned int i = 0; i < modules.size(); i++) {
		modules[i].mc = new CMenuOptionChooser(modules[i].comment.c_str(), &modules[i].active,
				ONOFF_OPTIONS, ONOFF_OPTION_COUNT, true, this);
		menu->addItem(modules[i].mc);
	}

	updateStatus();

	int ret = menu->exec(NULL, "");
	//menu->hide();
	delete menu;
	return ret;
}
