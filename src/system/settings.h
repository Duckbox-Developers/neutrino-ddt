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

#ifndef __settings__
#define __settings__

#include <system/localize.h>
#include <configfile.h>
#include <zapit/client/zapitclient.h>
#include <zapit/client/zapittools.h>
#include <eitd/edvbstring.h> // UTF8

#include <string>
#include <list>

#ifdef BOXMODEL_APOLLO
#define VIDEOMENU_VIDEOMODE_OPTION_COUNT 16
#else
#define VIDEOMENU_VIDEOMODE_OPTION_COUNT 12
#endif

struct SNeutrinoTheme
{
	unsigned char menu_Head_alpha;
	unsigned char menu_Head_red;
	unsigned char menu_Head_green;
	unsigned char menu_Head_blue;

	unsigned char menu_Head_Text_alpha;
	unsigned char menu_Head_Text_red;
	unsigned char menu_Head_Text_green;
	unsigned char menu_Head_Text_blue;

	unsigned char menu_Content_alpha;
	unsigned char menu_Content_red;
	unsigned char menu_Content_green;
	unsigned char menu_Content_blue;

	unsigned char menu_Content_Text_alpha;
	unsigned char menu_Content_Text_red;
	unsigned char menu_Content_Text_green;
	unsigned char menu_Content_Text_blue;

	unsigned char menu_Content_Selected_alpha;
	unsigned char menu_Content_Selected_red;
	unsigned char menu_Content_Selected_green;
	unsigned char menu_Content_Selected_blue;

	unsigned char menu_Content_Selected_Text_alpha;
	unsigned char menu_Content_Selected_Text_red;
	unsigned char menu_Content_Selected_Text_green;
	unsigned char menu_Content_Selected_Text_blue;

	unsigned char menu_Content_inactive_alpha;
	unsigned char menu_Content_inactive_red;
	unsigned char menu_Content_inactive_green;
	unsigned char menu_Content_inactive_blue;

	unsigned char menu_Content_inactive_Text_alpha;
	unsigned char menu_Content_inactive_Text_red;
	unsigned char menu_Content_inactive_Text_green;
	unsigned char menu_Content_inactive_Text_blue;

	unsigned char infobar_alpha;
	unsigned char infobar_red;
	unsigned char infobar_green;
	unsigned char infobar_blue;

	unsigned char infobar_Text_alpha;
	unsigned char infobar_Text_red;
	unsigned char infobar_Text_green;
	unsigned char infobar_Text_blue;

	unsigned char colored_events_alpha;
	unsigned char colored_events_red;
	unsigned char colored_events_green;
	unsigned char colored_events_blue;

	unsigned char clock_Digit_alpha;
	unsigned char clock_Digit_red;
	unsigned char clock_Digit_green;
	unsigned char clock_Digit_blue;
};

struct SNeutrinoSettings
{
	//video
	int video_Format;
	int video_Mode;
	int analog_mode1;
	int analog_mode2;
	int video_43mode;
#ifdef BOXMODEL_APOLLO
	int brightness;
	int contrast;
	int saturation;
	int enable_sd_osd;
#endif
	char current_volume;
	int current_volume_step;
	int start_volume;
	int channel_mode;
	int channel_mode_radio;
	int channel_mode_initial;
	int channel_mode_initial_radio;

	//misc
	int shutdown_real;
	int shutdown_real_rcdelay;
	int shutdown_count;
	int shutdown_min;
	int sleeptimer_min;
	int record_safety_time_before;
	int record_safety_time_after;
	int zapto_pre_time;
	int infobar_sat_display;
	int infobar_show_channeldesc;
	int infobar_subchan_disp_pos;
	int fan_speed;
	int infobar_show;
	int infobar_show_channellogo;
	int infobar_progressbar;
	int progressbar_design;
	int progressbar_gradient;
	int progressbar_timescale_red;
	int progressbar_timescale_green;
	int progressbar_timescale_yellow;
	int progressbar_timescale_invert;
	int casystem_display;
	int scrambled_message;
	int volume_pos;
	int volume_digits;
	int volume_size;
	int show_mute_icon;
	int menu_pos;
	int show_menu_hints;
	int infobar_show_sysfs_hdd;
	int infobar_show_res;
	int infobar_show_tuner;
	int infobar_show_dd_available;
	int wzap_time;
	//audio
	int audio_AnalogMode;
	int audio_DolbyDigital;
	int auto_lang;
	int auto_subs;
	int srs_enable;
	int srs_algo;
	int srs_ref_volume;
	int srs_nmgr_enable;
	int hdmi_dd;
	int spdif_dd;
	int analog_out;
	//video
	int video_dbdr;
	int hdmi_cec_mode;
	int hdmi_cec_view_on;
	int hdmi_cec_standby;
	int enabled_video_modes[VIDEOMENU_VIDEOMODE_OPTION_COUNT];
	int cpufreq;
	int standby_cpufreq;
	int make_hd_list;
	int make_webtv_list;
	int make_new_list;
	int make_removed_list;
	int keep_channel_numbers;
	int avsync;
	int clockrec;
	int rounded_corners;
	int ci_standby_reset;
	int ci_clock;
	int ci_ignore_messages;
	int radiotext_enable;
	int easymenu;

	//vcr
	int vcr_AutoSwitch;

	//language
	std::string language;
	std::string timezone;

	std::string pref_lang[3];
	std::string pref_subs[3];
	std::string subs_charset;

	// EPG
	int epg_save;
	int epg_save_standby;
	int epg_cache;
	int epg_old_events;
	int epg_max_events;
	int epg_extendedcache;
	std::string epg_dir;
	int epg_scan;
	int epg_scan_mode;

	int epg_search_history_size;
	int epg_search_history_max;
	std::list<std::string> epg_search_history;

	//network
	std::string network_ntpserver;
	std::string network_ntprefresh;
	int network_ntpenable;
	std::string ifname;

	std::list<std::string> webtv_xml;

	//personalize
	enum PERSONALIZE_SETTINGS  //settings.h
	{
		P_MAIN_PINSTATUS,

		//user menu
		P_MAIN_BLUE_BUTTON,
		P_MAIN_YELLOW_BUTTON,
		P_MAIN_GREEN_BUTTON,
		P_MAIN_RED_BUTTON,

		//main menu
		P_MAIN_TV_MODE,
		P_MAIN_TV_RADIO_MODE, //togglemode
		P_MAIN_RADIO_MODE,
		P_MAIN_TIMER,
		P_MAIN_MEDIA,

		P_MAIN_GAMES,
		P_MAIN_TOOLS,
		P_MAIN_SCRIPTS,
		P_MAIN_LUA,
		P_MAIN_SETTINGS,
		P_MAIN_SERVICE,
		P_MAIN_SLEEPTIMER,
		P_MAIN_STANDBY,
		P_MAIN_REBOOT,
		P_MAIN_SHUTDOWN,
		P_MAIN_INFOMENU,
		P_MAIN_CISETTINGS,

		//settings menu
		P_MSET_SETTINGS_MANAGER,
		P_MSET_VIDEO,
		P_MSET_AUDIO,
		P_MSET_NETWORK,
		P_MSET_RECORDING,
		P_MSET_OSDLANG,
		P_MSET_OSD,
		P_MSET_VFD,
		P_MSET_DRIVES,
		P_MSET_CISETTINGS,
		P_MSET_KEYBINDING,
		P_MSET_MEDIAPLAYER,
		P_MSET_MISC,

		//service menu
		P_MSER_TUNER,
		P_MSER_SCANTS,
		P_MSER_RELOAD_CHANNELS,
		P_MSER_BOUQUET_EDIT,
		P_MSER_RESET_CHANNELS,
		P_MSER_RESTART,
		P_MSER_RELOAD_PLUGINS,
		P_MSER_SERVICE_INFOMENU,
		P_MSER_SOFTUPDATE,

		//media menu
		P_MEDIA_MENU,
		P_MEDIA_AUDIO,
		P_MEDIA_INETPLAY,
		P_MEDIA_MPLAYER,
		P_MEDIA_PVIEWER,
		P_MEDIA_UPNP,

		//movieplayer menu
		P_MPLAYER_MBROWSER,
		P_MPLAYER_FILEPLAY,
		P_MPLAYER_YTPLAY,

		//feature keys
		P_FEAT_KEY_FAVORIT,
		P_FEAT_KEY_TIMERLIST,
		P_FEAT_KEY_VTXT,
		P_FEAT_KEY_RC_LOCK,

		//user menu
		P_UMENU_SHOW_CANCEL,

		//plugins types
		P_UMENU_PLUGIN_TYPE_GAMES,
		P_UMENU_PLUGIN_TYPE_TOOLS,
		P_UMENU_PLUGIN_TYPE_SCRIPTS,
		P_UMENU_PLUGIN_TYPE_LUA,

		P_SETTINGS_MAX
	};

	int  personalize[P_SETTINGS_MAX];
	std::string personalize_pincode;

	//timing
	enum TIMING_SETTINGS 
	{
		TIMING_MENU		= 0,
		TIMING_CHANLIST		= 1,
		TIMING_EPG		= 2,
		TIMING_INFOBAR		= 3,
		TIMING_INFOBAR_RADIO	= 4,
		TIMING_INFOBAR_MOVIE	= 5,
		TIMING_VOLUMEBAR	= 6,
		TIMING_FILEBROWSER	= 7,
		TIMING_NUMERICZAP	= 8,

		TIMING_SETTING_COUNT
	};

	int timing [TIMING_SETTING_COUNT];

	//widget settings
	int widget_fade;

	SNeutrinoTheme theme;

	int colored_events_channellist;
	int colored_events_infobar;
	int contrast_fonts;
	int gradiant;

	//network
#define NETWORK_NFS_NR_OF_ENTRIES 8
	struct {
		std::string ip;
		std::string mac;
		std::string local_dir;
		std::string dir;
		int  automount;
		std::string mount_options1;
		std::string mount_options2;
		int  type;
		std::string username;
		std::string password;
	} network_nfs[NETWORK_NFS_NR_OF_ENTRIES];
	std::string network_nfs_audioplayerdir;
	std::string network_nfs_picturedir;
	std::string network_nfs_moviedir;
	std::string network_nfs_recordingdir;
	std::string timeshiftdir;
	std::string downloadcache_dir;

	//recording
	int  recording_type;
	int  recording_stopsectionsd;
	unsigned char recording_audio_pids_default;
	int recording_audio_pids_std;
	int recording_audio_pids_alt;
	int recording_audio_pids_ac3;
	int recording_stream_vtxt_pid;
	int recording_stream_subtitle_pids;
	int recording_stream_pmt_pid;
	int recording_choose_direct_rec_dir;
	int recording_epg_for_filename;
	int recording_epg_for_end;
	int recording_save_in_channeldir;
	int recording_zap_on_announce;
	int recording_slow_warning;
	int recording_startstop_msg;
	int shutdown_timer_record_type;
	std::string recording_filename_template;

	int filesystem_is_utf8;
	// default plugin for ts-movieplayer (red button)
	std::string movieplayer_plugin;
	std::string plugin_hdd_dir;

	std::string logo_hdd_dir;

	std::string plugins_disabled;
	std::string plugins_game;
	std::string plugins_tool;
	std::string plugins_script;
	std::string plugins_lua;

	//key configuration
	int key_tvradio_mode;

	int key_pageup;
	int key_pagedown;

	int key_channelList_cancel;
	int key_channelList_sort;
	int key_channelList_addrecord;
	int key_channelList_addremind;

	int key_quickzap_up;
	int key_quickzap_down;
	int key_bouquet_up;
	int key_bouquet_down;
	int key_subchannel_up;
	int key_subchannel_down;
	int key_zaphistory;
	int key_lastchannel;
	int key_list_start;
	int key_list_end;
	int key_power_off;
	int menu_left_exit;
	int key_click;
	int timeshift_pause;
	int auto_timeshift;
	int temp_timeshift;
	int auto_delete;
	int record_hours;
	int timeshift_hours;
	int key_record;
	int key_help;
	int key_next43mode;
	int key_switchformat;
	int key_volumeup;
	int key_volumedown;

	int mpkey_rewind;
	int mpkey_forward;
	int mpkey_pause;
	int mpkey_stop;
	int mpkey_play;
	int mpkey_audio;
	int mpkey_time;
	int mpkey_bookmark;
	int mpkey_plugin;
	int mpkey_goto;
	int mpkey_subtitle;
	int mpkey_next_repeat_mode;
	int key_timeshift;
	int key_plugin;

	int key_unlock;

	int key_screenshot;
	int screenshot_count;
	int screenshot_format;
	int screenshot_cover;
	int screenshot_mode;
	int screenshot_video;
	int screenshot_scale;
	int auto_cover;
	std::string screenshot_dir;

	int key_current_transponder;
	int key_pip_close;
	int key_pip_setup;
	int key_pip_swap;
	int key_format_mode_active;
	int key_pic_mode_active;
	int key_pic_size_active;

	int cacheTXT;
	int minimode;
	int mode_clock;

	enum MODE_LEFT_RIGHT_KEY_TV_SETTINGS 
	{
		ZAP     = 0,
		VZAP    = 1,
		VOLUME  = 2,
		INFOBAR = 3
	};
	int mode_left_right_key_tv;

	int spectrum;
	int pip_width;
	int pip_height;
	int pip_x;
	int pip_y;
	int pip_radio_width;
	int pip_radio_height;
	int pip_radio_x;
	int pip_radio_y;
	int bigFonts;
	int window_size;
	int window_width;
	int window_height;
	int eventlist_additional;
	int channellist_additional;
	int channellist_epgtext_align_right;
	int channellist_progressbar_design;
	int channellist_foot;
	int channellist_new_zap_mode;
	int channellist_sort_mode;
	int channellist_numeric_adjust;
	int channellist_show_channellogo;
	int channellist_show_numbers;
	int repeat_blocker;
	int repeat_genericblocker;
#define LONGKEYPRESS_OFF 499
	int longkeypress_duration;
	int remote_control_hardware;
	int audiochannel_up_down_enable;

	//screen configuration
	int screen_StartX;
	int screen_StartY;
	int screen_EndX;
	int screen_EndY;
	int screen_StartX_crt;
	int screen_StartY_crt;
	int screen_EndX_crt;
	int screen_EndY_crt;
	int screen_StartX_lcd;
	int screen_StartY_lcd;
	int screen_EndX_lcd;
	int screen_EndY_lcd;
	int screen_preset;
	int screen_width;
	int screen_height;
	int screen_xres;
	int screen_yres;

	//Software-update
	int softupdate_mode;
	std::string softupdate_url_file;
	std::string softupdate_proxyserver;
	std::string softupdate_proxyusername;
	std::string softupdate_proxypassword;
	int softupdate_name_mode_apply;
	int softupdate_name_mode_backup;
	int apply_settings;
	int apply_kernel;

	int flashupdate_createimage_add_uldr;
	int flashupdate_createimage_add_u_boot;
	int flashupdate_createimage_add_env;
	int flashupdate_createimage_add_spare;
	int flashupdate_createimage_add_kernel;

	//BouquetHandling
	int bouquetlist_mode;

	// parentallock
	int parentallock_prompt;
	int parentallock_lockage;
	int parentallock_defaultlocked;
	std::string parentallock_pincode;


	// Font sizes
	enum FONT_TYPES {
		FONT_TYPE_MENU = 0,
		FONT_TYPE_MENU_TITLE,
		FONT_TYPE_MENU_INFO,
		FONT_TYPE_EPG_TITLE,
		FONT_TYPE_EPG_INFO1,
		FONT_TYPE_EPG_INFO2,
		FONT_TYPE_EPG_DATE,
		FONT_TYPE_EVENTLIST_TITLE,
		FONT_TYPE_EVENTLIST_ITEMLARGE,
		FONT_TYPE_EVENTLIST_ITEMSMALL,
		FONT_TYPE_EVENTLIST_DATETIME,
		FONT_TYPE_EVENTLIST_EVENT,
		FONT_TYPE_CHANNELLIST,
		FONT_TYPE_CHANNELLIST_DESCR,
		FONT_TYPE_CHANNELLIST_NUMBER,
		FONT_TYPE_CHANNELLIST_EVENT,
		FONT_TYPE_CHANNEL_NUM_ZAP,
		FONT_TYPE_INFOBAR_NUMBER,
		FONT_TYPE_INFOBAR_CHANNAME,
		FONT_TYPE_INFOBAR_INFO,
		FONT_TYPE_INFOBAR_SMALL,
		FONT_TYPE_FILEBROWSER_ITEM,
		FONT_TYPE_MENU_HINT,
		FONT_TYPE_SUBTITLES,
		FONT_TYPE_COUNT
	};

	int infoClockFontSize;
	int infoClockSeconds;
	int infoClockBackground;

	// lcdd
	enum LCD_SETTINGS {
		LCD_BRIGHTNESS         = 0,
		LCD_STANDBY_BRIGHTNESS ,
		LCD_CONTRAST           ,
		LCD_POWER              ,
		LCD_INVERSE            ,
		LCD_SHOW_VOLUME        ,
		LCD_AUTODIMM           ,
		LCD_DEEPSTANDBY_BRIGHTNESS,
#if HAVE_TRIPLEDRAGON
		LCD_EPGMODE            ,
#endif
		LCD_SETTING_COUNT
	};
	int lcd_setting[LCD_SETTING_COUNT];
	int lcd_info_line;
	std::string lcd_setting_dim_time;
	int lcd_setting_dim_brightness;
	int led_tv_mode;
	int led_standby_mode;
	int led_deep_mode;
	int led_rec_mode;
	int led_blink;
	int backlight_tv;
	int backlight_standby;
	int backlight_deepstandby;
	int lcd_scroll;
	//#define FILESYSTEM_ENCODING_TO_UTF8(a) (g_settings.filesystem_is_utf8 ? (a) : ZapitTools::Latin1_to_UTF8(a).c_str())
#define FILESYSTEM_ENCODING_TO_UTF8(a) (isUTF8(a) ? (a) : ZapitTools::Latin1_to_UTF8(a).c_str())
#define UTF8_TO_FILESYSTEM_ENCODING(a) (g_settings.filesystem_is_utf8 ? (a) : ZapitTools::UTF8_to_Latin1(a).c_str())
	//#define FILESYSTEM_ENCODING_TO_UTF8_STRING(a) (g_settings.filesystem_is_utf8 ? (a) : ZapitTools::Latin1_to_UTF8(a))
#define FILESYSTEM_ENCODING_TO_UTF8_STRING(a) (isUTF8(a) ? (a) : ZapitTools::Latin1_to_UTF8(a))

	// pictureviewer
	int picviewer_slide_time;
	int picviewer_scaling;

	//audioplayer
	int   audioplayer_display;
	int   audioplayer_follow;
	int   audioplayer_screensaver;
	int   audioplayer_highprio;
	int   audioplayer_select_title_by_name;
	int   audioplayer_repeat_on;
	int   audioplayer_show_playlist;
	int   audioplayer_enable_sc_metadata;
	std::string shoutcast_dev_id;
	//Filebrowser
	int filebrowser_showrights;
	int filebrowser_sortmethod;
	int filebrowser_denydirectoryleave;

	//movieplayer
	int   movieplayer_repeat_on;

	//zapit setup
	std::string StartChannelTV;
	std::string StartChannelRadio;
	t_channel_id startchanneltv_id;
	t_channel_id startchannelradio_id;
	int uselastchannel;

	int	power_standby;
	int	hdd_sleep;
	int	hdd_noise;
	int	hdd_fs;
	enum { HDD_STATFS_OFF = 0, HDD_STATFS_ALWAYS, HDD_STATFS_RECORDING };
	int	hdd_statfs_mode;
	int	zap_cycle;
	int	sms_channel;
	std::string	font_file;
	std::string	ttx_font_file;
	std::string	update_dir;
	// USERMENU
	typedef enum
	{
		BUTTON_RED = 0,  // Do not change ordering of members, add new item just before BUTTON_MAX!!!
		BUTTON_GREEN = 1,
		BUTTON_YELLOW = 2,
		BUTTON_BLUE = 3,
		BUTTON_MAX   // MUST be always the last in the list
	} USER_BUTTON;
	typedef enum
	{
		ITEM_NONE = 0, // Do not change ordering of members, add new item just before ITEM_MAX!!!
		ITEM_BAR = 1,
		ITEM_EPG_LIST = 2,
		ITEM_EPG_SUPER = 3,
		ITEM_EPG_INFO = 4,
		ITEM_EPG_MISC = 5,
		ITEM_AUDIO_SELECT = 6,
		ITEM_SUBCHANNEL = 7,
		ITEM_RECORD = 8,
		ITEM_MOVIEPLAYER_MB = 9,
		ITEM_TIMERLIST = 10,
		ITEM_FAVORITS = 12,
		ITEM_VTXT = 11,
		ITEM_TECHINFO = 13,
		ITEM_REMOTE = 14,
		ITEM_PLUGIN_TYPES = 15,
		ITEM_IMAGEINFO = 16,
		ITEM_BOXINFO = 17,
		ITEM_CAM = 18,
		ITEM_CLOCK = 19,
		ITEM_GAMES = 20,
		ITEM_SCRIPTS = 21,
		ITEM_YOUTUBE = 22,
		ITEM_FILEPLAY = 23,
		ITEM_TOOLS = 24,
		ITEM_LUA = 25,

		ITEM_HDDMENU = 26,
		ITEM_AUDIOPLAY = 27,
		ITEM_INETPLAY = 28,

		ITEM_MAX   // MUST be always the last in the list
	} USER_ITEM;
	typedef struct {
		unsigned int key;
		std::string items;
		std::string title;
		std::string name;
	} usermenu_t;
	std::vector<usermenu_t *> usermenu;

	//progressbar arrangement for infobar
	typedef enum
	{
		INFOBAR_PROGRESSBAR_ARRANGEMENT_DEFAULT = 0,
		INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME = 1,
		INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME_SMALL = 2,
		INFOBAR_PROGRESSBAR_ARRANGEMENT_BETWEEN_EVENTS = 3	
	}INFOBAR_PROGRESSBAR_ARRANGEMENT_TYPES;

};

/* some default Values */

extern const struct personalize_settings_t personalize_settings[SNeutrinoSettings::P_SETTINGS_MAX];

typedef struct time_settings_t
{
	const int default_timing;
	const neutrino_locale_t name;
} time_settings_struct_t;

const time_settings_struct_t timing_setting[SNeutrinoSettings::TIMING_SETTING_COUNT] =
{
	{ 0,	LOCALE_TIMING_MENU        },
	{ 60,	LOCALE_TIMING_CHANLIST    },
	{ 240,	LOCALE_TIMING_EPG         },
	{ 6,	LOCALE_TIMING_INFOBAR     },
 	{ 0,	LOCALE_TIMING_INFOBAR_RADIO },
 	{ 6,	LOCALE_TIMING_INFOBAR_MOVIEPLAYER},
 	{ 3,	LOCALE_TIMING_VOLUMEBAR   },
	{ 60,	LOCALE_TIMING_FILEBROWSER },
	{ 3,	LOCALE_TIMING_NUMERICZAP  }
};

// lcdd
#define DEFAULT_VFD_BRIGHTNESS			15
#define DEFAULT_VFD_STANDBYBRIGHTNESS		5

#define DEFAULT_LCD_BRIGHTNESS			0xff
#define DEFAULT_LCD_STANDBYBRIGHTNESS		0xaa
#define DEFAULT_LCD_CONTRAST			0x0F
#define DEFAULT_LCD_POWER			0x01
#define DEFAULT_LCD_INVERSE			0x00
#define DEFAULT_LCD_AUTODIMM			0x00
#define DEFAULT_LCD_SHOW_VOLUME			0x01

#define CORNER_RADIUS_LARGE             11
#define CORNER_RADIUS_MID               7
#define CORNER_RADIUS_SMALL             5
#define CORNER_RADIUS_MIN	        3

#define RADIUS_LARGE    (g_settings.rounded_corners ? CORNER_RADIUS_LARGE : 0)
#define RADIUS_MID      (g_settings.rounded_corners ? CORNER_RADIUS_MID : 0)
#define RADIUS_SMALL    (g_settings.rounded_corners ? CORNER_RADIUS_SMALL : 0)
#define RADIUS_MIN      (g_settings.rounded_corners ? CORNER_RADIUS_MIN : 0)

// shadow
#define SHADOW_OFFSET                   6

/* end default values */


struct SglobalInfo
{
	unsigned char     box_Type;
	delivery_system_t delivery_system;
	bool has_fan;
};

const int RECORDING_OFF    = 0;
const int RECORDING_SERVER = 1;
const int RECORDING_VCR    = 2;
const int RECORDING_FILE   = 3;

const int PARENTALLOCK_PROMPT_NEVER          = 0;
const int PARENTALLOCK_PROMPT_ONSTART        = 1;
const int PARENTALLOCK_PROMPT_CHANGETOLOCKED = 2;
const int PARENTALLOCK_PROMPT_ONSIGNAL       = 3;


class CScanSettings
{
	public:
		CConfigFile	configfile;
		int		bouquetMode;
		int		scanType;

		int		scan_nit;
		int		scan_nit_manual;
		int		scan_bat;
		int		scan_fta_flag;
		int		scan_reset_numbers;
		int		scan_logical_numbers;
		int		scan_logical_hd;
		int		fast_type;
		int		fast_op;
		int		fst_version;
		int		fst_update;
		int		cable_nid;

		std::string	satName;
		int		sat_TP_fec;
		int		sat_TP_pol;
		std::string	sat_TP_freq;
		std::string	sat_TP_rate;
		int		sat_TP_delsys;
		int		sat_TP_mod;

		std::string	cableName;
		int		cable_TP_mod;
		int		cable_TP_fec;
		std::string	cable_TP_freq;
		std::string	cable_TP_rate;
		int		cable_TP_delsys;

		std::string	terrestrialName;
		std::string	terrestrial_TP_freq;
		int		terrestrial_TP_constel;
		int		terrestrial_TP_bw;
		int		terrestrial_TP_coderate_HP;
		int		terrestrial_TP_coderate_LP;
		int		terrestrial_TP_guard;
		int		terrestrial_TP_hierarchy;
		int		terrestrial_TP_transmit_mode;
		int		terrestrial_TP_delsys;

		CScanSettings();

		bool loadSettings(const char * const fileName);
		bool saveSettings(const char * const fileName);
};

#endif
