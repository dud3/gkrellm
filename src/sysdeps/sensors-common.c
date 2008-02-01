/* GKrellM
|  Copyright (C) 1999-2007 Bill Wilson
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
*/

/* A system dependent interface can include this file to get sensors
|  from daemons that can run under different operating systems.
*/

#define SENSORS_COMMON	1

/* --------------- Interface to mbmon daemon sensor reading ---------
*/
typedef struct
	{
	gchar	*name;
	gfloat	value;
	}
	MbmonSensor;

static GList	*mbmon_list;

static gchar	gkrellm_decimal_point,
				mbmon_decimal_point;
static gboolean	mbmon_need_decimal_point_fix;

static gboolean (*mbmon_check_func)();

static gboolean
mbmon_decimal_point_fix(gchar *buf)
	{
	gchar   *s;

	for (s = buf; *s; ++s)
		if (*s == mbmon_decimal_point)
			{
			*s = gkrellm_decimal_point;
			return TRUE;
			}
	return FALSE;
	}

static gboolean
mbmon_decimal_point_check(gchar *buf)
	{
	struct lconv	*lc;
	gchar			*s;

	lc = localeconv();
	gkrellm_decimal_point = *lc->decimal_point;

	mbmon_decimal_point = (gkrellm_decimal_point == ',' ? '.' : ',');

	s = g_strdup(buf);
	if (mbmon_decimal_point_fix(s))
		{
		mbmon_need_decimal_point_fix = TRUE;
		if (_GK.debug_level & DEBUG_SENSORS)
			printf("mbmon_need_decimal_point_fix: %c -> %c\n",
					mbmon_decimal_point, gkrellm_decimal_point);
		}
	g_free(s);

	return mbmon_need_decimal_point_fix;;
	}

static MbmonSensor *
mbmon_lookup(gchar *name)
	{
	GList		*list;
	MbmonSensor	*mb;

	for (list = mbmon_list; list; list = list->next)
		{
		mb = (MbmonSensor *) list->data;
		if (!strcmp(name, mb->name))
			return mb;
		}
	return NULL;
	}

  /* Read sensor data from the mbmon daemon, which must be run with the
  |  -r option and no -f option:  mbmon -r -P port
  |  and 'port' must be configured in the sensors config.
  |  With '-r' mbmon output will be:
  |		TEMP0 : 37.0
  |		TEMP1 : 35.5
  |		TEMP2 : 43.0
  |		FAN0  : 1704
  |		FAN1  : 2220
  |		FAN2  : 2057
  |		VC0   :  +1.71
  |		VC1   :  +2.51
  |		V33   :  +3.22
  |		V50P  :  +4.87
  |		V12P  : +11.80
  |		V12N  : -12.12
  |		V50N  :  -5.25
  */
static gboolean
mbmon_daemon_read(void)
	{
	gchar				*server = "127.0.0.1";
	gpointer			sr;
	MbmonSensor			*mb;
	gchar				*default_label, *id_name;
	gchar				name[32], buf[256];
	gfloat				value;
	gint				fd, n, type;
	gboolean			result = FALSE;
	static GString		*mbmon_gstring;
	static gboolean		decimal_point_check_done;

	if ((fd = gkrellm_connect_to(server, _GK.mbmon_port)) < 0)
		{
		if (_GK.debug_level & DEBUG_SENSORS)
			printf("mbmon_daemon_read: can't connect to %s:%d.\n",
			       server, _GK.mbmon_port);
		return FALSE;
		}

	if (!mbmon_gstring)
		mbmon_gstring = g_string_new("");
	mbmon_gstring = g_string_truncate(mbmon_gstring, 0);

	while ((n = read(fd, buf, sizeof(buf) - 1)) > 0)
		{
		buf[n] = '\0';
		mbmon_gstring = g_string_append(mbmon_gstring, buf);
		}
	close(fd);

	if (_GK.debug_level & DEBUG_SENSORS)
		printf("mbmon_daemon_read:\n%s\n", mbmon_gstring->str);

	while (gkrellm_getline_from_gstring(&mbmon_gstring, buf, sizeof(buf)))
		{
		if (mbmon_need_decimal_point_fix)
			mbmon_decimal_point_fix(buf);
		if (   sscanf(buf, "%31s : %f", name, &value) != 2
		    || value == 0.0		/* Sensor not available */
		   )
			continue;
		if (name[0] == 'T')
			type = SENSOR_TEMPERATURE;
		else if (name[0] == 'F')
			type = SENSOR_FAN;
		else if (name[0] == 'V')
			type = SENSOR_VOLTAGE;
		else
			continue;

		if (   !decimal_point_check_done
			&& (type == SENSOR_TEMPERATURE || type == SENSOR_VOLTAGE)
		   )
			{
			mbmon_decimal_point_check(buf);
			decimal_point_check_done = TRUE;
			}

		if ((mb = mbmon_lookup(name)) == NULL)
			{
			mb = g_new0(MbmonSensor, 1);
			mbmon_list = g_list_append(mbmon_list, mb);
			mb->name = g_strdup(name);
			default_label = name;
			id_name = g_strdup_printf("mbmon/%s", name);
			sr = gkrellm_sensors_add_sensor(type,
						name, id_name,
						0, 0, MBMON_INTERFACE,
						1.0, 0.0, NULL, default_label);
			g_free(id_name);
			}
		mb->value = value;		/* Assume centigrade, mbmon gives no units */
		result = TRUE;
		}
	return result;
	}


gboolean
gkrellm_sys_sensors_mbmon_check(gboolean force)
	{
	GList			*list;
	MbmonSensor		*mb;
	gboolean		result = TRUE;
	static gint		port;
	static gint		check_time = -1;
	static gboolean	tmp;

	mbmon_check_func = gkrellm_sys_sensors_mbmon_check;

	if (port > 0 && port != _GK.mbmon_port)
		{
		for (list = mbmon_list; list; list = list->next)
			{
			mb = (MbmonSensor *) list->data;
			g_free(mb->name);
			}
		gkrellm_free_glist_and_data(&mbmon_list);
		}
	if (_GK.mbmon_port <= 0)
		return FALSE;
	port = _GK.mbmon_port;
	if (check_time < _GK.time_now || force)
		{
		/* The first mbmon_daemon_read can set need_decimal_point_fix in
		|  which case don't update check_time so the mbmon daemon will be
		|  read again immediately at next call to this function.  The first
		|  call of this function should be made from gkrellm_sys_sensors_init()
		|  function where we just want to get the sensors loaded into
		|  sensors.c and we don't actually use the mbmon sensor values.
		*/
		tmp = mbmon_need_decimal_point_fix;
		result = mbmon_daemon_read();
		if (tmp == mbmon_need_decimal_point_fix)
			check_time = _GK.time_now + 3;	/* Interval < sensor update */
		}
	return result;
	}


gboolean
gkrellm_sys_sensors_mbmon_get_value(gchar *name, gfloat *value)
	{
	MbmonSensor	*mb;

	if ((mb = mbmon_lookup(name)) != NULL)
		{
		*value = mb->value;
		return TRUE;
		}
	return FALSE;
	}



/* --------------- Interface to hddtemp daemon sensor reading ---------
*/
  /* Use hddtemp default port.  Should make this configurable.
  */
#define	HDDTEMP_PORT	7634

typedef struct
	{
	gchar	*device;
	gfloat	value;
	gchar	unit;
	}
	HddtempSensor;

static GList	*hddtemp_list;

static HddtempSensor *
hddtemp_lookup(gchar *device)
	{
	GList			*list;
	HddtempSensor	*hdd;

	for (list = hddtemp_list; list; list = list->next)
		{
		hdd = (HddtempSensor *) list->data;
		if (!strcmp(device, hdd->device))
			return hdd;
		}
	return NULL;
	}

  /* Read output from the hddtemp daemon which must have been started in
  |  daemon mode:  hddtemp -d /dev/hda /dev/hdb ...
  |  And example hddtemp output will be:
  |
  |		|/dev/hda|SAMSUNG SP1614N|30|C||/dev/hdc|SAMSUNG SP1614N|30|C|
  |
  */
static gboolean
hddtemp_daemon_read(void)
	{
	gchar			*server = "127.0.0.1";
	gpointer			sr;
	HddtempSensor		*hdd;
	gchar				**argv, **info, *id_name, *default_label;
	gchar				buf[256], sep;
	gint				fd, n, j;
	static GString		*hddtemp_gstring;
	gboolean			result = FALSE;

	if ((fd = gkrellm_connect_to(server, HDDTEMP_PORT)) < 0)
		{
		if (_GK.debug_level & DEBUG_SENSORS)
			printf("hddtemp_daemon_read: can't connect to %s:%d.\n",
			       server, HDDTEMP_PORT);
		return FALSE;
		}

	if (!hddtemp_gstring)
		hddtemp_gstring = g_string_new("");
	hddtemp_gstring = g_string_truncate(hddtemp_gstring, 0);

	while ((n = read(fd, buf, sizeof(buf) - 1)) > 0)
		{
		buf[n] = '\0';
		hddtemp_gstring = g_string_append(hddtemp_gstring, buf);
		}
	close(fd);

	if (_GK.debug_level & DEBUG_SENSORS)
		printf("hddtemp_daemon_read (once a minute):\n\t%s\n",
					hddtemp_gstring->str);

	sep = hddtemp_gstring->str[0];
	if (sep == '\0')
		return FALSE;
	sprintf(buf, "%c%c", sep, sep);

	argv = g_strsplit(hddtemp_gstring->str + 1, buf, 20);
	buf[1] = '\0';
	for (n = 0; argv[n] != NULL; ++n)
		{
		info = g_strsplit(argv[n], buf, 4);
		for (j = 0; info[j] != NULL; ++j)
			;
		if (j < 4)
			{
			g_strfreev(info);
			continue;
			}
		if ((hdd = hddtemp_lookup(info[0])) == NULL)
			{
			hdd = g_new0(HddtempSensor, 1);
			hddtemp_list = g_list_append(hddtemp_list, hdd);
			hdd->device = g_strdup(info[0]);
			default_label = strrchr(hdd->device, '/');
			if (default_label)
				++default_label;
			else
				default_label = hdd->device;
			id_name = g_strdup_printf("hddtemp/%s", default_label);
			sr = gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
						hdd->device, id_name,
						0, 0, HDDTEMP_INTERFACE,
						1.0, 0.0, NULL, default_label);
			gkrellm_sensors_set_group(sr, SENSOR_GROUP_DISK);
			g_free(id_name);
			}
		hdd->value = atof(info[2]);
		if (*info[3] == 'F')
			hdd->value = (hdd->value - 32.0) / 1.8;
		g_strfreev(info);
		result = TRUE;
		}
	g_strfreev(argv);
	return result;
	}

void
gkrellm_sys_sensors_hddtemp_check(void)
	{
	static gint	check_time = -1;

	/* hddtemp docs say shouldn't check more than once per minute.
	*/
	if (check_time < _GK.time_now)
		{
		hddtemp_daemon_read();
		check_time = _GK.time_now + 60;
		}
	}

gboolean
gkrellm_sys_sensors_hddtemp_get_value(gchar *name, gfloat *value)
	{
	HddtempSensor	*hdd;

	if ((hdd = hddtemp_lookup(name)) != NULL)
		{
		*value = hdd->value;
		return TRUE;
		}
	return FALSE;
	}

