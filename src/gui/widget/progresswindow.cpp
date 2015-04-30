/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Implementation of CComponent Window class.
	Copyright (C) 2014 Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "progresswindow.h"

#include <global.h>
#include <neutrino.h>

#include <driver/display.h>

CProgressWindow::CProgressWindow(CComponentsForm *parent) 
: CComponentsWindow(0, 0, 700, 200, string(), NEUTRINO_ICON_INFO, NULL, parent)
{
	Init();
}

void CProgressWindow::Init()
{
	global_progress = local_progress = 100;

	showFooter(false);
	shadow = true;

	int x_item = 10;
	int y_item = 10;
	setWidthP(75);
	int w_item = width-2*x_item;
	int h_item = 14;
	int h_pbar = 20;
	w_bar_frame = 0;

	//create status text object
	status_txt = new CComponentsLabel();
	int h_txt = max(g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), h_item);
	status_txt->setDimensionsAll(x_item, y_item, w_item, h_txt);
	status_txt->setColorBody(col_body);
	addWindowItem(status_txt);
	y_item += h_txt + 10;

	//create local_bar object
	local_bar = new CProgressBar();
	local_bar->allowPaint(false);
	local_bar->setDimensionsAll(x_item, y_item, w_item, h_pbar);
	local_bar->setColorBody(col_body);
	local_bar->setActiveColor(COL_MENUCONTENT_PLUS_7);
	local_bar->setFrameThickness(w_bar_frame);
	local_bar->setColorFrame(COL_MENUCONTENT_PLUS_7);
	addWindowItem(local_bar);
	y_item += 2*h_pbar;

	//create global_bar object
	global_bar = new CProgressBar();
	global_bar->allowPaint(false);
	global_bar->setDimensionsAll(x_item, y_item, w_item, h_pbar);
	global_bar->setColorBody(col_body);
	global_bar->setActiveColor(COL_MENUCONTENT_PLUS_7);
	global_bar->setFrameThickness(w_bar_frame);
	global_bar->setColorFrame(COL_MENUCONTENT_PLUS_7);
	addWindowItem(global_bar);
	y_item += 2*h_pbar;

	h_height = ccw_head->getHeight();
	height = y_item + h_height;

	setCenterPos();
}

void CProgressWindow::setTitle(const neutrino_locale_t title)
{
	setWindowCaption(title);

#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(-1,NULL,-1,g_Locale->getText(ccw_caption)); // set global text in VFD
#endif // VFD_UPDATE
}

//if header is disabled we need new position for body items
void CProgressWindow::fitItems()
{
	if (ccw_show_header)
		return;

	for(size_t i=0; i<ccw_body->size() ;i++){
		int y_item = ccw_body->getCCItem(i)->getYPos() + h_height - 10;
		ccw_body->getCCItem(i)->setYPos(y_item);
	}
}

void CProgressWindow::showStatus(const unsigned int prog)
{
	if (global_progress == prog)
		return;

	if (!global_bar->isPainted()){
		int g_height = global_bar->getHeight();
		global_bar->setYPos(local_bar->getYPos() + g_height/2);
		global_bar->setHeight(g_height + g_height/2);
	}

	showGlobalStatus(prog);
}

void CProgressWindow::showGlobalStatus(const unsigned int prog)
{
	if (global_progress == prog)
		return;

	global_bar->allowPaint(true);
	global_progress = prog;
	global_bar->setValues(prog, 100);
	global_bar->paint(false);
	frameBuffer->blit();

#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(-1,NULL,global_progress);
#endif // VFD_UPDATE
}

void CProgressWindow::showLocalStatus(const unsigned int prog)
{
	if (local_progress == prog)
		return;

	local_bar->allowPaint(true);
	local_progress = prog;
	local_bar->setValues(prog, 100);
	local_bar->paint(false);
	frameBuffer->blit();

#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(local_progress);
#else
	CVFD::getInstance()->showPercentOver((uint8_t)local_progress);
#endif // VFD_UPDATE
}

void CProgressWindow::showStatusMessageUTF(const std::string & text)
{
	string txt = text;
	int w_txt = status_txt->getWidth();
	int h_txt = status_txt->getHeight();
	status_txt->setText(txt, CTextBox::CENTER, *CNeutrinoFonts::getInstance()->getDynFont(w_txt, h_txt, txt, CNeutrinoFonts::FONT_STYLE_BOLD), COL_MENUCONTENT_TEXT);

	status_txt->paint(false);
	frameBuffer->blit();

#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(-1,text.c_str()); // set local text in VFD
#endif // VFD_UPDATE
}


unsigned int CProgressWindow::getGlobalStatus(void)
{
	return global_progress;
}

void CProgressWindow::hide(bool no_restore)
{
	CComponentsWindow::hide(no_restore);
	frameBuffer->blit();
}

int CProgressWindow::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	if(parent)
	{
		parent->hide();
	}
	paint();
	frameBuffer->blit();

	return menu_return::RETURN_REPAINT;
}

void CProgressWindow::paint(bool do_save_bg)
{
	fitItems();
	CComponentsWindow::paint(do_save_bg);
}
