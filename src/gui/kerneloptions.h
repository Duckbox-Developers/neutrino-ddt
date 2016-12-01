/*
		KernelOptions Menu

		Copyright (C) 2012-2013 martii

		Kommentar:

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

#ifndef __kerneloptions_h__
#define __kerneloptions_h__

#include <string>
#include <vector>

#include <driver/framebuffer.h>
#include <system/lastchannel.h>
#include <system/setting_helpers.h>
#include <configfile.h>

class CKernelOptions : public CMenuTarget, public CChangeObserver
{
	private:
		int width;
		struct module {
			std::string comment;
			std::vector<std::pair <std::string,std::string> > moduleList;
			int active_orig;
			int active;
			bool installed;
			CMenuOptionChooser *mc;
		};
		std::vector<module> modules;
		void load();
		void save();
		void updateStatus();
		void loadModule(int i);
		void unloadModule(int i);
	public:
		CKernelOptions();
		void hide();
		int exec(CMenuTarget* parent, const std::string & actionKey);
		int Settings();
		bool isEnabled(std::string name);
		bool Enable(std::string name, bool active);
		bool changeNotify(const neutrino_locale_t /*OptionName*/, void * /*Data*/);
};

#endif //__kerneloptions_h__
// vim:ts=4
