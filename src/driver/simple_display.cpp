/*
	Routines to drive simple one-line text or SPARK's 4 digit LED display

	(C) 2012 Stefan Seyfried

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
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <driver/lcdd.h>
#include <driver/framebuffer.h>

#include <global.h>
#include <neutrino.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
//#include <math.h>
#include <sys/stat.h>
#if HAVE_SPARK_HARDWARE
#include <aotom_main.h>
#define DISPLAY_DEV "/dev/vfd"
#endif
#if HAVE_AZBOX_HARDWARE
#define DISPLAY_DEV "/proc/vfd"
#define LED_DEV "/proc/led"
#endif
#if HAVE_GENERIC_HARDWARE
#define DISPLAY_DEV "/dev/null"
#endif
#ifdef BOXMODEL_SPARK7162
#include <zapit/zapit.h>
#include <system/helpers.h>
static bool usb_icon = false;
static bool timer_icon = false;
#endif

static char volume = 0;
//static char percent = 0;
static bool power = true;
static bool muted = false;
static bool showclock = true;
static time_t last_display = 0;
static char display_text[64] = { 0 };
static bool led_r = false;
static bool led_g = false;
static bool upd_display = false;
static bool vol_active = false;

static inline int dev_open()
{
	int fd = open(DISPLAY_DEV, O_RDWR);
	if (fd < 0)
		fprintf(stderr, "[neutrino] simple_display: open " DISPLAY_DEV ": %m\n");
	return fd;
}

#if HAVE_AZBOX_HARDWARE
static inline int led_open()
{
	int fd = open(LED_DEV, O_RDWR);
	if (fd < 0)
		fprintf(stderr, "[neutrino] simple_display: open " LED_DEV ": %m\n");
	return fd;
}
#endif

static void replace_umlauts(std::string &s)
{
	/* this is crude, it just replaces ÄÖÜ with AOU since the display can't show them anyway */
	/*                       Ä           ä           Ö           ö           Ü           ü   */
	char tofind[][3] = { "\xc3\x84", "\xc3\xa4", "\xc3\x96", "\xc3\xb6", "\xc3\x9c", "\xc3\xbc" };
	char toreplace[] = { "AaOoUu" };
	char repl[2];
	repl[1] = '\0';
	int i = 0;
	size_t pos;
	// print("%s:>> '%s'\n", __func__, s.c_str());
	while (toreplace[i] != 0x0) {
		pos = s.find(tofind[i]);
		if (pos == std::string::npos) {
			i++;
			continue;
		}
		repl[0] = toreplace[i];
		s.replace(pos, 2, std::string(repl));
	}
	// printf("%s:<< '%s'\n", __func__, s.c_str());
}

static void display(const char *s, bool update_timestamp = true)
{
	int fd = dev_open();
	int len = strlen(s);
	if (fd < 0)
		return;
printf("%s '%s'\n", __func__, s);
	write(fd, s, len);
	close(fd);
	if (update_timestamp)
	{
		last_display = time(NULL);
		/* increase timeout to ensure that everything is displayed
		 * the driver displays 5 characters per second */
		if (len > g_info.hw_caps->display_xres)
			last_display += (len - g_info.hw_caps->display_xres) / 5;
	}
}

CLCD::CLCD()
{
	/* do not show menu in neutrino...,at spark7162 true, because there is th GLCD Menu */
#ifdef BOXMODEL_SPARK7162
	has_lcd = true;
#else
	has_lcd = false;
#endif
	servicename = "";
	thread_running = false;
}

CLCD::~CLCD()
{
	if (thread_running)
	{
		thread_running = false;
		pthread_cancel(thrTime);
		pthread_join(thrTime, NULL);
	}
}

CLCD* CLCD::getInstance()
{
	static CLCD* lcdd = NULL;
	if (lcdd == NULL)
		lcdd = new CLCD();
	return lcdd;
}

void CLCD::wake_up()
{
}

void* CLCD::TimeThread(void *)
{
	while (CLCD::getInstance()->thread_running) {
		sleep(1);
		CLCD::getInstance()->showTime();
		/* hack, just if we missed the blit() somewhere
		 * this will update the framebuffer once per second */
		if (getenv("SPARK_NOBLIT") == NULL) {
			CFrameBuffer *fb = CFrameBuffer::getInstance();
			/* plugin start locks the framebuffer... */
			if (!fb->Locked())
				fb->blit();
		}
	}
	return NULL;
}

void CLCD::init(const char *, const char *, const char *, const char *, const char *, const char *)
{
	setMode(MODE_TVRADIO);
	thread_running = true;
	if (pthread_create (&thrTime, NULL, TimeThread, NULL) != 0 ) {
		perror("[neutino] CLCD::init pthread_create(TimeThread)");
		thread_running = false;
		return ;
	}
}

void CLCD::setlcdparameter(void)
{
}

void CLCD::showServicename(std::string name, bool)
{
	if (g_info.hw_caps->display_type == HW_DISPLAY_LED_NUM)
		return;
	servicename = name;
	if (mode != MODE_TVRADIO && mode != MODE_AUDIO)
		return;
	replace_umlauts(name);
	strncpy(display_text, name.c_str(), sizeof(display_text) - 1);
	display_text[sizeof(display_text) - 1] = '\0';
	upd_display = true;
}

#if HAVE_SPARK_HARDWARE
void CLCD::setled(int red, int green)
{
	struct aotom_ioctl_data d;
	int leds[2] = { red, green };
	int i;
	int fd = dev_open();
	if (fd < 0)
		return;

printf("%s red:%d green:%d\n", __func__, red, green);

	for (i = 0; i < 2; i++)
	{
		if (leds[i] == -1)
			continue;
		d.u.led.led_nr = i;
		d.u.led.on = leds[i];
		if (ioctl(fd, VFDSETLED, &d) < 0)
			fprintf(stderr, "[neutrino] %s setled VFDSETLED: %m\n", __func__);
	}
	close(fd);
}
#elif HAVE_AZBOX_HARDWARE
void CLCD::setled(int red, int green)
{
	static unsigned char col = '0'; /* need to remember the state. 1 == blue, 2 == red */
	int leds[3] = { -1, green, red };
	int i;
	char s[3];
	int fd = led_open();
	if (fd < 0)
		return;
	for (i = 1; i <= 2; i++)
	{
		if (leds[i] == -1)	/* don't touch */
			continue;
		col &= ~(i);		/* clear the bit... */
		if (leds[i])
			col |= i;	/* ...and set it again */
	}
	sprintf(s, "%c\n", col);
	write(fd, s, 3);
	close(fd);
	//printf("%s(%d, %d): %c\n", __func__, red, green, col);
}
#else
void CLCD::setled(int /*red*/, int /*green*/)
{
}
#endif

void CLCD::showTime(bool force)
{
	static bool blink = false;
	int red = -1, green = -1;

	if (mode == MODE_SHUTDOWN)
	{
		setled(1, 1);
		return;
	}

	time_t now = time(NULL);
	if (upd_display)
	{
		display(display_text);
		upd_display = false;
	}
	else if (power && (force || (showclock && (now - last_display) > 4)))
	{
		char timestr[64]; /* todo: change if we have a simple display with 63+ chars ;) */
		struct tm *t;
		static int hour = 0, minute = 0;

		t = localtime(&now);
		if (force || last_display || (hour != t->tm_hour) || (minute != t->tm_min)) {
			hour = t->tm_hour;
			minute = t->tm_min;
			int ret = -1;
#if HAVE_SPARK_HARDWARE
			now += t->tm_gmtoff;
			int fd = dev_open();
#if 0 /* VFDSETTIME is broken and too complicated anyway -> use VFDSETTIME2 */
			int mjd = 40587 + now  / 86400; /* 1970-01-01 is mjd 40587 */
			struct aotom_ioctl_data d;
			d.u.time.time[0] = mjd >> 8;
			d.u.time.time[1] = mjd & 0xff;
			d.u.time.time[2] = hour;
			d.u.time.time[3] = minute;
			d.u.time.time[4] = t->tm_sec;
			int ret = ioctl(fd, VFDSETTIME, &d);
#else
			ret = ioctl(fd, VFDSETTIME2, &now);
#endif
			close(fd);
#endif
			if (ret < 0 && servicename.empty())
			{
				if (g_info.hw_caps->display_xres < 5)
					sprintf(timestr, "%02d%02d", hour, minute);
				else	/* pad with spaces on the left side to center the time string */
					sprintf(timestr, "%*s%02d:%02d",(g_info.hw_caps->display_xres - 5)/2, "", hour, minute);
				display(timestr, false);
			}
			else
			{
				if (vol_active)
					showServicename(servicename);
				vol_active = false;
			}
			last_display = 0;
		}
	}

	if (led_r)
		red = blink;
	blink = !blink;
	if (led_g)
		green = blink;
#ifdef BOXMODEL_SPARK7162
	if (led_r)
		SetIcons(SPARK_REC1, red);
	if (led_g)
		SetIcons(SPARK_PLAY, green);
#else
	if (led_r || led_g)
		setled(red, green);
#endif
}

void CLCD::showRCLock(int)
{
}

/* update is default true, the mute code sets it to false
 * to force an update => inverted logic! */
void CLCD::showVolume(const char vol, const bool update)
{
	char s[32];
	const int type = (g_info.hw_caps->display_xres < 5);
	const char *vol_fmt[] = { "Vol:%3d%%", "%4d" };
	const char *mutestr[] = { "Vol:MUTE", "mute" };
	if (vol == volume && update)
		return;
	volume = vol;
	/* char is unsigned, so vol is never < 0 */
	if (volume > 100)
		volume = 100;

	if (muted)
	{
#ifdef BOXMODEL_SPARK7162
		SetIcons(SPARK_MUTE, 1);
#endif
		strcpy(s, mutestr[type]);
	} else {
#ifdef BOXMODEL_SPARK7162
		SetIcons(SPARK_MUTE, 0);
#endif
		sprintf(s, vol_fmt[type], volume);
	}
	display(s);
	vol_active = true;
}

void CLCD::showPercentOver(const unsigned char /*perc*/, const bool /*perform_update*/, const MODES)
{
}

void CLCD::showMenuText(const int, const char *text, const int, const bool)
{
	if (mode != MODE_MENU_UTF8)
		return;
	std::string tmp = text;
	replace_umlauts(tmp);
	strncpy(display_text, tmp.c_str(), sizeof(display_text) - 1);
	display_text[sizeof(display_text) - 1] = '\0';
	upd_display = true;
}

void CLCD::showAudioTrack(const std::string &, const std::string & title, const std::string &)
{
	if (mode != MODE_AUDIO)
		return;
	ShowText(title.c_str());
}

void CLCD::showAudioPlayMode(AUDIOMODES)
{
}

void CLCD::showAudioProgress(const char, bool)
{
}

void CLCD::setMode(const MODES m, const char * const)
{
	mode = m;

	switch (m) {
	case MODE_TVRADIO:
#ifdef BOXMODEL_SPARK7162
		SetIcons(SPARK_CYCLE, 0);
#else
		setled(0, 0);
#endif
		showclock = true;
		power = true;
		if (g_info.hw_caps->display_type != HW_DISPLAY_LED_NUM) {
			strncpy(display_text, servicename.c_str(), sizeof(display_text) - 1);
			display_text[sizeof(display_text) - 1] = '\0';
			upd_display = true;
		}
		showTime();
		break;
	case MODE_SHUTDOWN:
		showclock = false;
		Clear();
		break;
	case MODE_STANDBY:
#ifdef BOXMODEL_SPARK7162
		SetIcons(SPARK_CYCLE, 1);
#else
		setled(0, 1);
#endif
		showclock = true;
		showTime(true);
		break;
	default:
		showclock = true;
		showTime();
	}
}

void CLCD::setBrightness(int)
{
}

int CLCD::getBrightness()
{
	return 0;
}

void CLCD::setBrightnessStandby(int)
{
}

int CLCD::getBrightnessStandby()
{
	return 0;
}

void CLCD::setPower(int)
{
}

int CLCD::getPower()
{
	return 0;
}

void CLCD::togglePower(void)
{
	power = !power;
	if (!power)
		Clear();
	else
		showTime(true);
}

void CLCD::setMuted(bool mu)
{
printf("spark_led:%s %d\n", __func__, mu);
	muted = mu;
	showVolume(volume, false);
}

void CLCD::resume()
{
}

void CLCD::pause()
{
}

void CLCD::Lock()
{
}

void CLCD::Unlock()
{
}

#if HAVE_SPARK_HARDWARE
void CLCD::Clear()
{
	int fd = dev_open();
	if (fd < 0)
		return;
	int ret = ioctl(fd, VFDDISPLAYCLR);
	if(ret < 0)
		perror("[neutrino] spark_led Clear() VFDDISPLAYCLR");
	close(fd);
#ifdef BOXMODEL_SPARK7162
	SetIcons(SPARK_ALL, false);
	SetIcons(SPARK_CLOCK, timer_icon);
#endif
	servicename.clear();
printf("spark_led:%s\n", __func__);
}
#else
void CLCD::Clear()
{
	display(" ", false);
}
#endif

#ifdef BOXMODEL_SPARK7162
void CLCD::SetIcons(int icon, bool on)
{
	struct aotom_ioctl_data d;
	d.u.icon.icon_nr = icon;
	if (on == true)
		d.u.icon.on = 1;
	else
		d.u.icon.on = 0;
	int fd = dev_open();
	if (fd < 0)
		return;
	if (ioctl(fd, VFDICONDISPLAYONOFF, &d) <0)
		perror("[neutrino] SetIcons() VFDICONDISPLAYONOFF");
	close(fd);
}
void CLCD::ShowDiskLevel()
{
	int hdd_icons[9] ={24, 23, 21, 20, 19, 18, 17, 16, 22};
	int percent, digits, i, j;
	uint64_t t, u;
	if (get_fs_usage(g_settings.network_nfs_recordingdir.c_str(), t, u))
	{
		SetIcons(SPARK_HDD, true);
		percent = (int)((u * 1000ULL) / t + 60); 
		digits = percent / 125;
		if (percent > 1050)
			digits = 9;
		//printf("HDD Fuell = %d Digits = %d\n", percent, digits);
		if (digits > 0)
		{
			for (i=0; i<digits; i++)
				SetIcons(hdd_icons[i], true);
						
			for (j=i; j < 9; j++)
				SetIcons(hdd_icons[j], false);
		}
	}
	else
	{
		SetIcons(SPARK_HDD, false);

	}
}
void CLCD::UpdateIcons()
{
	CFrontend *aktFE = CFEManager::getInstance()->getLiveFE();
	SetIcons(SPARK_SAT, aktFE->isSat(aktFE->getCurrentDeliverySystem()));
	SetIcons(SPARK_CAB, aktFE->isCable(aktFE->getCurrentDeliverySystem()));
	SetIcons(SPARK_TER, aktFE->isTerr(aktFE->getCurrentDeliverySystem()));

	ShowDiskLevel();
	SetIcons(SPARK_USB, usb_icon);

	CZapitChannel * chan = CZapit::getInstance()->GetCurrentChannel();
	if (chan)
	{
		ShowIcon(FP_ICON_HD,chan->isHD());
		ShowIcon(FP_ICON_LOCK,!chan->camap.empty());
		if (chan->getAudioChannel() != NULL)
		{
			ShowIcon(FP_ICON_DD, chan->getAudioChannel()->audioChannelType == CZapitAudioChannel::AC3);
			SetIcons(SPARK_MP3, chan->getAudioChannel()->audioChannelType == CZapitAudioChannel::MPEG);
		}
	}
}
#endif

void CLCD::ShowIcon(fp_icon i, bool on)
{
	switch (i)
	{
		case FP_ICON_CAM1:
			led_r = on;
#ifdef BOXMODEL_SPARK7162
			SetIcons(SPARK_REC1, on);
#else
			setled(led_r, -1); /* switch instant on / switch off if disabling */
#endif
			break;
		case FP_ICON_PLAY:
			led_g = on;
#ifdef BOXMODEL_SPARK7162
			SetIcons(SPARK_PLAY, on);
#else
			setled(-1, led_g);
#endif
			break;
#ifdef BOXMODEL_SPARK7162
		case FP_ICON_USB:
			usb_icon = on;
			SetIcons(SPARK_USB, on);
			break;
		case FP_ICON_HDD:
			SetIcons(SPARK_HDD, on);
			break;
		case FP_ICON_PAUSE:
			SetIcons(SPARK_PAUSE, on);
			break;
		case FP_ICON_FF:
			SetIcons(SPARK_PLAY_FASTFORWARD, on);
			break;
		case FP_ICON_FR:
			SetIcons(SPARK_PLAY_FASTBACKWARD, on);
			break;
		case FP_ICON_DD:
			SetIcons(SPARK_DD, on);
			SetIcons(SPARK_AC3, on);
			break;
		case FP_ICON_LOCK:
			SetIcons(SPARK_CA, on);
			break;
		case FP_ICON_RADIO:
			SetIcons(SPARK_AUDIO, on);
			break;
		case FP_ICON_TV:
			SetIcons(SPARK_TVMODE_LOG, on);
			break;
		case FP_ICON_HD:
			SetIcons(SPARK_DOUBLESCREEN, on);
			break;
		case FP_ICON_CLOCK:
			timer_icon = on;
			SetIcons(SPARK_CLOCK, on);
			break;
#endif
		default:
			break;
	}
}

void CLCD::setEPGTitle(const std::string)
{
}

