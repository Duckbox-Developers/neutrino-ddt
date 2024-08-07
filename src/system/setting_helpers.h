#ifndef __setting_helpers__
#define __setting_helpers__

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


#include <global.h>
#include <gui/widget/menue.h>
#include <zapit/client/zapittypes.h>
#include <hardware/playback.h>
#if HAVE_SH4_HARDWARE
#include <hardware/audio.h>
#endif

#include <string>

class CGenericMenuActivate
{
	private:
		std::vector<CMenuItem *> items;
	public:
		CGenericMenuActivate()		{};
		~CGenericMenuActivate()
		{
			items.clear();
		};

		void Add(CMenuItem *item)
		{
			items.push_back(item);
		}
		void Clear()
		{
			items.clear();
		}
		void Activate(bool enable)
		{
			for (std::vector<CMenuItem *>::iterator it = items.begin(); it != items.end(); ++it)
				(*it)->setActive(enable);
		}
};

class COnOffNotifier : public CChangeObserver
{
	private:
		int offValue;
		std::vector<CMenuItem *> toDisable;

	public:
		COnOffNotifier(int OffValue = 0);
		bool changeNotify(const neutrino_locale_t, void *Data);

		void addItem(CMenuItem *menuItem);
};

class CSectionsdConfigNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void *);
};

class CTouchFileNotifier : public CChangeObserver
{
		const char *filename;
	public:
		inline CTouchFileNotifier(const char *_filename)
		{
			filename = _filename;
		};
		bool changeNotify(const neutrino_locale_t, void *data);
};

class CColorSetupNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void *);
		static void setPalette();
};

class CAudioSetupNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t OptionName, void *);
};

class CFontSizeNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void *);
};

class CSubtitleChangeExec : public CMenuTarget
{
	private:
		cPlayback *playback;
	public:
		CSubtitleChangeExec(cPlayback *p = NULL)
		{
			playback = p;
		}
		int exec(CMenuTarget *parent, const std::string &actionKey);
};

class CNVODChangeExec : public CMenuTarget
{
	public:
		int exec(CMenuTarget *parent, const std::string &actionKey);
};

class CTZChangeNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void *data);
};

class CDataResetNotifier : public CMenuTarget
{
	public:
		int exec(CMenuTarget *parent, const std::string &actionKey);
};

#if BOXMODEL_DM8000 || BOXMODEL_DM800SE || BOXMODEL_DM800SEV2 || BOXMODEL_DM820 || BOXMODEL_DM7080 || BOXMODEL_UFS922 || BOXMODEL_CUBEREVO || BOXMODEL_CUBEREVO_MINI2 || BOXMODEL_CUBEREVO_250HD || BOXMODEL_CUBEREVO_3000HD || BOXMODEL_IPBOX9900 || BOXMODEL_IPBOX99 || BOXMODEL_VUDUO2 || BOXMODEL_VUULTIMO || BOXMODEL_VUUNO
class CFanControlNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void *data);
		static void setSpeed(unsigned int speed);
};
#endif

//do we need a class?
inline int check_shoutcast_dev_id()
{
	return ((g_settings.shoutcast_dev_id != "XXXXXXXXXXXXXXXX") && !g_settings.shoutcast_dev_id.empty());
}
inline int check_tmdb_api_key()
{
	return ((g_settings.tmdb_api_key != "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX") && !g_settings.tmdb_api_key.empty());
}

#endif
