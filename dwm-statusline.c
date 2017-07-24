#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include <alsa/asoundlib.h>
#include <alsa/control.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/randr.h>

#define TIME_FORMAT "%m/%d.%H:%M:%S"
#define BATTERY_ID "BAT0"
#define CPUTEMP_PATH "hwmon1/temp1_input"
#define BRIGHTNESS_ID "intel_backlight"
#define ALSA_SPEAKER "Master",0

/* --- xcb --- */
static xcb_connection_t *m_conn = NULL;
static xcb_window_t m_root = {0};

/* --- nanosleep --- */
static struct timespec m_interval = {0, 500 * 1000000};

/* --- sysfs --- */
static void
read_sysfs_string(const char *path, char *ptr, size_t len)
{
	FILE *fp;
	size_t strlen = len - 1;

	memset(ptr, '\0', len);

	fp = fopen(path, "r");
	if (fread(ptr, strlen, 1, fp) < strlen && ferror(fp))
		perror("fread");
	fclose(fp);
}

static int
get_cpu_temperture()
{
	char buf[8];
	read_sysfs_string("/sys/class/hwmon/" CPUTEMP_PATH,
			buf, sizeof(buf));
	return atoi(buf) / 1000;
}

static int
is_battery_charging()
{
	char buf[9];
	read_sysfs_string("/sys/class/power_supply/" BATTERY_ID "/status",
			buf, sizeof(buf));
	return !strncmp("Charging", buf, sizeof(buf));
}

static double
get_battery_dischargingrate()
{
	char buf[9];
	long vol, cur;

	read_sysfs_string("/sys/class/power_supply/" BATTERY_ID "/voltage_now",
			buf, sizeof(buf));
	vol = atol(buf);

	read_sysfs_string("/sys/class/power_supply/" BATTERY_ID "/current_now",
			buf, sizeof(buf));
	cur = atol(buf);

	return (cur / 1000000.0) * (vol / 1000000.0);
}

static int
get_battery_remaining()
{
	char buf[9];
	int full, now;

	read_sysfs_string("/sys/class/power_supply/" BATTERY_ID "/charge_full",
			buf, sizeof(buf));
	full = atoi(buf);

	read_sysfs_string("/sys/class/power_supply/" BATTERY_ID "/charge_now",
			buf, sizeof(buf));
	now = atoi(buf);

	return now * 100 / full;
}


/* --- alsa --- */
static int
is_volume_mute(snd_mixer_t *handle, const char *name, int index)
{
	int ret = -1;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_name(sid, name);
	snd_mixer_selem_id_set_index(sid, index);

	elem = snd_mixer_find_selem(handle, sid);
	if (elem)
		snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_MONO, &ret);

	return ret;
}

static int
get_audio_volume(snd_mixer_t *handle, const char *name, const int index)
{
	long min, max, current, range;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_name(sid, name);
	snd_mixer_selem_id_set_index(sid, index);

	elem = snd_mixer_find_selem(handle, sid);
	if (!elem)
		return -1;

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &current);

	range = max - min;
	if (!range)
		return 0;

	current -= min;
	return (int)(current * 100 / range);
}

static int
get_brightness()
{
	char buf[8];
	int full, now;

	read_sysfs_string("/sys/class/backlight/" BRIGHTNESS_ID "/max_brightness",
			buf, sizeof(buf));
	full = atoi(buf);

	read_sysfs_string("/sys/class/backlight/" BRIGHTNESS_ID "/actual_brightness",
			buf, sizeof(buf));
	now = atoi(buf);

	return now * 100 / full;
}

static char*
mkvolume(char *ptr, size_t ptrlen)
{
	char *retptr = NULL;
	snd_mixer_t *handle;

	if (snd_mixer_open(&handle, 0) >= 0
		&& snd_mixer_attach(handle, "default") >= 0
		&& snd_mixer_selem_register(handle, NULL, NULL) >= 0
		&& !(snd_mixer_load(handle) >= 0))
		goto finalize;

	snprintf(ptr, ptrlen,
			"%s%d%%",
			is_volume_mute(handle, ALSA_SPEAKER) ? "o" : "x",
			get_audio_volume(handle, ALSA_SPEAKER));

	retptr = ptr;

finalize:
	if (handle)
		snd_mixer_close(handle);

	return retptr;
}


static int
initialize_xcb_service()
{
	int nbr, ret = 0;

	m_conn = xcb_connect(NULL, &nbr);
	if ((ret = xcb_connection_has_error(m_conn))) {
		fprintf(stderr, "xcb_connection failed.\n");
		return ret;
	}

	xcb_screen_iterator_t iter;
	iter = xcb_setup_roots_iterator(xcb_get_setup(m_conn));
	for(; iter.rem; --nbr, xcb_screen_next(&iter)) {
		if (nbr > 0)
			continue;

		xcb_screen_t *screen = iter.data;
		m_root = screen->root;
	}

	return ret;
}

static void
finalize_xcb_service()
{
	xcb_disconnect(m_conn);
}

static void
setstatus(const char *str)
{
	xcb_change_property(
			m_conn,
			XCB_PROP_MODE_REPLACE,
			m_root,
			XCB_ATOM_WM_NAME,
			XCB_ATOM_STRING,
			8,
			strlen(str),
			str
	);
	xcb_flush(m_conn);
}


/* --- datetime --- */
static char*
mktimes(const char *fmt, char *ptr, size_t ptrlen)
{
	time_t tim = time(NULL);
	struct tm *timtm = localtime(&tim);

	if (timtm == NULL) {
		perror("localtime");
		return NULL;
	}

	if (!strftime(ptr, ptrlen, fmt, timtm)) {
		fprintf(stderr, "strftime failed.\n");
		return NULL;
	}

	return ptr;
}


/* --- main --- */
static void
epoch()
{
	char tmstr[32] = { 0 };
	char volstr[32] = { 0 };
	char statusline[128] = { 0 };

	mktimes(TIME_FORMAT, tmstr, sizeof(tmstr));
	mkvolume(volstr, sizeof(volstr));

	snprintf(statusline, sizeof(statusline)-1,
			"%02dC b%d%% %s%.1fW/%d%% %s %s",
			get_cpu_temperture(),
			get_brightness(),
			is_battery_charging() ? "+" : "-",
			get_battery_dischargingrate(),
			get_battery_remaining(),
			volstr,
			tmstr);

	setstatus(statusline);
}

void crush_signal(int sig)
{
	puts("Gracefully stopping... (send signal again to force)");
	signal(sig, SIG_DFL);
}

int
main()
{
	int ret = 0;

	if ((ret = initialize_xcb_service())) {
		goto fail;
	}


	if (signal(SIGINT, crush_signal) == SIG_ERR
			|| signal(SIGTERM, crush_signal) == SIG_ERR) {
		perror("signal");
		ret = errno;
		goto defer;
	}

	do {
		epoch();
	} while (nanosleep(&m_interval, NULL) == 0);

defer:
	finalize_xcb_service();

fail:
	return ret;
}

