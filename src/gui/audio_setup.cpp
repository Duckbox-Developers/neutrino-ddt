/*
	$port: audio_setup.cpp,v 1.6 2010/12/06 21:00:15 tuxbox-cvs Exp $

	audio setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

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


#include "audio_setup.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>

#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>

#include <driver/screen_max.h>

#include <audio.h>

#include <system/debug.h>

extern CAudioSetupNotifier	* audioSetupNotifier;
extern cAudio *audioDecoder;

CAudioSetup::CAudioSetup(bool wizard_mode)
{
	is_wizard = wizard_mode;

	width = w_max (40, 10);
	selected = -1;
}

CAudioSetup::~CAudioSetup()
{

}

int CAudioSetup::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	dprintf(DEBUG_DEBUG, "init audio setup\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	res = showAudioSetup();

	return res;
}

#define AUDIOMENU_ANALOGOUT_OPTION_COUNT 3
const CMenuOptionChooser::keyval AUDIOMENU_ANALOGOUT_OPTIONS[AUDIOMENU_ANALOGOUT_OPTION_COUNT] =
{
	{ 0, LOCALE_AUDIOMENU_STEREO    },
	{ 1, LOCALE_AUDIOMENU_MONOLEFT  },
	{ 2, LOCALE_AUDIOMENU_MONORIGHT }
};

#ifdef BOXMODEL_APOLLO
#define AUDIOMENU_SRS_OPTION_COUNT 3
#else
#define AUDIOMENU_SRS_OPTION_COUNT 2
#endif
const CMenuOptionChooser::keyval AUDIOMENU_SRS_OPTIONS[AUDIOMENU_SRS_OPTION_COUNT] =
{
	{ 0 , LOCALE_AUDIO_SRS_ALGO_LIGHT },
	{ 1 , LOCALE_AUDIO_SRS_ALGO_NORMAL },
#ifdef BOXMODEL_APOLLO
	{ 2 , LOCALE_AUDIO_SRS_ALGO_HEAVY }
#endif
};

#define AUDIOMENU_AVSYNC_OPTION_COUNT 3
const CMenuOptionChooser::keyval AUDIOMENU_AVSYNC_OPTIONS[AUDIOMENU_AVSYNC_OPTION_COUNT] =
{
	{ 0, LOCALE_OPTIONS_OFF },
	{ 1, LOCALE_OPTIONS_ON  },
	{ 2, LOCALE_AUDIOMENU_AVSYNC_AM }
};

#define AUDIOMENU_HDMI_DD_OPTION_COUNT 3
const CMenuOptionChooser::keyval AUDIOMENU_HDMI_DD_OPTIONS[AUDIOMENU_HDMI_DD_OPTION_COUNT] =
{
	{ HDMI_ENCODED_OFF,		LOCALE_OPTIONS_OFF		},
	{ HDMI_ENCODED_AUTO,		LOCALE_AUDIOMENU_HDMI_DD_AUTO	},
	{ HDMI_ENCODED_FORCED,		LOCALE_AUDIOMENU_HDMI_DD_FORCE	}
};

// #define AUDIOMENU_CLOCKREC_OPTION_COUNT 2
// const CMenuOptionChooser::keyval AUDIOMENU_CLOCKREC_OPTIONS[AUDIOMENU_CLOCKREC_OPTION_COUNT] =
// {
// 	{ 0, LOCALE_OPTIONS_OFF },
// 	{ 1, LOCALE_OPTIONS_ON  },
// };

/* audio settings menu */
int CAudioSetup::showAudioSetup()
{
	//menue init
	CMenuWidget* audioSettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width);
	audioSettings->setSelected(selected);
	audioSettings->setWizardMode(is_wizard);

	//analog modes (stereo, mono l/r...)
	CMenuOptionChooser * as_oj_analogmode 	= new CMenuOptionChooser(LOCALE_AUDIOMENU_ANALOG_MODE, &g_settings.audio_AnalogMode, AUDIOMENU_ANALOGOUT_OPTIONS, AUDIOMENU_ANALOGOUT_OPTION_COUNT, true, audioSetupNotifier);
	as_oj_analogmode->setHint("", LOCALE_MENU_HINT_AUDIO_ANALOG_MODE);

	//dd subchannel auto on/off
	CMenuOptionChooser * as_oj_ddsubchn 	= new CMenuOptionChooser(LOCALE_AUDIOMENU_DOLBYDIGITAL, &g_settings.audio_DolbyDigital, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, audioSetupNotifier);
	as_oj_ddsubchn->setHint("", LOCALE_MENU_HINT_AUDIO_DD);

	//dd via hdmi
	CMenuOptionChooser * as_oj_dd_hdmi 	= new CMenuOptionChooser(LOCALE_AUDIOMENU_HDMI_DD, &g_settings.hdmi_dd, AUDIOMENU_HDMI_DD_OPTIONS, AUDIOMENU_HDMI_DD_OPTION_COUNT, true, audioSetupNotifier);
	as_oj_dd_hdmi->setHint("", LOCALE_MENU_HINT_AUDIO_HDMI_DD);

	//dd via spdif
	CMenuOptionChooser * as_oj_dd_spdif 	= new CMenuOptionChooser(LOCALE_AUDIOMENU_SPDIF_DD, &g_settings.spdif_dd, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, audioSetupNotifier);
	as_oj_dd_spdif->setHint("", LOCALE_MENU_HINT_AUDIO_SPDIF_DD);

	CMenuOptionChooser * as_oj_avsync = NULL;
	CMenuOptionNumberChooser * as_oj_vsteps = NULL;
	CMenuOptionNumberChooser * st = NULL;
	if (!g_settings.easymenu) {
		//av synch
		as_oj_avsync	= new CMenuOptionChooser(LOCALE_AUDIOMENU_AVSYNC, &g_settings.avsync, AUDIOMENU_AVSYNC_OPTIONS, AUDIOMENU_AVSYNC_OPTION_COUNT, true, audioSetupNotifier);
		as_oj_avsync->setHint("", LOCALE_MENU_HINT_AUDIO_AVSYNC);

		//volume steps
		as_oj_vsteps = new CMenuOptionNumberChooser(LOCALE_AUDIOMENU_VOLUME_STEP, (int *)&g_settings.current_volume_step, true, 1, 25, NULL);
		as_oj_vsteps->setHint("", LOCALE_MENU_HINT_AUDIO_VOLSTEP);

		st    = new CMenuOptionNumberChooser(LOCALE_AUDIOMENU_VOLUME_START, &g_settings.start_volume, true, -1, 100, NULL, CRCInput::RC_nokey, NULL, 0, -1, LOCALE_OPTIONS_OFF);
		st->setHint("", LOCALE_MENU_HINT_AUDIO_VOLSTART);
		//clock rec
		//CMenuOptionChooser * as_oj_clockrec new CMenuOptionChooser(LOCALE_AUDIOMENU_CLOCKREC, &g_settings.clockrec, AUDIOMENU_CLOCKREC_OPTIONS, AUDIOMENU_CLOCKREC_OPTION_COUNT, true, audioSetupNotifier);
	}

	//SRS
	//SRS algo
	CMenuOptionChooser * as_oj_algo 	= new CMenuOptionChooser(LOCALE_AUDIO_SRS_ALGO, &g_settings.srs_algo, AUDIOMENU_SRS_OPTIONS, AUDIOMENU_SRS_OPTION_COUNT, g_settings.srs_enable, audioSetupNotifier);
	as_oj_algo->setHint("", LOCALE_MENU_HINT_AUDIO_SRS_ALGO);

	//SRS noise manage
	CMenuOptionChooser * as_oj_noise 	= new CMenuOptionChooser(LOCALE_AUDIO_SRS_NMGR, &g_settings.srs_nmgr_enable, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.srs_enable, audioSetupNotifier);
	as_oj_noise->setHint("", LOCALE_MENU_HINT_AUDIO_SRS_NMGR);

	//SRS reverence volume
	CMenuOptionNumberChooser * as_oj_volrev = new CMenuOptionNumberChooser(LOCALE_AUDIO_SRS_VOLUME, &g_settings.srs_ref_volume, g_settings.srs_enable, 1, 100, audioSetupNotifier);
	as_oj_volrev->setHint("", LOCALE_MENU_HINT_AUDIO_SRS_VOLUME);

	//SRS on/off
	CTruVolumeNotifier truevolSetupNotifier(as_oj_algo, as_oj_noise, as_oj_volrev);
	CMenuOptionChooser * as_oj_srsonoff 	= new CMenuOptionChooser(LOCALE_AUDIO_SRS_IQ, &g_settings.srs_enable, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, &truevolSetupNotifier);
	as_oj_srsonoff->setHint("", LOCALE_MENU_HINT_AUDIO_SRS);

	//paint items
	audioSettings->addIntroItems(LOCALE_MAINSETTINGS_AUDIO);
	//---------------------------------------------------------
	audioSettings->addItem(as_oj_analogmode);
	audioSettings->addItem(GenericMenuSeparatorLine);
	//---------------------------------------------------------
	audioSettings->addItem(as_oj_dd_hdmi);
	audioSettings->addItem(as_oj_dd_spdif);
	audioSettings->addItem(as_oj_ddsubchn);
	//---------------------------------------------------------
	if (!g_settings.easymenu) {
		audioSettings->addItem(GenericMenuSeparatorLine);
		audioSettings->addItem(as_oj_avsync);
		audioSettings->addItem(as_oj_vsteps);
		audioSettings->addItem(st);
		//audioSettings->addItem(as_clockrec);
	}
	//---------------------------------------------------------
	audioSettings->addItem(GenericMenuSeparatorLine);
	audioSettings->addItem(as_oj_srsonoff);
	audioSettings->addItem(as_oj_algo);
#ifndef BOXMODEL_APOLLO
	audioSettings->addItem(as_oj_noise);
#endif
	audioSettings->addItem(as_oj_volrev);
#if 0
	audioSettings->addItem(mf);
#endif

	int res = audioSettings->exec(NULL, "");
	selected = audioSettings->getSelected();
	delete audioSettings;
#ifdef BOXMODEL_APOLLO
	delete as_oj_noise;
#endif
	return res;
}

//sets menu mode to "wizard" or "default"
void CAudioSetup::setWizardMode(bool mode)
{
	printf("[neutrino audio setup] %s set audio settings menu to mode %d...\n", __FUNCTION__, mode);
	is_wizard = mode;
}

CTruVolumeNotifier::CTruVolumeNotifier(CMenuOptionChooser* o1, CMenuOptionChooser* o2, CMenuOptionNumberChooser *n1)
{
	toDisable_oj[0]=o1;
	toDisable_oj[1]=o2;
	toDisable_nj=n1;
}

bool CTruVolumeNotifier::changeNotify(const neutrino_locale_t, void * data)
{
	int active = (*(int *)data);

	for (int i = 0; i < 2; i++)
		toDisable_oj[i]->setActive(active);

	toDisable_nj->setActive(active);

	audioDecoder->SetSRS(g_settings.srs_enable, g_settings.srs_nmgr_enable, g_settings.srs_algo, g_settings.srs_ref_volume);

	return false;
}
