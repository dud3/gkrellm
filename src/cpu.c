/* GKrellM
|  Copyright (C) 1999-2010 Bill Wilson
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


static void	cb_alert_config(GkrellmAlert *ap, gpointer data);
#if !defined(WIN32)
static void	cb_alert_config_create(GkrellmAlert *ap, GtkWidget *vbox,
					gpointer data);
#endif


#ifdef CLK_TCK
#define	CPU_TICKS_PER_SECOND	CLK_TCK
#else
#define	CPU_TICKS_PER_SECOND	100		/* XXX */
#endif

  /* Values for smp_mode - must be same order as buttons in config */
#define	SMP_REAL_MODE				0
#define	SMP_COMPOSITE_MODE			1
#define	SMP_COMPOSITE_AND_REAL_MODE	2

typedef struct
	{
	gchar			*name;
	gchar			*panel_label;
	GtkWidget		*vbox;
	GkrellmChart	*chart;
	GkrellmChartconfig	*chart_config;
	GkrellmChartdata	*sys_cd,
					*user_cd,
					*nice_cd;
	gint			enabled;
	gint			extra_info;
	gint			is_composite;
	gulong			previous_total;
	gint			instance;

	gpointer		sensor_temp,
					sensor_fan;
	GkrellmDecal	*sensor_decal,		/* temperature and possibly fan */
					*fan_decal;			/* fan if separate draw mode	*/
	gboolean		show_temperature,
					show_fan,
					new_text_format;
	gint			save_label_position;  /* When toggling sensors on/off */

	GkrellmAlert	*alert;
	gulong			previous_alert_value,
					previous_alert_total;

	GkrellmLauncher	launch;
	GtkWidget		*launch_entry,
					*tooltip_entry;

	gulong			user,
					nice,
					sys,
					idle;
	}
	CpuMon;


static GkrellmMonitor
				*mon_cpu;

GList			*cpu_mon_list,
				*instance_list;

void			(*read_cpu_data)();
static CpuMon	*composite_cpu;

static GkrellmAlert	*cpu_alert;		/* One alert dupped for each CPU */
static gboolean	alert_includes_nice;

static gint		smp_mode;
static gboolean	cpu_enabled  = TRUE;
static gboolean	omit_nice_mode,
				config_tracking,
				sys_handles_composite_data,
				nice_time_unsupported;

static gint		style_id;
static gint		sensor_separate_mode;

static gchar    *text_format,
				*text_format_locale;

static gint		n_cpus;
static gint		n_smp_cpus;


static gboolean
setup_cpu_interface(void)
	{
	if (!read_cpu_data && !_GK.client_mode && gkrellm_sys_cpu_init())
		{
		read_cpu_data = gkrellm_sys_cpu_read_data;
		}
	return read_cpu_data ? TRUE : FALSE;
	}

void
gkrellm_cpu_client_divert(void (*read_func)())
	{
	read_cpu_data = read_func;
	}

  /* Must be called from gkrellm_sys_cpu_init()
  */
void
gkrellm_cpu_set_number_of_cpus(gint n)
	{
	CpuMon	*cpu;
	GList	*list;
	gint	i;

	n_cpus = n;
	if (instance_list && g_list_length(instance_list) != n)
		{
		instance_list = NULL;
		fprintf(stderr, "Bad sysdep cpu instance list length.\n");
		}
	if (!instance_list)
		for (i = 0; i < n; ++i)
			instance_list = g_list_append(instance_list, GINT_TO_POINTER(i));
	if (n > 1)
		{
		++n_cpus;	/* Include the composite cpu */
		n_smp_cpus = n;
		}
	for (i = 0; i < n_cpus; ++i)
		{
		cpu = g_new0(CpuMon, 1);
		if (i == 0)
			{
			cpu->name = g_strdup("cpu");
			if (n_smp_cpus > 0)
				{
				cpu->is_composite = TRUE;
				cpu->instance = -1;
				composite_cpu = cpu;
				}
			else
				cpu->instance = GPOINTER_TO_INT(instance_list->data);
			}
		else
			{
			list = g_list_nth(instance_list, i - 1);
			cpu->instance = GPOINTER_TO_INT(list->data);
			cpu->name = g_strdup_printf("cpu%d", cpu->instance);
			}
		cpu->panel_label = g_strdup_printf(_("CPU%s"), &(cpu->name)[3]);
		cpu_mon_list = g_list_append(cpu_mon_list, cpu);
		}
	}

  /* If cpu numbering is not sequential starting from zero, then sysdep code
  |  should call this function for each cpu to make a list of cpu numbers.
  */
void
gkrellm_cpu_add_instance(gint instance)
	{
	instance_list = g_list_append(instance_list, GINT_TO_POINTER(instance));
	}

void
gkrellm_cpu_nice_time_unsupported(void)
	{
	nice_time_unsupported = TRUE;
	}

void
gkrellm_cpu_assign_composite_data(gulong user, gulong nice,
			gulong sys, gulong idle)
	{
	if (!composite_cpu)
		return;
	sys_handles_composite_data = TRUE;
	composite_cpu->user = user;
	composite_cpu->nice = nice;
	composite_cpu->sys = sys;
	composite_cpu->idle = idle;
	}

void
gkrellm_cpu_assign_data(gint n, gulong user, gulong nice,
			gulong sys, gulong idle)
	{
	CpuMon	*cpu = NULL;
	GList	*list;

	for (list = cpu_mon_list; list; list = list->next)
		{
		cpu = (CpuMon *) list->data;
		if (cpu->instance == n)
			break;
		}
	if (list)
		{
		cpu->user = user;
		cpu->nice = nice;
		cpu->sys = sys;
		cpu->idle = idle;
		if (composite_cpu && !sys_handles_composite_data)
			{
			composite_cpu->user += user;
			composite_cpu->nice += nice;
			composite_cpu->sys += sys;
			composite_cpu->idle += idle;
			}
		}
	}

/* ======================================================================== */
/* Exporting CPU data for plugins */

gint
gkrellm_smp_cpus(void)
	{
	return n_smp_cpus;
	}

gboolean
gkrellm_cpu_stats(gint n, gulong *user, gulong *nice,
			gulong *sys, gulong *idle)
	{
	GList	*list;
	CpuMon	*cpu;

	list = g_list_nth(cpu_mon_list, n);
	if (!list)
		return FALSE;
	cpu = (CpuMon *) list->data;
	if (user)
		*user = cpu->user;
	if (nice)
		*nice = cpu->nice;
	if (sys)
		*sys = cpu->sys;
	if (idle)
		*idle = cpu->idle;
	return TRUE;
	}

/* ======================================================================== */

static void
format_cpu_data(CpuMon *cpu, gchar *src_string, gchar *buf, gint size)
	{
	GkrellmChart	*cp;
	gchar			c, *s;
	gint			len, sys, user, nice = 0, total, t;

	if (!buf || size < 1)
		return;
	--size;
	*buf = '\0';
	if (!src_string)
		return;
	cp = cpu->chart;
	sys = gkrellm_get_current_chartdata(cpu->sys_cd);
	user = gkrellm_get_current_chartdata(cpu->user_cd);
	total = sys + user;
	if (!nice_time_unsupported)
		{
		nice = gkrellm_get_current_chartdata(cpu->nice_cd);
		if (!omit_nice_mode && !gkrellm_get_chartdata_hide(cpu->nice_cd))
			total += nice;
		}
	for (s = src_string; *s != '\0' && size > 0; ++s)
		{
		len = 1;
		if (*s == '$' && *(s + 1) != '\0')
			{
			t = -1;
			if ((c = *(s + 1)) == 'T')
				t = total;
			else if (c == 's')
				t = sys;
			else if (c == 'u')
				t = user;
			else if (c == 'n')
				t = nice;
			else if (c == 'L')
				len = snprintf(buf, size, "%s", cp->panel->label->string);
			else if (c == 'H')
				len = snprintf(buf, size, "%s", gkrellm_sys_get_host_name());
			else
				{
				*buf = *s;
				if (size > 1)
					{
					*(buf + 1) = *(s + 1);
					++len;
					}
				}
			if (t >= 0)
				{
				/* ChartData values have been scaled to the chart max scale
				|  of CPU_TICKS_PER_SECOND.
				*/
				t = ((200 * t / CPU_TICKS_PER_SECOND) + 1) / 2;
				if (t > 100)
					t = 100;
				len = snprintf(buf, size, "%d%%", t);
				}
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
draw_cpu_extra(CpuMon *cpu)
	{
	GkrellmChart	*cp = cpu->chart;
	gchar			buf[128];

	if (!cp)
		return;
	format_cpu_data(cpu, text_format_locale, buf, sizeof(buf));
	if (!cpu->new_text_format)
		gkrellm_chart_reuse_text_format(cp);
	cpu->new_text_format = FALSE;
	gkrellm_draw_chart_text(cp, style_id, buf);
	}

static void
cb_command_process(GkrellmAlert *alert, gchar *src, gchar *dst, gint len,
				CpuMon *cpu)
    {
    format_cpu_data(cpu, src, dst, len);
	}

static void
refresh_cpu_chart(CpuMon *cpu)
	{
	GkrellmChart	*cp	= cpu->chart;

	gkrellm_draw_chartdata(cp);
	if (cpu->extra_info)
		draw_cpu_extra(cpu);
	gkrellm_draw_chart_to_screen(cp);
	}

static void
draw_sensor_decals(CpuMon *cpu)
	{
	GkrellmPanel	*p = cpu->chart->panel;
	gchar			units;
	gfloat			t, f;
	gint			toggle;

	if (sensor_separate_mode && cpu->show_temperature && cpu->show_fan)
		{
		gkrellm_sensor_read_temperature(cpu->sensor_temp, &t, &units);
		gkrellm_sensor_draw_temperature_decal(p, cpu->sensor_decal, t, units);
		gkrellm_sensor_read_fan(cpu->sensor_fan, &f);
		gkrellm_sensor_draw_fan_decal(p, cpu->fan_decal, f);
		}
	else
		{
		toggle = _GK.time_now & 2;
		if (cpu->show_fan && (toggle || !cpu->show_temperature) )
			{
			gkrellm_sensor_read_fan(cpu->sensor_fan, &f);
			gkrellm_sensor_draw_fan_decal(p, cpu->sensor_decal, f);
			}
		else if (cpu->show_temperature && (!toggle || !cpu->show_fan) )
			{
			gkrellm_sensor_read_temperature(cpu->sensor_temp, &t, &units);
			gkrellm_sensor_draw_temperature_decal(p, cpu->sensor_decal, t,
							units);
			}
		}
	}

void
gkrellm_cpu_draw_sensors(gpointer sr)
	{
	GList	*list;
	CpuMon	*cpu;

	for (list = cpu_mon_list; list; list = list->next)
		{
		cpu = (CpuMon *) list->data;
		if (sr && sr != cpu->sensor_temp && sr != cpu->sensor_fan)
			continue;
		if (cpu->enabled)
			draw_sensor_decals(cpu);
		}
	}

static void
update_cpu(void)
	{
	GList			*list;
	CpuMon			*cpu;
	GkrellmChart	*cp;
	GkrellmPanel	*p;
	GkrellmKrell	*krell;
	gulong			total, krell_value, full_scale;
	gulong			alert_value, alert_total_diff;

	_GK.cpu_sys_activity = 0;
	if (composite_cpu)
		{
		composite_cpu->user = 0;
		composite_cpu->nice = 0;
		composite_cpu->sys = 0;
		composite_cpu->idle = 0;
		}
	(*read_cpu_data)();

	for (list = cpu_mon_list; list; list = list->next)
		{
		cpu = (CpuMon *) list->data;
		if (!cpu->enabled)
			continue;
		cp = cpu->chart;
		p = cp->panel;
		if (smp_mode == SMP_REAL_MODE)
			_GK.cpu_sys_activity += (int)(cpu->sys - cpu->sys_cd->previous);
		else if (list == cpu_mon_list)	/* Use composite cpu values */
			_GK.cpu_sys_activity = (int)(cpu->sys - cpu->sys_cd->previous);
		total = cpu->user + cpu->nice + cpu->sys + cpu->idle;
		if (GK.second_tick)
			{
			gkrellm_store_chartdata(cp, total, cpu->sys, cpu->user, cpu->nice);
			refresh_cpu_chart(cpu);

			alert_value = cpu->sys + cpu->user;
			if (alert_includes_nice)
				alert_value += cpu->nice;
			alert_total_diff = total - cpu->previous_alert_total;
			if (   alert_total_diff > 0
				&& (   ( cpu->is_composite && smp_mode == SMP_COMPOSITE_MODE)
					|| (!cpu->is_composite && smp_mode != SMP_COMPOSITE_MODE)
				   )
			   )
				gkrellm_check_alert(cpu->alert,
					100.0 * (gfloat) (alert_value - cpu->previous_alert_value)
					/ (gfloat) alert_total_diff);
			cpu->previous_alert_value = alert_value;
			cpu->previous_alert_total = total;
			}
		if (   (GK.two_second_tick && !sensor_separate_mode)
			|| (GK.five_second_tick && sensor_separate_mode)
		   )
			draw_sensor_decals(cpu);

		krell = KRELL(cp->panel);
		full_scale = total - cpu->previous_total;
		if (full_scale > 0)		/* Can be 0 for data from gkrellmd */
			gkrellm_set_krell_full_scale(krell, (gint) full_scale, 10);

		krell_value = cpu->sys + cpu->user;

		if (cpu->previous_total > 0)
			{
			if (!omit_nice_mode && !gkrellm_get_chartdata_hide(cpu->nice_cd))
				krell_value += cpu->nice;
			gkrellm_update_krell(p, krell, krell_value);
			gkrellm_panel_label_on_top_of_decals(p,
						gkrellm_alert_decal_visible(cpu->alert));
			gkrellm_draw_panel_layers(p);
			}
		cpu->previous_total = total;
		}
	}


static gint
cpu_expose_event(GtkWidget *widget, GdkEventExpose *ev)
	{
	GList			*list;
	GkrellmChart	*cp;
	GdkPixmap		*pixmap	= NULL;

	for (list = cpu_mon_list; list; list = list->next)
		{
		cp = ((CpuMon *) list->data)->chart;
		if (widget == cp->drawing_area)
			pixmap = cp->pixmap;
		else if (widget == cp->panel->drawing_area)
			pixmap = cp->panel->pixmap;
		if (pixmap)
			{
			gdk_draw_drawable(widget->window, gkrellm_draw_GC(1), pixmap,
					ev->area.x, ev->area.y, ev->area.x, ev->area.y,
					ev->area.width, ev->area.height);
			break;
			}
		}
	return FALSE;
	}


static gint
cb_cpu_extra(GtkWidget *widget, GdkEventButton *ev)
	{
	GList			*list;
	CpuMon			*cpu;
	GkrellmChart	*cp;

	for (list = cpu_mon_list; list; list = list->next)
		{
		cpu = (CpuMon *) list->data;
		cp = cpu->chart;
		if (widget != cpu->chart->drawing_area)
			continue;
		if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS)
			{
			cpu->extra_info = !cpu->extra_info;
			refresh_cpu_chart(cpu);
			gkrellm_config_modified();
			}
		else if (   ev->button == 3
				 || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS)
				)
			gkrellm_chartconfig_window_create(cpu->chart);
		break;
		}
	return FALSE;
	}

static void
setup_cpu_scaling(GkrellmChartconfig *cf)
	{
	gint	grids;

	grids = gkrellm_get_chartconfig_fixed_grids(cf);
	if (!grids)
		grids = FULL_SCALE_GRIDS;

	gkrellm_set_chartconfig_grid_resolution(cf,
				CPU_TICKS_PER_SECOND / grids);
	}

static gboolean
enable_cpu_visibility(CpuMon *cpu)
	{
	gint	enabled = cpu_enabled;

	if (n_smp_cpus > 0)
		{
		if (   (cpu->is_composite && smp_mode == SMP_REAL_MODE)
			|| (! cpu->is_composite && smp_mode == SMP_COMPOSITE_MODE)
		   )
			enabled = FALSE;
		}
	return gkrellm_chart_enable_visibility(cpu->chart, enabled, &cpu->enabled);
	}


static void
cb_cpu_temp_alert_trigger(GkrellmAlert *alert, CpuMon *cpu)
	{
	GkrellmAlertdecal	*ad;
	GkrellmDecal		*d;

	if (alert && cpu && cpu->chart)
		{
		ad = &alert->ad;
		d = cpu->sensor_decal;
		if (d)
			{
			ad->x = d->x - 1;
			ad->y = d->y - 1;
			ad->w = d->w + 2;
			ad->h = d->h + 2;
			gkrellm_render_default_alert_decal(alert);
			}
		alert->panel = cpu->chart->panel;
		}
	}

static void
cb_cpu_fan_alert_trigger(GkrellmAlert *alert, CpuMon *cpu)
	{
	GkrellmAlertdecal	*ad;
	GkrellmDecal		*d;

	if (alert && cpu && cpu->chart)
		{
		ad = &alert->ad;
		if (sensor_separate_mode)
			d = cpu->fan_decal;
		else
			d = cpu->sensor_decal;
		if (d)
			{
			ad->x = d->x - 1;
			ad->y = d->y - 1;
			ad->w = d->w + 2;
			ad->h = d->h + 2;
			gkrellm_render_default_alert_decal(alert);
			}
		alert->panel = cpu->chart->panel;
		}
	}

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
adjust_sensors_display(CpuMon *cpu, gint force)
	{
	GkrellmPanel	*p;
	GkrellmDecal	*ds, *df;
	GkrellmAlert	*alert;
	gint			position = 0;

	ds = cpu->sensor_decal;
	df = cpu->fan_decal;
	if (!ds || !df)
		return FALSE;
	/* The test for state change is decal state vs success at reading
	|  a temperature.
	*/
	p = cpu->chart->panel;
	cpu->show_temperature = cpu->show_fan = FALSE;
	if (!_GK.demo)
		{
		gkrellm_sensor_alert_connect(cpu->sensor_temp,
					cb_cpu_temp_alert_trigger, cpu);
		gkrellm_sensor_alert_connect(cpu->sensor_fan,
					cb_cpu_fan_alert_trigger, cpu);
		}

	/* If a fan alert is triggered, turn it off in case fan decal being used
	|  is changed.  The alert will just retrigger at next fan update.
	*/
	alert = gkrellm_sensor_alert(cpu->sensor_fan);
	gkrellm_reset_alert_soft(alert);

	if (cpu->sensor_temp || _GK.demo)
		cpu->show_temperature = TRUE;
	if (cpu->sensor_fan || _GK.demo)
		cpu->show_fan = TRUE;

	if (cpu->show_temperature || cpu->show_fan)
		{
		if (! gkrellm_is_decal_visible(ds) || force)
			gkrellm_make_decal_visible(p, ds);
		position = 0;
		}
	else
		{
		if (gkrellm_is_decal_visible(ds) || force)
			gkrellm_make_decal_invisible(p, ds);
		position = cpu->save_label_position;
		}
	if (cpu->show_fan && cpu->show_temperature && sensor_separate_mode)
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
		if (cpu->save_label_position >= 0)	/* Reassign position only if the */
			p->label->position = position;	/* original label was visible.   */
		gkrellm_draw_panel_label(p);
		draw_sensor_decals(cpu);
		gkrellm_draw_panel_layers(p);
		}
	return TRUE;
	}

static gint
cb_panel_press(GtkWidget *widget, GdkEventButton *ev)
	{
	if (ev->button == 3)
		gkrellm_open_config_window(mon_cpu);
	return FALSE;
	}

static GkrellmPiximage	*nice_data_piximage,
						*nice_data_grid_piximage;
static GdkPixmap		*nice_data_pixmap,
						*nice_data_grid_pixmap;
static GdkColor			nice_color,
						nice_grid_color;
static gchar			*nice_color_string,
						*nice_grid_color_string;

static void
cb_height(GkrellmChartconfig *cf, CpuMon *cpu)
	{
	GList			*list;
	GkrellmChart	*cp;
	gint			h, h_max = 0;

	h = gkrellm_get_chartconfig_height(cf);
	for (list = cpu_mon_list; list; list = list->next)
		{
		cp = ((CpuMon *) list->data)->chart;
		if (cp->h != h && config_tracking)
			{
			gkrellm_chartconfig_callback_block(cp->config, TRUE);
			gkrellm_set_chart_height(cp, h);
			gkrellm_chartconfig_callback_block(cp->config, FALSE);
			}
		if (cp->h > h_max)
			h_max = cp->h;
		}
	gkrellm_render_data_pixmap(nice_data_piximage,
				&nice_data_pixmap, &nice_color, h_max);
	}

static void
load_nice_data_piximages(void)
	{
	if (nice_time_unsupported)
		return;
	g_free(nice_color_string);
	g_free(nice_grid_color_string);
	nice_color_string = gkrellm_get_gkrellmrc_string("cpu_nice_color");
	nice_grid_color_string =
				gkrellm_get_gkrellmrc_string("cpu_nice_grid_color");
	gkrellm_map_color_string(nice_color_string, &nice_color);
	gkrellm_map_color_string(nice_grid_color_string, &nice_grid_color);
		
	if (nice_data_piximage)
		gkrellm_destroy_piximage(nice_data_piximage);
	if (nice_data_grid_piximage)
		gkrellm_destroy_piximage(nice_data_grid_piximage);
	nice_data_piximage = nice_data_grid_piximage = NULL;

	gkrellm_free_pixmap(&nice_data_pixmap);
	gkrellm_free_pixmap(&nice_data_grid_pixmap);

	gkrellm_load_piximage("nice", NULL, &nice_data_piximage, CPU_STYLE_NAME);
	gkrellm_load_piximage("nice_grid", NULL, &nice_data_grid_piximage,
					CPU_STYLE_NAME);
	}

static void
render_nice_data_pixmaps(void)
	{
	GList		*list;
	CpuMon		*cpu;
	gint		h_max;

	gkrellm_render_data_grid_pixmap(nice_data_grid_piximage,
				&nice_data_grid_pixmap, &nice_grid_color);

	h_max = 2;
	for (list = cpu_mon_list; list; list = list->next)
		{
		cpu = (CpuMon *) list->data;
		if (cpu->chart && (cpu->chart->h > h_max))
			h_max = cpu->chart->h;
		}
	gkrellm_render_data_pixmap(nice_data_piximage,
				&nice_data_pixmap, &nice_color, h_max);
	}

static void
create_cpu(GtkWidget *vbox, gint first_create)
	{
	CpuMon			*cpu;
	GkrellmChart	*cp;
	GkrellmPanel	*p;
	GkrellmStyle	*style;
	GList			*list;

	load_nice_data_piximages();
	for (list = cpu_mon_list; list; list = list->next)
		{
		cpu = (CpuMon *) list->data;

		/* Default to all cpu charts visible.  Correct this as last step.
		*/
		if (first_create)
			{
			/* don't really need the cpu->vbox unless I start destroying...
			*/
			cpu->vbox = gtk_vbox_new(FALSE, 0);
			gtk_container_add(GTK_CONTAINER(vbox), cpu->vbox);
			gtk_widget_show(cpu->vbox);
			cpu->chart = gkrellm_chart_new0();
			cpu->chart->panel = gkrellm_panel_new0();
			cpu->enabled = TRUE;
			}
		cp = cpu->chart;
		p = cp->panel;

		style = gkrellm_panel_style(style_id);
		gkrellm_create_krell(p, gkrellm_krell_panel_piximage(style_id), style);

		gkrellm_chart_create(cpu->vbox, mon_cpu, cp, &cpu->chart_config);
		gkrellm_set_draw_chart_function(cp, refresh_cpu_chart, cpu);
		cpu->sys_cd = gkrellm_add_default_chartdata(cp, _("sys time"));
		cpu->user_cd = gkrellm_add_default_chartdata(cp, _("user time"));
		if (!nice_time_unsupported)
			{
			if (   (nice_data_piximage && nice_data_grid_piximage)
				|| (nice_color_string && nice_grid_color_string)
			   )
				{
				render_nice_data_pixmaps();
				cpu->nice_cd = gkrellm_add_chartdata(cp, &nice_data_pixmap,
							nice_data_grid_pixmap, _("nice time"));
				}
			else
				cpu->nice_cd = gkrellm_add_default_chartdata(cp,
							_("nice time"));
			}
		gkrellm_set_chartdata_flags(cpu->nice_cd, CHARTDATA_ALLOW_HIDE);

		/* Since there is a constant Max value of the chart (CPU_TICKS/SEC)
		|  I control the grid resolution when fixed grids change.
		|  So don't call gkrellm_grid_resolution_adjustment() so there won't
		|  be a grid resolution part in the chart config.  Also, make sure
		|  auto grid res is off.
		*/
		gkrellm_set_chartconfig_auto_grid_resolution(cp->config, FALSE);
		gkrellm_chartconfig_fixed_grids_connect(cp->config,
					setup_cpu_scaling, NULL);
		gkrellm_chartconfig_height_connect(cp->config, cb_height, cpu);
		setup_cpu_scaling(cp->config);

		gkrellm_sensors_create_decals(p, style_id,
					&cpu->sensor_decal, &cpu->fan_decal);

		gkrellm_panel_configure(p, cpu->panel_label, style);
		gkrellm_panel_create(cpu->vbox, mon_cpu, p);

		cpu->save_label_position = p->label->position;
		if (cpu->sensor_decal)
			adjust_sensors_display(cpu, TRUE);

		gkrellm_alloc_chartdata(cp);
		enable_cpu_visibility(cpu);
		cpu->new_text_format = TRUE;

		if (first_create)
			{
			g_signal_connect(G_OBJECT (cp->drawing_area), "expose_event",
					G_CALLBACK(cpu_expose_event), NULL);
			g_signal_connect(G_OBJECT (p->drawing_area), "expose_event",
					G_CALLBACK(cpu_expose_event), NULL);

			g_signal_connect(G_OBJECT(cp->drawing_area), "button_press_event",
					G_CALLBACK(cb_cpu_extra), NULL);
			g_signal_connect(G_OBJECT(p->drawing_area), "button_press_event",
					G_CALLBACK(cb_panel_press), NULL);
			}
		else
			refresh_cpu_chart(cpu);
		gkrellm_setup_launcher(p, &cpu->launch, CHART_PANEL_TYPE, 4);
		}
	if (cpu_enabled)
		gkrellm_spacers_show(mon_cpu);
	else
		gkrellm_spacers_hide(mon_cpu);
	}


/* ------------------------------------------------------------------ */
#define	CPU_CONFIG_KEYWORD	"cpu"

static GtkWidget	*text_format_combo_box;
static GtkWidget	*smp_button[3];
#if !defined(WIN32)
static GtkWidget	*alert_config_nice_button;
#endif

static void
cb_alert_trigger(GkrellmAlert *alert, CpuMon *cpu)
	{
	GkrellmAlertdecal	*ad;
	GkrellmDecal		*ds, *df;

	alert->panel = cpu->chart->panel;
	ds = cpu->sensor_decal;
	df = cpu->fan_decal;
	if (gkrellm_is_decal_visible(ds) && !gkrellm_is_decal_visible(df))
		{
		ad = &alert->ad;
		ad->x = 0;
		ad->y = ds->y - 1;
		ad->w = ds->x - 1;
		ad->h = ds->h + 2;
		gkrellm_render_default_alert_decal(alert);
		}
	}

static void
dup_cpu_alert(void)
	{
	GList	*list;
	CpuMon	*cpu;

	for (list = cpu_mon_list; list; list = list->next)
		{
		cpu = (CpuMon *) list->data;
		gkrellm_alert_dup(&cpu->alert, cpu_alert);
		gkrellm_alert_trigger_connect(cpu->alert, cb_alert_trigger, cpu);
		gkrellm_alert_command_process_connect(cpu->alert,
					cb_command_process, cpu);
		}
	}

static void
create_alert(void)
	{
	GList		*list;
	CpuMon		*cpu;

	list = g_list_nth(cpu_mon_list, 0);
	if (!list)
		return;
	cpu = (CpuMon *) list->data;
	cpu_alert = gkrellm_alert_create(NULL, _("CPU"),
					_("Percent Usage"),
					TRUE, FALSE, TRUE,
					100, 10, 1, 10, 0);
	gkrellm_alert_delay_config(cpu_alert, 1, 60 * 60, 2);
	gkrellm_alert_config_connect(cpu_alert, cb_alert_config, NULL);
#if !defined(WIN32)
	gkrellm_alert_config_create_connect(cpu_alert,
							cb_alert_config_create, NULL);
#endif
	/* This alert is a master to be dupped and is itself never checked */
	}

static void
save_cpu_config(FILE *f)
	{
	GList	*list;
	CpuMon	*cpu;

	for (list = cpu_mon_list; list; list = list->next)
		{
		cpu = (CpuMon *) list->data;
		if (*(cpu->launch.command) != '\0')
			fprintf(f, "%s launch %s %s\n", CPU_CONFIG_KEYWORD,
						cpu->name, cpu->launch.command);
		if (*(cpu->launch.tooltip_comment) != '\0')
			fprintf(f, "%s tooltip_comment %s %s\n", CPU_CONFIG_KEYWORD,
						cpu->name, cpu->launch.tooltip_comment);
		fprintf(f, "%s extra_info %s %d\n", CPU_CONFIG_KEYWORD,
					cpu->name, cpu->extra_info);
		gkrellm_save_chartconfig(f, cpu->chart_config,
					CPU_CONFIG_KEYWORD, cpu->name);
		}
	fprintf(f, "%s enable %d\n", CPU_CONFIG_KEYWORD, cpu_enabled);
	fprintf(f, "%s smp_mode %d\n", CPU_CONFIG_KEYWORD, smp_mode);
	fprintf(f, "%s omit_nice_mode %d\n", CPU_CONFIG_KEYWORD, omit_nice_mode);
	fprintf(f, "%s config_tracking %d\n", CPU_CONFIG_KEYWORD, config_tracking);
	fprintf(f, "%s sensor_mode %d\n", CPU_CONFIG_KEYWORD,
			sensor_separate_mode);
	fprintf(f, "%s text_format %s\n", CPU_CONFIG_KEYWORD, text_format);
	if (cpu_alert)
		{
		gkrellm_save_alertconfig(f, cpu_alert, CPU_CONFIG_KEYWORD, NULL);
		fprintf(f, "%s alert_includes_nice %d\n", CPU_CONFIG_KEYWORD,
					alert_includes_nice);
		}
	}

static void
load_cpu_config(gchar *arg)
	{
	GList	*list;
	CpuMon	*cpu;
	gchar	config[32], item[CFG_BUFSIZE],
			cpu_name[32], command[CFG_BUFSIZE];
	gint	n;

	n = sscanf(arg, "%31s %[^\n]", config, item);
	if (n == 2)
		{
		if (!strcmp(config, "enable"))
			sscanf(item, "%d", &cpu_enabled);
		else if (!strcmp(config, "smp_mode"))
			sscanf(item, "%d\n", &smp_mode);
		else if (!strcmp(config, "omit_nice_mode"))
			sscanf(item, "%d\n", &omit_nice_mode);
		else if (!strcmp(config, "config_tracking"))
			sscanf(item, "%d\n", &config_tracking);
		else if (!strcmp(config, "sensor_mode"))
			sscanf(item, "%d\n", &sensor_separate_mode);
		else if (!strcmp(config, "text_format"))
			gkrellm_locale_dup_string(&text_format, item, &text_format_locale);
		else if (!strcmp(config, "alert_includes_nice"))
			sscanf(item, "%d\n", &alert_includes_nice);
		else if (!strcmp(config, GKRELLM_CHARTCONFIG_KEYWORD))
			{
			sscanf(item, "%31s %[^\n]", cpu_name, command);
			for (list = cpu_mon_list; list; list = list->next)
				{
				cpu = (CpuMon *) list->data;
				if (strcmp(cpu->name, cpu_name) == 0)
					gkrellm_load_chartconfig(&cpu->chart_config, command,
							nice_time_unsupported ? 2 : 3);
				}
			}
		else if (!strcmp(config, GKRELLM_ALERTCONFIG_KEYWORD))
			{
			if (!cpu_alert)
				create_alert();
			gkrellm_load_alertconfig(&cpu_alert, item);
			dup_cpu_alert();
			}
		else if (!strcmp(config, "extra_info"))
			{
			sscanf(item, "%31s %[^\n]", cpu_name, command);
			for (list = cpu_mon_list; list; list = list->next)
				{
				cpu = (CpuMon *) list->data;
				if (strcmp(cpu->name, cpu_name) == 0)
					sscanf(command, "%d\n", &cpu->extra_info);
				}
			}
		else if (!strcmp(config, "launch"))
			{
			sscanf(item, "%31s %[^\n]", cpu_name, command);
			for (list = cpu_mon_list; list; list = list->next)
				{
				cpu = (CpuMon *) list->data;
				if (strcmp(cpu->name, cpu_name) == 0)
					cpu->launch.command = g_strdup(command);
				}
			}
		else if (!strcmp(config, "tooltip_comment"))
			{
			sscanf(item, "%31s %[^\n]", cpu_name, command);
			for (list = cpu_mon_list; list; list = list->next)
				{
				cpu = (CpuMon *) list->data;
				if (strcmp(cpu->name, cpu_name) == 0)
					cpu->launch.tooltip_comment = g_strdup(command);
				}
			}
		}
	}

static void
cb_alert_config(GkrellmAlert *ap, gpointer data)
	{
#if !defined(WIN32)
	alert_includes_nice = GTK_TOGGLE_BUTTON(alert_config_nice_button)->active;
#endif
	dup_cpu_alert();
	}

#if !defined(WIN32)
static void
cb_alert_config_create(GkrellmAlert *ap, GtkWidget *vbox, gpointer data)
	{
	gkrellm_gtk_check_button(vbox, &alert_config_nice_button,
			alert_includes_nice, FALSE, 2,
			_("nice time"));
	}
#endif

static void
cb_set_alert(GtkWidget *button, gpointer data)
	{
	if (!cpu_alert)
		create_alert();
	gkrellm_alert_config_window(&cpu_alert);
	}

static gboolean
fix_panel(CpuMon *cpu)
	{
	gboolean	result;

	if ((result = adjust_sensors_display(cpu, FALSE)) && cpu->launch.button)
		{
		gkrellm_destroy_button(cpu->launch.button);
		cpu->launch.button = 
			gkrellm_put_label_in_panel_button(cpu->chart->panel,
				gkrellm_launch_button_cb, &cpu->launch, cpu->launch.pad);
		}
	return result;
	}

gboolean
gkrellm_cpu_set_sensor(gpointer sr, gint type, gint n)
	{
	CpuMon	*cpu;

	if (   (cpu = (CpuMon *) g_list_nth_data(cpu_mon_list, n)) == NULL
		|| !cpu->enabled
	   )
		return FALSE;

	if (type == SENSOR_TEMPERATURE)
		cpu->sensor_temp = sr;
	else if (type == SENSOR_FAN)
		cpu->sensor_fan = sr;
	else
		return FALSE;
	return fix_panel(cpu);
	}

static void
cb_sensor_separate(GtkWidget *button, gpointer data)
    {
	GList	*list;

	sensor_separate_mode = GTK_TOGGLE_BUTTON(button)->active;
	for (list = cpu_mon_list; list; list = list->next)
		fix_panel((CpuMon *) list->data);
	}

static void
cb_omit_nice(GtkWidget *button, gpointer data)
    {
	omit_nice_mode = GTK_TOGGLE_BUTTON(button)->active;
	}

static void
cb_text_format(GtkWidget *widget, gpointer data)
	{
	GList	*list;
	CpuMon	*cpu;
	gchar	*s;
	GtkWidget *entry;

	entry = gtk_bin_get_child(GTK_BIN(text_format_combo_box));
	s = gkrellm_gtk_entry_get_text(&entry);
	gkrellm_locale_dup_string(&text_format, s, &text_format_locale);
	for (list = cpu_mon_list; list; list = list->next)
		{
		cpu = (CpuMon *) list->data;
		cpu->new_text_format = TRUE;
		refresh_cpu_chart(cpu);
		}
	}

static void
cb_enable(GtkWidget *button, gpointer data)
    {
	GList	*list;
	CpuMon	*cpu;

	cpu_enabled = GTK_TOGGLE_BUTTON(button)->active;
	for (list = cpu_mon_list; list; list = list->next)
		{
		cpu = (CpuMon *) list->data;
		if (enable_cpu_visibility(cpu) && cpu->enabled)
			gkrellm_reset_and_draw_chart(cpu->chart);
		gkrellm_apply_launcher(&cpu->launch_entry, &cpu->tooltip_entry,
					cpu->chart->panel, &cpu->launch, gkrellm_launch_button_cb);
		gkrellm_reset_alert_soft(cpu->alert);
		}
	if (cpu_enabled)
		gkrellm_spacers_show(mon_cpu);
	else
		gkrellm_spacers_hide(mon_cpu);
	}

static void
cb_smp_mode(GtkWidget *button, gpointer data)
	{
	GList		*list;
	CpuMon		*cpu;
	gint		i = GPOINTER_TO_INT(data);
	gboolean	prev_enabled, rebuild_temps = FALSE, rebuild_fans = FALSE;

	if (GTK_TOGGLE_BUTTON(button)->active)
		smp_mode = i;
	for (list = cpu_mon_list; list; list = list->next)
		{
		cpu = (CpuMon *) list->data;
		prev_enabled = cpu->enabled;
		if (enable_cpu_visibility(cpu) && cpu->enabled)
			gkrellm_reset_and_draw_chart(cpu->chart);
		if (prev_enabled && !cpu->enabled)
			{
			if (cpu->sensor_temp)
				{
				gkrellm_sensor_reset_location(cpu->sensor_temp);
				rebuild_temps |= TRUE;
				}
			if (cpu->sensor_fan)
				{
				gkrellm_sensor_reset_location(cpu->sensor_fan);
				rebuild_fans |= TRUE;
				}
			cpu->sensor_temp = NULL;
			cpu->sensor_fan = NULL;
			fix_panel(cpu);
			}
		gkrellm_apply_launcher(&cpu->launch_entry, &cpu->tooltip_entry,
					cpu->chart->panel, &cpu->launch, gkrellm_launch_button_cb);
		}
	if (rebuild_temps || rebuild_fans)
		gkrellm_sensors_rebuild(rebuild_temps, rebuild_fans, FALSE);
	}

static void
cb_config_tracking(GtkWidget *button, gpointer data)
	{
	config_tracking = GTK_TOGGLE_BUTTON(button)->active;
	}

static void
cb_launch_entry(GtkWidget *widget, CpuMon *cpu)
	{
	gkrellm_apply_launcher(&cpu->launch_entry, &cpu->tooltip_entry,
				cpu->chart->panel, &cpu->launch, gkrellm_launch_button_cb);
	}

#define	DEFAULT_TEXT_FORMAT	"$T"

static gchar	*cpu_info_text[] =
{
N_("<h>Chart Labels\n"),
N_("Substitution variables for the format string for chart labels:\n"),
N_("\t$L    the CPU label\n"),
N_("\t$T    total CPU time percent usage\n"),
N_("\t$s    sys time percent usage\n"),
N_("\t$u    user time percent usage\n"),
N_("\t$n    nice time percent usage\n"),
"\n",
N_("Substitution variables may be used in alert commands.\n")
};

static void
create_cpu_tab(GtkWidget *tab_vbox)
	{
	GtkWidget	*tabs;
	GtkWidget	*button;
	GtkWidget	*hbox, *vbox, *vbox1;
	GtkWidget	*text;
	GtkWidget	*table;
	GSList		*group;
	GList		*list;
	CpuMon		*cpu;
	gchar		buf[128];
	gint		i;


	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

/* -- Options tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Options"));
	gkrellm_gtk_check_button_connected(vbox, NULL, cpu_enabled,
			FALSE, FALSE, 10,
			cb_enable, NULL,
			_("Enable CPU"));
	if (!nice_time_unsupported)
		gkrellm_gtk_check_button_connected(vbox, NULL,
				omit_nice_mode, FALSE, FALSE, 0,
				cb_omit_nice, NULL,
		_("Exclude nice CPU time from krell even if nice is shown on chart"));
	if (gkrellm_sensors_available())
		gkrellm_gtk_check_button_connected(vbox, NULL,
					sensor_separate_mode, FALSE, FALSE, 0,
					cb_sensor_separate, NULL,
		_("Draw fan and temperature values separately (not alternating)."));

	if (n_smp_cpus > 0)
		{
		gkrellm_gtk_check_button_connected(vbox, &button,
				config_tracking, FALSE, FALSE, 10,
				cb_config_tracking, NULL,
		_("Apply any CPU chart config height change to all CPU charts"));

		vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("SMP Charts Select"),
				4, 0, TRUE);

		button = gtk_radio_button_new_with_label(NULL, _("Real CPUs."));
		gtk_box_pack_start(GTK_BOX(vbox1), button, TRUE, TRUE, 0);
		smp_button[0] = button;
		group = gtk_radio_button_get_group(GTK_RADIO_BUTTON (button));

		button = gtk_radio_button_new_with_label(group, _("Composite CPU."));
		gtk_box_pack_start(GTK_BOX(vbox1), button, TRUE, TRUE, 0);
		group = gtk_radio_button_get_group(GTK_RADIO_BUTTON (button));
		smp_button[1] = button;

		button = gtk_radio_button_new_with_label(group,
					_("Composite and real"));
		gtk_box_pack_start(GTK_BOX(vbox1), button, TRUE, TRUE, 0);
		smp_button[2] = button;

		button = smp_button[smp_mode];
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);

		for (i = 0; i < 3; ++i)
			g_signal_connect(G_OBJECT(smp_button[i]), "toggled",
						G_CALLBACK(cb_smp_mode), GINT_TO_POINTER(i));
		}

/* -- Setup tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Format String for Chart Labels"),
				4, 0, TRUE);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);

	text_format_combo_box = gtk_combo_box_entry_new_text();
	gtk_box_pack_start(GTK_BOX(hbox), text_format_combo_box, TRUE, TRUE, 0);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		text_format);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		DEFAULT_TEXT_FORMAT);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		_("\\fu \\.$u\\n\\fs \\.$s"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		_("\\ww\\D2\\f\\au\\.$u\\D1\\f\\as\\.$s"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		_("\\ww\\D3\\f\\au\\.$u\\D0\\f\\as\\.$s"));
	if (!nice_time_unsupported)
		{
		gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
			"\\ww\\C\\f$L\\D5\\f\\an\\.$n\\D2\\f\\au\\.$u\\D1\\f\\as\\.$s");
		}
	gtk_combo_box_set_active(GTK_COMBO_BOX(text_format_combo_box), 0);
	g_signal_connect(G_OBJECT(GTK_COMBO_BOX(text_format_combo_box)), "changed",
			G_CALLBACK(cb_text_format), NULL);

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Launch Commands"),
				4, 0, TRUE);
	vbox1 = gkrellm_gtk_scrolled_vbox(vbox1, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	table = gkrellm_gtk_launcher_table_new(vbox1, n_cpus);
	for (i = 0, list = cpu_mon_list; list; list = list->next, ++i)
		{
		cpu = (CpuMon *) list->data;
		snprintf(buf, sizeof(buf), _("%s"), cpu->name);
		gkrellm_gtk_config_launcher(table, i,  &cpu->launch_entry,
				&cpu->tooltip_entry, buf, &cpu->launch);
		g_signal_connect(G_OBJECT(cpu->launch_entry), "changed",
				G_CALLBACK(cb_launch_entry), cpu);
		g_signal_connect(G_OBJECT(cpu->tooltip_entry), "changed",
				G_CALLBACK(cb_launch_entry), cpu);
		}

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gkrellm_gtk_alert_button(hbox, NULL, FALSE, FALSE, 4, TRUE,
			cb_set_alert, NULL);

/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	for (i = 0; i < sizeof(cpu_info_text)/sizeof(gchar *); ++i)
		{
		if (nice_time_unsupported && strstr(cpu_info_text[i], "nice"))
			continue;
		gkrellm_gtk_text_view_append(text, _(cpu_info_text[i]));
		}

	}

gchar *
gkrellm_cpu_get_sensor_panel_label(gint n)
	{
	CpuMon	*cpu;
	gchar	*s = "cpu?";

	cpu = (CpuMon *) g_list_nth_data(cpu_mon_list, n);
	if (cpu)
		s = cpu->panel_label;
	return s;
	}

GkrellmMonitor *
gkrellm_get_cpu_mon(void)
	{
	return mon_cpu;
	}

static GkrellmMonitor	monitor_cpu =
	{
	N_("CPU"),				/* Name, for config tab.	*/
	MON_CPU,			/* Id,  0 if a plugin		*/
	create_cpu,			/* The create function		*/
	update_cpu,			/* The update function		*/
	create_cpu_tab,		/* The config tab create function	*/
	NULL,				/* Apply the config function		*/

	save_cpu_config,	/* Save user conifg			*/
	load_cpu_config,	/* Load user config			*/
	CPU_CONFIG_KEYWORD,	/* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_cpu_monitor(void)
	{
	GList	*list;

	monitor_cpu.name = _(monitor_cpu.name);
	style_id = gkrellm_add_chart_style(&monitor_cpu, CPU_STYLE_NAME);
	gkrellm_locale_dup_string(&text_format, DEFAULT_TEXT_FORMAT,
				&text_format_locale);

	mon_cpu = &monitor_cpu;

	if (setup_cpu_interface())
		{
		for (list = cpu_mon_list; list; list = list->next)
			((CpuMon *)(list->data))->extra_info = TRUE;
		return &monitor_cpu;
		}
	return NULL;
	}

