/*
	(C)2012 by martii

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


#ifndef __3dsetup__
#define __3dsetup__

#define THREE_D_CONFIG_FILE CONFIGDIR "/3dchannels.conf"

#include <gui/widget/menue.h>
#include <driver/framebuffer.h>
#include <system/localize.h>
#include <zapit/client/zapittypes.h>
#include <string>
#include <map>


#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#define THREE_D_OPTIONS_COUNT 3
#else
#define THREE_D_OPTIONS_COUNT 4
#endif

class C3DSetup:public CMenuTarget
{
public:
	struct threeDList
	{
		CMenuForwarder *cmf;
		const std::string actionKey;
		CFrameBuffer::Mode3D mode;
	};
private:
	CFrameBuffer * frameBuffer;
	int x;
	int y;
	int dx;
	int dy;
	int selected;
	int width;
	int show3DSetup();
	std::map<t_channel_id, CFrameBuffer::Mode3D> threeDMap;
	void load();
	void save();
	C3DSetup ();
	~C3DSetup ();
public:
	int exec (CMenuTarget * parent, const std::string & actionKey);
	static C3DSetup *getInstance();
};

#endif
