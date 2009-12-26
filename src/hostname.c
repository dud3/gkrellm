/* GKrellM
|  Copyright (C) 1999-2009 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|  Latest versions might be found at:  http://gkrellm.net
|
|
|  GKrellM is free software: you can redistribute it and/or modify it
|  under the terms of the GNU General Public License as published by
|  the Free Software Foundation, either version 3 of the License, or
|  (at your option) any later version.
|
|  GKrellM is distributed in the hope that it will be useful, but WITHOUT
|  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
|  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
|  License for more details.
|
|  You should have received a copy of the GNU General Public License
|  along with this program. If not, see http://www.gnu.org/licenses/
|
|
|  Additional permission under GNU GPL version 3 section 7
|
|  If you modify this program, or any covered work, by linking or
|  combining it with the OpenSSL project's OpenSSL library (or a
|  modified version of that library), containing parts covered by
|  the terms of the OpenSSL or SSLeay licenses, you are granted
|  additional permission to convey the resulting work.
|  Corresponding Source for a non-source form of such a combination
|  shall include the source code for the parts of OpenSSL used as well
|  as that of the covered work.
*/

#include "gkrellm.h"
#include "gkrellm-private.h"
#include "gkrellm-sysdeps.h"


/* ----------------------------------------------------------------------*/
/* Hostname label on the top frame event_box. */

static GkrellmMonitor		*mon_host;

static GkrellmPanel		*host;
static GkrellmDecal		*decal_host;
static GkrellmDecal		*decal_sysname;
static GkrellmAlert		*server_alert;
static GkrellmDecalbutton *sysname_mode_button;

static GtkWidget		*host_vbox;

static gint				style_id;

static gboolean			hostname_visible,
						hostname_short,
						system_name_visible,
						hostname_can_shorten;

gint					system_name_mode = 1;

typedef struct
	{
	gchar				*string;	/* String to draw on a decal	*/
	GkrellmTextstyle	*ts;		/* using this textstyle			*/
	gint				yoff,
						xoff,
						w;			/* Pixel width of string using ts */
	gboolean			reduced;	/* TRUE if default string/font won't fit */
	gboolean			no_fit;
	}
	DecalText;

static DecalText	host_dtext,
					system_dtext,
					kname_dtext,
					kversion_dtext;

static gchar		*hostname,
					*system_name;

static void create_hostname(GtkWidget *vbox, gint first_create);

gboolean
gkrellm_hostname_can_shorten(void)
	{
	return hostname_can_shorten;
	}

static gint
host_expose_event(GtkWidget *widget, GdkEventExpose *ev)
	{
	if (widget == host->drawing_area)
		{
		gdk_draw_drawable(widget->window, gkrellm_draw_GC(1), host->pixmap,
			ev->area.x, ev->area.y, ev->area.x, ev->area.y,
			ev->area.width, ev->area.height);
		}
	return FALSE;
	}


static void
pick_textstyle(GkrellmDecal *d, DecalText *dtext)
	{
	GkrellmTextstyle	*ts;
	gint				w;

	g_free(dtext->ts);
	ts = gkrellm_meter_textstyle(style_id);
	dtext->ts = gkrellm_copy_textstyle(ts);	/* May change only font */
	w = gkrellm_gdk_string_width(ts->font, dtext->string);
	if (w > d->w)
		{
		ts = gkrellm_meter_alt_textstyle(style_id);
		w = gkrellm_gdk_string_width(ts->font, dtext->string);
		dtext->reduced = TRUE;
		}
	dtext->w = w;
	dtext->ts->font = ts->font;
	dtext->xoff = (d->w - w) / 2;
	if (dtext->xoff < 0)
		dtext->xoff = 0;
	}

static void
prune(gchar **name, gchar *s, gboolean split, gboolean only_vowels)
	{
	gchar *dst, *d;

	dst = g_new(gchar, strlen(s) + 1);
	for (d = dst ; *s ; ++s)
		{
		if (split && *s == ' ')
			break;
		if (!only_vowels)
			continue;
		if (   (*s != 'a' && *s != 'e' && *s != 'i' && *s != 'o' && *s != 'u')
			|| !*(s + 1)	/* Don't prune a trailing vowel */
		   )
			*d++ = *s;
		}
	if (*s)
		strcpy(d, only_vowels ? s : s + 1);
	else
		*d = '\0';
	g_free(*name);
	*name = dst;
	}

static void
setup_sysname(void)
	{
	if (!decal_sysname)
		return;

	g_free(system_dtext.string);
	system_dtext.string = g_strdup(system_name);
	system_dtext.reduced = FALSE;
	system_dtext.no_fit = FALSE;
	pick_textstyle(decal_sysname, &system_dtext);
	if (system_dtext.w > decal_sysname->w)
		{
		system_dtext.reduced = TRUE;
		prune(&system_dtext.string, system_name, TRUE, TRUE);
		pick_textstyle(decal_sysname, &system_dtext);
		if (system_dtext.w > decal_sysname->w)
			{
			prune(&system_dtext.string, system_name, FALSE, TRUE);
			pick_textstyle(decal_sysname, &system_dtext);
			if (system_dtext.w > decal_sysname->w)
				{
				prune(&system_dtext.string, system_name, TRUE, FALSE);
				pick_textstyle(decal_sysname, &system_dtext);
				if (system_dtext.w > decal_sysname->w)
					system_dtext.no_fit = TRUE;
				}
			}
		}
	kname_dtext.reduced = FALSE;
	pick_textstyle(decal_sysname, &kname_dtext);
	kname_dtext.yoff = 0;

	kversion_dtext.reduced = FALSE;
	pick_textstyle(decal_sysname, &kversion_dtext);
	kversion_dtext.yoff = decal_sysname->h;
	}

static void
draw_hostname(void)
	{
	gchar		*s, buf[128];

	if (!decal_host)
		return;

	strncpy(buf, hostname, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	if (hostname_can_shorten && _GK.hostname_short)		/* XXX */
		{
		s = strchr(buf, (int) '.');
		if (s)
			*s = '\0';
		}
	host_dtext.string = buf;
	pick_textstyle(decal_host, &host_dtext);

	decal_host->x_off = host_dtext.xoff;
	decal_host->text_style.font = host_dtext.ts->font;
	gkrellm_draw_decal_text(host, decal_host, host_dtext.string, -1);
	}

static void
draw_sysname(void)
	{
	GkrellmTextstyle	ts_save;
	gchar				buf[128];
	gint				x, y, w;

	if (!decal_sysname)
		return;

	ts_save = decal_sysname->text_style;
	if (system_name_mode == 0 || !system_dtext.reduced)
		{
		decal_sysname->text_style = *system_dtext.ts;
		snprintf(buf, sizeof(buf), "%s", system_dtext.string);
		y = system_dtext.yoff;
		}
	else
		{
		decal_sysname->text_style = *kversion_dtext.ts;
		if (system_dtext.no_fit)
			snprintf(buf, sizeof(buf), "%s\n<small><small>%s</small></small>",
					kname_dtext.string, kversion_dtext.string);
		else
			snprintf(buf, sizeof(buf), "%s\n%s",
					kname_dtext.string, kversion_dtext.string);

		y = kname_dtext.yoff;
		}
	if (system_dtext.no_fit)
		gkrellm_decal_scroll_text_set_markup(host, decal_sysname, buf);
	else
		gkrellm_decal_scroll_text_set_text(host, decal_sysname, buf);

	gkrellm_decal_scroll_text_get_size(decal_sysname, &w, NULL);
	x = (decal_sysname->w - w) / 2;
	gkrellm_decal_text_set_offset(decal_sysname, x, y);

	decal_sysname->text_style = ts_save;
	}


void
update_host(void)
	{
	gchar						**parts;
	gint						delta, step, h_scroll;
	gint						hz = gkrellm_update_HZ();
	static gint					reconnect_timeout, y_target, asym;
	static gboolean				recheck_sysname;
	enum GkrellmConnectState	connect_state;

	if (decal_sysname && system_name_mode == 1 && system_dtext.reduced)
		{
		if (GK.two_second_tick && y_target == 0)
			{
			gkrellm_decal_scroll_text_get_size(decal_sysname, NULL, &h_scroll);
			y_target = decal_sysname->h - h_scroll;
			asym = 3;		/* 2 secs name, 6 secs version */
			}
		else if (GK.two_second_tick && y_target != 0  && --asym <= 0)
			{
			y_target = 0;
			}
		delta = y_target - kname_dtext.yoff;
		if (delta != 0)
			{
			step = (delta < 0) ? -1 : 1;
			if (hz < 10)
				{
				if (hz < 5)
					step = delta;
				else if (delta > 1 || delta < -1)
					step *= 2;
				}
			kname_dtext.yoff += step;
			draw_sysname();
			gkrellm_draw_panel_layers(host);
			}
		}

	if (!_GK.client_mode || !GK.second_tick)
		return;

	/* If we loose the server connection, trigger a hardwired alarm.
	*/
	connect_state = gkrellm_client_server_connect_state();
	if (connect_state == CONNECTING)		/* thread is trying a reconnect */
		{
		if (_GK.client_server_reconnect_timeout <= 0)
			{
			gkrellm_check_alert(server_alert, 1.0);		/* Warning	*/
			draw_sysname();
			}
		}
	else if (connect_state == DISCONNECTED)	/* Lost connection			*/
		{
		gkrellm_check_alert(server_alert, 2.0);		/* Alarm 	*/
		if (   _GK.client_server_reconnect_timeout > 0
			&& ++reconnect_timeout > _GK.client_server_reconnect_timeout
		   )
			{
			gkrellm_client_mode_connect_thread();
			reconnect_timeout = 0;
			}
		draw_sysname();
		recheck_sysname = TRUE;		/* Server may be down for kernel upgrade */
		}
	else
		{
		gkrellm_check_alert(server_alert, 0.0);		/* Alert off	*/
		reconnect_timeout = 0;
		if (   _GK.client_server_io_timeout > 0
			&& _GK.time_now >
					_GK.client_server_read_time + _GK.client_server_io_timeout
		   )
			gkrellm_client_mode_disconnect();
		if (recheck_sysname)
			{
			if (strcmp(system_name, _GK.server_sysname))
				{
				g_free(system_name);
				g_free(kname_dtext.string);
				g_free(kversion_dtext.string);
				system_name = g_strdup(_GK.server_sysname);
				parts = g_strsplit(system_name, " ", 2);
				kname_dtext.string = parts[0];
				kversion_dtext.string = parts[1];
				gkrellm_panel_destroy(host);
				gkrellm_alert_destroy(&server_alert);
				create_hostname(host_vbox, TRUE);
				}
			else
				draw_sysname();				
			}
		recheck_sysname = FALSE;
		}
	}


static void
cb_command_process(GkrellmAlert *alert, gchar *src, gchar *buf, gint size,
			gpointer data)
	{
	gchar	*s, c;
	gint	len;

	if (!buf || size < 1)
		return;
	--size;
	*buf = '\0';
	if (!src)
		return;
	for (s = src; *s != '\0' && size > 0; ++s)
		{
		len = 1;
		if (*s == '$' && *(s + 1) != '\0')
			{
			if ((c = *(s + 1)) == 'H')
				len = snprintf(buf, size, "%s", gkrellm_sys_get_host_name());
			++s;
			}
		else
			*buf = *s;
		size -= len;
		buf += len;
		}
	*buf = '\0';
	}

static void
cb_alert_trigger(GkrellmAlert *alert, gpointer data)
	{
	alert->panel = host;
	}

static void
create_server_alert(void)
	{
	if (!server_alert)
		{
		/* This is a hardwired alert, so zero config values.
		|  Use the high alert as a binary alert by setting the high warn
		|  and alarm limits to 0.5 and 1.5.  A 1.0 check above will
		|  trigger a warning and a 2.0 check will trigger an alarm.
		|  A 0.0 check will reset it.
		*/
		server_alert = gkrellm_alert_create(host, NULL,
					_("gkrellmd server disconnect"),
					TRUE,	/* check high */
					FALSE, 	/* no check low */
					TRUE, 0.0, 0.0, 0.0, 0.0, 0);
		}
	gkrellm_alert_set_triggers(server_alert, 1.5, 0.5, 0.0, 0.0);
	gkrellm_alert_commands_config(server_alert, TRUE, FALSE);
	gkrellm_alert_trigger_connect(server_alert, cb_alert_trigger, NULL);
	gkrellm_alert_command_process_connect(server_alert,
				cb_command_process, NULL);
	}

static void
cb_sysname_mode(GkrellmDecalbutton *button, gpointer data)
	{
	system_name_mode = !system_name_mode;
	kname_dtext.yoff = 0;
	kversion_dtext.yoff = decal_sysname->h;
	draw_sysname();
	gkrellm_draw_panel_layers(host);
	gkrellm_config_modified();
	}

static void
create_hostname(GtkWidget *vbox, gint first_create)
	{
	GkrellmStyle	*style;
	gint			y, l;

	if (first_create)
		host = gkrellm_panel_new0();
	host_vbox = vbox;

	if (_GK.client_mode)
		create_server_alert();

	style = gkrellm_meter_style(style_id);

	decal_host = decal_sysname = NULL;
	y = 0;
	if (_GK.enable_hostname)
		{
		decal_host = gkrellm_create_decal_text(host, "8Hgk",
					gkrellm_meter_textstyle(style_id), style, -1, -1, -1);
		y = decal_host->h + 1;
		}
	if (_GK.enable_system_name && system_name)
		{
		decal_sysname = gkrellm_create_decal_text(host, system_name,
					gkrellm_meter_textstyle(style_id), style, -1, -1, -1);
		decal_sysname->y += y;
		gkrellm_decal_scroll_text_align_center(decal_sysname, TRUE);
		}

	gkrellm_panel_configure(host, NULL, style);
	gkrellm_panel_configure_add_height(host, 1);

	gkrellm_panel_create(vbox, mon_host, host);

	if (first_create)
		g_signal_connect(G_OBJECT(host->drawing_area), "expose_event",
				G_CALLBACK(host_expose_event), NULL);

	hostname_visible = _GK.enable_hostname;
	hostname_short = _GK.hostname_short;
	system_name_visible = _GK.enable_system_name;
	setup_sysname();
	draw_hostname();
	draw_sysname();
	if (decal_host || decal_sysname)
		gkrellm_draw_panel_layers(host);

	if (decal_sysname && system_dtext.reduced)
		{
		l = decal_sysname->h * 4 / 5;
		sysname_mode_button = gkrellm_make_scaled_button(host, NULL,
					cb_sysname_mode, NULL, TRUE, TRUE,
					0, 0, 0,    /* Use builtin images */
					gkrellm_chart_width() - l,
					decal_sysname->y + decal_sysname->h - l,
					l, l);
		}
	else
		sysname_mode_button = NULL;

	if (!decal_host && !decal_sysname && !_GK.decorated)
		{
		gkrellm_panel_hide(host);
		gkrellm_spacers_hide(mon_host);
		}
	else
		gkrellm_spacers_show(mon_host);
	}

/* ================================================================== */
  /* Config is done in gui.c General
  */
#define	HOST_CONFIG_KEYWORD		"hostname"

void
gkrellm_apply_hostname_config(void)
	{
	if (   hostname_visible != _GK.enable_hostname
		|| system_name_visible != _GK.enable_system_name
	   )
		{
		hostname_visible = _GK.enable_hostname;
		system_name_visible = _GK.enable_system_name;
		gkrellm_panel_destroy(host);
		gkrellm_alert_destroy(&server_alert);
		create_hostname(host_vbox, TRUE);
		}
	else if (hostname_can_shorten && _GK.hostname_short != hostname_short)
		{
		hostname_short = _GK.hostname_short;
		draw_hostname();
		draw_sysname();
		if (decal_host || decal_sysname)
			gkrellm_draw_panel_layers(host);
		}
	}

void
gkrellm_gkrellmd_disconnect_cb(GtkWidget *button, gpointer data)
	{
	gkrellm_alert_config_window(&server_alert);
	}

static void
save_host_config(FILE *f)
	{
	fprintf(f, "%s sysname_mode %d\n", HOST_CONFIG_KEYWORD, system_name_mode);
	if (_GK.client_mode && server_alert)
		gkrellm_save_alertconfig(f, server_alert, HOST_CONFIG_KEYWORD, NULL);
	}

static void
load_host_config(gchar *arg)
	{
	gchar	config[32], item[CFG_BUFSIZE];
	gint	n;

	if ((n = sscanf(arg, "%31s %[^\n]", config, item)) != 2)
		return;

	if (!strcmp(config, "sysname_mode"))
		sscanf(item, "%d", &system_name_mode);
	else if (!strcmp(config, GKRELLM_ALERTCONFIG_KEYWORD))
		{
		if (!server_alert)
			create_server_alert();
		gkrellm_load_alertconfig(&server_alert, item);
		}
	}

static GkrellmMonitor	monitor_host =
	{
	NULL,		/* Name, for config tab. Done in General config */
	MON_HOST,	/* Id,  0 if a plugin		*/
	create_hostname,
	update_host, /* The update function		*/
	NULL,		/* The config tab create function	*/
	NULL,		/* Instant apply */

	save_host_config,	/* Save user conifg			*/
	load_host_config,	/* Load user config			*/
	HOST_CONFIG_KEYWORD, /* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_host_monitor(void)
	{
	gchar	**parts;

	style_id = gkrellm_add_meter_style(&monitor_host, HOST_STYLE_NAME);

	if (_GK.client_mode)
		{
		hostname = _GK.server_hostname;
		system_name = g_strdup(_GK.server_sysname);
		}
	else
		{
		hostname = gkrellm_sys_get_host_name();
		system_name = g_strdup(gkrellm_sys_get_system_name());
		}
	parts = g_strsplit(system_name, " ", 2);
	kname_dtext.string = parts[0];
	kversion_dtext.string = parts[1];
	hostname_can_shorten = strchr(hostname, (int) '.') ? TRUE : FALSE;

	mon_host = &monitor_host;
	return &monitor_host;
	}

GkrellmMonitor *
gkrellm_mon_host(void)
	{
	return &monitor_host;
	}
