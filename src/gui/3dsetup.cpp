/*
	(C)2012 by martii

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

#include <gui/3dsetup.h>

#include <driver/fontrenderer.h>
#include <driver/framebuffer.h>
#include <driver/screen_max.h>

#include <gui/widget/menue.h>
#include <gui/widget/icons.h>
#include <gui/widget/hintbox.h>

#include <zapit/zapit.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <global.h>
#include <neutrino.h>

static const CMenuOptionChooser::keyval THREE_D_OPTIONS[THREE_D_OPTIONS_COUNT] =
{
	{CFrameBuffer::Mode3D_off,		LOCALE_THREE_D_OFF },
	{CFrameBuffer::Mode3D_SideBySide,	LOCALE_THREE_D_SIDESIDE },
	{CFrameBuffer::Mode3D_TopAndBottom,	LOCALE_THREE_D_TOPBOTTOM }
#if !HAVE_ARM_HARDWARE && !HAVE_MIPS_HARDWARE
	,
	{CFrameBuffer::Mode3D_Tile,		LOCALE_THREE_D_TILE }
#endif
};

static C3DSetup::threeDList tdl[THREE_D_OPTIONS_COUNT] =
{
	{ NULL, "off",		CFrameBuffer::Mode3D_off },
	{ NULL, "sidebyside",	CFrameBuffer::Mode3D_SideBySide },
	{ NULL, "topandbottom",	CFrameBuffer::Mode3D_TopAndBottom }
#if !HAVE_ARM_HARDWARE && !HAVE_MIPS_HARDWARE
	,
	{ NULL, "tile",		CFrameBuffer::Mode3D_Tile }
#endif
};

C3DSetup::C3DSetup(void)
{
	width = 40;
	selected = -1;
	frameBuffer = CFrameBuffer::getInstance();
}

C3DSetup::~C3DSetup()
{
	threeDMap.clear();
	for (int i = 0; i < THREE_D_OPTIONS_COUNT; i++)
		if(tdl[i].cmf)
			delete tdl[i].cmf;
}

int C3DSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	int   res = menu_return::RETURN_REPAINT;
	fb_pixel_t *pixbuf = NULL;

	for (int i = 0; i < THREE_D_OPTIONS_COUNT; i++) {
		if (actionKey == tdl[i].actionKey) {
			frameBuffer->set3DMode(tdl[i].mode);
			for (int j = 0; j < THREE_D_OPTIONS_COUNT; j++)
				tdl[j].cmf->setOption(g_Locale->getText((i == j) ? LOCALE_OPTIONS_ON: LOCALE_OPTIONS_OFF));
			return res;
		}
	}

	if (actionKey == "save") {
		CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_MAINSETTINGS_SAVESETTINGSNOW_HINT));
		hintBox.paint();
		load();
		if (frameBuffer->get3DMode() == CFrameBuffer::Mode3D_off) {
			std::map<t_channel_id, CFrameBuffer::Mode3D>::iterator i;
			i = threeDMap.find(CZapit::getInstance()->GetCurrentChannelID());
			if (i != threeDMap.end())
				threeDMap.erase(i);
		} else
			threeDMap[CZapit::getInstance()->GetCurrentChannelID()] = frameBuffer->get3DMode();
		save();
		sleep(1);
		hintBox.hide();
		return res;
	}
	if (actionKey == "zapped") {
		load();
		std::map<t_channel_id, CFrameBuffer::Mode3D>::iterator i;
		i = threeDMap.find(CZapit::getInstance()->GetCurrentChannelID());
		if (i != threeDMap.end())
			frameBuffer->set3DMode((*i).second);
		else
			frameBuffer->set3DMode(CFrameBuffer::Mode3D_off);
		return res;
	}

	if (parent)
		parent->hide();
	else {
		pixbuf = new fb_pixel_t[dx * dy];
		if (pixbuf)
			frameBuffer->SaveScreen (x, y, dx, dy, pixbuf);
	}

	res = show3DSetup();

	if (pixbuf) {
		frameBuffer->RestoreScreen (x, y, dx, dy, pixbuf);
		frameBuffer->blit();
		delete[]pixbuf;
	}
	return res;
}

void C3DSetup::load()
{
	threeDMap.clear();
	FILE *f = fopen(THREE_D_CONFIG_FILE, "r");
	if (f) {
		char s[80];
		while (fgets(s, sizeof(s), f)) {
			t_channel_id chan;
			long long unsigned int _chan;
			int mode;
			if (sscanf(s, "%llx %d", &_chan, &mode) == 2) {
				chan = _chan;
				threeDMap[chan] = (CFrameBuffer::Mode3D) mode;
			}
		}
		fclose(f);
	}
}

void C3DSetup::save()
{
	std::map<t_channel_id, CFrameBuffer::Mode3D>::iterator i;
	FILE *f = fopen(THREE_D_CONFIG_FILE, "w");
	if (f) {
		for (i = threeDMap.begin(); i != threeDMap.end(); i++) {
			fprintf(f, "%llx %d\n", (long long unsigned int) i->first, (int) i->second);
		}
		fflush(f);
		fdatasync(fileno(f));
		fclose(f);
	}
}

int C3DSetup::show3DSetup()
{
	CMenuWidget* m = new CMenuWidget(LOCALE_THREE_D_SETTINGS, NEUTRINO_ICON_SETTINGS, width);

	CFrameBuffer::Mode3D mode3d = frameBuffer->get3DMode();

	m->setSelected(selected);

	m->addIntroItems(LOCALE_THREE_D_SETTINGS_GENERAL);

	int shortcut = 1;

	for (int i = 0; i < THREE_D_OPTIONS_COUNT; i++) {
		tdl[i].cmf = new CMenuForwarder(THREE_D_OPTIONS[i].value, true, g_Locale->getText((mode3d == i) ? LOCALE_OPTIONS_ON : LOCALE_OPTIONS_OFF),
			this, tdl[i].actionKey.c_str(), CRCInput::convertDigitToKey(shortcut++));
		m->addItem(tdl[i].cmf, selected == i);
	}

	m->addItem(GenericMenuSeparatorLine);
	m->addItem(new CMenuForwarder(LOCALE_THREE_D_SAVE, true, NULL, this, "save", CRCInput::RC_red));

	int res = m->exec (NULL, "");

	m->hide ();
	selected = m->getSelected();
	delete m;
	for (int i = 0; i < THREE_D_OPTIONS_COUNT; i++)
		tdl[i].cmf = NULL;
	return res;
}

static C3DSetup *inst = NULL;

C3DSetup *C3DSetup::getInstance()
{
	if (!inst)
		inst = new C3DSetup();
	return inst;
}
