/*
  Client-Interface für zapit  -   DBoxII-Project

  $Id: sectionsdclient.cpp,v 1.53 2007/01/12 22:57:57 houdini Exp $

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

#include <cstdio>
#include <cstring>
#include <eventserver.h>

#include <sectionsdclient/sectionsdclient.h>
#include <sectionsdclient/sectionsdMsg.h>

#include <system/helpers.h>

#ifdef PEDANTIC_VALGRIND_SETUP
#define VALGRIND_PARANOIA(x) memset(&x, 0, sizeof(x))
#else
#define VALGRIND_PARANOIA(x) {}
#endif

unsigned char   CSectionsdClient::getVersion() const
{
	return sectionsd::ACTVERSION;
}

const          char *CSectionsdClient::getSocketName() const
{
	return SECTIONSD_UDS_NAME;
}

int CSectionsdClient::readResponse(char *data, unsigned int size)
{
	struct sectionsd::msgResponseHeader responseHeader;
	if (!receive_data((char *)&responseHeader, sizeof(responseHeader)))
		return 0;

	if (data != NULL)
	{
		if (responseHeader.dataLength != (unsigned)size)
			return -1;
		else
			return receive_data(data, size);
	}
	else
		return responseHeader.dataLength;
}

bool CSectionsdClient::send(const unsigned char command, const char *data, const unsigned int size)
{
	sectionsd::msgRequestHeader msgHead;
	VALGRIND_PARANOIA(msgHead);

	msgHead.version    = getVersion();
	msgHead.command    = command;
	msgHead.dataLength = size;

	open_connection(); // if the return value is false, the next send_data call will return false, too

	if (!send_data((char *)&msgHead, sizeof(msgHead)))
		return false;

	if (size != 0)
		return send_data(data, size);

	return true;
}

void CSectionsdClient::registerEvent(const unsigned int eventID, const unsigned int clientID, const char *const udsName)
{
	CEventServer::commandRegisterEvent msg2;
	VALGRIND_PARANOIA(msg2);

	msg2.eventID = eventID;
	msg2.clientID = clientID;

	cstrncpy(msg2.udsName, udsName, sizeof(msg2.udsName));

	send(sectionsd::CMD_registerEvents, (char *)&msg2, sizeof(msg2));

	close_connection();
}

void CSectionsdClient::unRegisterEvent(const unsigned int eventID, const unsigned int clientID)
{
	CEventServer::commandUnRegisterEvent msg2;
	VALGRIND_PARANOIA(msg2);

	msg2.eventID = eventID;
	msg2.clientID = clientID;

	send(sectionsd::CMD_unregisterEvents, (char *)&msg2, sizeof(msg2));

	close_connection();
}

bool CSectionsdClient::getIsTimeSet()
{
	sectionsd::responseIsTimeSet rmsg;

	if (send(sectionsd::getIsTimeSet))
	{
		readResponse((char *)&rmsg, sizeof(rmsg));
		close_connection();

		return rmsg.IsTimeSet;
	}
	else
	{
		close_connection();
		return false;
	}
}

void CSectionsdClient::setPauseScanning(const bool doPause)
{
	int PauseIt = (doPause) ? 1 : 0;

	send(sectionsd::pauseScanning, (char *)&PauseIt, sizeof(PauseIt));

	readResponse();
	close_connection();
}

bool CSectionsdClient::getIsScanningActive()
{
	int scanning;

	if (send(sectionsd::getIsScanningActive))
	{
		readResponse((char *)&scanning, sizeof(scanning));
		close_connection();

		return scanning;
	}
	else
	{
		close_connection();
		return false;
	}
}

void CSectionsdClient::setServiceChanged(const t_channel_id channel_id, const bool requestEvent, int dnum)
{
	sectionsd::commandSetServiceChanged msg;
	VALGRIND_PARANOIA(msg);

	msg.channel_id   = channel_id;
	msg.requestEvent = requestEvent;
	msg.dnum = dnum;

	send(sectionsd::serviceChanged, (char *)&msg, sizeof(msg));

	readResponse();
	close_connection();
}

void CSectionsdClient::setServiceStopped()
{
	send(sectionsd::serviceStopped);

	readResponse();
	close_connection();
}

void CSectionsdClient::freeMemory()
{
	send(sectionsd::freeMemory);

	readResponse();
	close_connection();
}

void CSectionsdClient::readSIfromXML(const char *epgxmlname)
{
	send(sectionsd::readSIfromXML, (char *) epgxmlname, strlen(epgxmlname));

	readResponse();
	close_connection();
}

void CSectionsdClient::readSIfromXMLTV(const char *url)
{
	send(sectionsd::readSIfromXMLTV, (char *) url, strlen(url));

	readResponse();
	close_connection();
}

void CSectionsdClient::writeSI2XML(const char *epgxmlname)
{
	send(sectionsd::writeSI2XML, (char *) epgxmlname, strlen(epgxmlname));

	readResponse();
	close_connection();
}

void CSectionsdClient::setConfig(const epg_config config)
{
	sectionsd::commandSetConfig *msg;
	char *pData = new char[sizeof(sectionsd::commandSetConfig) + config.network_ntpserver.length() + 1 + config.epg_dir.length() + 1];
	msg = (sectionsd::commandSetConfig *)pData;

	msg->epg_cache		= config.epg_cache;
	msg->epg_old_events	= config.epg_old_events;
	msg->epg_max_events	= config.epg_max_events;
	msg->network_ntprefresh	= config.network_ntprefresh;
	msg->network_ntpenable	= config.network_ntpenable;
	msg->epg_extendedcache	= config.epg_extendedcache;
	msg->epg_save_frequently = config.epg_save_frequently;
	msg->epg_read_frequently = config.epg_read_frequently;
//	config.network_ntpserver:
	strcpy(&pData[sizeof(sectionsd::commandSetConfig)], config.network_ntpserver.c_str());
//	config.epg_dir:
	strcpy(&pData[sizeof(sectionsd::commandSetConfig) + config.network_ntpserver.length() + 1], config.epg_dir.c_str());

	send(sectionsd::setConfig, (char *)pData, sizeof(sectionsd::commandSetConfig) + config.network_ntpserver.length() + 1 + config.epg_dir.length() + 1);
	readResponse();
	close_connection();
	delete[] pData;
}

void CSectionsdClient::dumpStatus()
{
	send(sectionsd::dumpStatusinformation);
	close_connection();
}
