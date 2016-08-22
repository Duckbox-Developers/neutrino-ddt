/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2016 Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

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

#include <global.h>
#include <neutrino.h>
#include "cc_frm_window.h"
#include <system/debug.h>
using namespace std;

/*
	scheme of window object

		+x,y----------------------------------------------------------------+
		|+-----------------------------------------------------------------+|
		||header (ccw_head)				 		   ||
		|+---+-------------------------------------------------------+----+||
		||left |body (ccw_body)					     |right||
		||side |						     |side ||
		||bar  |						     |bar  ||
		||     |						     |	   ||
		||     |						     |	   ||
		||     |						     |	   ||
		||     |						     |	   ||
		||     |						     |	   ||
		||     |						     |	   ||
		||     |						     |	   ||
		|+-----+-----------------------------------------------------+-----+|
		||footer (ccw_footer)						   ||
		|+-----------------------------------------------------------------+|
		+-------------------------------------------------------------------+
*/

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsWindow inherit from CComponentsForm
CComponentsWindow::CComponentsWindow(CComponentsForm *parent)
{
	initVarWindow(0, 0, 800, 600, "", "", parent);
}

CComponentsWindow::CComponentsWindow(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					neutrino_locale_t locale_caption,
					const string& iconname,
					CComponentsForm *parent,
					int shadow_mode,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow)
{
	string s_caption = locale_caption != NONEXISTANT_LOCALE ? g_Locale->getText(locale_caption) : "";
	initVarWindow(x_pos, y_pos, w, h, s_caption, iconname, parent, shadow_mode, color_frame, color_body, color_shadow);
}

CComponentsWindow::CComponentsWindow(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const string& caption,
					const string& iconname,
					CComponentsForm *parent,
					int shadow_mode,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow)
{
	initVarWindow(x_pos, y_pos, w, h, caption, iconname, parent, shadow_mode, color_frame, color_body, color_shadow);
}

CComponentsWindowMax::CComponentsWindowMax(	const string& caption,
						const string& iconname,
						CComponentsForm *parent,
						int shadow_mode,
						fb_pixel_t color_frame,
						fb_pixel_t color_body,
						fb_pixel_t color_shadow)
						:CComponentsWindow(0, 0, 0, 0, caption,
						iconname, parent, shadow_mode, color_frame, color_body, color_shadow){};

CComponentsWindowMax::CComponentsWindowMax(	neutrino_locale_t locale_caption,
						const string& iconname,
						CComponentsForm *parent,
						int shadow_mode,
						fb_pixel_t color_frame,
						fb_pixel_t color_body,
						fb_pixel_t color_shadow)
						:CComponentsWindow(0, 0, 0, 0,
						locale_caption != NONEXISTANT_LOCALE ? g_Locale->getText(locale_caption) : "",
						iconname, parent, shadow_mode, color_frame, color_body, color_shadow){};

void CComponentsWindow::initVarWindow(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const string& caption,
					const string& iconname,
					CComponentsForm *parent,
					int shadow_mode,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow)
{
	//CComponentsForm
	cc_item_type 	= CC_ITEMTYPE_FRM_WINDOW;

	//using current screen settings for default dimensions,
	//do use full screen (from osd-settings) if default values for width/height = 0
	x = x_pos;
	y = y_pos;
	width = w;
	height = h;
	initWindowSize();
	initWindowPos();

	ccw_caption 	= caption;
	ccw_icon_name	= iconname;

	dprintf(DEBUG_DEBUG, "[CComponentsWindow]   [%s - %d] icon name = %s\n", __func__, __LINE__, ccw_icon_name.c_str());
	paint_bg	= false;
	shadow		= shadow_mode;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	ccw_head 	= NULL;
	ccw_left_sidebar= NULL;
	ccw_right_sidebar= NULL;	
	ccw_body	= NULL;
	ccw_footer	= NULL;
	ccw_button_font	= g_Font[SNeutrinoSettings::FONT_TYPE_MENU_FOOT];

	ccw_buttons	= 0; //no header buttons
	ccw_show_footer = true;
	ccw_show_header	= true;
	ccw_align_mode	= CTextBox::NO_AUTO_LINEBREAK;
	ccw_show_l_sideber = false;
	ccw_show_r_sideber = false;
	ccw_w_sidebar	= 40;
	ccw_col_head 	= COL_MENUHEAD_PLUS_0;
	ccw_col_head_text = COL_MENUHEAD_TEXT;
	ccw_col_footer	= COL_MENUFOOT_PLUS_0;

	page_scroll_mode = PG_SCROLL_M_OFF; //permanent disabled here, only in body used!

	initCCWItems();
	initParent(parent);
}

void CComponentsWindow::initWindowSize()
{
	if (cc_parent)
		return;

	if (width == 0)
		width = frameBuffer->getScreenWidth();
	else if ((unsigned)width > frameBuffer->getScreenWidth())
		width = frameBuffer->getScreenWidth();

	if (height == 0)
		height = frameBuffer->getScreenHeight();
	else if((unsigned)height > frameBuffer->getScreenHeight())
		height = frameBuffer->getScreenHeight();
}

void CComponentsWindow::initWindowPos()
{
	if (cc_parent)
		return;

	if (x == 0)
		x = frameBuffer->getScreenX();
	if (y == 0)
		y = frameBuffer->getScreenY();
}

void CComponentsWindow::setWindowCaption(neutrino_locale_t locale_text, const int& align_mode)
{
	ccw_caption = g_Locale->getText(locale_text);
	ccw_align_mode = align_mode;
}

void CComponentsWindow::initHeader()
{
	if (ccw_head == NULL)
		ccw_head = new CComponentsHeader();
	//add of header item happens initCCWItems()
	//set header properties //TODO: assigned properties with internal header objekt have no effect!
	if (ccw_head){
		ccw_head->setWidth(width-2*fr_thickness);
		ccw_head->setPos(0, 0);
		ccw_head->setIcon(ccw_icon_name);
		ccw_head->setCaption(ccw_caption, ccw_align_mode, ccw_col_head_text);
		ccw_head->setContextButton(ccw_buttons);
		ccw_head->setCorner(corner_rad, CORNER_TOP);
		ccw_head->setColorBody(ccw_col_head);
	}
}

void CComponentsWindow::initFooter()
{
	if (ccw_footer== NULL)
		ccw_footer= new CComponentsFooter();
	//add of footer item happens initCCWItems()
	//set footer properties
	if (ccw_footer){
		ccw_footer->setPos(0, cc_yr + height - ccw_footer->getHeight()- fr_thickness);
		ccw_footer->setWidth(width-2*fr_thickness);
		ccw_footer->enableShadow(false/*shadow*/);
		ccw_footer->setCorner(corner_rad-fr_thickness/2, CORNER_BOTTOM);
		ccw_footer->setButtonFont(ccw_button_font);
		ccw_footer->setColorBody(ccw_col_footer);
		ccw_footer->doPaintBg(true);
	}
}

void CComponentsWindow::initLeftSideBar()
{
	if (ccw_left_sidebar== NULL)
		ccw_left_sidebar = new CComponentsFrmChain();
	//set side bar properties
	if (ccw_left_sidebar){
		ccw_left_sidebar->setCornerType(0);
		int h_footer = 0;
		int h_header = 0;
		if (ccw_footer)
			h_footer = ccw_footer->getHeight();
		if (ccw_head)
			h_header = ccw_head->getHeight();
		int h_sbar = height - h_header - h_footer - 2*fr_thickness;
		int w_sbar = ccw_w_sidebar;
		ccw_left_sidebar->setDimensionsAll(0, CC_APPEND, w_sbar, h_sbar);
		ccw_left_sidebar->doPaintBg(true);
	}
}

void CComponentsWindow::initRightSideBar()
{
	if (ccw_right_sidebar== NULL)
		ccw_right_sidebar = new CComponentsFrmChain();
	//set side bar properties
	if (ccw_right_sidebar){
		ccw_right_sidebar->setCornerType(0);
		int h_footer = 0;
		int h_header = 0;
		if (ccw_footer)
			h_footer = ccw_footer->getHeight();
		if (ccw_head)
			h_header = ccw_head->getHeight();
		int h_sbar = height - h_header - h_footer - 2*fr_thickness;
		int w_sbar = ccw_w_sidebar;
		ccw_right_sidebar->setDimensionsAll(width - w_sbar, CC_APPEND, w_sbar, h_sbar);
		ccw_right_sidebar->doPaintBg(true);
	}
}

void CComponentsWindow::initBody()
{
	if (ccw_body== NULL)
		ccw_body = new CComponentsForm();
	//add of body item happens initCCWItems()
	//set body properties
	if (ccw_body){
		ccw_body->setCorner(corner_rad-fr_thickness/2, CORNER_NONE);
		int h_footer = 0;
		int h_header = 0;
		int w_l_sidebar = 0;
		int w_r_sidebar = 0;
		if (ccw_footer){
			h_footer = ccw_footer->getHeight();
		}
		if (ccw_head){
			h_header = ccw_head->getHeight();
		}
		if (ccw_left_sidebar)
			w_l_sidebar = ccw_left_sidebar->getWidth();
		if (ccw_right_sidebar)
			w_r_sidebar = ccw_right_sidebar->getWidth();
		int h_body = height - h_header - h_footer - fr_thickness;
		int x_body = w_l_sidebar;
		int w_body = width-2*fr_thickness - w_l_sidebar - w_r_sidebar;
		
		ccw_body->setDimensionsAll(x_body, h_header, w_body, h_body);
		ccw_body->doPaintBg(true);
		
		//handle corner behavior
		if (!ccw_show_header)
			ccw_body->setCornerType(CORNER_TOP);
		if (!ccw_show_footer)
			ccw_body->setCornerType(ccw_body->getCornerType() | CORNER_BOTTOM);
		if (!ccw_show_header)
			ccw_body->setCornerType(CORNER_TOP);

	}
}

void CComponentsWindow::initCCWItems()
{
	dprintf(DEBUG_DEBUG, "[CComponentsWindow]   [%s - %d] init items...\n", __func__, __LINE__);

	//add/remove header if required
	if (ccw_show_header){
		initHeader();
	}else{
		if (ccw_head){
			removeCCItem(ccw_head);
			ccw_head = NULL;
		}
	}

	//add/remove footer if required
	if (ccw_show_footer){
		initFooter();
	}else{
		if (ccw_footer){
			removeCCItem(ccw_footer);
			ccw_footer = NULL;
		}
	}
	
	//add/remove left sidebar
	if (ccw_show_l_sideber){
		initLeftSideBar();
	}else{
		if (ccw_left_sidebar){
			removeCCItem(ccw_left_sidebar);
			ccw_left_sidebar = NULL;
		}
	}

	//add/remove right sidebar
	if (ccw_show_r_sideber){
		initRightSideBar();
	}else{
		if (ccw_right_sidebar){
			removeCCItem(ccw_right_sidebar);
			ccw_right_sidebar = NULL;
		}
	}

	//init window body core
	initBody();

	/*Add header and footer items as first  and  body as last item.
	Render of items occurs in listed order. So it's better for performance while render of window.
	This is something more advantageously because all other items are contained inside body.
	So we avoid possible delay while rendering of base items. It looks better on screen.
	*/
	if (ccw_head)
		if (!ccw_head->isAdded())
			addCCItem(ccw_head);
	if (ccw_footer)
		if (!ccw_footer->isAdded())
			addCCItem(ccw_footer);
	if (!ccw_body->isAdded())
		addCCItem(ccw_body);
}

void CComponentsWindow::enableSidebar(const int& sidbar_type)
{
	ccw_show_l_sideber = ccw_show_r_sideber = false;

	if (sidbar_type & CC_WINDOW_LEFT_SIDEBAR)
		ccw_show_l_sideber = true;
	if (sidbar_type & CC_WINDOW_RIGHT_SIDEBAR)
		ccw_show_r_sideber = true;

	initCCWItems();
}

int CComponentsWindow::addWindowItem(CComponentsItem* cc_Item)
{
	if (ccw_body)
		return ccw_body->addCCItem(cc_Item);
	return -1;
}

void CComponentsWindow::setCurrentPage(const u_int8_t& current_page)
{
	ccw_body->setCurrentPage(current_page);
}

u_int8_t CComponentsWindow::getCurrentPage()
{
	return ccw_body->getCurrentPage();
}


bool CComponentsWindow::isPageChanged()
{
	for(size_t i=0; i<ccw_body->size(); i++){
		if (ccw_body->getCCItem(i)->getPageNumber() != getCurrentPage())
			return true;
	}
	return false;
}

void CComponentsWindow::setScrollBarWidth(const int& scrollbar_width)
{
	ccw_body->setScrollBarWidth(scrollbar_width);
}

void CComponentsWindow::enablePageScroll(const int& mode)
{
	ccw_body->enablePageScroll(mode);
}

void CComponentsWindow::paintCurPage(bool do_save_bg)
{
	if (is_painted) //ensure that we have painted already the parent form before paint body 
		ccw_body->paint(do_save_bg);
	else
		paint(do_save_bg);
}

void CComponentsWindow::paintPage(const u_int8_t& page_number, bool do_save_bg)
{
	CComponentsWindow::setCurrentPage(page_number);
	CComponentsWindow::paintCurPage(do_save_bg);
}

void CComponentsWindow::paint(bool do_save_bg)
{
	//prepare items before paint
	initCCWItems();

	//paint form contents
	paintForm(do_save_bg);
}
