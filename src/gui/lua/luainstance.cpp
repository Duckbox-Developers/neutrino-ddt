/*
 * neutrino-mp lua to c++ bridge
 *
 * (C) 2013 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>
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
#include <system/helpers.h>
#include <system/settings.h>
#include <system/set_threadname.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/messagebox.h>
#include <gui/movieplayer.h>
#include <driver/neutrinofonts.h>
#include <driver/pictureviewer/pictureviewer.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_cc_picture.h"
#include "lua_cc_signalbox.h"
#include "lua_cc_text.h"
#include "lua_cc_window.h"
#include "lua_configfile.h"
#include "lua_curl.h"
#include "lua_hintbox.h"
#include "lua_menue.h"
#include "lua_messagebox.h"
#include "lua_misc.h"
#include "lua_video.h"

extern CPictureViewer * g_PicViewer;

static void set_lua_variables(lua_State *L)
{
	/* keyname table created with
	 * sed -n '/^[[:space:]]*RC_0/,/^[[:space:]]*RC_analog_off/ {
	 * 	s#^[[:space:]]*RC_\([^[:space:]]*\).*#	{ "\1",	CRCInput::RC_\1 },#p
	 * }' driver/rcinput.h
	 */
	static table_key keyname[] =
	{
		{ "0",			CRCInput::RC_0 },
		{ "1",			CRCInput::RC_1 },
		{ "2",			CRCInput::RC_2 },
		{ "3",			CRCInput::RC_3 },
		{ "4",			CRCInput::RC_4 },
		{ "5",			CRCInput::RC_5 },
		{ "6",			CRCInput::RC_6 },
		{ "7",			CRCInput::RC_7 },
		{ "8",			CRCInput::RC_8 },
		{ "9",			CRCInput::RC_9 },
		{ "backspace",		CRCInput::RC_backspace },
		{ "up",			CRCInput::RC_up },
		{ "left",		CRCInput::RC_left },
		{ "right",		CRCInput::RC_right },
		{ "down",		CRCInput::RC_down },
		{ "spkr",		CRCInput::RC_spkr },
		{ "minus",		CRCInput::RC_minus },
		{ "plus",		CRCInput::RC_plus },
		{ "standby",		CRCInput::RC_standby },
		{ "help",		CRCInput::RC_help },
		{ "home",		CRCInput::RC_home },
		{ "setup",		CRCInput::RC_setup },
		{ "topleft",		CRCInput::RC_topleft },
		{ "topright",		CRCInput::RC_topright },
		{ "page_up",		CRCInput::RC_page_up },
		{ "page_down",		CRCInput::RC_page_down },
		{ "ok",			CRCInput::RC_ok },
		{ "red",		CRCInput::RC_red },
		{ "green",		CRCInput::RC_green },
		{ "yellow",		CRCInput::RC_yellow },
		{ "blue",		CRCInput::RC_blue },
		{ "top_left",		CRCInput::RC_top_left },
		{ "top_right",		CRCInput::RC_top_right },
		{ "bottom_left",	CRCInput::RC_bottom_left },
		{ "bottom_right",	CRCInput::RC_bottom_right },
		{ "audio",		CRCInput::RC_audio },
		{ "video",		CRCInput::RC_video },
		{ "tv",			CRCInput::RC_tv },
		{ "radio",		CRCInput::RC_radio },
		{ "text",		CRCInput::RC_text },
		{ "info",		CRCInput::RC_info },
		{ "epg",		CRCInput::RC_epg },
		{ "recall",		CRCInput::RC_recall },
		{ "favorites",		CRCInput::RC_favorites },
		{ "sat",		CRCInput::RC_sat },
		{ "sat2",		CRCInput::RC_sat2 },
		{ "record",		CRCInput::RC_record },
		{ "play",		CRCInput::RC_play },
		{ "pause",		CRCInput::RC_pause },
		{ "forward",		CRCInput::RC_forward },
		{ "rewind",		CRCInput::RC_rewind },
		{ "stop",		CRCInput::RC_stop },
		{ "timeshift",		CRCInput::RC_timeshift },
		{ "mode",		CRCInput::RC_mode },
		{ "games",		CRCInput::RC_games },
		{ "next",		CRCInput::RC_next },
		{ "prev",		CRCInput::RC_prev },
		{ "www",		CRCInput::RC_www },
		{ "power_on",		CRCInput::RC_power_on },
		{ "power_off",		CRCInput::RC_power_off },
		{ "standby_on",		CRCInput::RC_standby_on },
		{ "standby_off",	CRCInput::RC_standby_off },
		{ "mute_on",		CRCInput::RC_mute_on },
		{ "mute_off",		CRCInput::RC_mute_off },
		{ "analog_on",		CRCInput::RC_analog_on },
		{ "analog_off",		CRCInput::RC_analog_off },
#if 0
		{ "find",		CRCInput::RC_find },
		{ "pip",		CRCInput::RC_pip },
		{ "folder",		CRCInput::RC_archive },
		{ "forward",		CRCInput::RC_fastforward },
		{ "slow",		CRCInput::RC_slow },
		{ "playmode",		CRCInput::RC_playmode },
		{ "usb",		CRCInput::RC_usb },
		{ "f1",			CRCInput::RC_f1 },
		{ "f2",			CRCInput::RC_f2 },
		{ "f3",			CRCInput::RC_f3 },
		{ "f4",			CRCInput::RC_f4 },
		{ "prog1",		CRCInput::RC_prog1 },
		{ "prog2",		CRCInput::RC_prog2 },
		{ "prog3",		CRCInput::RC_prog3 },
		{ "prog4",		CRCInput::RC_prog4 },
#endif
		/* to check if it is in our range */
		{ "MaxRC",		CRCInput::RC_MaxRC },
		{ NULL, 0 }
	};

	/* list of colors, exported e.g. as COL['INFOBAR_SHADOW'] */
	static table_key_u colorlist[] =
	{
		{ "COLORED_EVENTS_CHANNELLIST",	MAGIC_COLOR | (COL_COLORED_EVENTS_CHANNELLIST) },
		{ "COLORED_EVENTS_INFOBAR",	MAGIC_COLOR | (COL_COLORED_EVENTS_INFOBAR) },
		{ "INFOBAR_SHADOW",		MAGIC_COLOR | (COL_INFOBAR_SHADOW) },
		{ "INFOBAR",			MAGIC_COLOR | (COL_INFOBAR) },
		{ "MENUHEAD",			MAGIC_COLOR | (COL_MENUHEAD) },
		{ "MENUCONTENT",		MAGIC_COLOR | (COL_MENUCONTENT) },
		{ "MENUCONTENTDARK",		MAGIC_COLOR | (COL_MENUCONTENTDARK) },
		{ "MENUCONTENTSELECTED",	MAGIC_COLOR | (COL_MENUCONTENTSELECTED) },
		{ "MENUCONTENTINACTIVE",	MAGIC_COLOR | (COL_MENUCONTENTINACTIVE) },
		{ "BACKGROUND",			MAGIC_COLOR | (COL_BACKGROUND) },
		{ "DARK_RED",			MAGIC_COLOR | (COL_DARK_RED0) },
		{ "DARK_GREEN",			MAGIC_COLOR | (COL_DARK_GREEN0) },
		{ "DARK_BLUE",			MAGIC_COLOR | (COL_DARK_BLUE0) },
		{ "LIGHT_GRAY",			MAGIC_COLOR | (COL_LIGHT_GRAY0) },
		{ "DARK_GRAY",			MAGIC_COLOR | (COL_DARK_GRAY0) },
		{ "RED",			MAGIC_COLOR | (COL_RED0) },
		{ "GREEN",			MAGIC_COLOR | (COL_GREEN0) },
		{ "YELLOW",			MAGIC_COLOR | (COL_YELLOW0) },
		{ "BLUE",			MAGIC_COLOR | (COL_BLUE0) },
		{ "PURP",			MAGIC_COLOR | (COL_PURP0) },
		{ "LIGHT_BLUE",			MAGIC_COLOR | (COL_LIGHT_BLUE0) },
		{ "WHITE",			MAGIC_COLOR | (COL_WHITE0) },
		{ "BLACK",			MAGIC_COLOR | (COL_BLACK0) },
		{ "COLORED_EVENTS_TEXT",	(lua_Unsigned) (COL_COLORED_EVENTS_TEXT) },
		{ "INFOBAR_TEXT",		(lua_Unsigned) (COL_INFOBAR_TEXT) },
		{ "INFOBAR_SHADOW_TEXT",	(lua_Unsigned) (COL_INFOBAR_SHADOW_TEXT) },
		{ "MENUHEAD_TEXT",		(lua_Unsigned) (COL_MENUHEAD_TEXT) },
		{ "MENUCONTENT_TEXT",		(lua_Unsigned) (COL_MENUCONTENT_TEXT) },
		{ "MENUCONTENT_TEXT_PLUS_1",	(lua_Unsigned) (COL_MENUCONTENT_TEXT_PLUS_1) },
		{ "MENUCONTENT_TEXT_PLUS_2",	(lua_Unsigned) (COL_MENUCONTENT_TEXT_PLUS_2) },
		{ "MENUCONTENT_TEXT_PLUS_3",	(lua_Unsigned) (COL_MENUCONTENT_TEXT_PLUS_3) },
		{ "MENUCONTENTDARK_TEXT",	(lua_Unsigned) (COL_MENUCONTENTDARK_TEXT) },
		{ "MENUCONTENTDARK_TEXT_PLUS_1",	(lua_Unsigned) (COL_MENUCONTENTDARK_TEXT_PLUS_1) },
		{ "MENUCONTENTDARK_TEXT_PLUS_2",	(lua_Unsigned) (COL_MENUCONTENTDARK_TEXT_PLUS_2) },
		{ "MENUCONTENTSELECTED_TEXT",		(lua_Unsigned) (COL_MENUCONTENTSELECTED_TEXT) },
		{ "MENUCONTENTSELECTED_TEXT_PLUS_1",	(lua_Unsigned) (COL_MENUCONTENTSELECTED_TEXT_PLUS_1) },
		{ "MENUCONTENTSELECTED_TEXT_PLUS_2",	(lua_Unsigned) (COL_MENUCONTENTSELECTED_TEXT_PLUS_2) },
		{ "MENUCONTENTINACTIVE_TEXT",		(lua_Unsigned) (COL_MENUCONTENTINACTIVE_TEXT) },
		{ "MENUHEAD_PLUS_0",			(lua_Unsigned) (COL_MENUHEAD_PLUS_0) },
		{ "MENUCONTENT_PLUS_0",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_0) },
		{ "MENUCONTENT_PLUS_1",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_1) },
		{ "MENUCONTENT_PLUS_2",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_2) },
		{ "MENUCONTENT_PLUS_3",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_3) },
		{ "MENUCONTENT_PLUS_4",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_4) },
		{ "MENUCONTENT_PLUS_5",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_5) },
		{ "MENUCONTENT_PLUS_6",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_6) },
		{ "MENUCONTENT_PLUS_7",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_7) },
		{ "MENUCONTENTDARK_PLUS_0",		(lua_Unsigned) (COL_MENUCONTENTDARK_PLUS_0) },
		{ "MENUCONTENTDARK_PLUS_2",		(lua_Unsigned) (COL_MENUCONTENTDARK_PLUS_2) },
		{ "MENUCONTENTSELECTED_PLUS_0",		(lua_Unsigned) (COL_MENUCONTENTSELECTED_PLUS_0) },
		{ "MENUCONTENTSELECTED_PLUS_2",		(lua_Unsigned) (COL_MENUCONTENTSELECTED_PLUS_2) },
		{ "MENUCONTENTINACTIVE_PLUS_0",		(lua_Unsigned) (COL_MENUCONTENTINACTIVE_PLUS_0) },
		{ NULL, 0 }
	};

	/* font list, the _TYPE_ is redundant, exported e.g. as FONT['MENU'] */
	static table_key fontlist[] =
	{
		{ "MENU",		SNeutrinoSettings::FONT_TYPE_MENU },
		{ "MENU_TITLE",		SNeutrinoSettings::FONT_TYPE_MENU_TITLE },
		{ "MENU_INFO",		SNeutrinoSettings::FONT_TYPE_MENU_INFO },
		{ "EPG_TITLE",		SNeutrinoSettings::FONT_TYPE_EPG_TITLE },
		{ "EPG_INFO1",		SNeutrinoSettings::FONT_TYPE_EPG_INFO1 },
		{ "EPG_INFO2",		SNeutrinoSettings::FONT_TYPE_EPG_INFO2 },
		{ "EPG_DATE",		SNeutrinoSettings::FONT_TYPE_EPG_DATE },
		{ "EVENTLIST_TITLE",	SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE },
		{ "EVENTLIST_ITEMLARGE",SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE },
		{ "EVENTLIST_ITEMSMALL",SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL },
		{ "EVENTLIST_DATETIME",	SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME },
		{ "CHANNELLIST",	SNeutrinoSettings::FONT_TYPE_CHANNELLIST },
		{ "CHANNELLIST_DESCR",	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR },
		{ "CHANNELLIST_NUMBER",	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER },
		{ "CHANNELLIST_EVENT",	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_EVENT },
		{ "CHANNEL_NUM_ZAP",	SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP },
		{ "INFOBAR_NUMBER",	SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER },
		{ "INFOBAR_CHANNAME",	SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME },
		{ "INFOBAR_INFO",	SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO },
		{ "INFOBAR_SMALL",	SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL },
		{ "FILEBROWSER_ITEM",	SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM },
		{ "MENU_HINT",		SNeutrinoSettings::FONT_TYPE_MENU_HINT },
		{ NULL, 0 }
	};

	table_key corners[] =
	{
		{ "TOP_LEFT",		CORNER_TOP_LEFT },
		{ "TOP_RIGHT",		CORNER_TOP_RIGHT },
		{ "BOTTOM_LEFT",	CORNER_BOTTOM_LEFT },
		{ "BOTTOM_RIGHT",	CORNER_BOTTOM_RIGHT },
		{ "RADIUS_LARGE",	RADIUS_LARGE },	/* those depend on g_settings.rounded_corners */
		{ "RADIUS_MID",		RADIUS_MID },
		{ "RADIUS_SMALL",	RADIUS_SMALL },
		{ "RADIUS_MIN",		RADIUS_MIN },
		{ NULL, 0 }
	};

	/* screen offsets, exported as e.g. SCREEN['END_Y'] */
	lua_Integer xRes = (lua_Integer)CFrameBuffer::getInstance()->getScreenWidth(true);
	lua_Integer yRes = (lua_Integer)CFrameBuffer::getInstance()->getScreenHeight(true);
	table_key screenopts[] =
	{
		{ "OFF_X", g_settings.screen_StartX },
		{ "OFF_Y", g_settings.screen_StartY },
		{ "END_X", g_settings.screen_EndX },
		{ "END_Y", g_settings.screen_EndY },
		{ "X_RES", xRes },
		{ "Y_RES", yRes },
		{ NULL, 0 }
	};
	table_key menureturn[] =
	{
		{ "NONE", menu_return::RETURN_NONE },
		{ "REPAINT", menu_return::RETURN_REPAINT },
		{ "EXIT", menu_return::RETURN_EXIT },
		{ "EXIT_ALL", menu_return::RETURN_EXIT_ALL },
		{ "EXIT_REPAINT", menu_return::RETURN_EXIT_REPAINT },
		{ NULL, 0 }
	};
	table_key apiversion[] =
	{
		{ "MAJOR", LUA_API_VERSION_MAJOR },
		{ "MINOR", LUA_API_VERSION_MINOR },
		{ NULL, 0 }
	};

	table_key playstate[] =
	{
		{ "NORMAL",    CMoviePlayerGui::PLUGIN_PLAYSTATE_NORMAL },
		{ "STOP",      CMoviePlayerGui::PLUGIN_PLAYSTATE_STOP },
		{ "NEXT",      CMoviePlayerGui::PLUGIN_PLAYSTATE_NEXT },
		{ "PREV",      CMoviePlayerGui::PLUGIN_PLAYSTATE_PREV },
		{ "LEAVE_ALL", CMoviePlayerGui::PLUGIN_PLAYSTATE_LEAVE_ALL },
		{ NULL, 0 }
	};

	table_key ccomponents[] =
	{
		{ "SHADOW_OFF",		(lua_Integer)CC_SHADOW_OFF },
		{ "SHADOW_ON",		(lua_Integer)CC_SHADOW_ON },
/*		{ "SHADOW_RIGHT",	(lua_Integer)CC_SHADOW_RIGHT },*/
/*		{ "SHADOW_BOTTOM",	(lua_Integer)CC_SHADOW_BOTTOM },*/
		{ "SAVE_SCREEN_YES",	(lua_Integer)CC_SAVE_SCREEN_YES },
		{ "SAVE_SCREEN_NO",	(lua_Integer)CC_SAVE_SCREEN_NO },
		{ NULL, 0 }
	};

	table_key dynfont[] =
	{
		{ "STYLE_REGULAR",	(lua_Integer)CNeutrinoFonts::FONT_STYLE_REGULAR },
		{ "STYLE_BOLD",		(lua_Integer)CNeutrinoFonts::FONT_STYLE_BOLD },
		{ "STYLE_ITALIC",	(lua_Integer)CNeutrinoFonts::FONT_STYLE_ITALIC },
		{ "MAX",		(lua_Integer)CNeutrinoFonts::DYNFONTEXT_MAX },
		{ "MAXIMUM_FONTS",	(lua_Integer)CLuaInstance::DYNFONT_MAXIMUM_FONTS },
		{ "TO_WIDE",		(lua_Integer)CLuaInstance::DYNFONT_TO_WIDE },
		{ "TOO_HIGH",		(lua_Integer)CLuaInstance::DYNFONT_TOO_HIGH },
		{ NULL, 0 }
	};

	table_key curl_status[] =
	{
		{ "OK",			(lua_Integer)CLuaInstCurl::LUA_CURL_OK },
		{ "ERR_HANDLE",		(lua_Integer)CLuaInstCurl::LUA_CURL_ERR_HANDLE },
		{ "ERR_NO_URL",		(lua_Integer)CLuaInstCurl::LUA_CURL_ERR_NO_URL },
		{ "ERR_CREATE_FILE",	(lua_Integer)CLuaInstCurl::LUA_CURL_ERR_CREATE_FILE },
		{ "ERR_CURL",		(lua_Integer)CLuaInstCurl::LUA_CURL_ERR_CURL },
		{ NULL, 0 }
	};

	table_key neutrino_mode[] =
	{
		{ "UNKNOWN",		(lua_Integer)CNeutrinoApp::mode_unknown },
		{ "TV",			(lua_Integer)CNeutrinoApp::mode_tv },
		{ "RADIO",		(lua_Integer)CNeutrinoApp::mode_radio },
		{ "SCART",		(lua_Integer)CNeutrinoApp::mode_scart },
		{ "STANDBY",		(lua_Integer)CNeutrinoApp::mode_standby },
		{ "AUDIO",		(lua_Integer)CNeutrinoApp::mode_audio },
		{ "PIC",		(lua_Integer)CNeutrinoApp::mode_pic },
		{ "TS",			(lua_Integer)CNeutrinoApp::mode_ts },
		{ "OFF",		(lua_Integer)CNeutrinoApp::mode_off },
		{ "WEBTV",		(lua_Integer)CNeutrinoApp::mode_webtv },
		{ "MASK",		(lua_Integer)CNeutrinoApp::mode_mask },
		{ "NOREZAP",		(lua_Integer)CNeutrinoApp::norezap },
		{ NULL, 0 }
	};

	table_key post_msg[] =
	{
		{ "STANDBY_ON",		(lua_Integer)CLuaInstMisc::POSTMSG_STANDBY_ON },
		{ NULL, 0 }
	};

	/* list of environment variable arrays to be exported */
	lua_envexport e[] =
	{
		{ "RC",		keyname },
		{ "SCREEN",	screenopts },
		{ "FONT",	fontlist },
		{ "CORNER",	corners },
		{ "MENU_RETURN", menureturn },
		{ "APIVERSION",  apiversion },
		{ "PLAYSTATE",   playstate },
		{ "CC",          ccomponents },
		{ "DYNFONT",     dynfont },
		{ "CURL",        curl_status },
		{ "NMODE",       neutrino_mode },
		{ "POSTMSG",     post_msg },
		{ NULL, NULL }
	};

	int i = 0;
	while (e[i].name) {
		int j = 0;
		lua_newtable(L);
		while (e[i].t[j].name) {
			lua_pushstring(L, e[i].t[j].name);
			lua_pushinteger(L, e[i].t[j].code);
			lua_settable(L, -3);
			j++;
		}
		lua_setglobal(L, e[i].name);
		i++;
	}

	lua_envexport_u e_u[] =
	{
		{ "COL",	colorlist },
		{ NULL, NULL }
	};

	i = 0;
	while (e_u[i].name) {
		int j = 0;
		lua_newtable(L);
		while (e_u[i].t[j].name) {
			lua_pushstring(L, e_u[i].t[j].name);
			lua_pushunsigned(L, e_u[i].t[j].code);
			lua_settable(L, -3);
			j++;
		}
		lua_setglobal(L, e_u[i].name);
		i++;
	}
}

const char CLuaInstance::className[] = LUA_CLASSNAME;

CLuaInstance::CLuaInstance()
{
	/* Create the intepreter object.  */
	lua = luaL_newstate();

	/* register standard + custom functions. */
	registerFunctions();
}

CLuaInstance::~CLuaInstance()
{
	if (lua != NULL)
	{
		lua_close(lua);
		lua = NULL;
	}
}

#define SET_VAR1(NAME) \
	lua_pushinteger(lua, NAME); \
	lua_setglobal(lua, #NAME);
#define SET_VAR2(NAME, VALUE) \
	lua_pushinteger(lua, VALUE); \
	lua_setglobal(lua, #NAME);

/* Run the given script. */
void CLuaInstance::runScript(const char *fileName, std::vector<std::string> *argv, std::string *result_code, std::string *result_string, std::string *error_string)
{
	// luaL_dofile(lua, fileName);
	/* run the script */
	int status = luaL_loadfile(lua, fileName);
	if (status) {
		fprintf(stderr, "[CLuaInstance::%s] Can't load file: %s\n", __func__, lua_tostring(lua, -1));
		DisplayErrorMessage(lua_tostring(lua, -1), "Lua Script Error:");
		if (error_string)
			*error_string = std::string(lua_tostring(lua, -1));
		return;
	}
	int argvSize = 1;
	int n = 0;
	set_lua_variables(lua);
	if (argv && (!argv->empty()))
		argvSize += argv->size();
	lua_createtable(lua, argvSize, 0);

	// arg0 is scriptname
	lua_pushstring(lua, fileName);
	lua_rawseti(lua, -2, n++);

	if (argv && (!argv->empty())) {
		for(std::vector<std::string>::iterator it = argv->begin(); it != argv->end(); ++it) {
			lua_pushstring(lua, it->c_str());
			lua_rawseti(lua, -2, n++);
		}
	}
	lua_setglobal(lua, "arg");
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
	CFrameBuffer::getInstance()->autoBlit();
#endif
	status = lua_pcall(lua, 0, LUA_MULTRET, 0);
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
	CFrameBuffer::getInstance()->autoBlit(false);
#endif
	if (result_code)
		*result_code = to_string(status);
	if (result_string && lua_isstring(lua, -1))
		*result_string = std::string(lua_tostring(lua, -1));
	if (status)
	{
		fprintf(stderr, "[CLuaInstance::%s] error in script: %s\n", __func__, lua_tostring(lua, -1));
		DisplayErrorMessage(lua_tostring(lua, -1), "Lua Script Error:");
		if (error_string)
			*error_string = std::string(lua_tostring(lua, -1));
		/* restoreNeutrino at plugin crash, when blocked from plugin */
		if (CMoviePlayerGui::getInstance().getBlockedFromPlugin()) {
			CMoviePlayerGui::getInstance().setBlockedFromPlugin(false);
			CMoviePlayerGui::getInstance().restoreNeutrino();
		}
	}
}

// Example: runScript(fileName, "Arg1", "Arg2", "Arg3", ..., NULL);
//	Type of all parameters: const char*
//	The last parameter to NULL is imperative.
void CLuaInstance::runScript(const char *fileName, const char *arg0, ...)
{
	int i = 0;
	std::vector<std::string> args;
	args.push_back(arg0);
	va_list list;
	va_start(list, arg0);
	const char* temp = va_arg(list, const char*);
	while (temp != NULL) {
		if (i >= 64) {
			fprintf(stderr, "CLuaInstance::runScript: too many arguments!\n");
			args.clear();
			va_end(list);
			return;
		}
		args.push_back(temp);
		temp = va_arg(list, const char*);
		i++;
	}
	va_end(list);
	runScript(fileName, &args);
	args.clear();
}

static void abortHook(lua_State *lua, lua_Debug *)
{
	luaL_error(lua, "aborted");
}

void CLuaInstance::abortScript()
{
	lua_sethook(lua, &abortHook, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

const luaL_Reg CLuaInstance::methods[] =
{
	{ "GetInput",               CLuaInstance::GetInput },
	{ "Blit",                   CLuaInstance::Blit },
	{ "GetLanguage",            CLuaInstance::GetLanguage },
	{ "PaintBox",               CLuaInstance::PaintBox },
	{ "paintHLine",             CLuaInstance::paintHLineRel },
	{ "paintVLine",             CLuaInstance::paintVLineRel },
	{ "RenderString",           CLuaInstance::RenderString },
	{ "getRenderWidth",         CLuaInstance::getRenderWidth },
	{ "FontHeight",             CLuaInstance::FontHeight },
	{ "getDynFont",             CLuaInstance::getDynFont },
	{ "PaintIcon",              CLuaInstance::PaintIcon },
	{ "DisplayImage",           CLuaInstance::DisplayImage },
	{ "GetSize",                CLuaInstance::GetSize },
	{ "saveScreen",             CLuaInstance::saveScreen },
	{ "restoreScreen",          CLuaInstance::restoreScreen },
	{ "deleteSavedScreen",      CLuaInstance::deleteSavedScreen },

	/*
	   lua_misc.cpp
	   Deprecated, for the future separate class for misc functions
	*/
	{ "strFind",                CLuaInstMisc::getInstance()->strFind_old },
	{ "strSub",                 CLuaInstMisc::getInstance()->strSub_old },
	{ "enableInfoClock",        CLuaInstMisc::getInstance()->enableInfoClock_old },
	{ "runScript",              CLuaInstMisc::getInstance()->runScriptExt_old },
	{ "GetRevision",            CLuaInstMisc::getInstance()->GetRevision_old },
	{ "checkVersion",           CLuaInstMisc::getInstance()->checkVersion_old },

	/*
	   lua_video.cpp
	   Deprecated, for the future separate class for video
	*/
	{ "setBlank",               CLuaInstVideo::getInstance()->setBlank_old },
	{ "ShowPicture",            CLuaInstVideo::getInstance()->ShowPicture_old },
	{ "StopPicture",            CLuaInstVideo::getInstance()->StopPicture_old },
	{ "PlayFile",               CLuaInstVideo::getInstance()->PlayFile_old },
	{ "zapitStopPlayBack",      CLuaInstVideo::getInstance()->zapitStopPlayBack_old },
	{ "channelRezap",           CLuaInstVideo::getInstance()->channelRezap_old },
	{ "createChannelIDfromUrl", CLuaInstVideo::getInstance()->createChannelIDfromUrl_old },
	{ NULL, NULL }
};

#ifdef STATIC_LUAPOSIX
/* hack: we link against luaposix, which is included in our
 * custom built lualib */
extern "C" { LUAMOD_API int (luaopen_posix_c) (lua_State *L); }
#endif

/* load basic functions and register our own C callbacks */
void CLuaInstance::registerFunctions()
{
	luaL_openlibs(lua);
	luaopen_table(lua);
	luaopen_io(lua);
	luaopen_string(lua);
	luaopen_math(lua);
	lua_newtable(lua);
	int methodtable = lua_gettop(lua);
	luaL_newmetatable(lua, className);
	int metatable = lua_gettop(lua);
	lua_pushliteral(lua, "__metatable");
	lua_pushvalue(lua, methodtable);
	lua_settable(lua, metatable);

	lua_pushliteral(lua, "__index");
	lua_pushvalue(lua, methodtable);
	lua_settable(lua, metatable);

	lua_pushliteral(lua, "__gc");
	lua_pushcfunction(lua, GCWindow);
	lua_settable(lua, metatable);

	lua_pop(lua, 1);

	luaL_setfuncs(lua, methods, 0);
	lua_pop(lua, 1);

	lua_register(lua, className, NewWindow);

	CLuaInstCCPicture::getInstance()->CCPictureRegister(lua);
	CLuaInstCCSignalbox::getInstance()->CCSignalBoxRegister(lua);
	CLuaInstCCText::getInstance()->CCTextRegister(lua);
	CLuaInstCCWindow::getInstance()->CCWindowRegister(lua);
	CLuaInstConfigFile::getInstance()->LuaConfigFileRegister(lua);
	CLuaInstCurl::getInstance()->LuaCurlRegister(lua);
	CLuaInstHintbox::getInstance()->HintboxRegister(lua);
	CLuaInstMenu::getInstance()->MenuRegister(lua);
	CLuaInstMessagebox::getInstance()->MessageboxRegister(lua);
	CLuaInstMisc::getInstance()->LuaMiscRegister(lua);
	CLuaInstVideo::getInstance()->LuaVideoRegister(lua);
}

CLuaData *CLuaInstance::CheckData(lua_State *L, int narg)
{
	luaL_checktype(L, narg, LUA_TUSERDATA);
	void *ud = luaL_checkudata(L, narg, className);
	if (!ud)
		fprintf(stderr, "[CLuaInstance::%s] wrong type %p, %d, %s\n", __func__, L, narg, className);
	return *(CLuaData **)ud;  // unbox pointer
}

int CLuaInstance::NewWindow(lua_State *L)
{
	int count = lua_gettop(L);
	int x = g_settings.screen_StartX;
	int y = g_settings.screen_StartY;
	int w = g_settings.screen_EndX - x;
	int h = g_settings.screen_EndY - y;
	CLuaData *D = new CLuaData();
	if (count > 0)
		x = luaL_checkint(L, 1);
	if (count > 1)
		y = luaL_checkint(L, 2);
	if (count > 2)
		w = luaL_checkint(L, 3);
	if (count > 3)
		h = luaL_checkint(L, 4);
	CFBWindow *W = new CFBWindow(x, y, w, h);
	D->fbwin = W;
	D->rcinput = g_RCInput;
	lua_boxpointer(L, D);
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstance::GCWindow(lua_State *L)
{
	LUA_DEBUG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));
	CLuaData *w = (CLuaData *)lua_unboxpointer(L, 1);

	if (w && w->fbwin) {
		for (screenmap_iterator_t it = w->screenmap.begin(); it != w->screenmap.end(); ++it) {
			if (it->second != NULL)
				delete[] it->second;
		}
		LUA_DEBUG("CLuaInstance::%s delete screenmap\n", __func__);
		w->screenmap.clear();
		w->fontmap.clear();
	}
	CNeutrinoFonts::getInstance()->deleteDynFontExtAll();

	/* restoreNeutrino at plugin closing, when blocked from plugin */
	LUA_DEBUG(">>>>[%s:%d] (restoreNeutrino()) BlockedFromPlugin: %d, Playing: %d\n", __func__, __LINE__,
		CMoviePlayerGui::getInstance().getBlockedFromPlugin, CMoviePlayerGui::getInstance().Playing());
	if (CMoviePlayerGui::getInstance().getBlockedFromPlugin() &&
	    CMoviePlayerGui::getInstance().Playing()) {
		CMoviePlayerGui::getInstance().setBlockedFromPlugin(false);
		CMoviePlayerGui::getInstance().restoreNeutrino();
	}

	delete w->fbwin;
	w->rcinput = NULL;
	delete w;
	return 0;
}

int CLuaInstance::GetInput(lua_State *L)
{
	int numargs = lua_gettop(L);
	int timeout = 0;
	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	CLuaData *W = CheckData(L, 1);
	if (!W)
		return 0;
	if (numargs > 1)
		timeout = luaL_checkint(L, 2);
	W->rcinput->getMsg_ms(&msg, &data, timeout);
	/* TODO: I'm not sure if this works... */
	if (msg != CRCInput::RC_timeout && msg > CRCInput::RC_MaxRC)
	{
		LUA_DEBUG("CLuaInstance::%s: msg 0x%08" PRIx32 " data 0x%08" PRIx32 "\n", __func__, msg, data);
		CNeutrinoApp::getInstance()->handleMsg(msg, data);
	}
	/* signed int is debatable, but the "big" messages can't yet be handled
	 * inside lua scripts anyway. RC_timeout == -1, RC_nokey == -2 */
	lua_pushinteger(L, (int)msg);
	lua_pushunsigned(L, data);
	return 2;
}

#if 1
int CLuaInstance::Blit(lua_State *)
{
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
	CFrameBuffer::getInstance()->autoBlit(false);
#endif
	return 0;
}
#else
int CLuaInstance::Blit(lua_State *L)
{
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
	CFrameBuffer::getInstance()->autoBlit(false);
#endif
	CLuaData *W = CheckData(L, 1);
	if (W && W->fbwin) {
		if (lua_isnumber(L, 2))
			W->fbwin->blit((int)lua_tonumber(L, 2)); // enable/disable automatic blit
		else
			W->fbwin->blit();
	}
	return 0;
}
#endif

int CLuaInstance::GetLanguage(lua_State *L)
{
	// FIXME -- should conform to ISO 639-1/ISO 3166-1
	lua_pushstring(L, g_settings.language.c_str());

	return 1;
}

/* --------------------------------------------------------------- */

int CLuaInstance::PaintBox(lua_State *L)
{
	int count = lua_gettop(L);
	LUA_DEBUG("CLuaInstance::%s %d\n", __func__, count);
	int x, y, w, h, radius = 0, corner = CORNER_ALL;
	unsigned int c;

	CLuaData *W = CheckData(L, 1);
	if (!W || !W->fbwin)
		return 0;
	x = luaL_checkint(L, 2);
	y = luaL_checkint(L, 3);
	w = luaL_checkint(L, 4);
	h = luaL_checkint(L, 5);

#if HAVE_COOL_HARDWARE
	c = luaL_checkunsigned(L, 6);
#else
	/* luaL_checkint does not like e.g. 0xffcc0000 on powerpc (returns INT_MAX) instead */
	c = (unsigned int)luaL_checknumber(L, 6);
#endif

	if (count > 6)
		radius = luaL_checkint(L, 7);
	if (count > 7)
		corner = luaL_checkint(L, 8);
	/* those checks should be done in CFBWindow instead... */
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (w < 0 || x + w > W->fbwin->dx)
		w = W->fbwin->dx - x;
	if (h < 0 || y + h > W->fbwin->dy)
		h = W->fbwin->dy - y;
	checkMagicMask(c);
	W->fbwin->paintBoxRel(x, y, w, h, c, radius, corner);
	return 0;
}

int CLuaInstance::paintHLineRel(lua_State *L)
{
	int x, y, dx;
	unsigned int c;

	CLuaData *W = CheckData(L, 1);
	if (!W || !W->fbwin)
		return 0;
	x  = luaL_checkint(L, 2);
	dx = luaL_checkint(L, 3);
	y  = luaL_checkint(L, 4);

#if HAVE_COOL_HARDWARE
	c = luaL_checkunsigned(L, 5);
#else
	/* luaL_checkint does not like e.g. 0xffcc0000 on powerpc (returns INT_MAX) instead */
	c = (unsigned int)luaL_checknumber(L, 5);
#endif
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (dx < 0 || x + dx > W->fbwin->dx)
		dx = W->fbwin->dx - x;
	checkMagicMask(c);
	W->fbwin->paintHLineRel(x, dx, y, c);
	return 0;
}

int CLuaInstance::paintVLineRel(lua_State *L)
{
	int x, y, dy;
	unsigned int c;

	CLuaData *W = CheckData(L, 1);
	if (!W || !W->fbwin)
		return 0;
	x  = luaL_checkint(L, 2);
	y  = luaL_checkint(L, 3);
	dy = luaL_checkint(L, 4);

#if HAVE_COOL_HARDWARE
	c = luaL_checkunsigned(L, 5);
#else
	/* luaL_checkint does not like e.g. 0xffcc0000 on powerpc (returns INT_MAX) instead */
	c = (unsigned int)luaL_checknumber(L, 5);
#endif
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (dy < 0 || y + dy > W->fbwin->dy)
		dy = W->fbwin->dy - y;
	checkMagicMask(c);
	W->fbwin->paintVLineRel(x, y, dy, c);
	return 0;
}

/* --------------------------------------------------------------- */

int CLuaInstance::RenderString(lua_State *L)
{
	int x, y, w, boxh, center;
	Font* font = NULL;
	unsigned int c;
	const char *text;
	int numargs = lua_gettop(L);
	LUA_DEBUG("CLuaInstance::%s %d\n", __func__, numargs);
	c = COL_MENUCONTENT_TEXT;
	boxh = 0;
	center = 0;

	CLuaData *W = CheckData(L, 1);
	if (!W || !W->fbwin)
		return 0;

	int step = 0;
	bool isDynFont = false;
	if (lua_isboolean(L, 2)) {
		if (lua_toboolean(L, 2) == true)
			isDynFont = true;
		step = 1;
	}

	if (!isDynFont) {
		int f = luaL_checkint(L, 2+step);	/* font number, use FONT_TYPE_XXX in the script */
		if (f >= SNeutrinoSettings::FONT_TYPE_COUNT || f < 0)
			f = SNeutrinoSettings::FONT_TYPE_MENU;
		font = g_Font[f];
	}
	else {
		int id = luaL_checkint(L, 2+step);	/* dynfont */
		for (fontmap_iterator_t it = W->fontmap.begin(); it != W->fontmap.end(); ++it) {
			if (it->first == id) {
				font = it->second;
				break;
			}
		}
		if (font == NULL) {
			printf("[CLuaInstance::%s:%d] no font found\n", __func__, __LINE__);
			lua_pushinteger(L, 0); /* no font found */
			return 1;
		}
	}

	text = luaL_checkstring(L, 3+step);	/* text */
	x = luaL_checkint(L, 4+step);
	y = luaL_checkint(L, 5+step);
	if (numargs > 5+step)
#if HAVE_COOL_HARDWARE
		c = luaL_checkunsigned(L, 6+step);
#else
		c = luaL_checkint(L, 6+step);
#endif
	if (numargs > 6+step)
		w = luaL_checkint(L, 7+step);
	else
		w = W->fbwin->dx - x;
	if (numargs > 7+step)
		boxh = luaL_checkint(L, 8+step);
	if (numargs > 8+step)
		center = luaL_checkint(L, 9+step);

	int rwidth = font->getRenderWidth(text);

	if (center) { /* center the text inside the box */
		if (rwidth < w)
			x += (w - rwidth) / 2;
	}
	checkMagicMask(c);
	if (boxh > -1) /* if boxh < 0, don't paint string */
		W->fbwin->RenderString(font, x, y, w, text, c, boxh);
	lua_pushinteger(L, rwidth); /* return renderwidth */
	return 1;
}

int CLuaInstance::getRenderWidth(lua_State *L)
{
	Font* font = NULL;
	const char *text;
	LUA_DEBUG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));

	CLuaData *W = CheckData(L, 1);
	if (!W)
		return 0;

	int step = 0;
	bool isDynFont = false;
	if (lua_isboolean(L, 2)) {
		if (lua_toboolean(L, 2) == true)
			isDynFont = true;
		step = 1;
	}

	if (!isDynFont) {
		int f = luaL_checkint(L, 2+step);	/* font number, use FONT_TYPE_XXX in the script */
		if (f >= SNeutrinoSettings::FONT_TYPE_COUNT || f < 0)
			f = SNeutrinoSettings::FONT_TYPE_MENU;
		font = g_Font[f];
	}
	else {
		int id = luaL_checkint(L, 2+step);	/* dynfont */
		for (fontmap_iterator_t it = W->fontmap.begin(); it != W->fontmap.end(); ++it) {
			if (it->first == id) {
				font = it->second;
				break;
			}
		}
		if (font == NULL) {
			printf("[CLuaInstance::%s:%d] no font found\n", __func__, __LINE__);
			lua_pushinteger(L, 0); /* no font found */
			return 1;
		}
	}

	text = luaL_checkstring(L, 3+step);	/* text */

	lua_pushinteger(L, (int)font->getRenderWidth(text));
	return 1;
}

int CLuaInstance::FontHeight(lua_State *L)
{
	Font* font = NULL;
	LUA_DEBUG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));

	CLuaData *W = CheckData(L, 1);
	if (!W)
		return 0;

	int step = 0;
	bool isDynFont = false;
	if (lua_isboolean(L, 2)) {
		if (lua_toboolean(L, 2) == true)
			isDynFont = true;
		step = 1;
	}

	if (!isDynFont) {
		int f = luaL_checkint(L, 2+step);	/* font number, use FONT_TYPE_XXX in the script */
		if (f >= SNeutrinoSettings::FONT_TYPE_COUNT || f < 0)
			f = SNeutrinoSettings::FONT_TYPE_MENU;
		font = g_Font[f];
	}
	else {
		int id = luaL_checkint(L, 2+step);	/* dynfont */
		for (fontmap_iterator_t it = W->fontmap.begin(); it != W->fontmap.end(); ++it) {
			if (it->first == id) {
				font = it->second;
				break;
			}
		}
		if (font == NULL) {
			printf("[CLuaInstance::%s:%d] no font found\n", __func__, __LINE__);
			lua_pushinteger(L, 0); /* no font found */
			return 1;
		}
	}

	lua_pushinteger(L, (int)font->getHeight());
	return 1;
}

int CLuaInstance::getDynFont(lua_State *L)
{
	int numargs = lua_gettop(L);
	CLuaData *W = CheckData(L, 1);
	if (!W || !W->fbwin)
		return 0;

	if (numargs < 3) {
		printf("CLuaInstance::%s: not enough arguments (%d, expected 2)\n", __func__, numargs);
		return 0;
	}

	lua_Integer fontID = W->fontmap.size();
	if (fontID >= CNeutrinoFonts::DYNFONTEXT_MAX) {
		lua_pushnil(L);
		lua_pushinteger(L, DYNFONT_MAXIMUM_FONTS);
		return 2;
	}

	lua_Integer dx = 0, dy = 0, style = CNeutrinoFonts::FONT_STYLE_REGULAR;
	std::string text="";

	dx = luaL_checkint(L, 2);
	if (dx > (lua_Integer)CFrameBuffer::getInstance()->getScreenWidth(true)) {
		lua_pushnil(L);
		lua_pushinteger(L, DYNFONT_TO_WIDE);
		return 2;
	}
	dy = luaL_checkint(L, 3);
	if (dy > 100) {
		lua_pushnil(L);
		lua_pushinteger(L, DYNFONT_TOO_HIGH);
		return 2;
	}
	if (numargs > 3)
		text = luaL_checkstring(L, 4);
	if (numargs > 4) {
		style = luaL_checkint(L, 5);
		if (style > CNeutrinoFonts::FONT_STYLE_ITALIC)
			style = CNeutrinoFonts::FONT_STYLE_REGULAR;
	}

	Font* f = CNeutrinoFonts::getInstance()->getDynFontExt(dx, dy, fontID, text, style);

	lua_Integer id = fontID + 1;
	W->fontmap.insert(fontmap_pair_t(id, f));
	lua_pushinteger(L, id);
	lua_pushinteger(L, DYNFONT_NO_ERROR);
	return 2;
}

/* --------------------------------------------------------------- */

int CLuaInstance::PaintIcon(lua_State *L)
{
	LUA_DEBUG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));
	int x, y, h;
	unsigned int o;
	const char *fname;

	CLuaData *W = CheckData(L, 1);
	if (!W || !W->fbwin)
		return 0;
	fname = luaL_checkstring(L, 2);
	x = luaL_checkint(L, 3);
	y = luaL_checkint(L, 4);
	h = luaL_checkint(L, 5);
	o = luaL_checkint(L, 6);
	W->fbwin->paintIcon(fname, x, y, h, o);
	return 0;
}

int CLuaInstance::DisplayImage(lua_State *L)
{
	LUA_DEBUG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));
	int x, y, w, h;
	const char *fname;

	fname = luaL_checkstring(L, 2);
	x = luaL_checkint(L, 3);
	y = luaL_checkint(L, 4);
	w = luaL_checkint(L, 5);
	h = luaL_checkint(L, 6);
	int trans = 0;
	if (lua_isnumber(L, 7))
		trans = luaL_checkint(L, 7);
	g_PicViewer->DisplayImage(fname, x, y, w, h, trans);
	return 0;
}

int CLuaInstance::GetSize(lua_State *L)
{
	LUA_DEBUG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));
	int w = 0, h = 0;
	const char *fname;

	fname = luaL_checkstring(L, 2);
	g_PicViewer->getSize(fname, &w, &h);
	lua_pushinteger(L, w);
	lua_pushinteger(L, h);
	return 2;
}

/* --------------------------------------------------------------- */

int CLuaInstance::saveScreen(lua_State *L)
{
	int x, y, w, h;
	fb_pixel_t* buf;
	CLuaData *W = CheckData(L, 1);
	if (!W || !W->fbwin)
		return 0;
	x = luaL_checkint(L, 2);
	y = luaL_checkint(L, 3);
	w = luaL_checkint(L, 4);
	h = luaL_checkint(L, 5);

	buf = W->fbwin->saveScreen(x, y, w, h);

	lua_Integer id = W->screenmap.size() + 1;
	W->screenmap.insert(screenmap_pair_t(id, buf));
	lua_pushinteger(L, id);
	return 1;
}

int CLuaInstance::restoreScreen(lua_State *L)
{
	int x, y, w, h, id;
	fb_pixel_t* buf = NULL;
	bool del;
	CLuaData *W = CheckData(L, 1);
	if (!W || !W->fbwin)
		return 0;
	x = luaL_checkint(L, 2);
	y = luaL_checkint(L, 3);
	w = luaL_checkint(L, 4);
	h = luaL_checkint(L, 5);
	id = luaL_checkint(L, 6);
	del = _luaL_checkbool(L, 7);

	for (screenmap_iterator_t it = W->screenmap.begin(); it != W->screenmap.end(); ++it) {
		if (it->first == id) {
			buf = it->second;
			if (del)
				it->second = NULL;
			break;
		}
	}
	if (buf != NULL)
		W->fbwin->restoreScreen(x, y, w, h, buf, del);
	return 0;
}

int CLuaInstance::deleteSavedScreen(lua_State *L)
{
	int id;
	CLuaData *W = CheckData(L, 1);
	if (!W || !W->fbwin)
		return 0;
	id = luaL_checkint(L, 2);

	for (screenmap_iterator_t it = W->screenmap.begin(); it != W->screenmap.end(); ++it) {
		if (it->first == id) {
			if (it->second != NULL) {
				delete[] it->second;
				it->second = NULL;
			}
			break;
		}
	}
	return 0;
}

/* --------------------------------------------------------------- */
