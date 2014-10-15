/* GKrellM
|  Copyright (C) 1999-2014 Bill Wilson
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

/* Useful info:
|  http://mbm.livewiredev.com/
|		Look up boards here for sensor chip and temperature sensor type
|		so sensor[1,2,3] can be set correctly in sensors.conf.
*/

  /* On Linux, the sensor id_name includes the parent chip directory, so
  |  "lm78/temp" will be a id_name, and not just "temp".  This is to
  |  allow unique identification in case of multiple "temp" files.
  |  ie, more than 1 chip directory, each with a "temp" file.
  */

typedef struct _sensor
	{
	gchar		*name;			/* cpuX, mb, Vx name mapped to this sensor */
	gchar		*name_locale;	/* gdk_draw compat */
	gchar		*default_label;	/* Only voltages have default labels */
	gchar		*path;			/* Pathname to sensor data or device file */
	gchar		*id_name;		/* Unique sensor identifier for config */
	gint		type;

	gint		id;
	gint		iodev;
	gint		inter;

	gint		enabled;
	gint		group;
	gint		location;		/* default, Proc panel, or cpu panel */
	gfloat		factor,			/* Scale sensor reading		*/
				offset;			/* Add to sensor reading	*/
	gfloat		default_factor,
				default_offset;
	gchar		*vref_name;
	struct _sensor
				*vref;			/* A neg volt may be function of a ref volt */

	gboolean	has_config;

	gfloat		value,
				raw_value;
	gboolean	value_valid;

	GkrellmAlert *alert;
	void		(*cb_alert)();
	gpointer	cb_alert_data;
	gpointer	smon;
	}
	Sensor;


static GList	*sensor_list	= NULL;

static GList	*temp_order_list,	/* For ordering from the config. */
				*fan_order_list,
				*volt_order_list;

static gboolean	using_new_config,
				need_disk_temperature_update;

static gboolean	(*get_temperature)(gchar *name, gint id,
			gint iodev, gint inter, gfloat *t);
static gboolean	(*get_fan)(gchar *name, gint id,
			gint iodev, gint inter, gfloat *f);
static gboolean	(*get_voltage)(gchar *name, gint id,
			gint iodev, gint inter, gfloat *v);

static void read_sensors_config(void);
static gboolean	(*config_migrate)(gchar *current_name, gchar *config_name,
					gint current, gint config);

static gint		sensor_config_version;
static gint		sensor_current_sysdep_private;
static gint		sensor_config_sysdep_private;

void
gkrellm_sensors_client_divert(gboolean (*get_temp_func)(),
			gboolean (*get_fan_func)(), gboolean (*get_volt_func)())
	{
	get_temperature = get_temp_func;
	get_fan = get_fan_func;
	get_voltage = get_volt_func;
	}

void
gkrellm_sensors_config_migrate_connect(gboolean (*migrate_func)(),
			gint sysdep_private)
	{
	config_migrate = migrate_func;
	sensor_current_sysdep_private = sysdep_private;
	}

static gboolean
setup_sensor_interface(void)
	{
	if (!get_temperature && !_GK.client_mode && gkrellm_sys_sensors_init())
		{
		get_temperature = gkrellm_sys_sensors_get_temperature;
		get_fan = gkrellm_sys_sensors_get_fan;
		get_voltage = gkrellm_sys_sensors_get_voltage;
		}
	return get_temperature ? TRUE : FALSE;
	}

void
gkrellm_sensors_set_group(gpointer sr, gint group)
	{
	Sensor	*sensor = (Sensor *) sr;

	if (sensor)
		sensor->group = group;
	}

gpointer
gkrellm_sensors_add_sensor(gint type, gchar *sensor_path, gchar *id_name,
		gint id, gint iodev, gint inter,
		gfloat factor, gfloat offset, gchar *vref, gchar *default_label)
	{
	Sensor	*sensor;
	gchar	*r;

	if (!id_name || !*id_name || type < 0 || type > 2)
		return NULL;

	sensor = g_new0(Sensor, 1);
	sensor->id_name = g_strdup(id_name);

	if (sensor_path)
		sensor->path = g_strdup(sensor_path);
	else
		sensor->path = g_strdup(id_name);
	if (!default_label)
		{
		r = strrchr(id_name, '/');
		default_label = r ? r+1 : id_name;
		}
	gkrellm_locale_dup_string(&sensor->name, default_label,
				&sensor->name_locale);
	sensor->default_label = g_strdup(default_label);

	sensor->default_factor = factor;
	sensor->factor
			= (sensor->default_factor != 0.0 ? sensor->default_factor : 1.0);
	sensor->default_offset = sensor->offset = offset;
	sensor->type = type;
	sensor->id = id;
	sensor->iodev = iodev;
	sensor->inter = inter;
	if (type == SENSOR_VOLTAGE && vref)
		sensor->vref_name = g_strdup(vref);
	sensor_list = g_list_append(sensor_list, sensor);
	return (gpointer) sensor;
	}

/* ======================================================================== */
static gboolean	use_threads,
				thread_data_valid,
				units_fahrenheit,
				show_units = TRUE;

static gboolean thread_busy;

static gpointer
read_sensors_thread(void *data)
	{
	GList	*list;
	Sensor	*sensor;

	for (list = sensor_list; list; list = list->next)
		{
		sensor = (Sensor *) list->data;
		if (!sensor->enabled)
			continue;
		if (sensor->type == SENSOR_TEMPERATURE && get_temperature)
			(*get_temperature)(sensor->path, sensor->id,
				sensor->iodev, sensor->inter, &sensor->raw_value);
		if (sensor->type == SENSOR_FAN && get_fan)
			(*get_fan)(sensor->path, sensor->id,
				sensor->iodev, sensor->inter, &sensor->raw_value);
		if (sensor->type == SENSOR_VOLTAGE && get_voltage)
			(*get_voltage)(sensor->path, sensor->id,
				sensor->iodev, sensor->inter, &sensor->raw_value);
		}
	thread_busy = FALSE;
	return NULL;
	}

static void
run_sensors_thread(void)
	{
	GThread		*gth;

	if (thread_busy)
		return;
	thread_busy = TRUE;
	gth = g_thread_new("read_sensors", read_sensors_thread, NULL);
	g_thread_unref(gth);
	}

  /* Sort so that sensors are ordered: temp, fan, voltage.
  */
static gint
strcmp_sensor_path(Sensor *s1, Sensor *s2)
	{
	if (s1->type == SENSOR_TEMPERATURE && s2->type != SENSOR_TEMPERATURE)
		return -1;
	if (s1->type != SENSOR_TEMPERATURE && s2->type == SENSOR_TEMPERATURE)
		return 1;

	if (s1->type == SENSOR_FAN && s2->type != SENSOR_FAN)
		return -1;
	if (s1->type != SENSOR_FAN && s2->type == SENSOR_FAN)
		return 1;

	return strcmp(s1->id_name, s2->id_name);
	}

static void
append_sensor_to_order_list(Sensor *sr)
	{
	if (sr->type == SENSOR_TEMPERATURE)
		temp_order_list = g_list_append(temp_order_list, sr);
	else if (sr->type == SENSOR_FAN)
		fan_order_list = g_list_append(fan_order_list, sr);
	else if (sr->type == SENSOR_VOLTAGE)
		volt_order_list = g_list_append(volt_order_list, sr);
	}


  /* This is called as sensors are read from the config and I will want to
  |  re-order the sensors_list to reflect the config order. Re-ordering is
  |  done by appending found sensors to type specific lists and later the
  |  sensors_list will be rebuilt from the ordered type lists.  If the
  |  id_name is found in the sensor_list, assign the label to it.
  */
static Sensor *
map_sensor_label(gchar *label, gchar *name)
	{
	GList		*list;
	Sensor		*sr;

	for (list = sensor_list; list; list = list->next)
		{
		sr = (Sensor *) list->data;
		if (   !sr->has_config
		    && (   !strcmp(sr->id_name, name)
		        || (   config_migrate
		            && (*config_migrate)(sr->id_name, name,
							sensor_current_sysdep_private,
							sensor_config_sysdep_private)
		           )
		       )
		   )
			{
			gkrellm_locale_dup_string(&sr->name, label, &sr->name_locale);
			append_sensor_to_order_list(sr);
			sr->has_config = TRUE;
			return sr;
			}
		}
	return NULL;
	}

gboolean
gkrellm_sensors_available(void)
	{
	return (sensor_list || _GK.demo) ? TRUE : FALSE;
	}

  /* The cpu and proc monitors both need a couple of sensor decals
  |  created on their panels.  The left one will only display fan speeds
  |  while the right one will display both fan and temps depending on modes.
  */
void
gkrellm_sensors_create_decals(GkrellmPanel *p, gint style_id,
			GkrellmDecal **dsensor, GkrellmDecal **dfan)
	{
	GkrellmStyle		*style;
	GkrellmMargin		*m;
	GkrellmTextstyle	*ts;
	GkrellmDecal		*ds	= NULL,
						*df	= NULL;
	gint				w, w_avail;

	if (sensor_list || _GK.demo)
		{
		style = gkrellm_panel_style(style_id);
		m = gkrellm_get_style_margins(style);
		ts = gkrellm_panel_alt_textstyle(style_id);
		w_avail = gkrellm_chart_width() - m->left - m->right;

		df = gkrellm_create_decal_text(p, "8888", ts, style, -1, -1, 0);

		/* Sensor decal (fan and/or temp) carves out space remaining to right.
		|  Try to get enough for .1 deg resolution, otherwise what is left.
		*/
		w = gkrellm_gdk_string_width(ts->font, "188.8F") + ts->effect;
		if (w > w_avail - df->w - 3)
			w = gkrellm_gdk_string_width(ts->font, "88.8C") + ts->effect;

		ds = gkrellm_create_decal_text(p, "8.C", ts, style, -1, -1, w);
		ds->x = w_avail + m->left - w;
		df->x = m->left;
		}
	*dsensor = ds;
	*dfan = df;
	}

void
gkrellm_sensor_draw_fan_decal(GkrellmPanel *p, GkrellmDecal *d, gfloat f)
	{
	gchar	buf[8];
	gint	w;

	if (!p || !d)
		return;
	snprintf(buf, sizeof(buf), "%.0f", f);
	w = gkrellm_gdk_string_width(d->text_style.font, buf)
				+ d->text_style.effect;
	d->x_off = d->w - w;
	if (d->x_off < 0)
		d->x_off = 0;
	gkrellm_draw_decal_text(p, d, buf, 0);
	}

void
gkrellm_sensor_draw_temperature_decal(GkrellmPanel *p, GkrellmDecal *d,
				gfloat t, gchar units)
	{
	gchar	*s, buf[8];
	gint	w;

	if (!p || !d)
		return;
	snprintf(buf, sizeof(buf), "%.1f%c", t, units);
	if ((s = strchr(buf, '.')) == NULL)
		s = strchr(buf, ',');			/* Locale may use commas */
	w = gkrellm_gdk_string_width(d->text_style.font, buf)
				+ d->text_style.effect;
	if (w > d->w + 1)
		{
		snprintf(buf, sizeof(buf), "%.0f%c", t, units);
		w = gkrellm_gdk_string_width(d->text_style.font, buf)
				+ d->text_style.effect;
		}
	
	d->x_off = d->w - w;
	if (d->x_off < 0)
		d->x_off = 0;
	gkrellm_draw_decal_text(p, d, buf, 0 /* no longer used */);
	}

static Sensor *
lookup_sensor_from_id_name(gchar *name)
	{
	GList	*list;
	Sensor	*s;

	if (!name)
		return NULL;
	for (list = sensor_list; list; list = list->next)
		{
		s = (Sensor *) list->data;
		if (   !strcmp(s->id_name, name)
		    || (   config_migrate
		        && (*config_migrate)(s->id_name, name,
							sensor_current_sysdep_private,
							sensor_config_sysdep_private)
		       )
		   )
			return s;
		}
	return NULL;
	}

  /* Given a in0, in1, ... name as a reference to use for a sensor,
  |  find the sensor with that name for the same chip as sr.
  */
static Sensor *
lookup_vref(Sensor *sr, gchar *name)
	{
	GList	*list;
	Sensor	*sv;
	gchar	*s, buf[128];

	snprintf(buf, 96, "%s", sr->id_name);
	s = strrchr(buf, '/');
	if (s)
		++s;
	else
		s = buf;
	snprintf(s, 31, "%s", name);
	for (list = sensor_list; list; list = list->next)
		{
		sv = (Sensor *) list->data;
		if (   sv->type == SENSOR_VOLTAGE
			&& !strcmp(sv->id_name, buf)
		   )
			return sv;
		}
	return NULL;
	}

static void
cb_command_process(GkrellmAlert *alert, gchar *src, gchar *buf, gint size,
			Sensor *sensor)
	{
	gchar		c, *s, *fmt;
	gint		len;

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
			else if (c == 's')
				{
				if (sensor->type == SENSOR_FAN)
					fmt = "%.0f";
				else if (sensor->type == SENSOR_TEMPERATURE)
					fmt = "%.1f";
				else				/* SENSOR_VOLTAGE */
					fmt = "%.2f";
				len = snprintf(buf, size, fmt, sensor->value);
				}
			else if (c == 'l' || c == 'L')
				len = snprintf(buf, size, "%s", sensor->name_locale);
			++s;
			}
		else
			*buf = *s;
		size -= len;
		buf += len;
		}
	*buf = '\0';
	}

GkrellmAlert *
gkrellm_sensor_alert(gpointer sr)
	{
	if (!sr)
		return NULL;
	return ((Sensor *) sr)->alert;
	}

void
gkrellm_sensor_alert_connect(gpointer sr, void (*cb_func)(), gpointer data)
	{
	Sensor	*sensor = (Sensor *) sr;

	if (!sensor)
		return;
	sensor->cb_alert = cb_func;
	sensor->cb_alert_data = data;
	gkrellm_alert_trigger_connect(sensor->alert, cb_func, data);
	gkrellm_alert_command_process_connect(sensor->alert,
				cb_command_process, sensor);
	gkrellm_reset_alert_soft(sensor->alert);
	}

static gboolean
sensor_read_temperature(Sensor *sensor, gfloat *temp, gchar *units)
	{
	gfloat	t = 0;
	gint	found_temp = FALSE;

	if (sensor && get_temperature)
		{
		found_temp = thread_data_valid ? TRUE
			: (*get_temperature)(sensor->path, sensor->id,
					sensor->iodev, sensor->inter, &sensor->raw_value);
		sensor->value = sensor->raw_value * sensor->factor + sensor->offset;
		if (units_fahrenheit)
			sensor->value = 1.8 * sensor->value + 32.0;
		t = sensor->value;
		}
	if (! found_temp && _GK.demo)
		{
		t = 90.0 + (gfloat)(rand() & 0xf);
		found_temp = TRUE;
		}
	if (temp)
		*temp = t;
	if (units)
		{
		if (show_units)
			*units = units_fahrenheit ? 'F':'C';
	   	else
			*units = '\0';
		}
	if (sensor)
		gkrellm_debug(DEBUG_SENSORS, "sensor_temp: %s %s t=%.2f\n",
					sensor->name_locale, sensor->path, sensor->value);
	if (found_temp && sensor)
		gkrellm_check_alert(sensor->alert, sensor->value);
	return found_temp;
	}

gboolean
gkrellm_sensor_read_temperature(gpointer sr, gfloat *temp, gchar *units)
	{
	return sensor_read_temperature((Sensor *) sr, temp, units);
	}


static gboolean
sensor_read_fan(Sensor *sensor, gfloat *fan)
	{
	gfloat	f = 0;
	gint	found_fan = FALSE;

	if (sensor && get_fan)
		{
		found_fan = thread_data_valid ? TRUE
				: (*get_fan)(sensor->path, sensor->id,
						sensor->iodev, sensor->inter, &sensor->raw_value);
		sensor->value = sensor->raw_value * sensor->factor;
		f = sensor->value;
		}
	if (! found_fan && _GK.demo)
		{
		f = 4980 + (gfloat)(rand() & 0x3f);
		found_fan = TRUE;
		}
	if (fan)
		*fan = f;
	if (sensor)
		gkrellm_debug(DEBUG_SENSORS, "sensor_fan: %s %s rpm=%.0f\n",
					sensor->name_locale, sensor->path, sensor->value);
	if (found_fan && sensor)
		gkrellm_check_alert(sensor->alert, sensor->value);
	return found_fan;
	}

gboolean
gkrellm_sensor_read_fan(gpointer sr, gfloat *fan)
	{
	return sensor_read_fan((Sensor *) sr, fan);
	}


static gboolean
sensor_read_voltage(Sensor *sensor, gfloat *voltage)
	{
	gfloat		v = 0;
	gfloat		offset;
	gboolean	found_voltage = FALSE;

	if (sensor && get_voltage)
		{
		found_voltage = thread_data_valid ? TRUE
			: (*get_voltage)(sensor->path, sensor->id,
					sensor->iodev, sensor->inter, &sensor->raw_value);
		offset = sensor->offset;
		if (sensor->vref)	/* A negative voltage is level shifted by vref */
			offset *= sensor->vref->value;
		sensor->value = sensor->raw_value * sensor->factor + offset;
		v = sensor->value;
		}
	if (! found_voltage && _GK.demo)
		{
		v = 2.9 + (gfloat)(rand() & 0x7) * 0.1;
		found_voltage = TRUE;
		}
	if (voltage)
		*voltage = v;
	if (sensor)
		gkrellm_debug(DEBUG_SENSORS, "sensor_voltage: %s %s v=%.2f\n",
				sensor->name_locale, sensor->path, sensor->value);
	if (found_voltage && sensor)
		gkrellm_check_alert(sensor->alert, sensor->value);
	return found_voltage;
	}

gboolean
gkrellm_sensor_read_voltage(gpointer sr, gfloat *voltage)
	{
	return sensor_read_voltage((Sensor *) sr, voltage);
	}

/* =================================================================== */
/* The sensors monitor */

static void		sensor_reset_optionmenu(Sensor *sensor);

#define	SENSOR_STYLE_NAME	"sensors"

/* Temperature and fan sensors can be located on different panels depending
|  on the sensor group.
*/
#define	SENSOR_PANEL_LOCATION	0

#define	PROC_PANEL_LOCATION		1	/* SENSOR_GROUP_MAINBOARD */
#define	CPU_PANEL_LOCATION		2	/* cpu0 if smp */

#define	DISK_PANEL_LOCATION		1	/* SENSOR_GROUP_DISK	*/


#define DO_TEMP	1
#define	DO_FAN	1
#define	DO_VOLT	1

typedef struct
	{
	GkrellmPanel	**panel;
	GkrellmDecal	*name_decal,
					*sensor_decal;
	Sensor			*sensor;
	}
	SensorMon;

static GtkWidget
				*temp_vbox,
				*fan_vbox,
				*volt_vbox;

static GList	*volt_list,
				*temperature_list,
				*fan_list,
				*disk_temperature_list;

static GkrellmPanel
				*pVolt,
				*pTemp,
				*pFan;

static gint		style_id;
static gint		volt_mon_width,
				volt_mon_height,
				volt_name_width,
				volt_bezel_width;


  /* Display modes */
#define	DIGITAL_WITH_LABELS	0
#define	DIGITAL_NO_LABELS	1
#define	N_DISPLAY_MODES		2

#define	MONITOR_PAD		6
#define	NAME_PAD		4

GkrellmMonitor	*mon_sensors;
GkrellmMonitor	*mon_config_sensors;

static GkrellmPiximage
				*bezel_piximage;

static GkrellmStyle
				*bezel_style;		/* Just for the bezel image border */

static gint		display_mode,
				have_negative_volts;

static gint		minus_width;		/* If will be drawing neg voltages */

  /* If drawing '-' sign, grub a pixel or two to tighten the layout */
static gint		pixel_grub;


  /* Avoid writing decimal values into the config to avoid possible
  |  locale changing decimal point breakage (decimal point can be '.' or ',')
  */
#define	SENSOR_FLOAT_FACTOR		10000.0
static gfloat		sensor_float_factor = 1.0,
					gkrellm_float_factor = 1.0;

gboolean
gkrellm_sensor_reset_location(gpointer sr)
	{
	GList		*list;
	Sensor		*sensor;
	gboolean	result = FALSE;

	if (sr)
		{
		for (list = sensor_list; list; list = list->next)
			{
			sensor = (Sensor *) list->data;
			if (sr == sensor)
				{
				sensor->location = SENSOR_PANEL_LOCATION;
				sensor_reset_optionmenu(sensor);
				result = TRUE;
				break;
				}
			}
		}
	return result;
	}


static void
sensor_relocation_error(gchar *pname)
	{
	gchar	*msg;

	msg = g_strdup_printf(
				_("Can't find a %s panel to relocate sensor to."),
				pname ? pname : "?");
	gkrellm_config_message_dialog(NULL, msg);
	g_free(msg);
	}

  /* When moving off some other panel, reset that panel.
  */
static void
sensor_reset_location(Sensor *sr)
	{
	if (sr->group == SENSOR_GROUP_MAINBOARD)
		{
		if (sr->location == PROC_PANEL_LOCATION)
			gkrellm_proc_set_sensor(NULL, sr->type);
		else if (sr->location >= CPU_PANEL_LOCATION)
			gkrellm_cpu_set_sensor(NULL, sr->type,
						sr->location - CPU_PANEL_LOCATION);
		}
	else if (sr->group == SENSOR_GROUP_DISK)
		{
		if (sr->location == DISK_PANEL_LOCATION)
			gkrellm_disk_temperature_remove(sr->id_name);
		}
	}

void
gkrellm_sensors_interface_remove(gint _interface)
	{
	GList		*list;
	Sensor		*sensor;
	gboolean	removed_one;

	do
		{
		removed_one = FALSE;
		for (list = sensor_list; list; list = list->next)
			{
			sensor = (Sensor *) list->data;
			if (sensor->inter == _interface)
				{
				sensor_reset_location(sensor);
				g_free(sensor->id_name);
				g_free(sensor->path);
				g_free(sensor->name);
				g_free(sensor->default_label);
				g_free(sensor->vref_name);
				sensor_list = g_list_remove(sensor_list, sensor);
				g_free(sensor);
				removed_one = TRUE;
				break;
				}
			}
		}
	while (removed_one);		
	}

static void
add_sensor_monitor(Sensor *sr, GkrellmPanel **p, GList **smon_list)
	{
	SensorMon		*smon;
	gfloat			t;
	gchar			units;
	gboolean		set_loc = FALSE;

	sr->smon = NULL;
	if (!sr->enabled)
		return;

	if (sr->location != SENSOR_PANEL_LOCATION)
		{
		if (sr->group == SENSOR_GROUP_MAINBOARD)
			{
			if (sr->location == PROC_PANEL_LOCATION)
				set_loc = gkrellm_proc_set_sensor(sr, sr->type);
			else
				set_loc = gkrellm_cpu_set_sensor(sr, sr->type,
							sr->location - CPU_PANEL_LOCATION);
			}
		else if (sr->group == SENSOR_GROUP_DISK)
			{
			if (sr->location == DISK_PANEL_LOCATION)
				{
				gkrellm_freeze_alert(sr->alert);
				sensor_read_temperature(sr, &t, &units);
				gkrellm_thaw_alert(sr->alert);
				set_loc = gkrellm_disk_temperature_display((gpointer) sr,
							sr->id_name, t, units);
				if (set_loc)
					disk_temperature_list =
							g_list_append(disk_temperature_list, sr);
				}
			}
		if (set_loc)
			return;
		sr->location = SENSOR_PANEL_LOCATION;
		}
	smon = g_new0(SensorMon, 1);
	smon->sensor = sr;
	smon->panel = p;		/* Alerts need a GkrellmPanel ** */
	*smon_list = g_list_append(*smon_list, smon);
	sr->smon = (gpointer) smon;
	}

static void
make_sensor_monitor_lists(gboolean do_temp, gboolean do_fan, gboolean do_volt)
	{
	GList	*list;
	Sensor	*sr;

	if (do_temp)
		{
		gkrellm_free_glist_and_data(&temperature_list);
		g_list_free(disk_temperature_list);
		disk_temperature_list = NULL;
		}
	if (do_fan)
		gkrellm_free_glist_and_data(&fan_list);
	if (do_volt)
		gkrellm_free_glist_and_data(&volt_list);

	for (list = sensor_list; list; list = list->next)
		{
		sr = (Sensor *) list->data;
		if (do_temp && sr->type == SENSOR_TEMPERATURE)
			add_sensor_monitor(sr, &pTemp, &temperature_list);
		if (do_fan && sr->type == SENSOR_FAN)
			add_sensor_monitor(sr, &pFan, &fan_list);
		if (do_volt && sr->type == SENSOR_VOLTAGE)
			{
			if (!sr->has_config && sr->vref_name)
				sr->vref = lookup_vref(sr, sr->vref_name);
			add_sensor_monitor(sr, &pVolt, &volt_list);
			}
		}
	}

#include "pixmaps/sensors/bg_volt.xpm"

static void
cb_alert_trigger(GkrellmAlert *alert, SensorMon *smon)
	{
	GkrellmAlertdecal	*ad;
	GkrellmDecal		*d;

	ad = &alert->ad;
	alert->panel = *smon->panel;

	/* Make the GkrellmAlertdecal show up under the sensor decal
	*/
	d = smon->sensor_decal;
	if (d)
		{
		ad->x = d->x - 2;
		ad->y = d->y - 2;
		ad->w = d->w + 3;
		ad->h = d->h + 4;
		gkrellm_render_default_alert_decal(alert);
		}
	}


static void
draw_bezels(GkrellmPanel *p, GList *smon_list, gint w, gint h, gint x_adjust)
	{
	GList			*list;
	GkrellmBorder	*b		= &bezel_style->border;
	SensorMon		*smon;
	GkrellmDecal	*dv;
	gint			x;

	if (!bezel_piximage)
		return;
	for (list = smon_list; list; list = list->next)
		{
		smon = (SensorMon *) list->data;
		dv = smon->sensor_decal;
		x = dv->x + x_adjust;
		if (w == 0)
			w = b->left + dv->w + b->right - x_adjust;
		if (h == 0)
			h = b->top + b->bottom + dv->h;
		gkrellm_paste_piximage(bezel_piximage, p->bg_pixmap,
				x - b->left, dv->y - b->top, w, h);
		gkrellm_paste_piximage(bezel_piximage, p->pixmap,
				x - b->left, dv->y - b->top, w, h);
		}
	gdk_draw_drawable(p->bg_text_layer_pixmap, _GK.draw1_GC, p->bg_pixmap,
				0, 0,  0, 0,  p->w, p->h);
	}


static gboolean
any_negative_volts(void)
	{
	GList		*list;
	Sensor		*s;
	SensorMon	*volt;
	gfloat		v;
	gboolean	tmp, result = FALSE;

	/* This routine can be called before any volt decals exist, but reading
	|  voltages can trigger alerts which expect to find decals.  Hence freeze.
	*/
	tmp = thread_data_valid;
	thread_data_valid = FALSE;	/* Need results immediately */
	for (list = volt_list; list; list = list->next)
		{
		volt = (SensorMon *) list->data;
		gkrellm_freeze_alert(volt->sensor->alert);
		s = volt->sensor->vref;
		if (s && s->value == 0)
			sensor_read_voltage(s, &v);
		sensor_read_voltage(volt->sensor, &v);
		gkrellm_thaw_alert(volt->sensor->alert);
		if (v < 0.0)
			{
			result = TRUE;
			break;
			}
		}
	thread_data_valid = tmp;
	return result;
	}

static void
make_volt_decals(GkrellmPanel *p, GkrellmStyle *style)
	{
	GList				*list;
	GkrellmBorder		*b	= &bezel_style->border;
	Sensor				*sensor;
	SensorMon			*volt;
	GkrellmDecal		*dv, *dn;
	GkrellmTextstyle	*ts_volt, *ts_name;
	gchar				*fmt;
	gint				w_volt;

	ts_name = gkrellm_meter_alt_textstyle(style_id);
	ts_volt = gkrellm_meter_textstyle(style_id);

	volt_mon_width = 0;
	volt_mon_height = 0;
	volt_name_width = 0;
	w_volt = 0;

	minus_width = 0;
	have_negative_volts = FALSE;
	fmt = "8.88";
	if (any_negative_volts())
		{
		have_negative_volts = TRUE;
		minus_width = 1;
		fmt = "-8.88";
		}

	for (list = volt_list; list; list = list->next)
		{
		volt = (SensorMon *) list->data;
		sensor = volt->sensor;
		if (display_mode == DIGITAL_WITH_LABELS)
			{
			volt->name_decal = dn = gkrellm_create_decal_text(p,
						volt->sensor->name_locale, ts_name, style, 0, 0, 0);
			if (dn->w > volt_name_width)
				volt_name_width = dn->w;
			}
		dv = gkrellm_create_decal_text(p, fmt, ts_volt, style, 0, 0, 0);
		volt->sensor_decal = dv;
		if (minus_width == 1)
			minus_width = gkrellm_gdk_string_width(dv->text_style.font, "-");
		w_volt = dv->w;			/* Same for all volt decals */
		if (dv->h > volt_mon_height)
			volt_mon_height = dv->h;

		sensor->cb_alert = cb_alert_trigger;
		sensor->cb_alert_data = volt;
		gkrellm_alert_trigger_connect(sensor->alert, cb_alert_trigger, volt);
		gkrellm_alert_command_process_connect(sensor->alert,
					cb_command_process, sensor);
		gkrellm_reset_alert_soft(sensor->alert);
		}
	pixel_grub = minus_width ? 1 : 0;
	volt_bezel_width = b->left + w_volt + b->right - pixel_grub;
	volt_mon_height += b->top + b->bottom;

	/* If name decal I let bezel left border encroach into NAME_PAD space
	*/
	if (volt_name_width)
		volt_mon_width = volt_name_width + NAME_PAD + w_volt + b->right;
	else
		volt_mon_width = w_volt;	/* borders encroach into MONITOR_PAD */
	}

static void
layout_volt_decals(GkrellmPanel *p, GkrellmStyle *style)
	{
	GList			*list;
	SensorMon		*volt;
	GkrellmDecal	*dv, *dn;
	GkrellmMargin	*m;
	gint			x, y, w, c, n, cols;

	m = gkrellm_get_style_margins(style);
	w = gkrellm_chart_width() - m->left - m->right;
	cols = (w + MONITOR_PAD) / (volt_mon_width + MONITOR_PAD);
	if (cols < 1)
		cols = 1;
	n = g_list_length(volt_list);
	if (cols > n)
		cols = n;;
	volt_mon_width = w / cols;		/* spread them out */
	x = (w - cols * volt_mon_width) / 2 + m->left;
		
	gkrellm_get_top_bottom_margins(style, &y, NULL);
	c = 0;
	for (list = volt_list; list; list = list->next)
		{
		volt = (SensorMon *) list->data;
		dn = volt->name_decal;
		dv = volt->sensor_decal;
		/* Right justify the volt decal in each volt_mon field
		*/
		dv->x = x + (c+1) * volt_mon_width - dv->w - bezel_style->border.right;
		if (cols > 1 && !dn)
			dv->x -= (volt_mon_width - volt_bezel_width) / 2;
		dv->y = y + bezel_style->border.top;
		if (dn)
			{
			if (cols == 1)
				dn->x = m->left;
			else
				dn->x = dv->x - volt_name_width - NAME_PAD;
			dn->y = y + bezel_style->border.top;
			if (dn->h < dv->h)
				dn->y += (dv->h - dn->h + 1) / 2;
			}
		if (++c >= cols)
			{
			c = 0;
			y += volt_mon_height;
			}
		}
	}

static void
update_disk_temperatures(void)
	{
	GList		*list;
	Sensor		*sr;
	gfloat		t;
	gchar		units;
	gboolean	display_failed = FALSE;

	for (list = disk_temperature_list; list; list = list->next)
		{
		sr = (Sensor *) list->data;
		sensor_read_temperature(sr, &t, &units);
		if (!gkrellm_disk_temperature_display((gpointer) sr, sr->id_name,
					t, units))
			{
			/* disk panel was disabled, so put temp back on sensors panel
			*/
			display_failed = TRUE;
			sr->location = SENSOR_PANEL_LOCATION;
			sensor_reset_optionmenu(sr);
			}
		}
	if (display_failed)
		gkrellm_sensors_rebuild(TRUE, FALSE, FALSE);
	}

  /* Squeeze name decal text into a smaller font if it would overlap the
  |  sensor decal.  With smaller fonts, the y_ink value may be smaller which
  |  would bump the text upward.  So adjust decal offset by difference in
  |  y_ink value.  (GKrellM text decal heights don't include the y_ink
  |  space).
  */
static gchar *
name_text_fit(Sensor *s, GkrellmDecal *dn, GkrellmDecal *ds)
	{
	gchar	*string;
	gint	x_limit, w0, w1, y_ink0, y_ink1, h0, h1;

	x_limit = ds->x;

	/* Check for '<' in case user is doing his own markup
	*/
	if (*(s->name_locale) != '<' && dn->x + dn->w > x_limit)
		{
		gkrellm_text_markup_extents(dn->text_style.font, s->name_locale,
				strlen(s->name_locale), &w0, NULL, NULL, &y_ink0);
		string = g_strdup_printf("<small>%s</small>", s->name_locale);
		gkrellm_text_markup_extents(dn->text_style.font, string,
				strlen(string), &w1, &h0, NULL, &y_ink1);
		h1 = h0;
		if (dn->x + w1 > x_limit)
			{
			g_free(string);
			string = g_strdup_printf("<small><small>%s</small></small>",
						s->name_locale);
			gkrellm_text_markup_extents(dn->text_style.font, string,
					strlen(string), &w1, &h1, NULL, &y_ink1);
			}
		gkrellm_decal_text_set_offset(dn, 0,
					y_ink0 - y_ink1 + (h0 - h1 + 1) / 2);
		}
	else
		{
		gkrellm_decal_text_set_offset(dn, 0, 0);
		string = g_strdup(s->name_locale);
		}
	return string;
	}


static void
draw_temperatures(gboolean draw_name)
	{
	GList		*list;
	SensorMon	*smon;
	Sensor		*sensor;
	gfloat		t;
	gchar		*name, units;

	if (!pTemp)
		return;
	for (list = temperature_list; list; list = list->next)
		{
		smon = (SensorMon *) list->data;
		sensor = smon->sensor;

		if (draw_name && smon->name_decal)
			{
			name = name_text_fit(sensor, smon->name_decal, smon->sensor_decal);
			gkrellm_draw_decal_markup(pTemp, smon->name_decal, name);
			g_free(name);
			}
		if (smon->sensor_decal)
			{
			sensor_read_temperature(sensor, &t, &units);
			gkrellm_sensor_draw_temperature_decal(pTemp, smon->sensor_decal,
					t, units);
			}
		}
	gkrellm_draw_panel_layers(pTemp);
	}

static void
draw_fans(gboolean draw_name)
	{
	GList		*list;
	SensorMon	*smon;
	Sensor		*sensor;
	gchar		*name;
	gfloat		f;

	if (!pFan)
		return;
	for (list = fan_list; list; list = list->next)
		{
		smon = (SensorMon *) list->data;
		sensor = smon->sensor;

		if (draw_name && smon->name_decal)
			{
			name = name_text_fit(sensor, smon->name_decal, smon->sensor_decal);
			gkrellm_draw_decal_markup(pFan, smon->name_decal, name);
			g_free(name);
			}
		if (smon->sensor_decal)
			{
			sensor_read_fan(sensor, &f);
			gkrellm_sensor_draw_fan_decal(pFan, smon->sensor_decal, f);
			}
		}
	gkrellm_draw_panel_layers(pFan);
	}

  /* If s is NULL, draw 'em all
  */
static void
draw_voltages(Sensor *s, gint do_names)
	{
	GList			*list;
	SensorMon		*volt;
	Sensor			*sensor;
	GkrellmDecal	*ds, *dn;
	gchar			*name, *fmt, buf[32];
	gfloat			v;

	if (!pVolt)
		return;
	for (list = volt_list; list; list = list->next)
		{
		volt = (SensorMon *) list->data;
		sensor = volt->sensor;
		if (s && s != sensor)
			continue;
		sensor->value_valid = FALSE;	/* In case vref monitoring stops */
		dn = volt->name_decal;
		ds = volt->sensor_decal;
		if (do_names && dn)
			{
			name = name_text_fit(sensor, dn, ds);
			gkrellm_draw_decal_markup(pVolt, dn, name);
			g_free(name);
			}
		if (ds)
			{
			if (sensor->vref && !sensor->vref->value_valid)
				sensor_read_voltage(sensor->vref, NULL);
			sensor_read_voltage(sensor, &v);
			sensor->value_valid = TRUE;
			if ((v < 10.0 && v > 0.0) || (v > -10.0 && v < 0.0))
				fmt = "%.2f";
			else
				fmt = "%.1f";
			snprintf(buf, sizeof(buf), fmt, v);
			ds->x_off = (v < 0.0) ? 0 : minus_width;
			gkrellm_draw_decal_text(pVolt, ds, buf, -1);
			}
		}
	gkrellm_draw_panel_layers(pVolt);
	}

static void
update_sensors(void)
	{
	static gboolean		first_time_done;

	if (!GK.five_second_tick && first_time_done)
		{
		if (need_disk_temperature_update)	/* delayed until disks created */
			update_disk_temperatures();
		need_disk_temperature_update = FALSE;
		return;
		}
	if (use_threads)
		{
		thread_data_valid = TRUE;
		run_sensors_thread();
		}
	draw_temperatures(FALSE);
	draw_fans(FALSE);
	draw_voltages(NULL, FALSE);
	update_disk_temperatures();
	first_time_done = TRUE;
	}

static gint
expose_event(GtkWidget *widget, GdkEventExpose *ev, GkrellmPanel *p)
	{
	gdk_draw_drawable(widget->window, gkrellm_draw_GC(1), p->pixmap,
				ev->area.x, ev->area.y, ev->area.x, ev->area.y,
				ev->area.width, ev->area.height);
	return FALSE;
	}

static gint
cb_panel_press(GtkWidget *widget, GdkEventButton *ev, GkrellmPanel *p)
	{
	if (ev->button == 3)
		gkrellm_open_config_window(mon_config_sensors);
	return FALSE;
	}

static GkrellmBorder	default_bezel_border = {1,1,1,1};

static void
assign_textstyles(GList *smon_list, GkrellmTextstyle **ts_name, GkrellmTextstyle **ts_sensor,
		gchar *format)
	{
	GList		*list;
	GkrellmStyle *style;
	GkrellmMargin *margin;
	Sensor		*sensor;
	SensorMon	*smon;
	GkrellmTextstyle *ts, *ts_alt;
	gint		w, w_name, w_sensor;

	style = gkrellm_meter_style(style_id);
	margin = gkrellm_get_style_margins(style);
	ts = gkrellm_copy_textstyle(gkrellm_meter_textstyle(style_id));
	ts_alt = gkrellm_copy_textstyle(gkrellm_meter_alt_textstyle(style_id));
	w = gkrellm_chart_width() - margin->left - margin->right;
	w_sensor = gkrellm_gdk_string_width(ts->font, format);
	w_sensor += bezel_style->border.left + bezel_style->border.right;
	for (list = smon_list; list; list = list->next)
		{
		smon = (SensorMon *)list->data;
		sensor = smon->sensor;
		w_name = gkrellm_gdk_string_width(ts_alt->font, sensor->name_locale);
		if (w_name + w_sensor >  w - 2)
			{
			ts->font = ts_alt->font;	/* downsize the sensor font */
			break;
			}
		}
	*ts_name = ts_alt;		/* Caller must free these */
	*ts_sensor = ts;
	}

static gint
adjust_decal_positions(SensorMon *smon)
	{
	gint	y, d, h_pad;

	h_pad = bezel_style->border.top + bezel_style->border.bottom;
	d = smon->sensor_decal->h - smon->name_decal->h;
	y = smon->sensor_decal->y + smon->sensor_decal->h + h_pad;
	if (d >= 0)
		smon->name_decal->y += (d + 1) / 2;
	else
		{
		if (h_pad < -d)
			y = smon->name_decal->y + smon->name_decal->h;
		smon->sensor_decal->y += -d / 2;
		}
	return y;
	}

static void
make_temperature_panel(GtkWidget *vbox, gint first_create)
	{
	Sensor				*sensor;
	SensorMon			*smon = NULL;
	GkrellmStyle		*style;
	GkrellmMargin		*m;
	GkrellmDecal		*d;
	GList				*list;
	GkrellmTextstyle	*ts_sensor, *ts_name;
	gchar				*format;
	gint				y;

	if (!pTemp)
		return;
	style = gkrellm_meter_style(style_id);
	m = gkrellm_get_style_margins(style);
	if (show_units)
		format = units_fahrenheit ? "188.8F" : "88.8C";
	else
		format = units_fahrenheit ? "188.8" : "88.8";
	assign_textstyles(temperature_list, &ts_name, &ts_sensor, format);
	gkrellm_get_top_bottom_margins(style, &y, NULL);
	y += bezel_style->border.top;
	for (list = temperature_list; list; list = list->next)
		{
		smon = (SensorMon *) list->data;
		sensor = smon->sensor;
		d = gkrellm_create_decal_text(pTemp, format,
				ts_sensor, style, -1, y, 0);
		d->x = gkrellm_chart_width() - d->w - m->right - 1;
		smon->sensor_decal = d;

		smon->name_decal = gkrellm_create_decal_text(pTemp,
					sensor->name_locale, ts_name, style, -1, y, 0);
		y = adjust_decal_positions(smon);
		sensor->cb_alert = cb_alert_trigger;
		sensor->cb_alert_data = smon;
		gkrellm_alert_trigger_connect(sensor->alert, cb_alert_trigger, smon);
		gkrellm_alert_command_process_connect(sensor->alert,
					cb_command_process, sensor);
		gkrellm_reset_alert_soft(sensor->alert);
		}
	g_free(ts_name);
	g_free(ts_sensor);
	gkrellm_panel_configure(pTemp, NULL, style);
	if (smon && smon->sensor_decal->y + smon->sensor_decal->h >
		smon->name_decal->y + smon->name_decal->h - bezel_style->border.bottom
	   )
		gkrellm_panel_configure_add_height(pTemp, bezel_style->border.bottom);
	gkrellm_panel_create(vbox, mon_sensors, pTemp);
	draw_bezels(pTemp, temperature_list, 0, 0, 1);
	if (first_create)
		{
		g_signal_connect(G_OBJECT(pTemp->drawing_area), "expose_event",
				G_CALLBACK(expose_event), pTemp);
		g_signal_connect(G_OBJECT(pTemp->drawing_area), "button_press_event",
				G_CALLBACK(cb_panel_press), pTemp);
		}
	draw_temperatures(TRUE);
	}

static void
make_fan_panel(GtkWidget *vbox, gint first_create)
	{
	Sensor			*sensor;
	SensorMon		*smon = NULL;
	GkrellmStyle	*style;
	GkrellmMargin	*m;
	GkrellmDecal	*d;
	GList			*list;
	GkrellmTextstyle *ts_sensor, *ts_name;
	gchar			*format;
	gint			y;

	if (!pFan)
		return;
	style = gkrellm_meter_style(style_id);
	m = gkrellm_get_style_margins(style);
	format = "8888";
	assign_textstyles(temperature_list, &ts_name, &ts_sensor, format);
	gkrellm_get_top_bottom_margins(style, &y, NULL);
	y += bezel_style->border.top;
	for (list = fan_list; list; list = list->next)
		{
		smon = (SensorMon *) list->data;
		sensor = smon->sensor;
		d = gkrellm_create_decal_text(pFan, format,
				ts_sensor, style, -1, y, 0);
		d->x = gkrellm_chart_width() - d->w - m->right - 1;
		smon->sensor_decal = d;

		smon->name_decal = gkrellm_create_decal_text(pFan, sensor->name_locale,
					ts_name, style, -1, y, 0);
		y = adjust_decal_positions(smon);
		sensor->cb_alert = cb_alert_trigger;
		sensor->cb_alert_data = smon;
		gkrellm_alert_trigger_connect(sensor->alert, cb_alert_trigger, smon);
		gkrellm_alert_command_process_connect(sensor->alert,
					cb_command_process, sensor);
		gkrellm_reset_alert_soft(sensor->alert);
		}
	g_free(ts_name);
	g_free(ts_sensor);
	gkrellm_panel_configure(pFan, NULL, style);
	if (smon && smon->sensor_decal->y + smon->sensor_decal->h >
		smon->name_decal->y + smon->name_decal->h - bezel_style->border.bottom
	   )
		gkrellm_panel_configure_add_height(pFan, bezel_style->border.bottom);
	gkrellm_panel_create(vbox, mon_sensors, pFan);
	draw_bezels(pFan, fan_list, 0, 0, 0);
	if (first_create)
		{
		g_signal_connect(G_OBJECT(pFan->drawing_area), "expose_event",
				G_CALLBACK(expose_event), pFan);
		g_signal_connect(G_OBJECT(pFan->drawing_area), "button_press_event",
				G_CALLBACK(cb_panel_press), pFan);
		}
	draw_fans(TRUE);
	}

static void
make_volt_panel(GtkWidget *vbox, gint first_create)
	{
	GkrellmStyle	*style;

	if (!pVolt)
		return;
	style = gkrellm_meter_style(style_id);
	make_volt_decals(pVolt, style);
	layout_volt_decals(pVolt, style);
	
	gkrellm_panel_configure(pVolt, NULL, style);

	/* Make the bottom margin reference against the bottom volt decals
	|  bezel image.  The volt decal height does not include the bezel so
	|  gkrellm_panel_configure() did not account for the bezel.
	*/
	gkrellm_panel_configure_add_height(pVolt, bezel_style->border.bottom);
	gkrellm_panel_create(vbox, mon_sensors, pVolt);

	draw_bezels(pVolt, volt_list,
			volt_bezel_width, volt_mon_height, pixel_grub);

	if (first_create)
		{
		g_signal_connect(G_OBJECT(pVolt->drawing_area), "expose_event",
				G_CALLBACK(expose_event), pVolt);
		g_signal_connect(G_OBJECT(pVolt->drawing_area), "button_press_event",
				G_CALLBACK(cb_panel_press), pVolt);
		}
	draw_voltages(NULL, TRUE);
	}


static void
destroy_sensors_monitor(gboolean do_temp, gboolean do_fan, gboolean do_volt)
	{
	if (do_temp)
		{
		gkrellm_panel_destroy(pTemp);
		pTemp = NULL;
		}
	if (do_fan)
		{
		gkrellm_panel_destroy(pFan);
		pFan = NULL;
		}
	if (do_volt)
		{
		gkrellm_panel_destroy(pVolt);
		pVolt = NULL;
		}
	}

static void
create_sensors_monitor(gboolean do_temp, gboolean do_fan, gboolean do_volt,
			gboolean first_create)
	{
	make_sensor_monitor_lists(do_temp, do_fan, do_volt);
	if (do_temp && temperature_list)
		{
		if (!pTemp)
			pTemp = gkrellm_panel_new0();
		make_temperature_panel(temp_vbox, first_create);
		}
	if (do_fan && fan_list)
		{
		if (!pFan)
			pFan = gkrellm_panel_new0();
		make_fan_panel(fan_vbox, first_create);
		}
	if (do_volt && volt_list)
		{
		if (!pVolt)
			pVolt = gkrellm_panel_new0();
		make_volt_panel(volt_vbox, first_create);
		}
	if (temperature_list || fan_list || volt_list)
		gkrellm_spacers_show(mon_sensors);
	else
		gkrellm_spacers_hide(mon_sensors);
	}

void
gkrellm_sensors_rebuild(gboolean do_temp, gboolean do_fan, gboolean do_volt)
	{
	destroy_sensors_monitor(do_temp, do_fan, do_volt);
	create_sensors_monitor(do_temp, do_fan, do_volt, TRUE);
	}

static void
create_sensors(GtkWidget *vbox, gint first_create)
	{
	gchar			**xpm;
	static gboolean	config_loaded;

	if (!config_loaded)
		read_sensors_config();
	if (first_create)
		{
		temp_vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), temp_vbox, FALSE, FALSE, 0);
		gtk_widget_show(temp_vbox);

		fan_vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), fan_vbox, FALSE, FALSE, 0);
		gtk_widget_show(fan_vbox);

		volt_vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), volt_vbox, FALSE, FALSE, 0);
		gtk_widget_show(volt_vbox);

		bezel_style = gkrellm_style_new0();
		}
	else		/* To be done after disk panels created */
		need_disk_temperature_update = TRUE;

	config_loaded = TRUE;

	/* Here is where I define the volt panel theme image extensions.  I ask
	|  for a theme extension image:
	|      THEME_DIR/sensors/bg_volt.png
	|  and for a border for it from the gkrellmrc in the format:
	|      set_piximage_border sensors_bg_volt l,r,t,b
	| There is no default for bg_volt image, ie it may end up being NULL. 
	*/
	xpm = gkrellm_using_default_theme() ? bg_volt_xpm : NULL;
	if (bezel_piximage)
		gkrellm_destroy_piximage(bezel_piximage);
	bezel_piximage = NULL;
	gkrellm_load_piximage("bg_volt", xpm, &bezel_piximage, SENSOR_STYLE_NAME);
	if (!gkrellm_set_gkrellmrc_piximage_border("sensors_bg_volt", bezel_piximage, bezel_style))
		bezel_style->border = default_bezel_border;

	create_sensors_monitor(DO_TEMP, DO_FAN, DO_VOLT, first_create);
	}

  /* FIXME: monitor_sensors and monitor_config_sensors should be combined,
  |  but the issue is apply_sensors_config() must be called before the CPU
  |  and Proc apply, and I want create_sensors() called after the CPU and Proc
  |  create.  So for now, two GkrellmMonitor structs and have two sensor
  |  monitor add_builtins() in main.c.
  */
static GkrellmMonitor	monitor_sensors =
	{
	N_("Sensors"),		/* Voltage config handled in Sensors tab */
	MON_VOLTAGE,		/* Id,  0 if a plugin		*/
	create_sensors,		/* The create function		*/
	update_sensors,		/* The update function		*/
	NULL,				/* The config tab create function	*/
	NULL,				/* Voltage apply handled in sensors apply */

	NULL,				/* Voltage save config is in sensors save */
	NULL,				/* Voltage load config is in sensors load */
	NULL,				/* config keyword - use sensors */

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_sensor_monitor(void)
	{
	if (!sensor_list)
		return NULL;
	monitor_sensors.name = _(monitor_sensors.name);
	style_id = gkrellm_add_meter_style(&monitor_sensors, SENSOR_STYLE_NAME);
	mon_sensors = &monitor_sensors;
	return &monitor_sensors;
	}

/* =================================================================== */
/* Config for sensors monitor */

  /* Don't use the user-config.  Save into sensors-config and only if there
  |  is a sensor_list.  This preserves configs across a possible sensors
  |  modules load screw up.
  |
  |  2.2.3 sets sensor_config_version to 1 to allow sensor relocation
  |  to composite CPU on a SMP machine.
  |
  |  2.1.15 scales sensor factor/offset values by SENSOR_FLOAT_FACTOR to avoid
  |  writting decimal points in the config.  This is not backwards compatible
  |  with the pre 2.1.15 sensor_config format hence the config file name
  |  change to sensor-config.  But sensor_config is forward compatible
  |  since the float factor defaults to 1.0.
  */
#define	SENSOR_CONFIG_VERSION			1

#define	SENSOR_CONFIG_KEYWORD		"sensor"
#define	SENSOR_CONFIG_FILE			"sensor-config"
#define	SENSOR_2_1_14_CONFIG_FILE	"sensors_config"

typedef struct
	{
	gchar		*config_keyword,
				*config_label;
	gboolean	value;
	void		(*func)(gboolean value);
	}
	SysdepOption;

static void		cb_alert_config(GkrellmAlert *ap, Sensor *sr);

static GList	*sysdep_option_list;

static void
create_sensor_alert(Sensor *s)
	{
	if (s->type == SENSOR_VOLTAGE)
		s->alert = gkrellm_alert_create(NULL, s->name,
				_("Sensor Volt Limits"),
				TRUE, TRUE, TRUE, 20, -20, 0.01, 0.5, 2);
	else if (s->type == SENSOR_TEMPERATURE)
		s->alert = gkrellm_alert_create(NULL, s->name,
				_("Sensor Temperature Limits (in displayed degree units)"),
				TRUE, FALSE, TRUE, 300, 0, 1.0, 5.0, 1);
	else if (s->type == SENSOR_FAN)
		s->alert = gkrellm_alert_create(NULL, s->name,
				_("Sensor Fan RPM Limits"),
				FALSE, TRUE, TRUE, 20000, 0, 100, 1000, 0);
	else
		return;
	gkrellm_alert_delay_config(s->alert, 5, 60, 0);
	gkrellm_alert_trigger_connect(s->alert, s->cb_alert, s->cb_alert_data);
	gkrellm_alert_command_process_connect(s->alert, cb_command_process, s);
	gkrellm_alert_config_connect(s->alert, cb_alert_config, s);
	}

void
gkrellm_sensors_sysdep_option(gchar *keyword, gchar *label, void (*func)())
	{
	SysdepOption	*so = NULL;

	so = g_new0(SysdepOption, 1);
	sysdep_option_list = g_list_append(sysdep_option_list, so);
	so->config_keyword = g_strdup(keyword);
	so->config_label = g_strdup(label);
	so->func = func;
	}

static void
save_sensors_config(FILE *f_not_used)
	{
	FILE			*f;
	GList			*list;
	Sensor			*s;
	SysdepOption	*so;
	gchar			*config, quoted_name[128], buf[128];
	gfloat			factor, offset;

	if (!sensor_list || _GK.no_config)
		return;
	snprintf(buf, sizeof(buf), "%s/%s", GKRELLM_DIR, SENSOR_CONFIG_FILE);
	config = gkrellm_make_config_file_name(gkrellm_homedir(), buf);
	f = g_fopen(config, "w");
	g_free(config);
	if (!f)
		return;

	fprintf(f, "%s sensor_config_version %d\n",
				SENSOR_CONFIG_KEYWORD, SENSOR_CONFIG_VERSION);
	fprintf(f, "%s sensor_sysdep_private %d\n",
				SENSOR_CONFIG_KEYWORD, sensor_current_sysdep_private);
	fprintf(f, "%s sensor_float_factor %.0f\n",
				SENSOR_CONFIG_KEYWORD, SENSOR_FLOAT_FACTOR);
	fprintf(f, "%s gkrellm_float_factor %.0f\n",
				SENSOR_CONFIG_KEYWORD, GKRELLM_FLOAT_FACTOR);
	for (list = sysdep_option_list; list; list = list->next)
		{
		so = (SysdepOption *) list->data;
		fprintf(f, "%s sysdep_option %s %d\n", SENSOR_CONFIG_KEYWORD,
					so->config_keyword, so->value);
		}

	for (list = sensor_list; list; list = list->next)
		{
		s = (Sensor *) list->data;
		if (s->name && *(s->name))
			{
			snprintf(quoted_name, sizeof(quoted_name), "\"%s\"", s->id_name);
			factor = (s->default_factor != 0.0 ? s->factor : 0.0);
			offset = (s->default_factor != 0.0 ? s->offset : 0.0);
			fprintf(f, "%s \"%s\" %s %.0f %.0f %d %d\n",
				SENSOR_CONFIG_KEYWORD,
				s->name, quoted_name,
				factor * SENSOR_FLOAT_FACTOR,
				offset * SENSOR_FLOAT_FACTOR,
				s->enabled, s->location);
			if (s->alert)
				gkrellm_save_alertconfig(f, s->alert,
						SENSOR_CONFIG_KEYWORD, quoted_name);
			}
		}
	for (list = sensor_list; list; list = list->next)
		{
		s = (Sensor *) list->data;
		if (s->vref)
			fprintf(f, "%s vref \"%s\" \"%s\"\n", SENSOR_CONFIG_KEYWORD,
					s->id_name, s->vref->id_name);
		}

	fprintf(f, "%s units_fahrenheit %d\n", SENSOR_CONFIG_KEYWORD,
				units_fahrenheit);
	fprintf(f, "%s show_units %d\n", SENSOR_CONFIG_KEYWORD,
				show_units);
	fprintf(f, "%s volt_display_mode %d\n", SENSOR_CONFIG_KEYWORD,
				display_mode);
	/* _GK.mbmon_port is handled in config.c so that the port can be
	|  loaded early for sensor initialization.
	*/
	fclose(f);
	}

static void
load_sensors_config(gchar *arg)
	{
	Sensor			*s;
	SysdepOption	*so;
	GList			*list;
	gchar			config[32], item[CFG_BUFSIZE], item1[CFG_BUFSIZE];
	gchar			label[64], id_name[CFG_BUFSIZE];
	gint			n;
	gfloat			f	= 1.0,
					o	= 0.0;
	gint			e	= 0,
					location = 0;
	gfloat			save_factor;

	n = sscanf(arg, "%31s %[^\n]", config, item);
	if (n != 2)
		return;
	gkrellm_debug(DEBUG_SENSORS, "load_sensors_config: <%s> <%s>\n", config,
		item);
	if (!strcmp(config, "sensor_config_version"))
		sscanf(item, "%d", &sensor_config_version);
	else if (!strcmp(config, "sensor_sysdep_private"))
		sscanf(item, "%d", &sensor_config_sysdep_private);
	else if (!strcmp(config, "units_fahrenheit"))
		sscanf(item, "%d", &units_fahrenheit);
	else if (!strcmp(config, "show_units"))
		sscanf(item, "%d", &show_units);
	else if (!strcmp(config, "volt_display_mode"))
		sscanf(item, "%d", &display_mode);
	else if (!strcmp(config, "sensor_float_factor"))
		sscanf(item, "%f", &sensor_float_factor);
	else if (!strcmp(config, "gkrellm_float_factor"))
		sscanf(item, "%f", &gkrellm_float_factor);
	else if (!strcmp(config, "vref"))
		{
		if (   sscanf(item, "\"%63[^\"]\" \"%64[^\"]\"", id_name, item1) == 2
			&& (s = lookup_sensor_from_id_name(id_name)) != NULL
		   )
			s->vref = lookup_sensor_from_id_name(item1);
		}
	else if (!strcmp(config, "sysdep_option"))
		{
		if (sscanf(item, "%63s %[^\n]", id_name, item1) == 2)
			{
			so = NULL;
			for (list = sysdep_option_list; list; list = list->next)
				{
				so = (SysdepOption *) list->data;
				if (!strcmp(so->config_keyword, id_name))
					break;
				so = NULL;
				}
			if (so && so->func)
				{
				so->value = atoi(item1);
				(*so->func)(so->value);
				}
			}
		}
	else if (!strcmp(config, GKRELLM_ALERTCONFIG_KEYWORD))
		{
		if (   sscanf(item, "\"%63[^\"]\" %[^\n]", id_name, item1) == 2
			&& (s = lookup_sensor_from_id_name(id_name)) != NULL
		   )
			{
			/* Since config files may be copied around, make sure to use the
			|  gkrellm float factor in effect when the sensors config was
			|  created.
			*/
			save_factor = _GK.float_factor;
			_GK.float_factor = gkrellm_float_factor;
			if (!s->alert)
				create_sensor_alert(s);
			gkrellm_load_alertconfig(&s->alert, item1);
			_GK.float_factor = save_factor;
			}
		}
	else if (   sscanf(arg, "\"%63[^\"]\" \"%[^\"]\" %f %f %d %d",
						label, id_name, &f, &o, &e, &location) > 1
			 && (s = map_sensor_label(label, id_name)) != NULL
			)
		{
		if (f != 0.0 && s->default_factor != 0.0)
			{
			s->factor = f / sensor_float_factor;
			s->offset = o / sensor_float_factor;
			}
		s->enabled = e;

		if (s->type == SENSOR_VOLTAGE)
			s->location = 0;
		else
			{
			s->location = location;
			if (   sensor_config_version == 0 && gkrellm_smp_cpus() > 0
				&& location > PROC_PANEL_LOCATION
			   )
				/* gkrellm < 2.2.3 did not allow relocating to composite
				|  CPU if on a SMP machine.  But with hyperthreading, user
				|  may want to do this.
				*/
				s->location += 1;
			}
		if (!using_new_config && s->type != SENSOR_VOLTAGE)
			s->enabled = TRUE;		/* Old config enabled with a label */
		}
	if (display_mode < 0 || display_mode >= N_DISPLAY_MODES)
		display_mode = N_DISPLAY_MODES - 1;
	}

static void
read_sensors_config(void)
	{
	FILE	*f;
	Sensor	*sr;
	GList	*list;
	gchar	*config;
	gchar	buf[CFG_BUFSIZE];

	snprintf(buf, sizeof(buf), "%s/%s", GKRELLM_DIR, SENSOR_CONFIG_FILE);
	config = gkrellm_make_config_file_name(gkrellm_homedir(), buf);
	f = g_fopen(config, "r");
	g_free(config);

	if (!f)
		{
		snprintf(buf, sizeof(buf), "%s/%s", GKRELLM_DIR, SENSOR_2_1_14_CONFIG_FILE);
		config = gkrellm_make_config_file_name(gkrellm_homedir(), buf);
		f = g_fopen(config, "r");
		g_free(config);
		}
	if (f)
		{
		using_new_config = TRUE;
		while (fgets(buf, sizeof(buf), f))
			load_sensors_config(buf + strlen(SENSOR_CONFIG_KEYWORD) + 1);
		fclose(f);
		}

	/* In case not all sensors are in sensor_config (user edited?)
	*/
	for (list = sensor_list; list; list = list->next)
		{
		sr = (Sensor *) list->data;
		if (sr->has_config)		/* Was in sensor_config and is already	*/
			continue;			/* appended to an order_list			*/
		append_sensor_to_order_list(sr);
		}
	g_list_free(sensor_list);
	sensor_list = temp_order_list;
	sensor_list = g_list_concat(sensor_list, fan_order_list);
	sensor_list = g_list_concat(sensor_list, volt_order_list);
	}

enum
	{
	NAME_COLUMN,
	ENABLE_COLUMN,
	LABEL_COLUMN,
	SENSOR_COLUMN,
	VISIBLE_COLUMN,
	IMAGE_COLUMN,
	N_COLUMNS
	};

static GtkTreeModel			*sensor_model;
static GtkTreeView			*treeview;
static GtkTreeRowReference	*row_reference;
static GtkTreeSelection		*selection;


static GtkWidget	*optionmenu;

static GtkWidget	*display_mode_button[2];
static GtkWidget	*factor_spin_button,
					*offset_spin_button;
static GtkWidget	*alert_button,
					*mbmon_port_entry;

static Sensor		*dragged_sensor;

static gint			sensor_last_group;

static gboolean		(*original_row_drop_possible)();



static void
set_tree_store_model_data(GtkTreeStore *tree, GtkTreeIter *iter, Sensor *s)
	{
	if (!s)
		return;
	gtk_tree_store_set(tree, iter,
			NAME_COLUMN, s->id_name ? s->id_name : "??",
			ENABLE_COLUMN, s->enabled,
			LABEL_COLUMN, s->name ? s->name : "??",
			SENSOR_COLUMN, s,
			VISIBLE_COLUMN, TRUE,
			-1);
	if (s->alert)
		gtk_tree_store_set(tree, iter,
				IMAGE_COLUMN, gkrellm_alert_pixbuf(),
				-1);
	}

static void
append_sensors_to_model(GtkTreeStore *tree, GtkTreeIter *citer,
			GtkTreeIter *iter, gint type)
	{
	GList	*list;
	Sensor	*s;

	for (list = sensor_list; list; list = list->next)
		{
		s = (Sensor *) list->data;
		if (s->type != type)
			continue;
		gtk_tree_store_append(tree, citer, iter);
		set_tree_store_model_data(tree, citer, s);
		}
	}

static GtkTreeModel *
create_model(void)
	{
	GtkTreeStore	*tree;
	GtkTreeIter		iter, citer;

	tree = gtk_tree_store_new(N_COLUMNS,
				G_TYPE_STRING,
                G_TYPE_BOOLEAN,
                G_TYPE_STRING,
                G_TYPE_POINTER,
				G_TYPE_BOOLEAN,
				GDK_TYPE_PIXBUF
				);

	gtk_tree_store_append(tree, &iter, NULL);
	gtk_tree_store_set(tree, &iter,
				NAME_COLUMN, _("Temperatures"),
				VISIBLE_COLUMN, FALSE,
				-1);
	append_sensors_to_model(tree, &citer, &iter, SENSOR_TEMPERATURE);

	gtk_tree_store_append(tree, &iter, NULL);
	gtk_tree_store_set(tree, &iter,
				NAME_COLUMN, _("Fans"),
				VISIBLE_COLUMN, FALSE,
				-1);
	append_sensors_to_model(tree, &citer, &iter, SENSOR_FAN);

	gtk_tree_store_append(tree, &iter, NULL);
	gtk_tree_store_set(tree, &iter,
				NAME_COLUMN, _("Voltages"),
				VISIBLE_COLUMN, FALSE,
				-1);
	append_sensors_to_model(tree, &citer, &iter, SENSOR_VOLTAGE);
	return GTK_TREE_MODEL(tree);
	}

void
gkrellm_sensors_model_update(void)
	{
	GtkTreeModel	*model;

	if (!gkrellm_config_window_shown())
		return;
	model = sensor_model;
	sensor_model = create_model();
	gtk_tree_view_set_model(treeview, sensor_model);
	if (model)
		g_object_unref(G_OBJECT(model));
	}

static void
change_row_reference(GtkTreeModel *model, GtkTreePath *path)
	{
	gtk_tree_row_reference_free(row_reference);
	if (model && path)
		row_reference = gtk_tree_row_reference_new(model, path);
	else
		row_reference = NULL;
	}

static Sensor *
get_referenced_sensor(void)
	{
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	Sensor			*s;

	if (!row_reference)
		return NULL;
	model = gtk_tree_view_get_model(treeview);
	path = gtk_tree_row_reference_get_path(row_reference);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter,
			SENSOR_COLUMN, &s, -1);
	return s;
	}

static gboolean
get_child_iter(GtkTreeModel *model, gchar *parent_node, GtkTreeIter *citer)
	{
	GtkTreePath		*path;
	GtkTreeIter		iter;

	path = gtk_tree_path_new_from_string(parent_node);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	return gtk_tree_model_iter_children(model, citer, &iter);
	}


  /* Callback for a created or destroyed alert.  Find the sensor in the model
  |  and set the IMAGE_COLUMN.
  */
static void
cb_alert_config(GkrellmAlert *ap, Sensor *sr)
	{
	GtkTreeModel	*model;
	GtkTreeIter		iter;
	Sensor 			*s;
	GdkPixbuf		*pixbuf;
	gchar			node[2];
	gint			i;

	if (!gkrellm_config_window_shown())
		return;
	model = gtk_tree_view_get_model(treeview);
	pixbuf = ap->activated ? gkrellm_alert_pixbuf() : NULL;
	for (i = 0; i < 3; ++i)
		{
		node[0] = '0' + i;		/* toplevel temp, fan, or volt node */
		node[1] = '\0';
		if (get_child_iter(model, node, &iter))
			do
				{
				gtk_tree_model_get(model, &iter, SENSOR_COLUMN, &s, -1);
				if (s != sr)
					continue;
				gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
								IMAGE_COLUMN, pixbuf, -1);
				return;
				}
			while (gtk_tree_model_iter_next(model, &iter));
		}
	}

  /* Allow destination drops only on depth 2 paths and don't allow drops from
  |  source depths of 1 (top level nodes).  Also disallow drags from one sensor
  |  type to another. Note: from some reason if I allow drops on depth 3 nodes
  |  (destination is on top of a second level node) I am not getting
  |  "drag_end" callbacks.
  */
static gboolean
row_drop_possible(GtkTreeDragDest *drag_dest, GtkTreePath *path,
			GtkSelectionData *selection_data)
	{
	gint			*src_indices, *dst_indices;
	GtkTreePath		*src_path;

	if (!row_reference)
		return FALSE;

	src_path = gtk_tree_row_reference_get_path(row_reference);
	src_indices = gtk_tree_path_get_indices(src_path);
	dst_indices = gtk_tree_path_get_indices(path);
//g_debug("drop path: indices=[%d,%d]:%d, path=%s\n",
//	dst_indices[0], dst_indices[1], gtk_tree_path_get_depth(path),
//	gtk_tree_path_to_string(path));

	if (   gtk_tree_path_get_depth(src_path) == 1	/* Dragging top level */
		|| gtk_tree_path_get_depth(path) != 2
		|| src_indices[0] != dst_indices[0]		/* sensor types don't match */
	   )
		return FALSE;

	return (*original_row_drop_possible)(drag_dest, path,
									selection_data);
	}

  /* At each drag, divert the original Gtk row_drop_possible function to my
  |  custom row_drop_possible so I can control tree structure.  The original
  |  row_drop_possible function must be restored at "drag_end" else other
  |  monitors doing drag n' drop could get screwed.
  */
static gboolean
cb_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer data)
	{
	GtkTreeModel			*model;
	GtkTreePath				*path;
	GtkTreeIter				iter;
	GtkTreeDragDestIface	*dest_iface;

	model = gtk_tree_view_get_model(treeview);
	dest_iface = GTK_TREE_DRAG_DEST_GET_IFACE(GTK_TREE_DRAG_DEST(model));
	if (!original_row_drop_possible)
		original_row_drop_possible = dest_iface->row_drop_possible;
	dest_iface->row_drop_possible = row_drop_possible;

	if (row_reference)
		{
		path = gtk_tree_row_reference_get_path(row_reference);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get(model, &iter, SENSOR_COLUMN, &dragged_sensor, -1);
		}
	else
		dragged_sensor = NULL;
	return FALSE;
	}

static gboolean
cb_drag_end(GtkWidget *widget, GdkDragContext *context, gpointer data)
	{
	GtkTreeModel			*model;
	GtkTreeIter				iter;
	GtkTreeDragDestIface	*dest_iface;
	Sensor					*s;
	gchar					node[2];
	gint					i, type = -1;

	model = gtk_tree_view_get_model(treeview);
	dest_iface = GTK_TREE_DRAG_DEST_GET_IFACE(GTK_TREE_DRAG_DEST(model));
	dest_iface->row_drop_possible = original_row_drop_possible;

	change_row_reference(NULL, NULL);
	gtk_tree_selection_unselect_all(selection);

	g_list_free(sensor_list);
	sensor_list = NULL;

	/* Re-order the sensors list to match the model.
	*/
	model = gtk_tree_view_get_model(treeview);
	for (i = 0; i < 3; ++i)
		{
		node[0] = '0' + i;		/* toplevel temp, fan, or volt node */
		node[1] = '\0';
		if (get_child_iter(model, node, &iter))
			{
			do
				{
				gtk_tree_model_get(model, &iter, SENSOR_COLUMN, &s, -1);
				sensor_list = g_list_append(sensor_list, s);
				}
			while (gtk_tree_model_iter_next(model, &iter));
			}
		}

	if (dragged_sensor)
		type = dragged_sensor->type;
	dragged_sensor = NULL;

	if (type < 0)
		gkrellm_sensors_rebuild(DO_TEMP, DO_FAN, DO_VOLT);
	else
		gkrellm_sensors_rebuild(type == SENSOR_TEMPERATURE,
				type == SENSOR_FAN, type == SENSOR_VOLTAGE);

	return FALSE;
	}

static void
sensor_reset_optionmenu(Sensor *sensor)
	{
	Sensor	*sr;

	if (!optionmenu)
		return;
	sr = get_referenced_sensor();
	if (sr == sensor)
		gtk_combo_box_set_active(GTK_COMBO_BOX(optionmenu),
					SENSOR_PANEL_LOCATION);
	}

static void
cb_location_menu(GtkComboBox *om, gpointer data)
	{
	GList		*list;
	Sensor		*sr, *s;
	gchar		*pname = NULL;
	gint		location;

	location = gtk_combo_box_get_active(om);
	sr = get_referenced_sensor();
	if (!sr || !sr->enabled || sr->location == location)
		return;

	/* If trying to relocate, get a dst panel name so can report failures.
	*/
	if (location != SENSOR_PANEL_LOCATION)
		{
		if (sr->group == SENSOR_GROUP_MAINBOARD)
			pname = (location == PROC_PANEL_LOCATION)
				? gkrellm_proc_get_sensor_panel_label()
				: gkrellm_cpu_get_sensor_panel_label(
							location - CPU_PANEL_LOCATION);
		else if (sr->group == SENSOR_GROUP_DISK)
			pname = _("Disk");
		}

	/* If moving off some other panel, reset that panel.
	*/
	sensor_reset_location(sr);

	/* For mainboard sensor group, if relocating to a panel with some other
	|  sensor of same type on it, auto restore the other sensor to the sensor
	|  panel.  Disk sensor group should never conflict.
	*/
	sr->location = location;
	if (   location != SENSOR_PANEL_LOCATION
		&& sr->group == SENSOR_GROUP_MAINBOARD
	   )
		for (list = sensor_list; list; list = list->next)
			{
			s = (Sensor *) list->data;
			if (   s->group == SENSOR_GROUP_MAINBOARD
				&& s != sr
				&& s->type == sr->type
				&& s->location == sr->location
			   )
				s->location = SENSOR_PANEL_LOCATION;	/* is being replaced */
			}
	gkrellm_sensors_rebuild(DO_TEMP, DO_FAN, FALSE);

	if (sr->location != location)	/* location failed */
		{
		gtk_combo_box_set_active(GTK_COMBO_BOX(optionmenu),
					SENSOR_PANEL_LOCATION);
		sensor_relocation_error(pname);
		}
	}


static void
create_location_menu(gint group)
	{
	gchar		*label;
	gint		n, n_cpus;

	if (group == sensor_last_group)
		return;
	sensor_last_group = group;

	gtk_combo_box_append_text(GTK_COMBO_BOX(optionmenu), "default");

	if (group == SENSOR_GROUP_MAINBOARD)
		{
		label = gkrellm_proc_get_sensor_panel_label();
		if (label)
			gtk_combo_box_append_text(GTK_COMBO_BOX(optionmenu), label);

		n_cpus = gkrellm_smp_cpus() + 1;
		for (n = 0; n < n_cpus; ++n)
			{
			label = gkrellm_cpu_get_sensor_panel_label(n);
			if (label)
				gtk_combo_box_append_text(GTK_COMBO_BOX(optionmenu), label);
			}
		}
	else if (group == SENSOR_GROUP_DISK)
		{
		gtk_combo_box_append_text(GTK_COMBO_BOX(optionmenu), _("Disk"));
		}
	g_signal_connect(G_OBJECT(optionmenu), "changed",
				G_CALLBACK(cb_location_menu), NULL);
	}

static void
set_sensor_widget_states(Sensor *s)
	{
	gboolean	f_sensitive = FALSE,
				o_sensitive = FALSE,
				p_sensitive = FALSE;
	gfloat		factor = 1.0,
				offset = 0.0;
	gint		location = SENSOR_PANEL_LOCATION;

	if (s && s->enabled)
		{
		f_sensitive = TRUE;
		if (s->type != SENSOR_FAN)
			{
			o_sensitive = TRUE;
			offset = s->offset;
			}
		factor = s->factor;
		if (s->type != SENSOR_VOLTAGE)
			{
			location = s->location;
			p_sensitive = TRUE;
			}
		}
	create_location_menu(s ? s->group : 0);
	gtk_combo_box_set_active(GTK_COMBO_BOX(optionmenu), location);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(factor_spin_button), factor);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(offset_spin_button), offset);
	gtk_widget_set_sensitive(optionmenu, p_sensitive);
	gtk_widget_set_sensitive(alert_button, f_sensitive);

	if (s && s->default_factor == 0.0)
		f_sensitive = o_sensitive = FALSE;

	gtk_widget_set_sensitive(factor_spin_button, f_sensitive);
	gtk_widget_set_sensitive(offset_spin_button, o_sensitive);
	}

static void
cb_correction_modified(void)
	{
	Sensor			*s;

	s = get_referenced_sensor();
	if (!s || !s->enabled)
		return;
	s->factor = gtk_spin_button_get_value(GTK_SPIN_BUTTON(factor_spin_button));
	s->offset = gtk_spin_button_get_value(GTK_SPIN_BUTTON(offset_spin_button));

	if (s->factor == 0)
		{
		s->factor = s->default_factor;
		s->offset = s->default_offset;
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(factor_spin_button),
					s->factor);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(offset_spin_button),
					s->offset);
		}
	if (s->type == SENSOR_VOLTAGE)
		draw_voltages(s, FALSE);
	else
		{
		gkrellm_cpu_draw_sensors(s);
		gkrellm_proc_draw_sensors(s);
		draw_temperatures(FALSE);
		}
	}

static void
cb_tree_selection_changed(GtkTreeSelection *selection, gpointer data)
	{
	GtkTreeIter		iter;
	GtkTreeModel	*model;
	GtkTreePath		*path;
	Sensor			*s;
	gint            depth;

	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		{
	    change_row_reference(NULL, NULL);
		set_sensor_widget_states(NULL);
		return;
		}
	path = gtk_tree_model_get_path(model, &iter);
	depth = gtk_tree_path_get_depth(path);
    change_row_reference(model, path);
    gtk_tree_path_free(path);

	if (depth == 1)
		{
		set_sensor_widget_states(NULL);
		return;
		}
	s = get_referenced_sensor();
	set_sensor_widget_states(s);
	}

static void
label_edited_cb(GtkCellRendererText *cell, gchar *path_string,
			gchar *new_label, gpointer data)
	{
	GtkTreeModel	*model;
	GtkTreeIter		iter;
	GtkTreePath		*path;
	Sensor			*s;

	model = sensor_model;
	path = gtk_tree_path_new_from_string(path_string);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);

	gtk_tree_model_get(model, &iter,
				SENSOR_COLUMN, &s,
				-1);
	if (!*new_label)
		new_label = s->default_label;

	gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
				LABEL_COLUMN, new_label, -1);

	if (gkrellm_locale_dup_string(&s->name, new_label, &s->name_locale))
		{
		gkrellm_sensors_rebuild(s->type == SENSOR_TEMPERATURE,
				s->type == SENSOR_FAN, s->type == SENSOR_VOLTAGE);
		if (s->alert)
			{
			g_free(s->alert->name);
			s->alert->name = g_strdup(s->name);
//			gkrellm_reset_alert(s->alert);
			}
		}
	}

static void
enable_cb(GtkCellRendererText *cell, gchar *path_string, gpointer data)
	{
	GtkTreeModel	*model;
	GtkTreeIter		iter;
	GtkTreePath		*path;
	Sensor			*s;
	gboolean		enabled;

	model = sensor_model;
	path = gtk_tree_path_new_from_string(path_string);
	gtk_tree_model_get_iter(model, &iter, path);

	gtk_tree_model_get(model, &iter,
				ENABLE_COLUMN, &enabled,
				SENSOR_COLUMN, &s,
				-1);
	s->enabled = !enabled;
	gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
				ENABLE_COLUMN, s->enabled, 
				-1);
    change_row_reference(model, path);
	gtk_tree_path_free(path);

	gkrellm_sensors_rebuild(s->type == SENSOR_TEMPERATURE,
				s->type == SENSOR_FAN, s->type == SENSOR_VOLTAGE);

	set_sensor_widget_states(s);
	}

static void
fix_temp_alert(Sensor *s)
	{
	GkrellmAlert	*a = s->alert;

	if (s->type != SENSOR_TEMPERATURE || !a)
		return;
	if (units_fahrenheit)
		{
		if (a->high.warn_limit > 0)
			a->high.warn_limit = a->high.warn_limit * 9.0 / 5.0 + 32.0;
		if (a->high.alarm_limit > 0)
			a->high.alarm_limit = a->high.alarm_limit * 9.0 / 5.0 + 32.0;
		}
	else
		{
		if (a->high.warn_limit > 0)
			a->high.warn_limit = (a->high.warn_limit - 32.0) * 5.0 / 9.0;
		if (a->high.alarm_limit > 0)
			a->high.alarm_limit = (a->high.alarm_limit - 32.0) * 5.0 / 9.0;
		}
	gkrellm_alert_window_destroy(&s->alert);
	}

static void
sysdep_option_cb(GtkWidget *button, SysdepOption *so)
	{
	if (!so)
		return;
	so->value = GTK_TOGGLE_BUTTON(button)->active;
	}

static void
cb_temperature_units(GtkWidget *button, gpointer data)
	{
	GList	*list;
	gint	units;

	units = GTK_TOGGLE_BUTTON(button)->active;
	if (units == units_fahrenheit)
		return;
	units_fahrenheit = units;

	for (list = sensor_list; list; list = list->next)
		fix_temp_alert((Sensor *) list->data);

	gkrellm_sensors_rebuild(DO_TEMP, FALSE, FALSE);
	gkrellm_cpu_draw_sensors(NULL);
	gkrellm_proc_draw_sensors(NULL);
	}

static void
cb_show_units(GtkWidget *button, gpointer data)
	{
	gint	show;

	show = GTK_TOGGLE_BUTTON(button)->active;
	if (show == show_units)
		return;
	show_units = show;

	gkrellm_sensors_rebuild(DO_TEMP, FALSE, FALSE);
	gkrellm_cpu_draw_sensors(NULL);
	gkrellm_proc_draw_sensors(NULL);
	}

static void
cb_voltages_display(GtkWidget *entry, gpointer data)
	{
	gint	i;

	for (i = 0; i < N_DISPLAY_MODES; ++i)
		if (GTK_TOGGLE_BUTTON(display_mode_button[i])->active)
			display_mode = i;
	gkrellm_sensors_rebuild(FALSE, FALSE, DO_VOLT);
	}

static void
cb_set_alert(GtkWidget *widget, gpointer data)
	{
	Sensor	*s;

	s = get_referenced_sensor();
	if (!s || !s->enabled)
		return;
	if (!s->alert)
		create_sensor_alert(s);
	gkrellm_alert_config_window(&s->alert);
	}


static void
sensors_apply(void)
	{
	gchar	*str;
	gint	port;

	if (mbmon_port_entry)
		{
		str = gkrellm_gtk_entry_get_text(&mbmon_port_entry);
		if (isdigit((unsigned char)*str))
			{
			port = atoi(str);
			if (_GK.mbmon_port != port)
				{
				if (!gkrellm_sys_sensors_mbmon_port_change(port) && port > 0)
					gkrellm_message_dialog(NULL,
				_("Can't read sensor data from mbmon daemon.\n"
				  "Check mbmon port number and '-r' option.\n"
				  "Run gkrellm -d 0x80 for debug output.\n"));
				}
			}
		}
	}

static void
mbmon_port_entry_activate_cb(GtkWidget *widget, gpointer data)
	{
	sensors_apply();
	}

static void
cb_config_deleted(gpointer data)
	{
	treeview = NULL;
	}

static gchar	*sensor_info_text0[] =
	{
	N_("<b>No sensors detected.\n"),
	"\n",
	};

static gchar	*sensor_info_text1[] = 
	{
N_("<h>Setup\n"),
N_("Enter data scaling factors and offsets for the sensors if the default\n"
"values are not correct for your motherboard.  Do a man gkrellm or\n"
"see the GKrellM README for more information.\n"),
N_("Enter a zero factor and a blank label to restore default values.\n"),
"\n",
N_("Drag and drop sensor rows to change the displayed order.\n"),
"\n",
N_("Temperature offset values must be in centigrade units.\n"),
"\n",
N_("Substitution variables may be used in alert commands.\n"),
N_("\t$s    current sensor value.\n"),
N_("\t$l    sensor label.\n"),
	};

static void
sensors_tab_destroy(GtkWidget *w, gpointer data)
	{
	optionmenu = NULL;
	}

static void
create_sensors_tab(GtkWidget *tab_vbox)
	{
	GtkWidget				*tabs;
	GtkWidget				*button;
	GtkWidget				*text;
	GtkWidget				*vbox, *vbox1, *hbox, *box;
	GtkWidget				*scrolled;
	GtkWidget				*image;
	GtkWidget				*label, *entry;
	GtkTreeModel			*model;
	GtkCellRenderer			*renderer;
	GList					*list;
	SysdepOption			*so;
	gchar					buf[32];
	gint					i;

	row_reference = NULL;
	sensor_last_group = -1;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

	g_signal_connect(GTK_OBJECT(tabs), "destroy",
				G_CALLBACK(sensors_tab_destroy), NULL);

/* --Setup tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), vbox1, FALSE, FALSE, 5);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);

	model = create_model();
	sensor_model = model;

	treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	gtk_tree_view_set_rules_hint(treeview, TRUE);
	gtk_tree_view_set_reorderable(treeview, TRUE);
	g_signal_connect(G_OBJECT(treeview), "drag_begin",
				G_CALLBACK(cb_drag_begin), NULL);
	g_signal_connect(G_OBJECT(treeview), "drag_end",
				G_CALLBACK(cb_drag_end), NULL);
	g_signal_connect(G_OBJECT(treeview), "delete_event",
				G_CALLBACK(cb_config_deleted), NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Sensor"),
				renderer,
				"text", NAME_COLUMN,
				NULL);

	renderer = gtk_cell_renderer_toggle_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Enable"),
				renderer,
				"active", ENABLE_COLUMN,
				"visible", VISIBLE_COLUMN,
				NULL);
	g_signal_connect(G_OBJECT(renderer), "toggled",
				G_CALLBACK(enable_cb), NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Label"),
				renderer,
				"text", LABEL_COLUMN,
				"editable", TRUE,
				"visible", VISIBLE_COLUMN,
				NULL);
	g_signal_connect(G_OBJECT(renderer), "edited",
				G_CALLBACK(label_edited_cb), NULL);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, "",
				renderer,
				"pixbuf", IMAGE_COLUMN,
				NULL);


	gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(treeview));

	selection = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed",
				G_CALLBACK(cb_tree_selection_changed), NULL);

	box = gkrellm_gtk_framed_vbox(vbox1, _("Factor"), 4, FALSE, 0, 2);
	gkrellm_gtk_spin_button(box, &factor_spin_button, 1.0,
				-1000.0, 1000.0, 0.01, 1.0, 4, 60,
				cb_correction_modified, NULL, FALSE, NULL);

	box = gkrellm_gtk_framed_vbox(vbox1, _("Offset"), 4, FALSE, 0, 2);
	gkrellm_gtk_spin_button(box, &offset_spin_button, 0.0,
				-10000.0, 10000.0, 1.0, 5.0, 3, 60,
				cb_correction_modified, NULL, FALSE, NULL);

	box = gkrellm_gtk_framed_vbox(vbox1, _("Location"), 2, FALSE, 0, 2);

	optionmenu = gtk_combo_box_new_text();

	gtk_box_pack_start(GTK_BOX(box), optionmenu, FALSE, FALSE, 4);

	box = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(box), 2);
	image = gtk_image_new_from_pixbuf(gkrellm_alert_pixbuf());
	label = gtk_label_new(_("Alerts"));
	gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 3);
	alert_button = gtk_button_new();
	g_signal_connect(G_OBJECT(alert_button), "clicked",
				G_CALLBACK(cb_set_alert), NULL);
	gtk_widget_show_all(box);
	gtk_container_add(GTK_CONTAINER(alert_button), box);
	gtk_box_pack_end(GTK_BOX(vbox1), alert_button, FALSE, FALSE, 4);

//	gkrellm_gtk_button_connected(vbox1, &alert_button, FALSE, FALSE, -5,
//				cb_set_alert, NULL, "Alerts");

	set_sensor_widget_states(NULL);

/* -- Options tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Options"));
//	box = gkrellm_gtk_framed_vbox(vbox, _("Temperatures"), 4, FALSE, 0, 2);
	box = gkrellm_gtk_category_vbox(vbox, _("Temperatures"), 4, 0, TRUE);
	gkrellm_gtk_check_button_connected(box, &button,
				units_fahrenheit, FALSE, FALSE, 0,
				cb_temperature_units, NULL,
				_("Display fahrenheit"));
	gkrellm_gtk_check_button_connected(box, &button,
				show_units, FALSE, FALSE, 0,
				cb_show_units, NULL,
				_("Show units"));
	if (!sensor_list)
		gtk_widget_set_sensitive(button, FALSE);

//	box = gkrellm_gtk_framed_vbox(vbox, _("Voltages"), 6, FALSE, 0, 2);
	box = gkrellm_gtk_category_vbox(vbox, _("Voltages"), 4, 0, TRUE);
	button = gtk_radio_button_new_with_label(NULL,
				_("Normal with labels"));
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, TRUE, 0);
	display_mode_button[DIGITAL_WITH_LABELS] = button;
	g_signal_connect(G_OBJECT(button), "toggled",
				G_CALLBACK(cb_voltages_display), NULL);
	if (!sensor_list)
		gtk_widget_set_sensitive(button, FALSE);

	button = gtk_radio_button_new_with_label(
				gtk_radio_button_get_group(GTK_RADIO_BUTTON (button)),
				_("Compact with no labels"));
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, TRUE, 0);
	display_mode_button[DIGITAL_NO_LABELS] = button;
	if (!sensor_list)
		gtk_widget_set_sensitive(button, FALSE);

	if (sysdep_option_list)
		{
		box = gkrellm_gtk_category_vbox(vbox, _("Options"), 4, 0, TRUE);
		for (list = sysdep_option_list; list; list = list->next)
			{
			so= (SysdepOption *) list->data;
			if (so->config_label)
				gkrellm_gtk_check_button_connected(box, NULL,
						so->value, FALSE, FALSE, 0,
						sysdep_option_cb, so,
						so->config_label);
			}
		}

	button = display_mode_button[display_mode];
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);

	if (gkrellm_sys_sensors_mbmon_supported())
		{
		box = gkrellm_gtk_category_vbox(vbox, _("MBmon Daemon Port"),
					4, 0, TRUE);
		label = gtk_label_new("");
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_label_set_markup(GTK_LABEL(label),
			_("<small>Daemon command must be: <b>mbmon -r -P port</b>\n"
			 "where 'port' must match the port number entered here:</small>"));
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
		hbox = gtk_hbox_new(FALSE, 2);
		gtk_box_pack_start(GTK_BOX(box), hbox, TRUE, TRUE, 0);
		entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(entry), 6);
		mbmon_port_entry = entry;
		if (_GK.mbmon_port > 0)
			{
			snprintf(buf, sizeof(buf), "%d", _GK.mbmon_port);
			gtk_entry_set_text(GTK_ENTRY(entry), buf);
			}
		gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 4);
		label = gtk_label_new(
			_("See the README or do a \"man gkrellm\" for more information.\n"));
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 4);

		g_signal_connect(G_OBJECT(mbmon_port_entry), "activate",
	                G_CALLBACK(mbmon_port_entry_activate_cb), NULL);
		}

/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	if (!sensor_list)
		for (i = 0; i < sizeof(sensor_info_text0) / sizeof(gchar *); ++i)
			gkrellm_gtk_text_view_append(text, _(sensor_info_text0[i]));

	for (i = 0; i < sizeof(sensor_info_text1) / sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(sensor_info_text1[i]));

	/* Present as instant apply, but still get our apply function called.
	*/
	gkrellm_config_instant_apply(mon_config_sensors);
	}

GkrellmMonitor *
gkrellm_get_sensors_mon(void)
	{
	return mon_config_sensors;
	}

static GkrellmMonitor	monitor_config_sensors =
	{
	N_("Sensors"),		/* Name, for config tab.	*/
	-1,					/* Id,  0 if a plugin		*/
	NULL,				/* The create function		*/
	NULL,				/* The update function		*/
	create_sensors_tab,	/* The config tab create function	*/
	sensors_apply,

	save_sensors_config, /* Save user conifg			*/
	load_sensors_config, /* Load user config			*/
	SENSOR_CONFIG_KEYWORD,	/* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_sensors_config_monitor(void)
	{
	if (!setup_sensor_interface() && !_GK.demo)
		return NULL;
	if (!_GK.client_mode)
		use_threads = TRUE;
	sensor_list = g_list_sort(sensor_list, (GCompareFunc) strcmp_sensor_path);
    monitor_config_sensors.name = _(monitor_config_sensors.name);
	mon_config_sensors = &monitor_config_sensors;
	return &monitor_config_sensors;
	}

