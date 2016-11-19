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

#include <gui/screensetup.h>

#include <gui/color.h>
#include <gui/infoviewer.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/icons.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
#include <driver/screen_max.h>
#endif
#include <system/settings.h>
#include <system/helpers.h>

#include <global.h>
#include <neutrino.h>

#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
struct borderFrame { int sx, sy, ex, ey; };
static map<t_channel_id, borderFrame> borderMap;
#define BORDER_CONFIG_FILE CONFIGDIR "/zapit/borders.conf"
#else
//int x_box = 15 * 5;

inline unsigned int make16color(__u32 rgb)
{
	return 0xFF000000 | rgb;
}
#endif

CScreenSetup::CScreenSetup()
{
	frameBuffer = CFrameBuffer::getInstance();
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
	screenheight = frameBuffer->getScreenHeight(true);
	screenwidth = frameBuffer->getScreenWidth(true);
	startX = g_settings.screen_StartX_int;
	startY = g_settings.screen_StartY_int;
	endX = g_settings.screen_EndX_int;
	endY = g_settings.screen_EndY_int;
	channel_id = 0;
	coord_abs = true;
	m = NULL;
#endif
}

#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
int CScreenSetup::exec(CMenuTarget* parent, const std::string &action)
{
	if (!m) {
		selected = 0;
		x_coord[0] = startX;
		x_coord[1] = endX;
		y_coord[0] = startY;
		y_coord[1] = endY;
		if (!channel_id) {
			color_bak = frameBuffer->getBorderColor();
			frameBuffer->getBorder(x_coord_bak[0], y_coord_bak[0], x_coord_bak[1], y_coord_bak[1]);
		}
		updateCoords();
		frameBuffer->setBorder(x_coord[0], y_coord[0], x_coord[1], y_coord[1]);
		frameBuffer->setBorderColor(channel_id ? 0x44444444 : 0x88888888);
		frameBuffer->paintIcon(NEUTRINO_ICON_BORDER_UL, 0, 0);
		frameBuffer->paintIcon(NEUTRINO_ICON_BORDER_LR, screenwidth - 1 - 96, screenheight - 1 - 96 );

		m = new CMenuWidget(channel_id ? LOCALE_VIDEOMENU_MASKSETUP : LOCALE_VIDEOMENU_SCREENSETUP, NEUTRINO_ICON_SETTINGS, w_max (40, 10));
		m->addItem(new CMenuForwarder(LOCALE_SCREENSETUP_UPPERLEFT, true, coord[0], this, "ul", CRCInput::RC_red));
		m->addItem(new CMenuForwarder(LOCALE_SCREENSETUP_LOWERRIGHT, true, coord[1], this, "lr", CRCInput::RC_green));
		if (channel_id)
			m->addItem(new CMenuForwarder(LOCALE_SCREENSETUP_REMOVE, true, NULL, this, "rm", CRCInput::RC_yellow));
		m->addKey(CRCInput::RC_home, this, "ex");
		m->addKey(CRCInput::RC_timeout, this, "ti");
		m->addKey(CRCInput::RC_ok, this, "ok");
		m->addKey(CRCInput::RC_up, this, "u");
		m->addKey(CRCInput::RC_down, this, "d");
		m->addKey(CRCInput::RC_left, this, "l");
		m->addKey(CRCInput::RC_right, this, "r");
		m->addKey(CRCInput::RC_setup, this, "se");
		m->setSelected(selected);
		m->exec(parent, "");
		delete m;
		m = NULL;
		hide();
		return menu_return::RETURN_REPAINT;
	}
	if (action == "ex" || action == "ti") {
		if (action == "ex" && ((startX != x_coord[0] ) || ( endX != x_coord[1] ) || ( startY != y_coord[0] ) || ( endY != y_coord[1] ) ) &&
			(ShowMsg(LOCALE_VIDEOMENU_SCREENSETUP, LOCALE_MESSAGEBOX_DISCARD, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbCancel) == CMsgBox::mbrCancel))
			return menu_return::RETURN_NONE;
		loadBorder(channel_id);
		return (action == "ex") ? menu_return::RETURN_EXIT : menu_return::RETURN_EXIT_ALL;
	}
	if (action == "ok") {
		startX = x_coord[0];
		endX = x_coord[1];
		startY = y_coord[0];
		endY = y_coord[1];
		if (channel_id) {
			borderFrame b;
			b.sx = startX;
			b.sy = startY;
			b.ex = endX;
			b.ey = endY;
			borderMap[channel_id] = b;
			showBorder(channel_id);
			saveBorders();
		} else {
			x_coord_bak[0] = x_coord[0];
			y_coord_bak[0] = y_coord[0];
			x_coord_bak[1] = x_coord[1];
			y_coord_bak[1] = y_coord[1];
			g_settings.screen_StartX_int = x_coord[0];
			g_settings.screen_EndX_int = x_coord[1];
			g_settings.screen_StartY_int = y_coord[0];
			g_settings.screen_EndY_int = y_coord[1];
			showBorder(channel_id);

			if(g_settings.screen_preset) {
				g_settings.screen_StartX_lcd = g_settings.screen_StartX_int;
				g_settings.screen_StartY_lcd = g_settings.screen_StartY_int;
				g_settings.screen_EndX_lcd = g_settings.screen_EndX_int;
				g_settings.screen_EndY_lcd = g_settings.screen_EndY_int;
			} else {
				g_settings.screen_StartX_crt = g_settings.screen_StartX_int;
				g_settings.screen_StartY_crt = g_settings.screen_StartY_int;
				g_settings.screen_EndX_crt = g_settings.screen_EndX_int;
				g_settings.screen_EndY_crt = g_settings.screen_EndY_int;
			}
		}
		return menu_return::RETURN_EXIT;
	}
	if (action == "se") {
		coord_abs = !coord_abs;
		updateCoords();
		return menu_return::RETURN_REPAINT;
	}
	if (action == "ul") {
		selected = 0;
		return menu_return::RETURN_REPAINT;
	}
	if (action == "lr") {
		selected = 1;
		return menu_return::RETURN_REPAINT;
	}
	if (action == "rm") {
		if (channel_id) {
			startX = g_settings.screen_StartX;
			startY = g_settings.screen_StartY;
			endX = g_settings.screen_EndX;
			endY = g_settings.screen_EndY;
			resetBorder(channel_id);
			saveBorders();
			if (g_InfoViewer)
				g_InfoViewer->start();
			return menu_return::RETURN_EXIT;
		}
		return menu_return::RETURN_NONE;
	}
	if (action == "u" || action == "d" || action == "l" || action == "r") {
		if ((action == "u") && (((selected == 0) && (y_coord[0] > 0)) || ((selected == 1) && (y_coord[1] > y_coord[0] - 100))))
			y_coord[selected]--;
		else if ((action == "d") && (((selected == 0) && (y_coord[0] < y_coord[1] - 100)) || ((selected == 1) && (y_coord[1] < screenheight))))
			y_coord[selected]++;
		else if ((action == "l") && (((selected == 0) && (x_coord[0] > 0)) || ((selected == 1) && (x_coord[1] > x_coord[0] - 100))))
			x_coord[selected]--;
		else if ((action == "r") && (((selected == 0) && (x_coord[0] < x_coord[1] - 100)) || ((selected == 1) && (x_coord[1] < screenwidth))))
			x_coord[selected]++;
		else
			return menu_return::RETURN_NONE;

		frameBuffer->setBorder(x_coord[0], y_coord[0], x_coord[1], y_coord[1]);
		updateCoords();
		return menu_return::RETURN_REPAINT;
	}
	return menu_return::RETURN_NONE;
}
#else
int CScreenSetup::exec(CMenuTarget* parent, const std::string &)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	x_box = 15*5;
	y_box = frameBuffer->getScreenHeight(true) / 2;

        int icol_w, icol_h;
        frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_RED, &icol_w, &icol_h);

	BoxHeight = std::max(icol_h+4, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight());
	BoxWidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_SCREENSETUP_UPPERLEFT));

	int tmp = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_SCREENSETUP_LOWERRIGHT));
	if (tmp > BoxWidth)
		BoxWidth = tmp;

	BoxWidth += 10 + icol_w;

	x_coord[0] = g_settings.screen_StartX;
	x_coord[1] = g_settings.screen_EndX;
	y_coord[0] = g_settings.screen_StartY;
	y_coord[1] = g_settings.screen_EndY;

	paint();
	frameBuffer->blit();

	selected = 0;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings
::TIMING_MENU]);

	bool loop=true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd, true );

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings
::TIMING_MENU]);

		switch ( msg )
		{
			case CRCInput::RC_ok:
				// abspeichern
				g_settings.screen_StartX = x_coord[0];
				g_settings.screen_EndX = x_coord[1];
				g_settings.screen_StartY = y_coord[0];
				g_settings.screen_EndY = y_coord[1];
				if(g_settings.screen_preset) {
					g_settings.screen_StartX_lcd = g_settings.screen_StartX;
					g_settings.screen_StartY_lcd = g_settings.screen_StartY;
					g_settings.screen_EndX_lcd = g_settings.screen_EndX;
					g_settings.screen_EndY_lcd = g_settings.screen_EndY;
				} else {
					g_settings.screen_StartX_crt = g_settings.screen_StartX;
					g_settings.screen_StartY_crt = g_settings.screen_StartY;
					g_settings.screen_EndX_crt = g_settings.screen_EndX;
					g_settings.screen_EndY_crt = g_settings.screen_EndY;
				}
				if (g_InfoViewer) /* recalc infobar position */
					g_InfoViewer->start();
				loop = false;
				break;

			case CRCInput::RC_home:
				if ( ( ( g_settings.screen_StartX != x_coord[0] ) ||
							( g_settings.screen_EndX != x_coord[1] ) ||
							( g_settings.screen_StartY != y_coord[0] ) ||
							( g_settings.screen_EndY != y_coord[1] ) ) &&
						(ShowMsg(LOCALE_VIDEOMENU_SCREENSETUP, LOCALE_MESSAGEBOX_DISCARD, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbCancel) == CMsgBox::mbrCancel))
					break;

			case CRCInput::RC_timeout:
				loop = false;
				break;

			case CRCInput::RC_red:
			case CRCInput::RC_green:
				{
					selected = ( msg == CRCInput::RC_green ) ? 1 : 0 ;

					frameBuffer->paintBoxRel(x_box, y_box, BoxWidth, BoxHeight,
						(selected == 0)?COL_MENUCONTENTSELECTED_PLUS_0:COL_MENUCONTENT_PLUS_0);
					frameBuffer->paintBoxRel(x_box, y_box + BoxHeight, BoxWidth, BoxHeight,
						(selected ==1 )?COL_MENUCONTENTSELECTED_PLUS_0:COL_MENUCONTENT_PLUS_0);

					paintIcons(selected);
					break;
				}
			case CRCInput::RC_up:
			{

				int min = (selected == 0) ? 0 : 400;
				if (y_coord[selected] <= min)
					y_coord[selected] = min;
				else
				{
					unpaintBorder(selected);
					y_coord[selected]--;
					paintBorder(selected);
				}
				break;
			}
			case CRCInput::RC_down:
			{
				int max = (selected == 0 )? 200 : frameBuffer->getScreenHeight(true);
				if (y_coord[selected] >= max)
					y_coord[selected] = max;
				else
				{
					unpaintBorder(selected);
					y_coord[selected]++;
					paintBorder(selected);
				}
				break;
			}
			case CRCInput::RC_left:
			{
				int min = (selected == 0) ? 0 : 400;
				if (x_coord[selected] <= min)
					x_coord[selected] = min;
				else
				{
					unpaintBorder(selected);
					x_coord[selected]--;
					paintBorder( selected );
				}
				break;
			}
			case CRCInput::RC_right:
			{
				int max = (selected == 0) ? 200 : frameBuffer->getScreenWidth(true);
				if (x_coord[selected] >= max)
					x_coord[selected] = max;
				else
				{
					unpaintBorder(selected);
					x_coord[selected]++;
					paintBorder( selected );
				}
				break;
			}
			default:
				if (CNeutrinoApp::getInstance()->listModeKey(msg))
				{
					break;
				}
				else if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
				{
					loop = false;
					res = menu_return::RETURN_EXIT_ALL;
				}
		}
		frameBuffer->blit();

	}

	hide();
	return res;
}
#endif

void CScreenSetup::hide()
{
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
	frameBuffer->paintBackgroundBox(0, 0, screenwidth, screenheight);
	if (channel_id)
		showBorder(channel_id);
	else {
		frameBuffer->setBorderColor(color_bak);
		frameBuffer->setBorder(x_coord_bak[0], y_coord_bak[0], x_coord_bak[1], y_coord_bak[1]);
	}
#else
	int w = (int) frameBuffer->getScreenWidth(true);
	int h = (int) frameBuffer->getScreenHeight(true);
	frameBuffer->paintBackgroundBox(0, 0, w, h);
#endif
	frameBuffer->blit();
}

#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
bool CScreenSetup::loadBorder(t_channel_id cid)
{
	loadBorders();
	channel_id = cid;
	std::map<t_channel_id, borderFrame>::iterator it = borderMap.find(cid);
	if (it != borderMap.end()) {
		startX = it->second.sx;
		startY = it->second.sy;
		endX = it->second.ex;
		endY = it->second.ey;
	} else {
		startX = g_settings.screen_StartX_int;
		startY = g_settings.screen_StartY_int;
		endX = g_settings.screen_EndX_int;
		endY = g_settings.screen_EndY_int;
	}
	return (it != borderMap.end());
}

void CScreenSetup::showBorder(t_channel_id cid)
{
	if (loadBorder(cid)) {
		frameBuffer->setBorderColor(0xFF000000);
		frameBuffer->setBorder(startX, startY, endX, endY);
	} else
		hideBorder();
}

void CScreenSetup::hideBorder()
{
	frameBuffer->setBorderColor();
	frameBuffer->setBorder(startX, startY, endX, endY);
}

void CScreenSetup::resetBorder(t_channel_id cid)
{
	loadBorder(cid);
	frameBuffer->setBorderColor();
	std::map<t_channel_id, borderFrame>::iterator it = borderMap.find(cid);
	if (it != borderMap.end())
		borderMap.erase(cid);
	startX = g_settings.screen_StartX_int;
	startY = g_settings.screen_StartY_int;
	endX = g_settings.screen_EndX_int;
	endY = g_settings.screen_EndY_int;
	frameBuffer->setBorder(startX, startY, endX, endY);
}


void CScreenSetup::loadBorders()
{
	if (borderMap.empty()) {
		FILE *f = fopen(BORDER_CONFIG_FILE, "r");
		borderMap.clear();
		if (f) {
			char s[1000];
			while (fgets(s, sizeof(s), f)) {
				t_channel_id chan;
				long long unsigned _chan;
				borderFrame b;
				if (5 == sscanf(s, "%llx %d %d %d %d", &_chan, &b.sx, &b.sy, &b.ex, &b.ey)) {
					chan = _chan;
					borderMap[chan] = b;
				}
			}
			fclose(f);
		}
	}
}

void CScreenSetup::saveBorders()
{
	if (borderMap.empty())
		unlink(BORDER_CONFIG_FILE);
	else {
		FILE *f = fopen(BORDER_CONFIG_FILE, "w");
		if (f) {
			std::map<t_channel_id, borderFrame>::iterator it;
			for (it = borderMap.begin(); it != borderMap.end(); it++)
				fprintf(f, "%llx %d %d %d %d\n", (long long unsigned) it->first, it->second.sx, it->second.sy, it->second.ex, it->second.ey);
			fflush(f);
			fdatasync(fileno(f));
			fclose(f);
		}
	}
}

void CScreenSetup::updateCoords()
{
	coord[0] = "(" + to_string(x_coord[0]) + "," + to_string(y_coord[0]) + ")";
	if (coord_abs)
		coord[1] = "(" + to_string(x_coord[1]) + "," + to_string(y_coord[1]) + ")";
	else
		coord[1] = "(" + to_string(screenwidth - x_coord[1]) + "," + to_string(screenheight - y_coord[1]) + ")";
}
#else
void CScreenSetup::paintBorder( int pselected )
{
	if ( pselected == 0 )
		paintBorderUL();
	else
		paintBorderLR();

	paintCoords();
}

void CScreenSetup::unpaintBorder(int pselected)
{
	int cx = x_coord[pselected] - 96 * pselected;
	int cy = y_coord[pselected] - 96 * pselected;
	frameBuffer->paintBoxRel(cx, cy, 96, 96, make16color(0xA0A0A0));
}

void CScreenSetup::paintIcons(int pselected)
{
        int icol_w = 0, icol_h = 0;
        frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_RED, &icol_w, &icol_h);

	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_RED, x_box + 5, y_box, BoxHeight);
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_GREEN, x_box + 5, y_box+BoxHeight, BoxHeight);

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x_box + icol_w + 10, y_box + BoxHeight, BoxWidth,
		g_Locale->getText(LOCALE_SCREENSETUP_UPPERLEFT ), (pselected == 0) ? COL_MENUCONTENTSELECTED_TEXT:COL_MENUCONTENT_TEXT);
        g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x_box + icol_w + 10, y_box + BoxHeight * 2, BoxWidth,
		g_Locale->getText(LOCALE_SCREENSETUP_LOWERRIGHT), (pselected == 1) ? COL_MENUCONTENTSELECTED_TEXT:COL_MENUCONTENT_TEXT);
}

void CScreenSetup::paintBorderUL()
{
	frameBuffer->paintIcon(NEUTRINO_ICON_BORDER_UL, x_coord[0], y_coord[0] );
}

void CScreenSetup::paintBorderLR()
{
	frameBuffer->paintIcon(NEUTRINO_ICON_BORDER_LR, x_coord[1]- 96, y_coord[1]- 96 );
}

void CScreenSetup::paintCoords()
{
	Font *f = g_Font[SNeutrinoSettings::FONT_TYPE_MENU];
	int w = f->getRenderWidth("EX: 2222") * 5 / 4;	/* half glyph border left and right */
	int fh = f->getHeight();
	int h = fh * 4;		/* 4 lines, fonts have enough space around them, no extra border */

	int x1 = (frameBuffer->getScreenWidth(true) - w) / 2;	/* centered */
	int y1 = frameBuffer->getScreenHeight(true) / 2 - h;	/* above center, to avoid conflict */
	int x2 = x1 + w / 10;
	int y2 = y1 + fh;

	frameBuffer->paintBoxRel(x1, y1, w, h, COL_MENUCONTENT_PLUS_0);

	char str[4][16];
	snprintf(str[0], 16, "SX: %d", x_coord[0]);
	snprintf(str[1], 16, "SY: %d", y_coord[0]);
	snprintf(str[2], 16, "EX: %d", x_coord[1]);
	snprintf(str[3], 16, "EY: %d", y_coord[1]);
	/* the code is smaller with this loop instead of open-coded 4x RenderString() :-) */
	for (int i = 0; i < 4; i++)
	{
		f->RenderString(x2, y2, w, str[i], COL_MENUCONTENT_TEXT);
		y2 += fh;
	}
}

void CScreenSetup::paint()
{
	if (!frameBuffer->getActive())
		return;

	int w = (int) frameBuffer->getScreenWidth(true);
	int h = (int) frameBuffer->getScreenHeight(true);

	frameBuffer->paintBox(0,0, w, h, make16color(0xA0A0A0));

	for(int count = 0; count < h; count += 15)
		frameBuffer->paintHLine(0, w-1, count, make16color(0x505050) );

	for(int count = 0; count < w; count += 15)
		frameBuffer->paintVLine(count, 0, h-1, make16color(0x505050) );

	frameBuffer->paintBox(0, 0, w/3, h/3, make16color(0xA0A0A0));
	frameBuffer->paintBox(w-w/3, h-h/3, w-1, h-1, make16color(0xA0A0A0));

	frameBuffer->paintBoxRel(x_box, y_box, BoxWidth, BoxHeight, COL_MENUCONTENTSELECTED_PLUS_0);   //upper selected box
	frameBuffer->paintBoxRel(x_box, y_box + BoxHeight, BoxWidth, BoxHeight, COL_MENUCONTENT_PLUS_0); //lower selected box

	paintIcons(0);
	paintBorderUL();
	paintBorderLR();
	paintCoords();
}
#endif
