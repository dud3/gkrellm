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

#include <math.h>


typedef struct
	{
	gchar			*panel_label;
	GtkWidget		*vbox;
	GkrellmChart	*chart;
	GkrellmChartdata *forks_cd;
	GkrellmChartconfig *chart_config;
	gint			enabled;
	gboolean		extra_info;
	gint			save_label_position;

	gpointer		sensor_temp,
					sensor_fan;
	GkrellmDecal	*sensor_decal,
					*fan_decal;

	gint			n_users;
	gint			n_processes;
	gint			n_running;
	gulong			n_forks;
	gfloat			fload;
	}
	ProcMon;

ProcMon	proc;

void	(*read_proc_data)();
void	(*read_user_data)();

GkrellmAlert	*load_alert,
				*processes_alert,
				*users_alert;

static gboolean
setup_proc_interface(void)
	{
	if (!read_proc_data && !_GK.client_mode && gkrellm_sys_proc_init())
		{
		read_proc_data = gkrellm_sys_proc_read_data;
		read_user_data = gkrellm_sys_proc_read_users;
		}
	return read_proc_data ? TRUE : FALSE;
	}

void
gkrellm_proc_client_divert(void (*read_proc_func)(),
			void (*read_users_func)())
	{
	read_proc_data = read_proc_func;
	read_user_data = read_users_func;
	}

void
gkrellm_proc_assign_data(gint n_processes, gint n_running,
		gulong n_forks, gfloat load)
	{
	proc.n_processes = n_processes;
	proc.n_running = n_running;
	proc.n_forks = n_forks;
	proc.fload = load;
	}

void
gkrellm_proc_assign_users(gint n_users)
	{
	proc.n_users = n_users;
	}

/* ======================================================================== */

  /* GkrellmCharts are integer only and load is a small real, so scale all load
  |  reading by 100.
  */
#define	LOAD_SCALING	100.0

static GkrellmMonitor	*mon_proc;

static GkrellmLauncher	proc_launch;

static gint		style_id;
static gboolean	show_temperature,
				show_fan;
static gboolean	sensor_separate_mode;
static gint		fork_scaling = 1;
static gchar	*text_format,
				*text_format_locale;

static gboolean	new_text_format = TRUE;

static void
format_proc_data(ProcMon *p, gchar *src_string, gchar *buf, gint size)
	{
	GkrellmChart	*cp;
	gchar			c, *s;
	gint			len, value;

	if (!buf || size < 1)
		return;
	--size;
	*buf = '\0';
	if (!src_string)
		return;
	cp = p->chart;
	for (s = src_string; *s != '\0' && size > 0; ++s)
		{
		len = 1;
		if (*s == '$' && *(s + 1) != '\0')
			{
			value = -1;
			if ((c = *(s + 1)) == 'L')
				len = snprintf(buf, size, "%.1f",
					(gfloat) gkrellm_get_chart_scalemax(cp) / LOAD_SCALING);
			else if (c == 'F' && fork_scaling > 0)
				len = snprintf(buf, size, "%d",
						gkrellm_get_chart_scalemax(cp) / fork_scaling);
			else if (c == 'H')
				len = snprintf(buf, size, "%s", gkrellm_sys_get_host_name());
			else if (c == 'l')
				len = snprintf(buf, size, "%.1f", p->fload);
			else if (c == 'p')
				value = p->n_processes;
			else if (c == 'u')
				value = p->n_users;
			else if (c == 'f')
				value = gkrellm_get_current_chartdata(p->forks_cd)
						/ fork_scaling;
			if (value >= 0)
				len = snprintf(buf, size, "%d", value);
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
draw_proc_extra(void)
	{
	gchar		buf[128];

	if (!proc.chart || !proc.extra_info)
		return;
	format_proc_data(&proc, text_format_locale, buf, sizeof(buf));
	if (!new_text_format)
		gkrellm_chart_reuse_text_format(proc.chart);
	new_text_format = FALSE;
	gkrellm_draw_chart_text(proc.chart, style_id, buf);
	}

static void
cb_command_process(GkrellmAlert *alert, gchar *src, gchar *dst, gint len,
			gpointer data)
	{
	format_proc_data(&proc, src, dst, len);
	}

static void
refresh_proc_chart(GkrellmChart *cp)
	{
	if (proc.enabled)
		{
		gkrellm_draw_chartdata(cp);
		if (proc.extra_info)
			draw_proc_extra();
		gkrellm_draw_chart_to_screen(cp);
		}
	}

static void
draw_sensor_decals(void)
	{
	GkrellmPanel	*p    = proc.chart->panel;
	gchar			units;
	gfloat			t, f;
	gint			toggle;

	if (sensor_separate_mode && show_temperature && show_fan)
		{
		gkrellm_sensor_read_temperature(proc.sensor_temp, &t, &units);
		gkrellm_sensor_draw_temperature_decal(p, proc.sensor_decal, t, units);
		gkrellm_sensor_read_fan(proc.sensor_fan, &f);
		gkrellm_sensor_draw_fan_decal(p, proc.fan_decal, f);
		}
	else
		{
		toggle = _GK.time_now & 2;
		if (show_fan && (toggle || !show_temperature))
			{
			gkrellm_sensor_read_fan(proc.sensor_fan, &f);
			gkrellm_sensor_draw_fan_decal(p, proc.sensor_decal, f);
			}
		else if (show_temperature && (!toggle || !show_fan)
				)
			{
			gkrellm_sensor_read_temperature(proc.sensor_temp, &t, &units);
			gkrellm_sensor_draw_temperature_decal(p, proc.sensor_decal, t,
					units);
			}
		}
	}

void
gkrellm_proc_draw_sensors(gpointer sr)
	{
	if (sr && sr != proc.sensor_temp && sr != proc.sensor_fan)
		return;
	if (proc.enabled)
		draw_sensor_decals();
	}


static void
update_proc(void)
	{
	GkrellmChart	*cp;
	GkrellmPanel	*p;
	gint			load;

	if (!proc.enabled)
		return;
	(*read_proc_data)();
	if (GK.five_second_tick)
		{
		(*read_user_data)();
		gkrellm_check_alert(users_alert, proc.n_users);
		}
	cp = proc.chart;
	p = cp->panel;
	gkrellm_update_krell(p, KRELL(p), proc.n_forks);
	gkrellm_draw_panel_layers(p);

	if (GK.second_tick)
		{
		/* Scale load since it is a small real and charts are integer only.
		|  Scale the forks number by fork_scaling.  See setup_proc_scaling().
		*/
		load = (int) (LOAD_SCALING * proc.fload);
		gkrellm_store_chartdata(cp, 0, load, fork_scaling * proc.n_forks);
		refresh_proc_chart(cp);
		gkrellm_check_alert(load_alert, proc.fload);
		gkrellm_check_alert(processes_alert, proc.n_processes);
		gkrellm_panel_label_on_top_of_decals(p,
				gkrellm_alert_decal_visible(load_alert) ||
				gkrellm_alert_decal_visible(users_alert) ||
				gkrellm_alert_decal_visible(processes_alert));

		}
	if (   (GK.two_second_tick && !sensor_separate_mode)
		|| (GK.five_second_tick && sensor_separate_mode)
	   )
		draw_sensor_decals();
	}


static gint
proc_expose_event(GtkWidget *widget, GdkEventExpose *ev)
	{
	GkrellmChart	*cp		= proc.chart;
	GdkPixmap		*pixmap	= NULL;

	if (cp)
		{
		if (widget == cp->drawing_area)
			pixmap = cp->pixmap;
		else if (widget == cp->panel->drawing_area)
			pixmap = cp->panel->pixmap;
		}
	if (pixmap)
		gdk_draw_drawable(widget->window, gkrellm_draw_GC(1), pixmap,
				  ev->area.x, ev->area.y, ev->area.x, ev->area.y,
				  ev->area.width, ev->area.height);
	return FALSE;
	}


static gint
cb_proc_extra(GtkWidget *widget, GdkEventButton *ev)
	{
	if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS)
		{
		proc.extra_info = !proc.extra_info;
		gkrellm_config_modified();
		refresh_proc_chart(proc.chart);
		}
	else if (   ev->button == 3
			 || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS)
			)
		gkrellm_chartconfig_window_create(proc.chart);
	return FALSE;
	}

static void
setup_proc_scaling(void)
	{
	GkrellmChart	*cp		= proc.chart;
	gint			grids, res, new_fork_scaling;

	if (!cp)
		return;

	grids = gkrellm_get_chartconfig_fixed_grids(cp->config);
	if (!grids)
		grids = FULL_SCALE_GRIDS;

	res = gkrellm_get_chartconfig_grid_resolution(cp->config);

	/* Since grid_resolution is set for load, set krell_full_scale explicitely
	|  to get what I want, which is 10 forks full scale.
	|  When res or number of grids is changed, scale all fork data to keep a
	|  fixed 50 forks/sec max on the chart.
	*/
	KRELL(cp->panel)->full_scale = 10;

	new_fork_scaling = grids * res / 50;
	if (new_fork_scaling < 1)		/* shouldn't happen... */
		new_fork_scaling = 1;

	/* If load grid_resolution changed, scale all fork data to keep the
	|  constant 50 forks/sec.
	*/
	if (fork_scaling != new_fork_scaling)
		{
		/* When called as a callback a chart refresh will follow, but I'll
		|  need a rescale here.
		*/
		gkrellm_scale_chartdata(proc.forks_cd, new_fork_scaling, fork_scaling);
		gkrellm_rescale_chart(proc.chart);
		}
	fork_scaling = new_fork_scaling;
	}

static void
destroy_proc_monitor(void)
	{
	GkrellmChart	*cp		= proc.chart;

	if (proc_launch.button)
		gkrellm_destroy_button(proc_launch.button);
	proc_launch.button = NULL;
    proc_launch.tooltip = NULL;
	gkrellm_chart_destroy(cp);
	proc.chart = NULL;
	proc.enabled = FALSE;
	}

static void
cb_mb_temp_alert_trigger(GkrellmAlert *alert, ProcMon *proc)
	{
	GkrellmAlertdecal	*ad;
	GkrellmDecal		*d;

	if (alert && proc && proc->chart)
		{
		ad = &alert->ad;
		d = proc->sensor_decal;
		if (d)
			{
			ad->x = d->x - 1;
			ad->y = d->y - 1;
			ad->w = d->w + 2;
			ad->h = d->h + 2;
			gkrellm_render_default_alert_decal(alert);
			}
		alert->panel = proc->chart->panel;
		}
	}

static void
cb_mb_fan_alert_trigger(GkrellmAlert *alert, ProcMon *proc)
	{
	GkrellmAlertdecal	*ad;
	GkrellmDecal		*d;

	if (alert && proc && proc->chart)
		{
		ad = &alert->ad;
		if (sensor_separate_mode)
			d = proc->fan_decal;
		else
			d = proc->sensor_decal;
		if (d)
			{
			ad->x = d->x - 1;
			ad->y = d->y - 1;
			ad->w = d->w + 2;
			ad->h = d->h + 2;
			gkrellm_render_default_alert_decal(alert);
			}
		alert->panel = proc->chart->panel;
		}
	}

  /* Next routine is same as in cpu.c - perhaps should make into one?*/
  /* How to decide when to make the sensor_decal and fan_decal visible.
  |  The sensor_decal can show temp values, fan values, or alternating
  |  temp/fan values.  The fan_decal only shows fan values when non
  |  alternating mode is selected. The sensors are mapped in sensors.c
  |
  |   Sensor and fan decal display truth table:
  |   |-----decals visible----||--sensors mapped--|   separate  |
  |	  |sensor_decal  fan_decal||   temp     fan   |    mode     |
  |   |-----------------------||--------------------------------|
  |   |     0           0     ||     0       0          0       |
  |   |     1           0     ||     1       0          0       |
  |   |     1           0     ||     0       1          0       |
  |   |     1           0     ||     1       1          0       |
  |   |     0           0     ||     0       0          1       |
  |   |     1           0     ||     1       0          1       |
  |   |     1           0     ||     0       1          1       |
  |   |     1           1     ||     1       1          1       |
  |   |----------------------------------------------------------
  */
static gboolean
adjust_sensors_display(gint force)
	{
	GkrellmPanel	*p;
	GkrellmDecal	*ds, *df;
	GkrellmAlert	*alert;
	gint			position	= 0;

	ds = proc.sensor_decal;
	df = proc.fan_decal;
	if (!ds || !df)
		return FALSE;
	/* The test for state change is decal state vs success at reading
	|  a temperature.
	*/
	p = proc.chart->panel;
	show_temperature = show_fan = FALSE;
	if (!_GK.demo)
		{
		gkrellm_sensor_alert_connect(proc.sensor_temp,
					cb_mb_temp_alert_trigger, &proc);
		gkrellm_sensor_alert_connect(proc.sensor_fan,
					cb_mb_fan_alert_trigger, &proc);
		}

	/* If a fan alert is triggered, turn it off in case fan decal being used
	|  is changed.  The alert will just retrigger at next fan update.
	*/
	alert = gkrellm_sensor_alert(proc.sensor_fan);
	gkrellm_reset_alert_soft(alert);

	if (proc.sensor_temp || _GK.demo)
		show_temperature = TRUE;
	if (proc.sensor_fan || _GK.demo)
		show_fan = TRUE;

	if (show_temperature || show_fan)
		{
		if (! gkrellm_is_decal_visible(ds) || force)
			gkrellm_make_decal_visible(p, ds);
		position = 0;
		}
	else
		{
		if (gkrellm_is_decal_visible(ds) || force)
			gkrellm_make_decal_invisible(p, ds);
		position = proc.save_label_position;
		}
	if (show_fan && show_temperature && sensor_separate_mode)
		{
		if (! gkrellm_is_decal_visible(df) || force)
			gkrellm_make_decal_visible(p, df);
		position = -1;
		}
	else
		{
		if (gkrellm_is_decal_visible(df) || force)
			gkrellm_make_decal_invisible(p, df);
		}
	if (position != p->label->position || force)
		{
		if (proc.save_label_position >= 0)	/* Reassign position only if the */
			p->label->position = position;	/* original label was visible.   */
		gkrellm_draw_panel_label(p);
		draw_sensor_decals();
		gkrellm_draw_panel_layers(p);
		}
	return TRUE;
	}

static gint
cb_panel_press(GtkWidget *widget, GdkEventButton *ev)
	{
	if (ev->button == 3)
		gkrellm_open_config_window(mon_proc);
	return FALSE;
	}

static void
create_proc_monitor(GtkWidget *vbox, gint first_create)
	{
	GkrellmChart		*cp;
	GkrellmChartdata	*cd;
	GkrellmChartconfig	*cf;
	GkrellmPanel		*p;
	GkrellmStyle		*style;

	if (first_create)
		{
		proc.chart = gkrellm_chart_new0();
		proc.chart->panel = gkrellm_panel_new0();
		}
	cp = proc.chart;
	p = cp->panel;

	style = gkrellm_panel_style(style_id);
	gkrellm_create_krell(p, gkrellm_krell_panel_piximage(style_id), style);

	gkrellm_chart_create(proc.vbox, mon_proc, cp, &proc.chart_config);
	gkrellm_set_draw_chart_function(cp, refresh_proc_chart, proc.chart);
	cd = gkrellm_add_default_chartdata(cp, _("Load"));
	gkrellm_monotonic_chartdata(cd, FALSE);
	gkrellm_set_chartdata_draw_style_default(cd, CHARTDATA_LINE);
	proc.forks_cd = gkrellm_add_default_chartdata(cp, _("Forks"));
	gkrellm_set_chartdata_flags(proc.forks_cd, CHARTDATA_ALLOW_HIDE);

	cf = cp->config;
	gkrellm_chartconfig_fixed_grids_connect(cf, setup_proc_scaling, NULL);
	gkrellm_chartconfig_grid_resolution_connect(cf, setup_proc_scaling, NULL);
	gkrellm_chartconfig_grid_resolution_adjustment(cf, FALSE,
			LOAD_SCALING, 0.5, 5.0, 0.5, 0.5, 1, 50);
	gkrellm_chartconfig_grid_resolution_label(cf,
					_("Average process load per minute"));

	gkrellm_alloc_chartdata(cp);
	setup_proc_scaling();

	/* I put motherboard temp on Proc panel (if temperature sensors found)
	*/
	gkrellm_sensors_create_decals(p, style_id,
					&proc.sensor_decal, &proc.fan_decal);

	gkrellm_panel_configure(p, proc.panel_label, style);

	gkrellm_panel_create(proc.vbox, mon_proc, p);

	proc.save_label_position = p->label->position;
	if (proc.sensor_decal)
		adjust_sensors_display(TRUE);

	new_text_format = TRUE;
	if (first_create)
		{
		g_signal_connect(G_OBJECT(cp->drawing_area), "expose_event",
				G_CALLBACK(proc_expose_event), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area), "expose_event",
				G_CALLBACK(proc_expose_event), NULL);
		g_signal_connect(G_OBJECT(cp->drawing_area),"button_press_event",
				G_CALLBACK(cb_proc_extra), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area), "button_press_event",
				G_CALLBACK(cb_panel_press), NULL);
		}
	else
		refresh_proc_chart(cp);
	gkrellm_setup_launcher(p, &proc_launch, CHART_PANEL_TYPE, 4);
	}

static void
create_proc(GtkWidget *vbox, gint first_create)
	{
	proc.vbox = vbox;
	if (proc.enabled)
		{
		create_proc_monitor(proc.vbox, first_create);
		gkrellm_spacers_show(mon_proc);
		}
	else
		gkrellm_spacers_hide(mon_proc);
	}



#define	PROC_CONFIG_KEYWORD	"proc"

static void
cb_alert_trigger(GkrellmAlert *alert, gpointer data)
	{
	GkrellmPanel		*p;
	GkrellmAlertdecal   *ad;
	GkrellmDecal        *ds, *df;
	gint				x, w;

	p = proc.chart->panel;
	alert->panel = p;
	ds = proc.sensor_decal;
	df = proc.fan_decal;
	ad = &alert->ad;
	if (gkrellm_is_decal_visible(ds) && !gkrellm_is_decal_visible(df))
		w = ds->x - 1;
	else
		w = p->w;
	w /= 3;
	if (w < 2)
		w = 2;
	if (alert == load_alert)
		x = 0;
	else if (alert == users_alert)
		x = w;
	else
		x = 2 * w;
	ad->x = x;
	ad->y = 0;
	ad->w = w;
	ad->h = p->h;
	gkrellm_render_default_alert_decal(alert);
	}

static void
create_load_alert(void)
	{
	load_alert = gkrellm_alert_create(NULL, _("Load"),
			NULL,
			TRUE, FALSE, TRUE,
			20, 1, 0.5, 1, 2);
	gkrellm_alert_delay_config(load_alert, 1, 10000, 0);

	gkrellm_alert_trigger_connect(load_alert, cb_alert_trigger, NULL);
	gkrellm_alert_command_process_connect(load_alert,
			cb_command_process, NULL);
	}

static void
create_users_alert(void)
	{
	users_alert = gkrellm_alert_create(NULL, _("Users"),
			NULL,
			TRUE, FALSE, TRUE,
			100000, 2, 1, 10, 0);
	gkrellm_alert_trigger_connect(users_alert, cb_alert_trigger, NULL);
	gkrellm_alert_command_process_connect(users_alert,
			cb_command_process, NULL);
	}

static void
create_processes_alert(void)
	{
	processes_alert = gkrellm_alert_create(NULL, _("Processes"),
			NULL,
			TRUE, FALSE, TRUE,
			100000, 10, 1, 10, 0);
	gkrellm_alert_trigger_connect(processes_alert, cb_alert_trigger, NULL);
	gkrellm_alert_command_process_connect(processes_alert,
			cb_command_process, NULL);
	}

static void
save_proc_config(FILE *f)
	{
	fprintf(f, "%s enable %d %d\n", PROC_CONFIG_KEYWORD,
				proc.enabled, proc.extra_info);
	fprintf(f, "%s launch %s\n", PROC_CONFIG_KEYWORD, proc_launch.command);
	fprintf(f, "%s tooltip_comment %s\n", PROC_CONFIG_KEYWORD,
				proc_launch.tooltip_comment);
	fprintf(f, "%s sensor_mode %d\n", PROC_CONFIG_KEYWORD,
				sensor_separate_mode);
	fprintf(f, "%s text_format %s\n", PROC_CONFIG_KEYWORD, text_format);
	gkrellm_save_chartconfig(f, proc.chart_config,
				PROC_CONFIG_KEYWORD, NULL);
	if (load_alert)
		gkrellm_save_alertconfig(f, load_alert, PROC_CONFIG_KEYWORD, "load");
	if (users_alert)
		gkrellm_save_alertconfig(f, users_alert, PROC_CONFIG_KEYWORD, "users");
	if (processes_alert)
		gkrellm_save_alertconfig(f, processes_alert,
					PROC_CONFIG_KEYWORD, "processes");
	}

static void
load_proc_config(gchar *arg)
	{
	gchar	config[32], name[16], item[CFG_BUFSIZE], item1[CFG_BUFSIZE];
	gint	n;

	n = sscanf(arg, "%31s %[^\n]", config, item);
	if (n == 2)
		{
		if (!strcmp(config, "enable"))
			sscanf(item, "%d %d\n", &proc.enabled, &proc.extra_info);
		else if (!strcmp(config, "launch"))
			proc_launch.command = g_strdup(item);
		else if (!strcmp(config, "tooltip_comment"))
			proc_launch.tooltip_comment = g_strdup(item);
		else if (!strcmp(config, "sensor_mode"))
			sscanf(item, "%d\n", &sensor_separate_mode);
		else if (!strcmp(config, "text_format"))
			{
			gkrellm_locale_dup_string(&text_format, item, &text_format_locale);
			new_text_format = TRUE;
			}
		else if (!strcmp(config, GKRELLM_CHARTCONFIG_KEYWORD))
			gkrellm_load_chartconfig(&proc.chart_config, item, 2);
		else if (!strcmp(config, GKRELLM_ALERTCONFIG_KEYWORD))
			{
			if (sscanf(item, "%15s %[^\n]", name, item1) == 2)
				{
				if (!strcmp(name, "load"))
					{
					if (!load_alert)
						create_load_alert();
					gkrellm_load_alertconfig(&load_alert, item1);
					}
				else if (!strcmp(name, "users"))
					{
					if (!users_alert)
						create_users_alert();
					gkrellm_load_alertconfig(&users_alert, item1);
					}
				else if (!strcmp(name, "processes"))
					{
					if (!processes_alert)
						create_processes_alert();
					gkrellm_load_alertconfig(&processes_alert, item1);
					}
				}
			}
		}
	}

/* ---------------------------------------------------------------------- */
#define DEFAULT_TEXT_FORMAT	"\\w88\\a$p\\f procs\\n\\e$u\\f users"


static GtkWidget	*proc_launch_entry,
					*proc_tooltip_entry;

static GtkWidget	*text_format_combo_box;

static GtkWidget	*load_alert_button,
					*users_alert_button,
					*processes_alert_button;


static gboolean
fix_panel(void)
	{
	gboolean	result;

	if (!proc.enabled)
		return FALSE;
	if ((result = adjust_sensors_display(FALSE)) && proc_launch.button)
		{
		gkrellm_destroy_button(proc_launch.button);
		proc_launch.button =
			gkrellm_put_label_in_panel_button(proc.chart->panel,
				gkrellm_launch_button_cb, &proc_launch, proc_launch.pad);
		}
	return result;
	}

gboolean
gkrellm_proc_set_sensor(gpointer sr, gint type)
	{
	if (type == SENSOR_TEMPERATURE)
		proc.sensor_temp = sr;
	else if (type == SENSOR_FAN)
		proc.sensor_fan = sr;
	else
		return FALSE;
	return fix_panel();
	}

static void
cb_sensor_separate(GtkWidget *button, gpointer data)
	{
	sensor_separate_mode = GTK_TOGGLE_BUTTON(button)->active;
	fix_panel();
	}

static void
cb_text_format(GtkWidget *widget, gpointer data)
	{
	gchar		*s;
	GtkWidget	*entry;

	entry = gtk_bin_get_child(GTK_BIN(text_format_combo_box));
	s = gkrellm_gtk_entry_get_text(&entry);
	gkrellm_locale_dup_string(&text_format, s, &text_format_locale);
	new_text_format = TRUE;
	refresh_proc_chart(proc.chart);
	}

static void
cb_enable(GtkWidget *button, gpointer data)
    {
	gboolean enabled;

	enabled = GTK_TOGGLE_BUTTON(button)->active;
	if (enabled && ! proc.enabled)
		{
		create_proc_monitor(proc.vbox, TRUE);
		gkrellm_spacers_show(mon_proc);
		}
	else if (!enabled && proc.enabled)
		{
		destroy_proc_monitor();
		gkrellm_spacers_hide(mon_proc);
		}
	proc.enabled = enabled;

	gtk_widget_set_sensitive(load_alert_button, enabled);
	gtk_widget_set_sensitive(users_alert_button, enabled);
	gtk_widget_set_sensitive(processes_alert_button, enabled);
	}

static void
cb_load_alert(GtkWidget *button, gpointer data)
	{
	if (!load_alert)
		create_load_alert();
	gkrellm_alert_config_window(&load_alert);
	}

static void
cb_users_alert(GtkWidget *button, gpointer data)
	{
	if (!users_alert)
		create_users_alert();
	gkrellm_alert_config_window(&users_alert);
	}

static void
cb_processes_alert(GtkWidget *button, gpointer data)
	{
	if (!processes_alert)
		create_processes_alert();
	gkrellm_alert_config_window(&processes_alert);
	}

static void
cb_launch_entry(GtkWidget *widget, gpointer data)
	{
	if (proc.enabled)
		{
		gkrellm_apply_launcher(&proc_launch_entry, &proc_tooltip_entry,
					proc.chart->panel, &proc_launch, gkrellm_launch_button_cb);
		}
	}


static gchar	*proc_info_text[] =
{
N_("<h>Proc Chart"),
"\n",
N_("The krell shows process forks with a full scale value of 10 forks.\n"),
N_("While both load and fork data are drawn on the chart, the grid\n"
"resolution can be set for load only.  The resolution per grid for forks is\n"
"fixed at 10 when using the auto number of grids mode, and at 50 divided by\n"
"the number of grids when using a fixed number of grids mode.\n"),
"\n",
N_("<h>Chart Labels\n"),
N_("Substitution variables for the format string for chart labels:\n"),
N_("\t$L    maximum chart value (load)\n"),
N_("\t$F    maximum chart value (forks)\n"),
N_("\t$l    load\n"),
N_("\t$f    forks\n"),
N_("\t$p    processes\n"),
N_("\t$u    users\n"),
"\n",
N_("Substitution variables may be used in alert commands.\n")
};

static void
create_proc_tab(GtkWidget *tab_vbox)
	{
	GtkWidget	*tabs, *table, *vbox, *vbox1, *hbox, *text, *label;
	gint		i;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

/* ---Options tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Options"));
	gkrellm_gtk_check_button_connected(vbox, NULL,
				proc.enabled, FALSE, FALSE, 4,
				cb_enable, NULL,
				_("Enable Proc chart"));

	if (gkrellm_sensors_available())
		gkrellm_gtk_check_button_connected(vbox, NULL,
					sensor_separate_mode, FALSE, FALSE, 0,
					cb_sensor_separate, NULL,
		_("Draw fan and temperature values separately (not alternating)."));

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, TRUE, 5);
	gkrellm_gtk_alert_button(hbox, &processes_alert_button, FALSE, FALSE, 4,
				TRUE, cb_processes_alert, NULL);
	label = gtk_label_new(_("Processes"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 4);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, TRUE, 5);
	gkrellm_gtk_alert_button(hbox, &users_alert_button, FALSE, FALSE, 4, TRUE,
				cb_users_alert, NULL);
	label = gtk_label_new(_("Users"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 4);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, TRUE, 5);
	gkrellm_gtk_alert_button(hbox, &load_alert_button, FALSE, FALSE, 4, TRUE,
				cb_load_alert, NULL);
	label = gtk_label_new(_("Load"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 4);

	if (!proc.enabled)
		{
		gtk_widget_set_sensitive(load_alert_button, FALSE);
		gtk_widget_set_sensitive(users_alert_button, FALSE);
		gtk_widget_set_sensitive(processes_alert_button, FALSE);
		}

	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Format String for Chart Labels"),
				4, 0, TRUE);

	text_format_combo_box = gtk_combo_box_entry_new_text();
	gtk_widget_set_size_request (GTK_WIDGET(text_format_combo_box), 350, -1);
	gtk_box_pack_start(GTK_BOX(vbox1), text_format_combo_box, FALSE, FALSE, 2);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box), text_format);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
			_(DEFAULT_TEXT_FORMAT));
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
			_("\\f$L\\r\\f$F \\w88\\b\\p\\a$p\\f procs\\n\\e$u\\f users"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(text_format_combo_box), 0);
	g_signal_connect(G_OBJECT(GTK_COMBO_BOX(text_format_combo_box)), "changed",
			G_CALLBACK(cb_text_format), NULL);

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Launch Commands"),
				4, 0, TRUE);
	table = gkrellm_gtk_launcher_table_new(vbox1, 1);
	gkrellm_gtk_config_launcher(table, 0,
				&proc_launch_entry, &proc_tooltip_entry,
				_("Proc"), &proc_launch);
	g_signal_connect(G_OBJECT(proc_launch_entry), "changed",
				G_CALLBACK(cb_launch_entry), NULL);
	g_signal_connect(G_OBJECT(proc_tooltip_entry), "changed",
				G_CALLBACK(cb_launch_entry), NULL);

/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	for (i = 0; i < sizeof(proc_info_text)/sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(proc_info_text[i]));
	}


gchar *
gkrellm_proc_get_sensor_panel_label(void)
	{
	return proc.panel_label;
	}

GkrellmMonitor *
gkrellm_get_proc_mon(void)
	{
	return mon_proc;
	}

static GkrellmMonitor	monitor_proc =
	{
	N_("Proc"),			/* Name, for config tab.	*/
	MON_PROC,			/* Id,  0 if a plugin		*/
	create_proc,		/* The create function		*/
	update_proc,		/* The update function		*/
	create_proc_tab,	/* The config tab create function	*/
	NULL,				/* Instant apply */

	save_proc_config,	/* Save user conifg			*/
	load_proc_config,	/* Load user config			*/
	PROC_CONFIG_KEYWORD, /* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_proc_monitor(void)
	{
	GkrellmChartconfig	*cf;

	monitor_proc.name = _(monitor_proc.name);
	proc.panel_label = g_strdup(_("Proc"));
	proc.enabled = TRUE;
	proc.extra_info = TRUE;
	style_id = gkrellm_add_chart_style(&monitor_proc, PROC_STYLE_NAME);
	gkrellm_locale_dup_string(&text_format, _(DEFAULT_TEXT_FORMAT),
				&text_format_locale);

	mon_proc = &monitor_proc;
	if (setup_proc_interface())
		{
		/* Set chart config defaults.  Turn off auto grid resolution and
		|  don't let user config it back on.
		*/
		cf = proc.chart_config = gkrellm_chartconfig_new0();
		gkrellm_set_chartconfig_grid_resolution(cf, 100);
		gkrellm_set_chartconfig_auto_grid_resolution(cf, FALSE);
		gkrellm_set_chartconfig_flags(cf, NO_CONFIG_AUTO_GRID_RESOLUTION);

		return &monitor_proc;
		}
	return NULL;
	}

