/*
        Copyright (C) 2013 CoolStream International Ltd

        License: GPLv2

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation;

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

#include <system/helpers.h>
#include <system/setting_helpers.h>
#include <daemonc/remotecontrol.h>

#include <zapit/zapit.h>
#include <zapit/debug.h>
#include <eitd/sectionsd.h>

#include <gui/bouquetlist.h>
#include <gui/channellist.h>

#include <driver/scanepg.h>
#include <driver/record.h>
#include <driver/streamts.h>

#define EPG_RESCAN_TIME (24*60*60)

extern CBouquetList * bouquetList;
extern CBouquetList * TVfavList;
extern CBouquetList * TVbouquetList;
 
CEpgScan::CEpgScan()
{
	current_mode = 0;
	standby = false;
	rescan_timer = 0;
	scan_in_progress = false;
	Clear();
}

CEpgScan::~CEpgScan()
{
}

CEpgScan * CEpgScan::getInstance()
{
	static CEpgScan * inst;
	if (inst == NULL)
		inst = new CEpgScan();
	return inst;
}

void CEpgScan::Clear()
{
	scanmap.clear();
	current_bnum = -1;
	current_bmode = -1;
	next_chid = 0;
	allfav_done = false;
	selected_done = false;
}

bool CEpgScan::Running()
{
	return (CheckMode() && !scanmap.empty());
}

void CEpgScan::AddBouquet(CChannelList * clist)
{
	for (unsigned i = 0; i < clist->Size(); i++) {
		CZapitChannel * chan = clist->getChannelFromIndex(i);
		if (!IS_WEBTV(chan->getChannelID()) && scanned.find(chan->getTransponderId()) == scanned.end())
			scanmap.insert(eit_scanmap_pair_t(chan->getTransponderId(), chan->getChannelID()));
	}
}

bool CEpgScan::AddFavorites()
{
	INFO("allfav_done: %d", allfav_done);
	if ((g_settings.epg_scan != SCAN_FAV) || allfav_done)
		return false;

	allfav_done = true;
	unsigned old_size = scanmap.size();
	for (unsigned j = 0; j < TVfavList->Bouquets.size(); ++j) {
		CChannelList * clist = TVfavList->Bouquets[j]->channelList;
		AddBouquet(clist);
	}
	INFO("scan map size: %d -> %d\n", old_size, scanmap.size());
	return (old_size != scanmap.size());
}

bool CEpgScan::AddSelected()
{
	INFO("selected_done: %d", selected_done);
	if ((g_settings.epg_scan != SCAN_SEL) || selected_done)
		return false;

	selected_done = true;
	unsigned old_size = scanmap.size();
	for (unsigned j = 0; j < TVfavList->Bouquets.size(); ++j) {
		if (TVfavList->Bouquets[j]->zapitBouquet && TVfavList->Bouquets[j]->zapitBouquet->bScanEpg) {
			CChannelList * clist = TVfavList->Bouquets[j]->channelList;
			AddBouquet(clist);
		}
	}
	for (unsigned j = 0; j < TVbouquetList->Bouquets.size(); ++j) {
		if (TVbouquetList->Bouquets[j]->zapitBouquet && TVbouquetList->Bouquets[j]->zapitBouquet->bScanEpg) {
			CChannelList * clist = TVbouquetList->Bouquets[j]->channelList;
			AddBouquet(clist);
		}
	}
	INFO("scan map size: %d -> %d\n", old_size, scanmap.size());
	return (old_size != scanmap.size());
}

void CEpgScan::AddTransponders()
{
	if(bouquetList->Bouquets.empty())
		return;

	if (current_mode != g_settings.epg_scan) {
		current_mode = g_settings.epg_scan;
		Clear();
	}

	int mode = CNeutrinoApp::getInstance()->GetChannelMode();
	if (g_settings.epg_scan == SCAN_SEL) {
		if (current_bmode != mode) {
			current_bmode = mode;
			current_bnum = -1;
		}
		int bnum = bouquetList->getActiveBouquetNumber();
		bool bscan = bouquetList->Bouquets[bnum]->zapitBouquet &&
			bouquetList->Bouquets[bnum]->zapitBouquet->bScanEpg;

		if ((current_bnum != bnum) && bscan) {
			current_bnum = bnum;
			AddBouquet(bouquetList->Bouquets[current_bnum]->channelList);
		} else {
			AddSelected();
		}
		return;
	}

	if ((g_settings.epg_scan == SCAN_CURRENT) || (mode == LIST_MODE_FAV)) {
		/* current bouquet mode */
		if (current_bmode != mode) {
			current_bmode = mode;
			current_bnum = -1;
		}
		if (current_bnum != bouquetList->getActiveBouquetNumber()) {
			allfav_done = false;
			scanmap.clear();
			current_bnum = bouquetList->getActiveBouquetNumber();
			AddBouquet(bouquetList->Bouquets[current_bnum]->channelList);
			INFO("Added bouquet #%d, scan map size: %d", current_bnum, scanmap.size());
		}
	} else if (g_settings.epg_scan == SCAN_FAV) {
		AddFavorites();
	}
}

bool CEpgScan::CheckMode()
{
	bool webtv = IS_WEBTV(CZapit::getInstance()->GetCurrentChannelID());
	if ((g_settings.epg_scan_mode == CEpgScan::MODE_OFF)
			|| (standby && !(g_settings.epg_scan_mode & MODE_STANDBY))
			|| (!standby && !(g_settings.epg_scan_mode & MODE_LIVE))
			|| (!standby && !webtv && (CFEManager::getInstance()->getEnabledCount() <= 1))) {
		return false;
	}
	return true;
			
}

void CEpgScan::Start(bool instandby)
{
	standby = instandby;
	live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
	AddTransponders();
	INFO("starting %s scan, scanning %d, scan map size: %d", standby ? "standby" : "live", scan_in_progress, scanmap.size());
	if (standby || !scan_in_progress)
		Next();
}

void CEpgScan::Stop()
{
	if (g_settings.epg_scan_mode == CEpgScan::MODE_OFF)
		return;

	INFO("stopping %s scan...", standby ? "standby" : "live");
	if (standby) {
		standby = false;
		CZapit::getInstance()->SetCurrentChannelID(live_channel_id);
	}
}

int CEpgScan::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if ((msg == NeutrinoMessages::EVT_TIMER) && (data == rescan_timer)) {
		INFO("rescan timer in %s mode, scanning %d", standby ? "standby" : "live", scan_in_progress);
		scanned.clear();
		Clear();
		g_RCInput->killTimer(rescan_timer);
		if (CheckMode()) {
			if (standby)
				CNeutrinoApp::getInstance()->wakeupFromStandby();
			Start(standby);
		} else {
			AddTimer();
		}
		return messages_return::handled;
	}
	if (!CheckMode()) {
		int ret = messages_return::handled;
		if (msg == NeutrinoMessages::EVT_EIT_COMPLETE)
			scan_in_progress = false;
		else if (msg == NeutrinoMessages::EVT_BACK_ZAP_COMPLETE)
			scan_in_progress = true;
		else
			ret = messages_return::unhandled;
		return ret;
	}

	CZapitChannel * newchan;
	if (msg == NeutrinoMessages::EVT_ZAP_COMPLETE) {
		/* live channel changed, block scan channel change by timer */
		scan_in_progress = true;
		AddTransponders();
		INFO("EVT_ZAP_COMPLETE, scan map size: %d\n", scanmap.size());
#if 0
		t_channel_id chid = *(t_channel_id *)data;
		if (IS_WEBTV(chid))
			Next();
#endif
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_EIT_COMPLETE) {
		scan_in_progress = false;
		t_channel_id chid = *(t_channel_id *)data;
		newchan = CServiceManager::getInstance()->FindChannel(chid);
		if (newchan) {
			scanned.insert(newchan->getTransponderId());
			scanmap.erase(newchan->getTransponderId());
		}
		INFO("EIT read complete [" PRINTF_CHANNEL_ID_TYPE "], scan map size: %d", chid, scanmap.size());

		Next();
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_BACK_ZAP_COMPLETE) {
		scan_in_progress = true;
		t_channel_id chid = *(t_channel_id *)data;
		INFO("EVT_BACK_ZAP_COMPLETE [" PRINTF_CHANNEL_ID_TYPE "]", chid);
		if (next_chid) {
			newchan = CServiceManager::getInstance()->FindChannel(next_chid);
			if (newchan) {
				if(chid) {
					if (!CRecordManager::getInstance()->RecordingStatus()) {
						INFO("try to scan [%s]", newchan->getName().c_str());
						if (standby && !g_Sectionsd->getIsScanningActive())
							g_Sectionsd->setPauseScanning(false);
						g_Sectionsd->setServiceChanged(newchan->getChannelID(), false, newchan->getRecordDemux());
					}
				} else {
					INFO("tune failed [%s]", newchan->getName().c_str());
					scanmap.erase(newchan->getTransponderId());
					Next();
				}
			}
		}
		return messages_return::handled;
	}
	return messages_return::unhandled;
}

void CEpgScan::AddTimer()
{
	if (rescan_timer == 0)
		rescan_timer = g_RCInput->addTimer(EPG_RESCAN_TIME*1000ULL*1000ULL, true);
	INFO("rescan timer id %d", rescan_timer);
}

void CEpgScan::EnterStandby()
{
	AddTimer();
	if (standby) {
		CZapit::getInstance()->SetCurrentChannelID(live_channel_id);
		CNeutrinoApp::getInstance()->standbyToStandby();
	}
}

void CEpgScan::Next()
{
	bool llocked = false, plocked = false;

	next_chid = 0;

	if (!standby && CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_standby)
		return;
	if (CRecordManager::getInstance()->RecordingStatus() || CStreamManager::getInstance()->StreamStatus())
		return;

	if (g_settings.epg_scan == SCAN_FAV && scanmap.empty())
		AddFavorites();
	if (g_settings.epg_scan == SCAN_SEL && scanmap.empty())
		AddSelected();

	if (!CheckMode() || scanmap.empty()) {
		EnterStandby();
		return;
	}

	/* executed in neutrino thread - possible race with locks in zapit zap NOWAIT :
	   send zapTo_NOWAIT -> EIT_COMPLETE from sectionsd -> zap and this at the same time
	*/
	CFEManager::getInstance()->Lock();
	CFrontend *live_fe = NULL;
#ifdef ENABLE_PIP
	CFrontend *pip_fe = NULL;
#endif
	if (!standby) {
		bool webtv = IS_WEBTV(CZapit::getInstance()->GetCurrentChannelID());
		if (!webtv) {
			llocked = true;
			live_fe = CZapit::getInstance()->GetLiveFrontend();
			CFEManager::getInstance()->lockFrontend(live_fe);
		}
#ifdef ENABLE_PIP
		pip_fe = CZapit::getInstance()->GetPipFrontend();
		if (pip_fe /* && pip_fe != live_fe*/) {
			plocked = true;
			CFEManager::getInstance()->lockFrontend(pip_fe);
		}
#endif
	}
_repeat:
	for (eit_scanmap_iterator_t it = scanmap.begin(); it != scanmap.end(); /* ++it*/) {
		CZapitChannel * newchan = CServiceManager::getInstance()->FindChannel(it->second);
		if (newchan == NULL) {
			scanmap.erase(it++);
			continue;
		}
		if (CFEManager::getInstance()->canTune(newchan)) {
			INFO("try to tune [%s]", newchan->getName().c_str());
			next_chid = newchan->getChannelID();
			break;
		} else
			INFO("skip [%s], cannot tune", newchan->getName().c_str());
		++it;
	}
	if (!next_chid && ((g_settings.epg_scan == SCAN_FAV) && AddFavorites()))
		goto _repeat;
	if (!next_chid && ((g_settings.epg_scan == SCAN_SEL) && AddSelected()))
		goto _repeat;

	if (llocked)
		CFEManager::getInstance()->unlockFrontend(live_fe);
#ifdef ENABLE_PIP
	if (plocked)
		CFEManager::getInstance()->unlockFrontend(pip_fe);
#endif

	CFEManager::getInstance()->Unlock();
	if (next_chid)
		g_Zapit->zapTo_epg(next_chid, standby);
	else 
		EnterStandby();
}
