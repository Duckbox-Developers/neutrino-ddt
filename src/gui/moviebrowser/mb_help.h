/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

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

	***********************************************************

	Module Name: mb_help.h

	Description: Implementation of the CMovieBrowser class
	             This class provides a filebrowser window to view, select and start a movies from HD.
	             This class does replace the Filebrowser

	Date:	   Nov 2005

	Author: Guenther@tuxbox.berlios.org
		based on code of Steffen Hehn 'McClean'

	outsourced:
	(C) 2016, Thilo Graf 'dbt'
*/

#ifndef MOVIEBROWSER_HELP_H_
#define MOVIEBROWSER_HELP_H_


#include <gui/widget/helpbox.h>

// Class to show Moviebrowser Information, to be used by menu
class CMovieHelp : public CMenuTarget
{
	private:

	public:
		CMovieHelp(){};
		~CMovieHelp(){};

		int exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/)
		{
			Helpbox helpbox;
			helpbox.addLine(NEUTRINO_ICON_BUTTON_RED,	g_Locale->getText(LOCALE_MOVIEBROWSER_HELP_BUTTON_RED));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_GREEN,	g_Locale->getText(LOCALE_MOVIEBROWSER_HELP_BUTTON_GREEN));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_YELLOW,	g_Locale->getText(LOCALE_MOVIEBROWSER_HELP_BUTTON_YELLOW));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_BLUE,	g_Locale->getText(LOCALE_MOVIEBROWSER_HELP_BUTTON_BLUE));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_MENU_SMALL,g_Locale->getText(LOCALE_MOVIEBROWSER_HELP_BUTTON_MENU));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_PLAY,	g_Locale->getText(LOCALE_MOVIEBROWSER_HELP_BUTTON_PLAY));
			helpbox.addLine("");
			helpbox.addLine(NEUTRINO_ICON_BUTTON_OKAY,	g_Locale->getText(LOCALE_MOVIEBROWSER_HELP_BUTTON_OKAY));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_MUTE_SMALL,g_Locale->getText(LOCALE_MOVIEBROWSER_HELP_BUTTON_MUTE));
			helpbox.addLine("");
			helpbox.addLine(NEUTRINO_ICON_BUTTON_LEFT,	g_Locale->getText(LOCALE_MOVIEBROWSER_HELP_BUTTON_LEFT));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_RIGHT,	g_Locale->getText(LOCALE_MOVIEBROWSER_HELP_BUTTON_RIGHT));
			helpbox.show(LOCALE_MESSAGEBOX_INFO);
			return(0);
		}
};

#endif /*MOVIEBROWSER_HELP_H_*/
