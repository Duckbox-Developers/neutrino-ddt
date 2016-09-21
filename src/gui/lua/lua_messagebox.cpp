/*
 * lua messagebox
 *
 * (C) 2014 by martii
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2014 Sven Hoefer (svenhoefer)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <gui/widget/messagebox.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_messagebox.h"

CLuaInstMessagebox* CLuaInstMessagebox::getInstance()
{
	static CLuaInstMessagebox* LuaInstMessagebox = NULL;

	if(!LuaInstMessagebox)
		LuaInstMessagebox = new CLuaInstMessagebox();
	return LuaInstMessagebox;
}

void CLuaInstMessagebox::MessageboxRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "exec", CLuaInstMessagebox::MessageboxExec },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "messagebox");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "messagebox");
}

// messagebox.exec{caption="Title", text="text", icon="settings", width=500,timeout=-1,return_default_on_timeout=0,
//	default = "yes", buttons = { "yes", "no", "cancel", "all", "back", "ok" }, align="center1|center2|left|right" }
int CLuaInstMessagebox::MessageboxExec(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	std::string name, text, icon = std::string(NEUTRINO_ICON_INFO);
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);
	tableLookup(L, "text", text);
	tableLookup(L, "icon", icon);
	lua_Integer timeout = -1, width = 450, return_default_on_timeout = 0, show_buttons = 0, default_button = 0;
	tableLookup(L, "timeout", timeout);
	tableLookup(L, "width", width);
	tableLookup(L, "return_default_on_timeout", return_default_on_timeout);

	std::string tmp;
	if (tableLookup(L, "align", tmp)) {
		table_key mb[] = {
			{ "center1",	CMessageBox::mbBtnAlignCenter1 },
			{ "center2",	CMessageBox::mbBtnAlignCenter2 },
			{ "left",	CMessageBox::mbBtnAlignLeft },
			{ "right",	CMessageBox::mbBtnAlignRight },
			{ NULL,		0 }
		};
		for (int i = 0; mb[i].name; i++)
			if (!strcmp(mb[i].name, tmp.c_str())) {
				show_buttons |= mb[i].code;
				break;
			}
	}

	lua_pushstring(L, "buttons");
	lua_gettable(L, -2);
	for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 2)) {
		lua_pushvalue(L, -2);
		const char *val = lua_tostring(L, -2);
		table_key mb[] = {
			{ "yes",	CMessageBox::mbYes },
			{ "no",		CMessageBox::mbNo },
			{ "cancel",	CMessageBox::mbCancel },
			{ "all",	CMessageBox::mbAll },
			{ "back",	CMessageBox::mbBack },
			{ "ok",		CMessageBox::mbOk },
			{ NULL,		0 }
		};
		for (int i = 0; mb[i].name; i++)
			if (!strcmp(mb[i].name, val)) {
				show_buttons |= mb[i].code;
				break;
			}
	}
	lua_pop(L, 1);
	if ((show_buttons & 0xFF) == 0)
		show_buttons |= CMessageBox::mbAll;

	table_key mbr[] = {
		{ "yes",	CMessageBox::mbrYes },
		{ "no",		CMessageBox::mbrNo },
		{ "cancel",	CMessageBox::mbrCancel },
		{ "back",	CMessageBox::mbrBack },
		{ "ok",		CMessageBox::mbrOk },
		{ NULL,		0 }
	};
	if (tableLookup(L, "default", tmp)) {
		for (int i = 0; mbr[i].name; i++)
			if (!strcmp(mbr[i].name, tmp.c_str())) {
				default_button = mbr[i].code;
				break;
			}
	}

	int res = ShowMsg(name, text, (CMessageBox::result_) default_button, (CMessageBox::buttons_) show_buttons, icon.empty() ? NULL : icon.c_str(), width, timeout, return_default_on_timeout);

	tmp = "cancel";
	for (int i = 0; mbr[i].name; i++)
		if (res == mbr[i].code) {
			tmp = mbr[i].name;
			break;
		}
	lua_pushstring(L, tmp.c_str());

	return 1;
}
