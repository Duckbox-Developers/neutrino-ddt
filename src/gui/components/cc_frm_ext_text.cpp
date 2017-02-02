/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2013, 2014 Thilo Graf 'dbt'

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
#include "cc_frm_ext_text.h"

#include <sigc++/bind.h>

#define DEF_HEIGHT 27
#define DEF_LABEL_WIDTH_PERCENT 30

using namespace std;

CComponentsExtTextForm::CComponentsExtTextForm(CComponentsForm* parent)
{
	Font* t_font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO];
	initVarExtTextForm(0, 0, 300, t_font->getHeight(), "", "", t_font, parent, CC_SHADOW_OFF, COL_MENUCONTENTINACTIVE_TEXT, COL_MENUCONTENT_TEXT, COL_FRAME_PLUS_0, COL_MENUCONTENT_PLUS_0, COL_SHADOW_PLUS_0);
	initCCTextItems();
}

CComponentsExtTextForm::CComponentsExtTextForm(	const int& x_pos, const int& y_pos, const int& w, const int& h,
						const std::string& label_text, const std::string& text,
						Font* font_text,
						CComponentsForm* parent,
						int shadow_mode,
						fb_pixel_t label_color,
						fb_pixel_t text_color,
						fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	initVarExtTextForm(x_pos, y_pos, w, h, label_text, text, font_text, parent, shadow_mode, label_color, text_color, color_frame, color_body, color_shadow);
	initCCTextItems();
}

CComponentsExtTextForm::CComponentsExtTextForm(	const int& x_pos, const int& y_pos, const int& w, const int& h,
						neutrino_locale_t l_text, const std::string& text,
						Font* font_text,
						CComponentsForm* parent,
						int shadow_mode,
						fb_pixel_t label_color,
						fb_pixel_t text_color,
						fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	initVarExtTextForm(x_pos, y_pos, w, h, (string)g_Locale->getText(l_text), text, font_text, parent, shadow_mode, label_color, text_color, color_frame, color_body, color_shadow);
	initCCTextItems();
}


void CComponentsExtTextForm::initVarExtTextForm(const int& x_pos, const int& y_pos, const int& w, const int& h,
						const std::string& label_text, const std::string& text,
						Font* font_text,
						CComponentsForm* parent,
						int shadow_mode,
						fb_pixel_t label_color,
						fb_pixel_t text_color,
						fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	cc_item_type 	= CC_ITEMTYPE_FRM_EXT_TEXT;
	x = x_pos;
	y = y_pos;
	
	width = w;
	//init ccx_label_width and ccx_text_width
	//default ccx_label_width = 30% of form width
	ccx_percent_label_w = DEF_LABEL_WIDTH_PERCENT;
	ccx_label_width = ccx_percent_label_w * width/100;
	ccx_text_width	= width-ccx_label_width;

	height = h;
	if (height == 0)
		height = DEF_HEIGHT;

	y_text = 0;

	ccx_label_text 	= label_text;
	ccx_text 	= text;
	shadow 		= shadow_mode;
	ccx_label_color	= label_color;
	ccx_text_color	= text_color;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	ccx_label_obj	= NULL;
	ccx_text_obj	= NULL;
	corner_type	= 0;
	ccx_font 	= font_text;
	if (ccx_font == NULL){
		int dx = 0, dy 	= height;
		ccx_font =  *(CNeutrinoFonts::getInstance()->getDynFont(dx, dy));
	}
	ccx_label_align = ccx_text_align = CTextBox::NO_AUTO_LINEBREAK;

	//init slot to ensure paint text items after painted background
	sl_items_repaint = sigc::bind(sigc::mem_fun(*this, &CComponentsExtTextForm::forceItemsPaint), true);

	initParent(parent);

}

void CComponentsExtTextForm::initLabel()
{
	if (ccx_label_obj == NULL){
		//create ccx_label_obj and add to collection
		ccx_label_obj = new CComponentsLabel(this);
		ccx_label_obj->doPaintBg(false);
	}

	//set label properties
	if (ccx_label_obj){
		//assign general properties
		y_text = height/2 - height-2*fr_thickness;
		ccx_label_obj->setDimensionsAll(0, y_text, ccx_label_width-2*fr_thickness, height-2*fr_thickness);
		ccx_label_obj->setColorBody(col_body);
		if (cc_body_gradient_enable != cc_body_gradient_enable_old)
			ccx_label_obj->getCTextBoxObject()->clearScreenBuffer();
		ccx_label_obj->setTextColor(ccx_label_color);
		ccx_label_obj->setText(ccx_label_text, ccx_label_align, ccx_font);
		ccx_label_obj->enableTboxSaveScreen(cc_body_gradient_enable || cc_txt_save_screen);

		//corner of label item
		ccx_label_obj->setCorner(corner_rad-fr_thickness, CORNER_LEFT);
	}
}

void CComponentsExtTextForm::initText()
{
	//set text properties
	if (ccx_text_obj == NULL){
		//create ccx_text_obj and add to collection
		ccx_text_obj = new CComponentsText(this);
		ccx_text_obj->doPaintBg(false);
	}

	if (ccx_text_obj){
		//assign general properties
		y_text = height/2 - height-2*fr_thickness;
		ccx_text_obj->setDimensionsAll(ccx_label_obj->getWidth(), y_text, ccx_text_width-2*fr_thickness, height-2*fr_thickness);
		ccx_text_obj->setColorBody(col_body);
		if (cc_body_gradient_enable != cc_body_gradient_enable_old)
			ccx_text_obj->getCTextBoxObject()->clearScreenBuffer();
		ccx_text_obj->setTextColor(ccx_text_color);
		ccx_text_obj->setText(ccx_text, ccx_text_align, ccx_font);;
		ccx_text_obj->enableTboxSaveScreen(cc_body_gradient_enable || cc_txt_save_screen);

		//corner of text item
		ccx_text_obj->setCorner(corner_rad-fr_thickness, CORNER_RIGHT);
	}
}


void CComponentsExtTextForm::setLabelAndText(const std::string& label_text, const std::string& text, Font* font_text)
{
	ccx_label_text 	= label_text;
	ccx_text 	= text;
	if (font_text)
		ccx_font	= font_text;
	initCCTextItems();
}


void CComponentsExtTextForm::setLabelAndText(const neutrino_locale_t& locale_label_text, const neutrino_locale_t& locale_text, Font* font_text)
{
	setLabelAndText(g_Locale->getText(locale_label_text), g_Locale->getText(locale_text), font_text);
}

void CComponentsExtTextForm::setLabelAndTexts(const cc_string_ext_txt_t& texts)
{
	setLabelAndText(texts.label_text, texts.text, texts.font);
}

void CComponentsExtTextForm::setLabelAndTexts(const cc_locale_ext_txt_t& locale_texts)
{
	setLabelAndText(g_Locale->getText(locale_texts.label_text), g_Locale->getText(locale_texts.text), locale_texts.font);
}

void CComponentsExtTextForm::setLabelAndTextFont(Font* font)
{
	if (font == NULL)
		return;
	setLabelAndText(ccx_label_text, ccx_text, font);
}

void CComponentsExtTextForm::setTextModes(const int& label_mode, const int& text_mode)
{
	ccx_label_align = label_mode;
	ccx_text_align = text_mode;
	initCCTextItems();
}

void CComponentsExtTextForm::setLabelAndTextColor(const fb_pixel_t label_color , const fb_pixel_t text_color)
{
	ccx_label_color = label_color;
	ccx_text_color 	= text_color;
	initCCTextItems();
}

void CComponentsExtTextForm::initCCTextItems()
{
	initLabel();
	initText();

	if(!OnAfterPaintBg.empty())
		OnAfterPaintBg.clear();
	//init slot to handle repaint of text if background was repainted
	OnAfterPaintBg.connect(sl_items_repaint);
}

void CComponentsExtTextForm::setLabelWidthPercent(const uint8_t& percent_val)
{
	ccx_percent_label_w = (int)percent_val;
	initCCTextItems();
}

void CComponentsExtTextForm::paint(bool do_save_bg)
{
	//prepare items before paint
	initCCTextItems();

	//paint form contents
	paintForm(do_save_bg);
}
