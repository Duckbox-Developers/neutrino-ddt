/*
	LCD-Daemon  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/


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

#include <driver/vfd.h>

#include <global.h>
#include <neutrino.h>
#include <system/settings.h>
#include <system/debug.h>
#include <driver/record.h>

#include <fcntl.h>
#include <sys/timeb.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <sys/stat.h> 

#include <daemonc/remotecontrol.h>
#include <system/helpers.h>
#include <zapit/debug.h>

#include <cs_api.h>
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

#ifdef HAVE_DUCKBOX_HARDWARE
#include <zapit/zapit.h>
#include <stropts.h>
#define VFD_DEVICE "/dev/vfd"

#define SCROLL_TIME 100000

bool invert = false;
char g_str[64];
bool blocked = false;
int blocked_counter = 0;
int file_vfd = -1;
bool active_icon[45] = { false };

pthread_t vfd_scrollText;

struct vfd_ioctl_data {
	unsigned char start;
	unsigned char data[64];
	unsigned char length;
};

/* ########## j00zek starts ########## */
char grcstype[96] = { };
static int VFDLENGTH = 16; //the standard value
static bool isAriva = false;
static bool isSpark7162 = false;
static bool isADB5800VFD = false;
static bool isADB5800LED = false;
static bool isESI88 = false;
static int  StandbyIconID = -1;
static int  RecIconID = -1;
static int  PowerOnIconID = -1;

void j00zek_get_vfd_config()
{
	//char buf[96] = { };
	int len = -1;
	int h0;
	int myFile = open("/var/grun/grcstype", O_RDONLY);
	if (myFile != -1) {
		len = read(myFile, grcstype, sizeof(grcstype) - 1);
		close(myFile);
	}
	if (len > 0) {
		dprintf(DEBUG_INFO,"[j00zek_get_vfd_config] grcstype content:\n%s\n", grcstype);
		grcstype[len] = 0;
		char *p = strstr(grcstype, "vfdsize=");
		if (p && sscanf(p, "vfdsize=%d", &h0) == 1)
			VFDLENGTH = h0;
		if  (strstr(grcstype, "rcstype=SPARK7162")) {
			isSpark7162 = true;
			RecIconID = 0x07;
			StandbyIconID = 0x24;
		}
		else if  (strstr(grcstype, "rcstype=ArivaLink200"))
			isAriva = true;
		else if  (strstr(grcstype, "rcstype=ESI88")) {
			isESI88 = true;
			RecIconID = 1;
			PowerOnIconID = 2;
		}
		else if  (strstr(grcstype, "rcstype=ADB5800") && VFDLENGTH == 16) {
			isADB5800VFD = true;
			StandbyIconID = 1;
			RecIconID = 3;
			PowerOnIconID = 2;
		}
		else if  (strstr(grcstype, "rcstype=ADB5800") && VFDLENGTH != 16)
		    isADB5800LED = true;
	}
	dprintf(DEBUG_INFO,"j00zek>%s:%s config: size=%d,isAriva=%d,isSpark7162=%d,isADB5800VFD=%d,isADB5800LED=,%d,isESI88=%d\n", "CVFD::", __func__, h0, isAriva, isSpark7162, isADB5800VFD, isADB5800LED,isESI88);
	return;
}

/*scan grcstype for specific option and return if exists
rcstype=SPARK7162	ArivaLink200	ESI88	ADB5800	
vfdsize=8		16		16	16	
platform=7109		1709		7105	7100	
cpu_divider=10		9		10	9	
boxtype=NONE		NONE		ESI88	BSLA|BZZB
*/
bool CVFD::hasConfigOption(char *str) 
{
	int len = strlen(str);
	dprintf(DEBUG_INFO,"hasConfigOption\n%s\n%s\n%d\n",grcstype,str,len);
	if (len > 0 && strstr(grcstype, str))
		    return true;
	return false;
}

static void write_to_vfd(unsigned int DevType, struct vfd_ioctl_data * data, bool force = false)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	int file_closed = 0;
	if (blocked) {
		if (file_vfd > -1) {
			blocked_counter++;
			usleep(SCROLL_TIME);
		} else {
			blocked = false;
		}
	}
	if (blocked_counter > 10) {
		force = true;
		blocked_counter = 0;
	}
//	printf("[CVFD] - blocked_counter=%i, blocked=%i, force=%i\n", blocked, blocked_counter, force);
	if (force || !blocked) {
		if (blocked) {
			if (file_vfd > -1) {
				file_closed = close(file_vfd);
				file_vfd = -1;
			}
		}
		blocked = true;
		if (file_vfd == -1)
			file_vfd = open (VFD_DEVICE, O_RDWR);
		if (file_vfd > -1) {
			//printf("[write_to_vfd] FLUSHING data to vfd\n");
			ioctl(file_vfd, DevType, data);
			ioctl(file_vfd, I_FLUSH, FLUSHRW);
			file_closed = close(file_vfd);
			file_vfd = -1;
		} else
			dprintf(DEBUG_INFO,"[write_to_vfd] Error opening VFD_DEVICE\n");
		blocked = false;
		blocked_counter = 0;
	}
}

void SetIcon(int icon, bool status)
{
	if (isAriva)
		return;
	if (active_icon[icon] == status)
		return;
	else
		active_icon[icon] = status;
	dprintf(DEBUG_DEBUG,"j00zek>CVFD::SetIcon(icon=%d, status=%d)\n", icon, status);
	if (isSpark7162){
		int myVFD = -1;
		struct {
			int icon_nr;
			int on;
		} vfd_icon;
		vfd_icon.icon_nr = icon;
		vfd_icon.on = status;
		if ( (myVFD = open ( "/dev/vfd", O_RDWR )) != -1 ) {
			ioctl(myVFD, VFDICONDISPLAYONOFF, &vfd_icon);
			close(myVFD); }
	} else {
		struct vfd_ioctl_data data;
		memset(&data, 0, sizeof(struct vfd_ioctl_data));
		data.start = 0x00;
		data.data[0] = icon;
		data.data[4] = status;
		data.length = 5;
		write_to_vfd(VFDICONDISPLAYONOFF, &data);
	}
	return;
}

/* ########## j00zek ends ########## */

static void ShowNormalText(char * str, bool fromScrollThread = false)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	if (VFDLENGTH == 0)
	{
		dprintf(DEBUG_INFO,"[CVFD] ShowNormalText:VFDLENGTH=0 exiting\n");
		return;
	}
	if (blocked)
	{
		dprintf(DEBUG_INFO,"[CVFD] - blocked\n");
		usleep(SCROLL_TIME);
	}

	struct vfd_ioctl_data data;

	if (!fromScrollThread)
	{
		if(vfd_scrollText != 0)
		{
			pthread_cancel(vfd_scrollText);
			pthread_join(vfd_scrollText, NULL);

			vfd_scrollText = 0;
		}
	}
	if ((strlen(str) > VFDLENGTH && !fromScrollThread) && (g_settings.lcd_vfd_scroll >= 1))
	{
		CVFD::getInstance()->ShowScrollText(str);
		return;
	}

		memset(data.data, ' ', 63);
	if (!fromScrollThread)
	{
		memcpy (data.data, str, VFDLENGTH);
		data.start = 0;
		if ((strlen(str) % 2) == 1 && VFDLENGTH > 8) // do not center on small displays
			data.length = VFDLENGTH-1;
		else
			data.length = VFDLENGTH;
	}
	else
	{
		memcpy ( data.data, str, VFDLENGTH);
		data.start = 0;
		data.length = VFDLENGTH;
	}
	write_to_vfd(VFDDISPLAYCHARS, &data);
	return;
}

void CVFD::ShowScrollText(char *str)
{
	dprintf(DEBUG_INFO,"CVFD::ShowScrollText: [%s]\n", str);

	if (blocked)
	{
		dprintf(DEBUG_INFO,"[CVFD] - blocked\n");
		usleep(SCROLL_TIME);
	}

	//stop scrolltextthread
	if(vfd_scrollText != 0)
	{
		pthread_cancel(vfd_scrollText);
		pthread_join(vfd_scrollText, NULL);

		vfd_scrollText = 0;
		scrollstr = (char *)"";
	}

	//scroll text thread
	scrollstr = str;
	pthread_create(&vfd_scrollText, NULL, ThreadScrollText, (void *)scrollstr);
}


void* CVFD::ThreadScrollText(void * arg)
{
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	int i;
	char *str = (char *)arg;
	int len = strlen(str);
	char out[VFDLENGTH+1];
	char buf[VFDLENGTH+65];

	memset(out, 0, VFDLENGTH+1);

	int retries = g_settings.lcd_vfd_scroll;

	if (len > VFDLENGTH)
	{
		printf("CVFD::ThreadScrollText: [%s], length %d\n", str, len);
		memset(buf, ' ', (len + VFDLENGTH));
		memcpy(buf, str, len);

		while(retries--)
		{
//			usleep(SCROLL_TIME);

			for (i = 0; i <= (len-1); i++)
			{
				// scroll text until end
				memcpy(out, buf+i, VFDLENGTH);
				ShowNormalText(out,true);
				usleep(SCROLL_TIME*2);
			}
		}
	}
	memcpy(out, str, VFDLENGTH); // display first VFDLENGTH chars after scrolling
	ShowNormalText(out,true);

	pthread_exit(0);

	return NULL;
}
#endif

CVFD::CVFD()
{
	has_lcd = true; //trigger for vfd setup
	if (isAriva)
		supports_brightness = false;
	else
		supports_brightness = true;

	if (VFDLENGTH == 4)
		support_text = false;
	else
		support_text = true;
	support_numbers	= true;

	text[0] = 0;
	g_str[0] = 0;
	clearClock = 0;
	mode = MODE_TVRADIO;
	TIMING_INFOBAR_counter = 0;
	timeout_cnt = 0;
	service_number = -1;
}

CVFD::~CVFD()
{
}

CVFD* CVFD::getInstance()
{
	static CVFD* lcdd = NULL;
	if(lcdd == NULL) {
		lcdd = new CVFD();
	}
	return lcdd;
}

void CVFD::count_down() {
	if (timeout_cnt > 0) {
		dprintf(DEBUG_DEBUG,"j00zek>CVFD:count_down timeout_cnt=%d\n",timeout_cnt);
		timeout_cnt--;
		if (timeout_cnt == 0 ) {
			if (g_settings.lcd_setting_dim_brightness > -1) {
				// save lcd brightness, setBrightness() changes global setting
				int b = g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS];
				setBrightness(g_settings.lcd_setting_dim_brightness);
				g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = b;
			}
		}
	}
	//j00zek: ???
	if (g_settings.lcd_info_line && TIMING_INFOBAR_counter > 0) {
		dprintf(DEBUG_DEBUG,"j00zek>CVFD:count_down g_settings.lcd_info_line && TIMING_INFOBAR_counter=%d\n",TIMING_INFOBAR_counter);
		TIMING_INFOBAR_counter--;
		if (TIMING_INFOBAR_counter == 0) {
			if (g_settings.lcd_setting_dim_brightness > -1) {
				CVFD::getInstance()->showTime(true);
			}
		}
	}
}

void CVFD::wake_up() {
	//dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	if(fd < 0) return;

	if (atoi(g_settings.lcd_setting_dim_time.c_str()) > 0) {
		timeout_cnt = atoi(g_settings.lcd_setting_dim_time.c_str());
		if (g_settings.lcd_setting_dim_brightness > -1)
			setBrightness(g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS]);
	}
	if(g_settings.lcd_info_line){
		TIMING_INFOBAR_counter = g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] + 10;
	}
}

void* CVFD::TimeThread(void *)
{
	bool RecVisible = true;
	while(1) {
		sleep(1);
		struct stat buf;
		if (stat("/tmp/vfd.locked", &buf) == -1) {
			CVFD::getInstance()->showTime();
			CVFD::getInstance()->count_down();
		} else
			CVFD::getInstance()->wake_up();
		
		/* hack, just if we missed the blit() somewhere
		 * this will update the framebuffer once per second */
		if (getenv("AUTOBLIT") != NULL) {
			CFrameBuffer *fb = CFrameBuffer::getInstance();
			/* plugin start locks the framebuffer... */
			if (!fb->Locked())
				fb->blit();
		}
		if (RecIconID >=0 && CNeutrinoApp::getInstance()->recordingstatus && CVFD::getInstance()->mode != MODE_STANDBY) {
			RecVisible = !RecVisible;
			SetIcon(RecIconID, RecVisible);
		}
	}
	return NULL;
}

void CVFD::init(const char * /*fontfile*/, const char * /*fontname*/)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	j00zek_get_vfd_config();

	brightness = -1;
	setMode(MODE_TVRADIO);

	if (pthread_create (&thrTime, NULL, TimeThread, NULL) != 0 ) {
		perror("[lcdd]: pthread_create(TimeThread)");
		return ;
	}
}

void CVFD::setlcdparameter(int dimm, const int power)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	if(fd < 0) return;

	if(dimm < 0)
		dimm = 0;
	else if(dimm > 15)
		dimm = 15;

	if(!power)
		dimm = 0;

	if(brightness == dimm)
		return;

	struct vfd_ioctl_data data;

	printf("CVFD::setlcdparameter dimm %d power %d\n", dimm, power);
// Brightness
	if (isADB5800VFD || isESI88 || isSpark7162) {
		if (isADB5800VFD || isSpark7162)
			brightness = (int)dimm/2;
		else if (isESI88)
			brightness = (int)dimm/3;
		
		memset(&data, 0, sizeof(struct vfd_ioctl_data));
		data.start = brightness & 0x07;
		data.length = 0;
		write_to_vfd(VFDBRIGHTNESS, &data);
	}
#if 0 //j00zek na sh4 nie chyba potrzebujemy kontrolowac power
// Power on/off
	if (power) {
		if (isADB5800VFD)
			data.start = 0x00;
		else
			data.start = 0x01;
	} else {
		if (isADB5800VFD)
			data.start = 0x01;
		else
			data.start = 0x00;
	}
	data.length = 0;
	write_to_vfd(VFDDISPLAYWRITEONOFF, &data, true);
#endif
}

void CVFD::setlcdparameter(void)
{
	if(fd < 0)
		return;
	last_toggle_state_power = g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER];
	dprintf(DEBUG_DEBUG,"j00zek>CVFD::setlcdparameter(void) last_toggle_state_power=%d\n",last_toggle_state_power);
	setlcdparameter((mode == MODE_STANDBY) ? g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] : (mode == MODE_SHUTDOWN) ? g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS] : g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS],
			last_toggle_state_power);
}

void CVFD::showServicename(const std::string & name, int number) // UTF-8
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	if(fd < 0) return;

	servicename = name;
	service_number = number;

	if (mode != MODE_TVRADIO) {
		dprintf(DEBUG_INFO,"CVFD::showServicename: not in MODE_TVRADIO\n");
		return;
	}
	//dprintf(DEBUG_INFO,"CVFD::showServicename: g_settings.lcd_info_line=%d\n",g_settings.lcd_info_line);
	if (support_text && g_settings.lcd_info_line != 2)
	{ 
		int aqq = name.length();
		if ( aqq<1) {
		    dprintf(DEBUG_INFO,"CVFD::showServicename: empty string, end.\n");
		    return;
		}
		ShowText(name.c_str());
	}
	else
		ShowNumber(service_number);
	wake_up();
}

void CVFD::showTime(bool force)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	//unsigned int system_rev = cs_get_revision();
	static int recstatus = 0;
	
	if(fd >= 0 && mode == MODE_SHUTDOWN) {
		if (RecIconID>=0) SetIcon(RecIconID, false);
		return;
	}
	if (fd >= 0 && showclock) {
		if ( (mode == MODE_STANDBY && !isSpark7162) || ( g_settings.lcd_info_line == 1 && (MODE_TVRADIO == mode))) {
			char timestr[21] = {0};
			struct timeb tm;
			struct tm * t;
			static int hour = 0, minute = 0;

			ftime(&tm);
			t = localtime(&tm.time);
			if(force || ( TIMING_INFOBAR_counter == 0 && ((hour != t->tm_hour) || (minute != t->tm_min))) ) {
				hour = t->tm_hour;
				minute = t->tm_min;
				if (VFDLENGTH==4)
				    strftime(timestr, 5, "%H%M", t);
				else
				    strftime(timestr, 6, "%H:%M", t);
				ShowText(timestr);
			}
		}
	}

	int tmp_recstatus = CNeutrinoApp::getInstance()->recordingstatus;
	if (tmp_recstatus) {
		if(clearClock) {
			clearClock = 0;
			if (RecIconID>=0) SetIcon(RecIconID, false);
		} else {
			clearClock = 1;
			if (RecIconID>=0) SetIcon(RecIconID, false);
		}
	} else if(clearClock || (recstatus != tmp_recstatus)) { // in case icon ON after record stopped
		clearClock = 0;
		if (RecIconID>=0) SetIcon(RecIconID, false);

	}

	recstatus = tmp_recstatus;
}

void CVFD::UpdateIcons()
{
	dprintf(DEBUG_DEBUG,"j00zek>CVFD::UpdateIcons() - DISABLED DD/AC3/HD icons\n"); //j00zek> we do NOT display those small, almost invisible, icons. :)
#if 0
	CZapitChannel * chan = CZapit::getInstance()->GetCurrentChannel();
	if (chan) {
		ShowIcon(FP_ICON_HD,chan->isHD());
		ShowIcon(FP_ICON_LOCK,!chan->camap.empty());
		if (chan->getAudioChannel() != NULL)
			ShowIcon(FP_ICON_DD, chan->getAudioChannel()->audioChannelType == CZapitAudioChannel::AC3);
	}
#endif
}

void CVFD::showRCLock(int duration)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	if (!g_settings.lcd_notify_rclock) {
		sleep(duration);
		return;
	}

	std::string _text = text;
	ShowText(g_Locale->getText(LOCALE_RCLOCK_LOCKED));
	sleep(duration);
	ShowText(_text.c_str());
}

void CVFD::showVolume(const char vol, const bool force_update)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	static int oldpp = 0;

	ShowIcon(FP_ICON_MUTE, muted);

	if(!force_update && vol == volume)
		return;
	volume = vol;

	bool allowed_mode = (mode == MODE_TVRADIO || mode == MODE_AUDIO || mode == MODE_MENU_UTF8);
	if (!allowed_mode)
		return;

	if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] == 1) {
		wake_up();
		int pp = (int) round((double) vol / (double) 2);
		if(oldpp != pp)
		{
			char vol_chr[64] = "";
			if (VFDLENGTH==4)
				snprintf(vol_chr, sizeof(vol_chr)-1, "v%3d", (int)vol);
			else if (VFDLENGTH==8)
				snprintf(vol_chr, sizeof(vol_chr)-1, "VOL %d%%", (int)vol);
			else
				snprintf(vol_chr, sizeof(vol_chr)-1, "Volume: %d%%", (int)vol);
			ShowText(vol_chr);
			oldpp = pp;
		}
	}
}

void CVFD::showPercentOver(const unsigned char perc, const bool /*perform_update*/, const MODES origin)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
}

void CVFD::showMenuText(const int position, const char * ptext, const int /*highlight*/, const bool /*utf_encoded*/)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	if(fd < 0) return;
	if (mode != MODE_MENU_UTF8)
		return;

	ShowText(ptext);
	wake_up();
}

void CVFD::showAudioTrack(const std::string & /*artist*/, const std::string & title, const std::string & /*album*/)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	if(fd < 0) return;
	if (mode != MODE_AUDIO)
		return;
printf("CVFD::showAudioTrack: %s\n", title.c_str());
	ShowText(title.c_str());
	wake_up();
}

void CVFD::showAudioPlayMode(AUDIOMODES m)
{
	dprintf(DEBUG_DEBUG,"j00zek>DISABLED CVFD::%s\n", __func__);
#if 0
	if(fd < 0) return;
	if (mode != MODE_AUDIO)
		return;
	if (isAriva || isADB5800LED)
		return;
	
	switch(m) {
		case AUDIO_MODE_PLAY:
			ShowIcon(FP_ICON_PLAY, true);
			ShowIcon(FP_ICON_PAUSE, false);
			ShowIcon(FP_ICON_FF, false);
			ShowIcon(FP_ICON_FR, false);
			break;
		case AUDIO_MODE_STOP:
			ShowIcon(FP_ICON_PLAY, false);
			ShowIcon(FP_ICON_PAUSE, false);
			ShowIcon(FP_ICON_FF, false);
			ShowIcon(FP_ICON_FR, false);
			break;
		case AUDIO_MODE_PAUSE:
			ShowIcon(FP_ICON_PLAY, false);
			ShowIcon(FP_ICON_PAUSE, true);
			ShowIcon(FP_ICON_FF, false);
			ShowIcon(FP_ICON_FR, false);
			break;
		case AUDIO_MODE_FF:
			ShowIcon(FP_ICON_FF, true);
			ShowIcon(FP_ICON_FR, false);
			break;
		case AUDIO_MODE_REV:
			ShowIcon(FP_ICON_FF, false);
			ShowIcon(FP_ICON_FR, true);
			break;
	}
	wake_up();
#endif
	return;
}

void CVFD::showAudioProgress(const unsigned char perc)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	if(fd < 0) return;
	if (mode != MODE_AUDIO)
		return;

	showPercentOver(perc, true, MODE_AUDIO);
}

void CVFD::setMode(const MODES m, const char * const title)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	if(fd < 0) return;

	// Clear colon in display if it is still there

	if(strlen(title))
		ShowText(title);
	mode = m;
	setlcdparameter();

	switch (m) {
	case MODE_TVRADIO:
		if (StandbyIconID >=0) SetIcon(StandbyIconID, false);
		if (PowerOnIconID >=0) SetIcon(PowerOnIconID, true);
		if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] == 1) {
			showVolume(volume, false);
			break;
		}
		showServicename(servicename);
		showclock = true;
		if(g_settings.lcd_info_line)
			TIMING_INFOBAR_counter = g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] + 10;
		break;
	case MODE_AUDIO:
	{
		showAudioPlayMode(AUDIO_MODE_STOP);
		showVolume(volume, false);
		showclock = true;
		//showTime();      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
		break;
	}
	case MODE_SCART:
		showVolume(volume, false);
		showclock = true;
		//showTime();      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
		break;
	case MODE_MENU_UTF8:
		showclock = false;
		//fonts.menutitle->RenderString(0,28, 140, title, CLCDDisplay::PIXEL_ON);
		break;
	case MODE_SHUTDOWN:
		showclock = false;
		Clear();
		break;
	case MODE_STANDBY:
		ClearIcons();
		if (StandbyIconID >=0) SetIcon(StandbyIconID, true);
		showclock = true;
		showTime(true);      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
		                 /* "showTime()" clears the whole lcd in MODE_STANDBY                         */
		break;
	}
	wake_up();
}

void CVFD::setBrightness(int bright)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);

	g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = bright;
	setlcdparameter();
}

int CVFD::getBrightness()
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	//FIXME for old neutrino.conf
	if(g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] > 15)
		g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = 15;

	return g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS];
}

void CVFD::setBrightnessStandby(int bright)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);

	g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = bright;
	setlcdparameter();
}

int CVFD::getBrightnessStandby()
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	//FIXME for old neutrino.conf
	if(g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] > 15)
		g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = 15;

	return g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS];
}

void CVFD::setBrightnessDeepStandby(int bright)
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);

	g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS] = bright;
	setlcdparameter();
}

int CVFD::getBrightnessDeepStandby()
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	//FIXME for old neutrino.conf
	if(g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS] > 15)
		g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS] = 15;

	return g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS];
}

int CVFD::getPower()
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	return g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER];
}

void CVFD::togglePower(void)
{
	dprintf(DEBUG_DEBUG,"j00zek>CVFD::togglePower(void) - DISABLED\n");
#if 0
	if(fd < 0) return;

	last_toggle_state_power = 1 - last_toggle_state_power;
	setlcdparameter((mode == MODE_STANDBY) ? g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] : (mode == MODE_SHUTDOWN) ? g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS] : g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS],
			last_toggle_state_power);
#endif
}

void CVFD::setMuted(bool mu)
{
	dprintf(DEBUG_DEBUG,"j00zek>CVFD::setMuted(bool mu=)\n", mu);
	muted = mu;
	showVolume(volume);
}

void CVFD::resume()
{
	dprintf(DEBUG_DEBUG,"j00zek>CVFD::resume() - DISABLED\n");
}

void CVFD::pause()
{
	dprintf(DEBUG_DEBUG,"j00zek>CVFD::pause() - DISABLED\n");
}

void CVFD::Lock()
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	if(fd < 0) return;
	creat("/tmp/vfd.locked", 0);
}

void CVFD::Unlock()
{
	dprintf(DEBUG_DEBUG,"j00zek>%s:%s >>>\n", "CVFD::", __func__);
	if(fd < 0) return;
	unlink("/tmp/vfd.locked");
}

void CVFD::Clear()
{
	dprintf(DEBUG_DEBUG,"j00zek>CVFD::Clear()\n");
	if(fd < 0) return;
	if (VFDLENGTH == 4)
	  ShowText("    ");
	else if (VFDLENGTH == 8)
	  ShowText("        ");
	else
	  ShowText("                ");
	ClearIcons();
}

void CVFD::ShowIcon(fp_icon icon, bool show)
{
	dprintf(DEBUG_DEBUG,"j00zek>CVFD::ShowIcon(fp_icon icon=%d, bool show=%d)\n", icon, show);
	if (isAriva)
		return;
	else if (icon == 0)
		return;

	if (active_icon[icon & 0x0F] == show)
		return;
	else
		active_icon[icon & 0x0F] = show;

	//printf("CVFD::ShowIcon %s %x\n", show ? "show" : "hide", (int) icon);
	struct vfd_ioctl_data data;
	memset(&data, 0, sizeof(struct vfd_ioctl_data));
	data.start = 0x00;
	data.data[0] = icon;
	data.data[4] = show;
	data.length = 5;
	write_to_vfd(VFDICONDISPLAYONOFF, &data);
	return;
}

void CVFD::ClearIcons()
{
	dprintf(DEBUG_DEBUG,"j00zek>CVFD::ClearIcons()\n");
	if (isSpark7162) {
		for (int id = 1; id <= 44; id++)
			SetIcon(id, false);
	} else if (isAriva)
		return;
	else if (isADB5800VFD) {
		for (int id = 1; id <= 5; id++)
			SetIcon(id, false);
	}
	else {
		for (int id = 1; id <= 4; id++)
			SetIcon(id, false);
	}
	return;
}

void CVFD::ShowText(const char * str )
{
	//dprintf(DEBUG_DEBUG,"CVFD::ShowText(const char * str='%s' )\n",str);
	memset(g_str, 0, sizeof(g_str));
	memcpy(g_str, str, sizeof(g_str)-1);

	int i = strlen(str); //j00zek- don't know why, but this returns stupid values
	//dprintf(DEBUG_DEBUG,"strlen(str)=%d, g_str=%d\n",i);
	if (i > 63) {
		g_str[60] = '.';
		g_str[61] = '.';
		g_str[62] = '.';
		g_str[63] = '\0';
		i = 63;
	} /*else if (i < VFDLENGTH && !isAriva && !isSpark7162) { //workarround for poor vfd drivers
		while (i < VFDLENGTH) {
			g_str[i] = ' ';
			i += 1;
		}
	} 
	dprintf(DEBUG_DEBUG,"strlen(str)=$d\n",str);*/
	ShowNormalText(g_str, false);
}

void CVFD::ShowNumber(int number)
{
	//dprintf(DEBUG_DEBUG,"j00zek>CVFD::ShowNumber(int number)=%d\n", number);
	if (fd < 0 || (!support_text && !support_numbers))
		return;

	if (number < 0)
		return;
	
	char number_str[4];
	int retval;
	retval = snprintf(number_str, 4, "%03d", number);
	//dprintf(DEBUG_INFO,"CVFD::ShowNumber: channel number %d will be displayed as '%s'\n", number, number_str);
	ShowText(number_str);
}
