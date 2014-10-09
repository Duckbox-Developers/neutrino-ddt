/*
	$port: themes.h,v 1.6 2010/06/01 19:58:38 tuxbox-cvs Exp $
	
	Neutrino-GUI  -   DBoxII-Project

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

	Copyright (C) 2007, 2008, 2009 (flasher) Frank Liebelt
*/

#ifndef __cthemes__
#define __cthemes__
#include <string>
#include <configfile.h>
#include <system/setting_helpers.h>

class CThemes : public CMenuTarget, CChangeObserver
{
	private:
		CConfigFile themefile;
		CColorSetupNotifier *notifier;

		int width;
		SNeutrinoTheme oldTheme;

		bool hasThemeChanged;

		int Show();
		void readFile(char* themename);
		void saveFile(char* themename);
		void readThemes(CMenuWidget &);
		void rememberOldTheme(bool remember);

	public:
		CThemes();
		void setupDefaultColors();
		int exec(CMenuTarget* parent, const std::string & actionKey);
		static void setTheme(CConfigFile &configfile);
		static void getTheme(CConfigFile &configfile);
};

#endif
