/*

 Copyright (c) 2004 gmo18t, Germany. All rights reserved.
 Copyright (C) 2012 CoolStream International Ltd

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published
 by the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software Foundation,
 Inc., 675 Mass Ave, Cambridge MA 02139, USA.

 Mit diesem Programm koennen Neutrino TS Streams f�r das Abspielen unter Enigma gepatched werden 
 */
#ifndef __genpsi_h__
#define __genpsi_h__
#include <inttypes.h>

#define EN_TYPE_VIDEO		0x00
#define EN_TYPE_AUDIO		0x01
#define EN_TYPE_TELTEX		0x02
#define EN_TYPE_PCR		0x03
#define EN_TYPE_AVC		0x04
#define EN_TYPE_DVBSUB		0x06
#define EN_TYPE_AUDIO_EAC3	0x07
#define EN_TYPE_HEVC		0x08

class CGenPsi
{
	private:
		static const unsigned int pmt_pid = 0xcc;
		short  nba, nsub, neac3;
		uint16_t       vpid;
		uint8_t        vtype;
		uint16_t       pcrpid;
		uint16_t       vtxtpid;
		char           vtxtlang[3];
		uint16_t       apid[10];
		short          atypes[10];
		char           apid_lang[10][3];
		uint16_t       dvbsubpid[10];
		char           dvbsublang[10][3];
		uint16_t       eac3_pid[10];
		char           eac3_lang[10][3];
		uint32_t calc_crc32psi(uint8_t *dst, const uint8_t *src, uint32_t len);
		void build_pat(uint8_t* buffer);
		void build_pmt(uint8_t* buffer);

	public:
		CGenPsi();
		void addPid(uint16_t pid,uint16_t pidtype, short isAC3, const char *data = NULL);
		int genpsi(int fd);
};
#endif
