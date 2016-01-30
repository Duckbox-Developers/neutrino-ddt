/*
 * lua components header
 *
 * (C) 2016 M. Liebmann (micha-bbg)
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

#ifndef _LUACCHEADER_H
#define _LUACCHEADER_H

class CLuaCCHeader
{
	public:
		CComponentsHeader *ch;
		CComponentsForm *parent;
		CLuaCCHeader() { ch = NULL; parent = NULL; }
		~CLuaCCHeader() { if (parent == NULL) delete ch; }
};

class CLuaInstCCHeader
{
	public:
		CLuaInstCCHeader() {};
		~CLuaInstCCHeader() {};
		static CLuaInstCCHeader* getInstance();
		static void CCHeaderRegister(lua_State *L);

	private:
		static CLuaCCHeader *CCHeaderCheck(lua_State *L, int n);
		static int CCHeaderNew(lua_State *L);
		static int CCHeaderPaint(lua_State *L);
		static int CCHeaderHide(lua_State *L);
		static int CCHeaderDelete(lua_State *L);

};

#endif //_LUACCHEADER_H
