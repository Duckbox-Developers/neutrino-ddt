/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	volume bar - Neutrino-GUI
	Copyright (C) 2011-2013 M. Liebmann (micha-bbg)

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <global.h>
#include <neutrino.h>
#include <gui/infoclock.h>
#include <gui/keybind_setup.h>
#include <system/debug.h>
#include <audio.h>
#include <system/settings.h>
#include <system/helpers.h>
#include <daemonc/remotecontrol.h>
#include <driver/display.h>
#include <driver/volume.h>
#include <driver/display.h>
#include <gui/audiomute.h>
#include <gui/mediaplayer.h>
#include <zapit/zapit.h>
#ifdef ENABLE_GRAPHLCD
#include <driver/nglcd.h>
#endif


extern CRemoteControl * g_RemoteControl;
extern cAudio * audioDecoder;

CVolume::CVolume()
{
	frameBuffer	= CFrameBuffer::getInstance();
	volscale 	= NULL;
	mute_ax		= 0;
	mute_ay		= 0;
	mute_dx		= 0;
	mute_dy		= 0;
	m_mode 		= CNeutrinoApp::getInstance()->getMode();
	channel_id	= 0;
	apid		= 0;
}

CVolume::~CVolume()
{
	delete volscale;
}

CVolume* CVolume::getInstance()
{
	static CVolume* Volume = NULL;
	if(!Volume)
		Volume = new CVolume();
	return Volume;
}

void CVolume::setvol(int vol)
{
	CZapit::getInstance()->SetVolume(vol);
}

void CVolume::setVolumeExt(int vol)
{
	g_settings.current_volume = vol;
	CZapit::getInstance()->SetVolume(vol);
	CVFD::getInstance()->showVolume(vol);
	if (CNeutrinoApp::getInstance()->isMuted() && vol > 0)
		CAudioMute::getInstance()->AudioMute(false, true);
}

void CVolume::setVolume(const neutrino_msg_t key)
{
	neutrino_msg_t msg	= key;
	int mode = CNeutrinoApp::getInstance()->getMode();
	
	if (msg <= CRCInput::RC_MaxRC) {
		if(m_mode != mode) {
			m_mode = mode;
			setVolume(msg);
			return;
		}
	}

	hideVolscale();
	showVolscale();

	neutrino_msg_data_t data = 0;
	uint64_t timeoutEnd = 0;
	int vol = g_settings.current_volume;

	do {
		if (msg <= CRCInput::RC_MaxRC) 
		{
			bool sub_chan_keybind = g_settings.mode_left_right_key_tv == SNeutrinoSettings::VOLUME
						&& g_RemoteControl && g_RemoteControl->subChannels.size() < 1;
			if ((msg == (neutrino_msg_t) g_settings.key_volumeup || msg == (neutrino_msg_t) g_settings.key_volumedown) ||
			    (sub_chan_keybind && (msg == CRCInput::RC_right || msg == CRCInput::RC_left))) {
				int dir = (msg == (neutrino_msg_t) g_settings.key_volumeup || msg == CRCInput::RC_right) ? 1 : -1;
				if (CNeutrinoApp::getInstance()->isMuted() && (dir > 0 || g_settings.current_volume > 0)) {
					hideVolscale();
					CAudioMute::getInstance()->AudioMute(false, true);
					setVolume(msg);
					return;
				}

				if (!CNeutrinoApp::getInstance()->isMuted()) {
					/* current_volume is char, we need signed to catch v < 0 */
					int v = g_settings.current_volume;
					v += dir * g_settings.current_volume_step;
					if (v > 100)
						v = 100;
					else if (v < 1) {
						v = 0;
						g_settings.current_volume = 0;
						if (g_settings.show_mute_icon) {
							hideVolscale();
							CAudioMute::getInstance()->AudioMute(true, true);
							setVolume(msg);
							return;
						}
					}
					g_settings.current_volume = v;
#ifdef ENABLE_GRAPHLCD
					nGLCD::ShowVolume(true);
#endif
				}
			}
			else if (msg == CRCInput::RC_home)
				break;
			else {
				g_RCInput->postMsg(msg, data);
				break;
			}

			setvol(g_settings.current_volume);
			timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_VOLUMEBAR] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_VOLUMEBAR]);
		}
		else if (msg == NeutrinoMessages::EVT_VOLCHANGED) {
			timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_VOLUMEBAR] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_VOLUMEBAR]);
		}
		else if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::unhandled) {
			g_RCInput->postMsg(msg, data);
			break;
		}

		if (volscale) {
			if(vol != g_settings.current_volume) {
				vol = g_settings.current_volume;
				volscale->paint();
			}
		}

		CVFD::getInstance()->showVolume(g_settings.current_volume);
		if (msg != CRCInput::RC_timeout) {
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true );
		}
	} while (msg != CRCInput::RC_timeout);

#ifdef ENABLE_GRAPHLCD
	nGLCD::ShowVolume(false);
#endif
	hideVolscale();
}

bool CVolume::hideVolscale()
{
	bool ret = false;
	if (volscale) {
		if (volscale->isPainted()) {
			volscale->hide();
			frameBuffer->blit();
			ret = true;
		}
		delete volscale;
		volscale = NULL;
	}
	return ret;
}

void CVolume::showVolscale()
{
	if (volscale == NULL){
		volscale = new CVolumeBar();
		volscale->paint();
	}
}
