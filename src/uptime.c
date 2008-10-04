/* GKrellM
|  Copyright (C) 1999-2008 Bill Wilson
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

GkrellmMonitor			*mon_uptime;

static GkrellmPanel		*uptime;
static GkrellmDecal		*decal_uptime;
static GkrellmLauncher	launch;

static time_t			base_uptime,
						uptime_seconds;

time_t					(*read_uptime)();


static gint				style_id;
static gboolean			uptime_enabled	= TRUE;



static gboolean
setup_uptime_interface(void)
	{
	if (!read_uptime && !_GK.client_mode && gkrellm_sys_uptime_init())
		{
		read_uptime = gkrellm_sys_uptime_read_uptime;
		}
	return read_uptime ? TRUE : FALSE;
	}

void
gkrellm_uptime_client_divert(time_t (*read_func)())
	{
	read_uptime = read_func;
	}

void
gkrellm_uptime_set_base_uptime(time_t base)
	{
	base_uptime = base;
	}

static gint
uptime_expose_event(GtkWidget *widget, GdkEventExpose *ev)
	{
	if (widget == uptime->drawing_area)
		{
		gdk_draw_drawable(widget->window, gkrellm_draw_GC(1), uptime->pixmap,
				ev->area.x, ev->area.y, ev->area.x, ev->area.y,
				ev->area.width, ev->area.height);
		}
	return FALSE;
	}

static void
draw_upminutes(gint minutes)
	{
	GkrellmTextstyle	*ts;
	gint				w1, w2, w;
	gint				days, hours;
	gchar				buf1[16], buf2[16], *s;

	ts = gkrellm_meter_textstyle(style_id);
	hours = minutes / 60;
	minutes %= 60;
	days = hours / 24;
	hours %= 24;

	s = buf1;
	snprintf(buf1, sizeof(buf1), "%dd %2d:%02d", days, hours, minutes); 
	snprintf(buf2, sizeof(buf2), "%dd%2d:%02d", days, hours, minutes); 
	w = w1 = gkrellm_gdk_string_width(ts->font, buf1);
	if (w1 > decal_uptime->w)
		{
		if ((w2 = gkrellm_gdk_string_width(ts->font, buf2)) > decal_uptime->w)
			{
			ts = gkrellm_meter_alt_textstyle(style_id);
			w = gkrellm_gdk_string_width(ts->font, buf1);
			}
		else
			{
			s = buf2;
			w = w2;
			}
		}
	/* Last chance to fit it in.
	*/
	if (w > decal_uptime->w)
		{
		snprintf(buf1, sizeof(buf1), "%dd%2d:", days, hours);
		s = buf1; 
		}
	decal_uptime->x_off = (decal_uptime->w - w) / 2;
	if (decal_uptime->x_off < 0)
		decal_uptime->x_off = 0;

	decal_uptime->text_style.font = ts->font;
	gkrellm_draw_decal_text(uptime, decal_uptime, s, minutes);
	gkrellm_draw_panel_layers(uptime);
	}


static void
update_uptime(void)
	{
	gint	up_minutes;

	if (!uptime_enabled)
		return;

	/* Once every 10 seconds is default update period.
	*/
	if (GK.ten_second_tick || _GK.up_minutes < 0)
		{
		uptime_seconds = (*read_uptime)();
		if (uptime_seconds > 0)
			up_minutes = (gint) (uptime_seconds / 60);
		else
			up_minutes = (gint)(time(0) - _GK.start_time + base_uptime) / 60;
		if (_GK.up_minutes != up_minutes)
				draw_upminutes(up_minutes);
		_GK.up_minutes = up_minutes;
		}
	}

static void
create_uptime(GtkWidget *vbox, gint first_create)
	{
	GkrellmStyle		*style;
	GkrellmMargin		*m;
	GkrellmTextstyle	*ts;
	gint				w,
						chart_width = gkrellm_chart_width();

	if (first_create)
		uptime = gkrellm_panel_new0();

	style = gkrellm_meter_style(style_id);
	m = gkrellm_get_style_margins(style);

	ts = gkrellm_meter_textstyle(style_id);

	w = gkrellm_gdk_string_width(ts->font, "999d 23:99") + 2;
	if (w > chart_width - m->left - m->right)
		w = chart_width - m->left - m->right;

	decal_uptime = gkrellm_create_decal_text(uptime, "9d 12:99",
				ts, style, -1, -1, w);
	decal_uptime->x = (chart_width - decal_uptime->w) / 2;

	gkrellm_panel_configure(uptime, NULL, style);
	gkrellm_panel_create(vbox, mon_uptime, uptime);

	if (!uptime_enabled)
		{
		gkrellm_panel_hide(uptime);
		gkrellm_spacers_hide(mon_uptime);
		}
	else
		gkrellm_spacers_show(mon_uptime);

	if (first_create)
		g_signal_connect(G_OBJECT (uptime->drawing_area), "expose_event",
				G_CALLBACK(uptime_expose_event), NULL);
	gkrellm_setup_launcher(uptime, &launch, METER_PANEL_TYPE, 0);

	_GK.up_minutes = -1;
	update_uptime();
	}


#define	UPTIME_CONFIG_KEYWORD	"uptime"

static void
save_uptime_config(FILE *f)
	{
	fprintf(f, "%s enable %d\n", UPTIME_CONFIG_KEYWORD, uptime_enabled);
	fprintf(f, "%s launch %s\n", UPTIME_CONFIG_KEYWORD, launch.command);
	fprintf(f, "%s tooltip %s\n",
				UPTIME_CONFIG_KEYWORD, launch.tooltip_comment);
	}

static void
load_uptime_config(gchar *arg)
	{
	gchar	config[32], item[CFG_BUFSIZE];
	gint	n;

	n = sscanf(arg, "%31s %[^\n]", config, item);
	if (n == 2)
		{
		if (strcmp(config, "enable") == 0)
			sscanf(item, "%d", &uptime_enabled);
		else if (strcmp(config, "launch") == 0)
			launch.command = g_strdup(item);
		else if (strcmp(config, "tooltip") == 0)
			launch.tooltip_comment = g_strdup(item);
		}
	}

/* --------------------------------------------------------------------- */
static GtkWidget	*uptime_enabled_button;
static GtkWidget	*launch_entry,
					*tooltip_entry;

static void
cb_enable(GtkWidget *widget, gpointer data)
	{
	uptime_enabled = GTK_TOGGLE_BUTTON(uptime_enabled_button)->active;
	if (uptime_enabled)
		{
		gkrellm_panel_show(uptime);
		gkrellm_spacers_show(mon_uptime);
		}
	else
		{
		gkrellm_panel_hide(uptime);
		gkrellm_spacers_hide(mon_uptime);
		}
	}

static void
cb_launch_entry(GtkWidget *widget, gpointer data)
	{
	gkrellm_apply_launcher(&launch_entry, &tooltip_entry, uptime,
			&launch, gkrellm_launch_button_cb);
	}


static void
create_uptime_tab(GtkWidget *tab_vbox)
	{
	GtkWidget	*tabs, *table, *vbox, *vbox1;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

/* ---Options tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Options"),
				4, 0, TRUE);
	gkrellm_gtk_check_button_connected(vbox1, &uptime_enabled_button,
			uptime_enabled, FALSE, FALSE, 10,
			cb_enable, NULL,
			_("Enable Uptime"));

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Launch Commands"),
				4, 0, TRUE);
	table = gkrellm_gtk_launcher_table_new(vbox1, 1);
	gkrellm_gtk_config_launcher(table, 0,  &launch_entry, &tooltip_entry,
				_("Uptime"), &launch);
	g_signal_connect(G_OBJECT(launch_entry), "changed",
				G_CALLBACK(cb_launch_entry), NULL);
	g_signal_connect(G_OBJECT(tooltip_entry), "changed",
				G_CALLBACK(cb_launch_entry), NULL);
	}


static GkrellmMonitor	monitor_uptime =
	{
	N_("Uptime"),		/* Name, for config tab.	*/
	MON_UPTIME,			/* Id,  0 if a plugin		*/
	create_uptime,		/* The create function		*/
	update_uptime,		/* The update function		*/
	create_uptime_tab,	/* The config tab create function	*/
	NULL, 				/* Instant config			*/

	save_uptime_config,	/* Save user conifg			*/
	load_uptime_config,	/* Load user config			*/
	UPTIME_CONFIG_KEYWORD, /* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_uptime_monitor(void)
	{
	monitor_uptime.name = _(monitor_uptime.name);
	style_id = gkrellm_add_meter_style(&monitor_uptime, UPTIME_STYLE_NAME);
	mon_uptime = &monitor_uptime;
	if (setup_uptime_interface())
		return &monitor_uptime;
	return NULL;
	}
