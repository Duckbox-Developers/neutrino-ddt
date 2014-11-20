/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2014 CoolStream International Ltd

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, version 2 of the License.

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

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <driver/display.h>

#include <gui/color.h>

#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>

#include <system/helpers.h>
#include <gui/widget/keyboard_input.h>

static std::string keys_english[2][KEY_ROWS][KEY_COLUMNS] =
{
	{
		{ "`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-",  "=",  "§"  },
		{ "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[",  "]",  "{", "}"  },
		{ "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "\'", "\\", ":", "\"" },
		{ "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "<",  ">",  "?", " "  }
	},
	{
		{ "~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_",  "+",  "§", },
		{ "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{",  "}",  "{", "}"  },
		{ "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "\'", "\\", ":", "\"" },
		{ "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "<",  ">",  "?", " "  }
	}
};

static std::string keys_deutsch[2][KEY_ROWS][KEY_COLUMNS] =
{
	{
		{ "°", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "ß", "´", "@" },
		{ "q", "w", "e", "r", "t", "z", "u", "i", "o", "p", "ü", "+", "~", "/" },
		{ "a", "s", "d", "f", "g", "h", "j", "k", "l", "ö", "ä", "#", "[", "]" },
		{ "y", "x", "c", "v", "b", "n", "m", ",", ".", "-", "|", "<", ">", " " }
	},
	{
		{ "^", "!", "\"","§", "$", "%", "&", "/", "(", ")", "=", "?", "`", "€" },
		{ "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P", "Ü", "*", "\\","/" },
		{ "A", "S", "D", "F", "G", "H", "J", "K", "L", "Ö", "Ä", "\'","{", "}" },
		{ "Y", "X", "C", "V", "B", "N", "M", ";", ":", "_", "²", "³", "µ", " " }
	}
};

static std::string keys_russian[2][KEY_ROWS][KEY_COLUMNS] =
{
	{
		{ "ё", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-",  "=", "§"  },
		{ "й", "ц", "у", "к", "е", "н", "г", "ш", "щ", "з", "х", "ъ",  "[", "]"  },
		{ "ф", "ы", "в", "а", "п", "р", "о", "л", "д", "ж", "э", "\\", ":", "\"" },
		{ "я", "ч", "с", "м", "и", "т", "ь", "б", "ю", "/", ".",  ",", "?", " "  }
	},
	{
		{ "Ё", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "§" },
		{ "Й", "Ц", "У", "К", "Е", "Н", "Г", "Ш", "Щ", "З", "Х", "Х", "{", "}" },
		{ "Ф", "Ы", "В", "А", "П", "Р", "О", "Л", "Д", "Ж", "Э", "|", ";", "~" },
		{ "Я", "Ч", "С", "М", "И", "Т", "Ь", "Б", "Ю", "?", "<", ">", "?", " " }
	}
};

struct keyboard_layout keyboards[] =
{
	{ "English", "english", keys_english },
	{ "Deutsch", "deutsch", keys_deutsch },
	{ "Русский", "russkij", keys_russian },
};
#define LAYOUT_COUNT (sizeof(keyboards)/sizeof(struct keyboard_layout))

std::string UTF8ToString(const char * &text)
{
	std::string res;

	res = *text;
	if ((((unsigned char)(*text)) & 0x80) != 0)
	{
		int remaining_unicode_length = 0;
		if ((((unsigned char)(*text)) & 0xf8) == 0xf0)
			remaining_unicode_length = 3;
		else if ((((unsigned char)(*text)) & 0xf0) == 0xe0)
			remaining_unicode_length = 2;
		else if ((((unsigned char)(*text)) & 0xe0) == 0xc0)
			remaining_unicode_length = 1;

		for (int i = 0; i < remaining_unicode_length; i++)
		{
			text++;
			if (((*text) & 0xc0) != 0x80)
				break;
			res += *text;
		}
	}
	return res;
}

CInputString::CInputString(int Size)
{
	len = Size;
	std::string tmp;
	for (unsigned i = 0; i < len; i++)
		inputString.push_back(tmp);
}

void CInputString::clear()
{
	for (unsigned i = 0; i < len; i++)
		inputString[i].clear();
}

size_t CInputString::length()
{
	return inputString.size();
}

CInputString & CInputString::operator=(const std::string &str)
{
	clear();
	const char * text = str.c_str();
	for (unsigned i = 0; i < inputString.size() && *text; i++, text++)
		inputString[i] = UTF8ToString(text);
	return *this;
}

void CInputString::assign(size_t n, char c)
{
	clear();
	for (unsigned i = 0; i < n && i < inputString.size(); i++)
		inputString[i] = c;
}

std::string &CInputString::at(size_t pos)
{
	return inputString[pos];
}

std::string &CInputString::getValue()
{
	valueString.clear();
	for (unsigned i = 0; i < inputString.size(); i++) {
		if (!inputString[i].empty())
			valueString += inputString[i];
	}
	valueString = valueString.erase(valueString.find_last_not_of(" ") + 1);
	return valueString;
}

const char* CInputString::c_str()
{
	return getValue().c_str();
}

CKeyboardInput::CKeyboardInput(const neutrino_locale_t Name, std::string* Value, int Size, CChangeObserver* Observ, const char * const Icon, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2)
{
	name =  Name;
	head = g_Locale->getText(Name);
	valueString = Value;
	inputSize = Size;

	iconfile = Icon ? Icon : "";

	observ = Observ;
	hint_1 = Hint_1;
	hint_2 = Hint_2;
	inputString = new CInputString(inputSize);
	layout = NULL;
	selected = 0;
	caps = 0;
	srow = scol = 0;
	focus = FOCUS_STRING;
}

CKeyboardInput::CKeyboardInput(const std::string &Name, std::string *Value, int Size, CChangeObserver* Observ, const char * const Icon, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2)
{
	name = NONEXISTANT_LOCALE;
	head = Name;
	valueString = Value;
	inputSize =  Size;

	iconfile = Icon ? Icon : "";

	observ = Observ;
	hint_1 = Hint_1;
	hint_2 = Hint_2;
	inputString = new CInputString(inputSize);
	layout = NULL;
	selected = 0;
	caps = 0;
	srow = scol = 0;
	focus = FOCUS_STRING;
}

CKeyboardInput::~CKeyboardInput()
{
	delete inputString;
}

#define BORDER_OFFSET 20
#define INPUT_BORDER 2
#define KEY_FRAME_WIDTH 2 // keys frame width
#define KEY_BORDER 4 // spacing between keys

void CKeyboardInput::init()
{
	frameBuffer = CFrameBuffer::getInstance();
	setLayout();

	hheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	iheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->getHeight();
	input_h = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight() + 2;		// font height + border
	input_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("M") + INPUT_BORDER;	// hack font width + border
	offset  = BORDER_OFFSET;
	fheight = paintFooter(false);
	iwidth = inputSize*input_w + 2*offset;

	key_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth("M") + 20;
	key_h = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight() + 10;
	kwidth = key_w*KEY_COLUMNS + (KEY_COLUMNS-1)*KEY_BORDER + 2*offset;

	width = std::max(iwidth, kwidth);
	width = std::min(width, (int) frameBuffer->getScreenWidth());

	int tmp_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(head);
	if (!(iconfile.empty()))
	{
		int icol_w, icol_h;
		frameBuffer->getIconSize(iconfile.c_str(), &icol_w, &icol_h);
		hheight = std::max(hheight, icol_h + (offset/4));
		tmp_w += icol_w + (offset/2);
	}
	width = std::max(width, tmp_w + offset);

	bheight = input_h + (key_h+KEY_BORDER)*KEY_ROWS + 3*offset;
	if ((hint_1 != NONEXISTANT_LOCALE) || (hint_2 != NONEXISTANT_LOCALE))
	{
		if (hint_1 != NONEXISTANT_LOCALE)
		{
			tmp_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->getRenderWidth(g_Locale->getText(hint_1));
			width = std::max(width, tmp_w + 2*offset);
			bheight += iheight;
		}
		if (hint_2 != NONEXISTANT_LOCALE)
		{
			tmp_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->getRenderWidth(g_Locale->getText(hint_2));
			width = std::max(width, tmp_w + 2*offset);
			bheight += iheight;
		}
		bheight += offset;
	}

	height = hheight+ bheight + fheight;

	x = getScreenStartX(width);
	y = getScreenStartY(height);
	*inputString = *valueString;
	changed = false;
}

void CKeyboardInput::setLayout()
{
	if (layout != NULL)
		return;

	layout = &keyboards[0];
	keyboard = layout->keys[caps];
	for(unsigned i = 0; i < LAYOUT_COUNT; i++) {
		if (keyboards[i].locale == g_settings.language) {
			layout = &keyboards[i];
			keyboard = layout->keys[caps];
			return;
		}
	}
}

void CKeyboardInput::switchLayout()
{
	unsigned i;
	for (i = 0; i < LAYOUT_COUNT; i++) {
		if (layout == &keyboards[i])
			break;
	}
	i++;
	if (i >= LAYOUT_COUNT)
		i = 0;
	layout = &keyboards[i];
	keyboard = layout->keys[caps];
	paintFooter();
	paintKeyboard();
}

void CKeyboardInput::NormalKeyPressed()
{
	if (keyboard[srow][scol].empty())
		return;

	inputString->at(selected) = keyboard[srow][scol];
	if (selected < (inputSize - 1))
	{
		selected++;
		paintChar(selected - 1);
	}
	paintChar(selected);
	changed = true;
}

void CKeyboardInput::clearString()
{
	selected = 0;
	inputString->assign(inputString->length(), ' ');
	for (int i = 0 ; i < inputSize; i++)
		paintChar(i);
	changed = true;
}

void CKeyboardInput::switchCaps()
{
	caps = caps ? 0 : 1;
	keyboard = layout->keys[caps];
	paintKeyboard();
}

void CKeyboardInput::keyUpPressed()
{
	if (focus == FOCUS_KEY) {
		int old_row = srow;
		srow--;
		paintKey(old_row, scol);
		if (srow < 0) {
			focus = FOCUS_STRING;
			paintChar(selected);
		} else {
			paintKey(srow, scol);
		}
	} else {
		srow = KEY_ROWS - 1;
		focus = FOCUS_KEY;
		paintChar(selected);
		paintKey(srow, scol);
	}
}

void CKeyboardInput::keyDownPressed()
{
	if (focus == FOCUS_KEY) {
		int old_row = srow;
		srow++;
		paintKey(old_row, scol);
		if (srow >= KEY_ROWS) {
			focus = FOCUS_STRING;
			paintChar(selected);
		} else {
			paintKey(srow, scol);
		}
	} else {
		srow = 0;
		focus = FOCUS_KEY;
		paintChar(selected);
		paintKey(srow, scol);
	}
}

void CKeyboardInput::keyLeftPressed()
{
	if (focus == FOCUS_KEY) {
		int old_col = scol;;
		scol--;
		if (scol < 0)
			scol = KEY_COLUMNS -1;
		paintKey(srow, old_col);
		paintKey(srow, scol);
	} else {
		int old = selected;
		if (selected > 0)
			selected--;
		else
			selected = inputSize - 1;

		paintChar(old);
		paintChar(selected);
	}
}

void CKeyboardInput::keyRightPressed()
{
	if (focus == FOCUS_KEY) {
		int old_col = scol;;
		scol++;
		if (scol >= KEY_COLUMNS)
			scol = 0;
		paintKey(srow, old_col);
		paintKey(srow, scol);
	} else {
		int old = selected;
		if (selected < (inputSize - 1)) {
			selected++;
		} else
			selected = 0;

		paintChar(old);
		paintChar(selected);
	}
}

void CKeyboardInput::deleteChar()
{
	int item = selected;
	while (item < (inputSize -1))
	{
		inputString->at(item) = inputString->at(item+1);
		paintChar(item);
		item++;
	}
	inputString->at(item) = ' ';
	paintChar(item);
	changed = true;
}

void CKeyboardInput::keyBackspacePressed(void)
{
	if (selected > 0)
	{
		selected--;
		for (int i = selected; i < inputSize - 1; i++)
		{
			inputString->at(i) = inputString->at(i + 1);
			paintChar(i);
		}
		inputString->at(inputSize - 1) = ' ';
		paintChar(inputSize - 1);
		changed = true;
	}
}

void CKeyboardInput::insertChar()
{
	int item = inputSize -1;
	while (item > selected)
	{
		inputString->at(item) = inputString->at(item-1);
		paintChar(item);
		item--;
	}
	inputString->at(item) = ' ';
	paintChar(item);
	changed = true;
}

std::string &CKeyboardInput::getValue(void)
{
	return *valueString;
}

int CKeyboardInput::exec(CMenuTarget* parent, const std::string &)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	init();

	std::string oldval = *valueString;

	fb_pixel_t * pixbuf = NULL;
	if (!parent) {
		pixbuf = new fb_pixel_t[(width + SHADOW_OFFSET) * (height + SHADOW_OFFSET)];
		if (pixbuf)
			frameBuffer->SaveScreen(x, y, width + SHADOW_OFFSET, height + SHADOW_OFFSET, pixbuf);
	}

	paint();
	paintKeyboard();

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

	bool loop=true;
	while (loop)
	{
		if (changed)
		{
			changed = false;
			CVFD::getInstance()->showMenuText(1, inputString->c_str() , selected+1);
		}
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);

		if (msg <= CRCInput::RC_MaxRC)
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		if (msg==CRCInput::RC_left)
		{
			keyLeftPressed();
		}
		else if (msg==CRCInput::RC_right)
		{
			keyRightPressed();
		}
		else if (msg == CRCInput::RC_up)
		{
			keyUpPressed();
		}
		else if (msg == CRCInput::RC_down)
		{
			keyDownPressed();
		}
		else if (msg==CRCInput::RC_red)
		{
			loop = false;
		}
		else if (msg == CRCInput::RC_green)
		{
			insertChar();
		}
		else if (msg==CRCInput::RC_yellow)
		{
			clearString();
		}
		else if (msg == CRCInput::RC_blue)
		{
			switchCaps();
		}
		else if (msg == CRCInput::RC_rewind)
		{
			keyBackspacePressed();
		}
		else if (msg == CRCInput::RC_ok)
		{
			if (focus == FOCUS_KEY)
				NormalKeyPressed();
		}
		else if (msg == CRCInput::RC_setup)
		{
			switchLayout();
		}
		else if ((msg == CRCInput::RC_home) || (msg == CRCInput::RC_timeout))
		{
			if ((inputString->getValue() != oldval) &&
					(ShowMsg(name, LOCALE_MESSAGEBOX_DISCARD, CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbCancel) == CMessageBox::mbrCancel))
				continue;

			*inputString = oldval;
			loop = false;
			res = menu_return::RETURN_EXIT_REPAINT;
		}
		else if ((msg ==CRCInput::RC_sat) || (msg == CRCInput::RC_favorites))
		{
		}
		else
		{
			if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
			{
				loop = false;
				res = menu_return::RETURN_EXIT_ALL;
			}
		}
	}

	if (pixbuf)
	{
		frameBuffer->RestoreScreen(x, y, width + SHADOW_OFFSET, height + SHADOW_OFFSET, pixbuf);
		delete[] pixbuf;
	} else
		hide();

	*valueString = inputString->getValue();

	if ((observ) && (msg == CRCInput::RC_red))
		observ->changeNotify(name, (void *) valueString->c_str());

	return res;
}

void CKeyboardInput::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width + SHADOW_OFFSET, height + SHADOW_OFFSET);
}

int CKeyboardInput::paintFooter(bool show)
{
	button_label_ext footerButtons[] = {
		{ NEUTRINO_ICON_BUTTON_RED     , LOCALE_STRINGINPUT_SAVE     , NULL,                  0, false },
		{ NEUTRINO_ICON_BUTTON_GREEN   , LOCALE_STRINGINPUT_INSERT   , NULL,                  0, false },
		{ NEUTRINO_ICON_BUTTON_YELLOW  , LOCALE_STRINGINPUT_CLEAR    , NULL,                  0, false },
		{ NEUTRINO_ICON_BUTTON_BLUE    , LOCALE_STRINGINPUT_CAPS     , NULL,                  0, false },
		{ NEUTRINO_ICON_BUTTON_BACKWARD, LOCALE_STRINGINPUT_BACKSPACE, NULL,                  0, false },
		{ NEUTRINO_ICON_BUTTON_MENU    , NONEXISTANT_LOCALE          , NULL,                  0, false },
		{ layout->locale.c_str()       , NONEXISTANT_LOCALE          , layout->name.c_str() , 0, false }
	};

	int cnt = (sizeof(footerButtons)/sizeof(struct button_label_ext));
	if (show)
		return ::paintButtons(footerButtons, cnt, x, y+ hheight+ bheight, width, fheight, width, true);
	else
		return ::paintButtons(footerButtons, cnt, 0, 0, 0, 0, 0, false, NULL, NULL);
}

void CKeyboardInput::paint()
{
	frameBuffer->paintBoxRel(x + SHADOW_OFFSET, y + SHADOW_OFFSET, width, height, COL_MENUCONTENTDARK_PLUS_0, RADIUS_LARGE, CORNER_ALL); //round
	frameBuffer->paintBoxRel(x, y + hheight, width, bheight, COL_MENUCONTENT_PLUS_0);

	CComponentsHeader header(x, y, width, hheight, head, iconfile);
	header.paint(CC_SAVE_SCREEN_NO);

	key_y = y+ hheight+ offset+ input_h+ offset;
	if ((hint_1 != NONEXISTANT_LOCALE) || (hint_2 != NONEXISTANT_LOCALE))
	{
		if (hint_1 != NONEXISTANT_LOCALE)
		{
			key_y += iheight;
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->RenderString(x+ offset, key_y, width- 2*offset, g_Locale->getText(hint_1), COL_MENUCONTENT_TEXT);
		}
		if (hint_2 != NONEXISTANT_LOCALE)
		{
			key_y += iheight;
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->RenderString(x+ offset, key_y, width- 2*offset, g_Locale->getText(hint_2), COL_MENUCONTENT_TEXT);
		}
		key_y += offset;
	}

	for (int count = 0; count < inputSize; count++)
		paintChar(count);

	paintFooter();
}

void CKeyboardInput::paintChar(int pos)
{
	if (pos < (int) inputString->length())
		paintChar(pos, inputString->at(pos));
}

void CKeyboardInput::paintChar(int pos, std::string &c)
{
	int xpos = x + offset + (width-iwidth)/2 + pos* input_w;
	int ypos = y+ hheight+ offset;

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	if (pos == selected)
	{
		color   = COL_MENUCONTENTSELECTED_TEXT;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	else
	{
		color   = COL_MENUCONTENT_TEXT;
		bgcolor = COL_MENUCONTENT_PLUS_0;
	}

	frameBuffer->paintBoxRel(xpos, ypos, input_w, input_h, COL_MENUCONTENT_PLUS_2);
	frameBuffer->paintBoxRel(xpos+ 1, ypos+ 1, input_w- 2, input_h- 2, bgcolor);

	int ch_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(c);
	int ch_x = xpos + std::max(input_w/2 - ch_w/2, 0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(ch_x, ypos+ input_h, ch_w, c, color);
}

void CKeyboardInput::paintKeyboard()
{
	for (int i = 0; i < KEY_ROWS; i++) {
		for (int j = 0; j < KEY_COLUMNS; j++)
			paintKey(i, j);
	}
}

void CKeyboardInput::paintKey(int row, int column)
{
	int xpos = x + offset + (width-kwidth)/2 + (key_w + KEY_BORDER)*column;
	//int ypos = y + offset*2 + hheight + input_h + (key_h + KEY_BORDER)*row;
	//key_y = y+ hheight+ offset+ input_h+ offset;
	int ypos = key_y + (key_h + KEY_BORDER)*row;

	fb_pixel_t color;
	fb_pixel_t bgcolor;
	if (focus == FOCUS_KEY && row == srow && column == scol) {
		color   = COL_MENUCONTENTSELECTED_TEXT;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	} else {
		color   = COL_MENUCONTENT_TEXT;
		bgcolor = COL_MENUCONTENT_PLUS_0;
	}

	int radius = CORNER_RADIUS_SMALL;
	frameBuffer->paintBoxRel(xpos, ypos, key_w, key_h, bgcolor, radius);
	frameBuffer->paintBoxFrame(xpos, ypos, key_w, key_h, KEY_FRAME_WIDTH, COL_MENUCONTENT_PLUS_6, radius);

	if (keyboard[row][column].empty())
		return;

	std::string &s = keyboard[row][column];
	int ch_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(s);
	int ch_h = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	int ch_x = xpos + key_w/2 - ch_w/2;
	int ch_y = ypos + key_h/2 + ch_h/2;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(ch_x, ch_y, ch_w, s, color);
}
