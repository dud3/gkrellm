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

#include <gmodule.h>

#if defined(WIN32)
	#include "win32-plugin.h"
#endif

/* ======================================================================= */
/* Plugin interface to GKrellM.
*/

gchar *
gkrellm_get_hostname(void)
	{
	gchar	*hostname;

	if (_GK.client_mode)
		hostname = _GK.server_hostname;
	else
		hostname = gkrellm_sys_get_host_name();
	return hostname;
	}

gchar *
gkrellm_get_theme_path(void)
	{
	return _GK.theme_path;
	}

GkrellmKrell *
gkrellm_krell_new0(void)
	{
	GkrellmKrell *k;

	k = g_new0(GkrellmKrell, 1);
	return k;
	}

GkrellmDecal *
gkrellm_decal_new0(void)
	{
	GkrellmDecal	*d;

	d = g_new0(GkrellmDecal, 1);
	return d;
	}


GkrellmLabel *
gkrellm_label_new0(void)
	{
	GkrellmLabel *l;

	l = g_new0(GkrellmLabel, 1);
	return l;
	}


GkrellmStyle *
gkrellm_style_new0(void)
	{
	GkrellmStyle	*s;

	s = g_new0(GkrellmStyle, 1);
	return s;
	}

GkrellmStyle *
gkrellm_copy_style(GkrellmStyle *style)
	{
	GkrellmStyle *s = NULL;

	if (style)
		{
		s = gkrellm_style_new0();
		*s = *style;
		}
	return s;
	}

void
gkrellm_copy_style_values(GkrellmStyle *dst, GkrellmStyle *src)
	{
	if (src && dst)
		*dst = *src;
	}

GkrellmTextstyle *
gkrellm_textstyle_new0(void)
	{
	GkrellmTextstyle	*t;

	t = g_new0(GkrellmTextstyle, 1);
	return t;
	}

GkrellmTextstyle *
gkrellm_copy_textstyle(GkrellmTextstyle *ts)
	{
	GkrellmTextstyle *t	= gkrellm_textstyle_new0();

	*t = *ts;
	return t;
	}

GkrellmChart *
gkrellm_chart_new0(void)
	{
	GkrellmChart	*c;

	c = g_new0(GkrellmChart, 1);
	return c;
	}

GkrellmChartconfig *
gkrellm_chartconfig_new0(void)
	{
	GkrellmChartconfig	*config;

	config = g_new0(GkrellmChartconfig, 1);
	return config;
	}

GkrellmPanel *
gkrellm_panel_new0(void)
	{
	GkrellmPanel *p;

	p = g_new0(GkrellmPanel, 1);
	p->label = gkrellm_label_new0();
	return p;
	}


static GkrellmStyle *
get_style_from_list(GList *list, gint n)
	{
	GList			*l;
	GkrellmStyle	*style;

	l = g_list_nth(list, n);
	if (l == NULL || l->data == NULL)
		l = list;
	if (l->data == NULL)
		{
		printf("Warning: NULL style returned %d\n", n);
		abort();
		}
	style = (GkrellmStyle *) l->data;
	return style;
	}

GkrellmStyle *
gkrellm_meter_style(gint n)
	{
	return get_style_from_list(_GK.meter_style_list, n);
	}

GkrellmStyle *
gkrellm_panel_style(gint n)
	{
	return get_style_from_list(_GK.panel_style_list, n);
	}

GkrellmStyle *
gkrellm_chart_style(gint n)
	{
	return get_style_from_list(_GK.chart_style_list, n);
	}

static GkrellmStyle *
get_style_from_list_by_name(GList *name_list, GList *style_list, gchar *name)
	{
	GList	*list;
	gchar	*p, buf[128];
	gint	n;

	strncpy(buf, name, sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	if (   (p = strchr(buf, '.')) != NULL
		&& (n = gkrellm_string_position_in_list(_GK.custom_name_list, name)) >= 0
	   )
		list = _GK.custom_style_list;
	else
		{
		if (p)
			*p = '\0';
		if ((n = gkrellm_string_position_in_list(name_list, buf)) < 0)
			n = 0;
		list = style_list;
		}
	return get_style_from_list(list, n);
	}

GkrellmStyle *
gkrellm_meter_style_by_name(gchar *name)
	{
	return get_style_from_list_by_name(_GK.meter_name_list,
				_GK.meter_style_list, name);
	}

GkrellmStyle *
gkrellm_panel_style_by_name(gchar *name)
	{
	return get_style_from_list_by_name(_GK.chart_name_list,
				_GK.panel_style_list, name);
	}

GkrellmStyle *
gkrellm_chart_style_by_name(gchar *name)
	{
	return get_style_from_list_by_name(_GK.chart_name_list,
				_GK.chart_style_list, name);
	}

gint
gkrellm_lookup_chart_style_id(gchar *name)
	{
	GList	*list;
	gint	i;

	for (list = _GK.chart_name_list, i = 0; list; list = list->next, ++i)
		if (name && !strcmp(name, (gchar *)list->data))
			return i;
	return 0;
	}

gint
gkrellm_lookup_meter_style_id(gchar *name)
	{
	GList	*list;
	gint	i;

	for (list = _GK.meter_name_list, i = 0; list; list = list->next, ++i)
		if (name && !strcmp(name, (gchar *)list->data))
			return i;
	return 0;
	}

static GkrellmPiximage *
get_piximage_from_list(GList *list, gint n)
	{
	GList	*l;

	l = g_list_nth(list, n);
	if (l == NULL || l->data == NULL)
		l = list;
	if (l->data == NULL)
		{
		printf("Warning: NULL image returned %d\n", n);
		abort();
		}
	return (GkrellmPiximage *) l->data;
	}

GkrellmPiximage *
gkrellm_bg_chart_piximage(gint n)
	{
	return get_piximage_from_list(_GK.bg_chart_piximage_list, n);
	}

GkrellmPiximage *
gkrellm_bg_grid_piximage(gint n)
	{
	return get_piximage_from_list(_GK.bg_grid_piximage_list, n);
	}

GkrellmPiximage *
gkrellm_bg_panel_piximage(gint n)
	{
	return get_piximage_from_list(_GK.bg_panel_piximage_list, n);
	}

GkrellmPiximage *
gkrellm_bg_meter_piximage(gint n)
	{
	return get_piximage_from_list(_GK.bg_meter_piximage_list, n);
	}

GkrellmPiximage *
gkrellm_krell_panel_piximage(gint n)
	{
	return get_piximage_from_list(_GK.krell_panel_piximage_list, n);
	}

GkrellmPiximage *
gkrellm_krell_meter_piximage(gint n)
	{
	return get_piximage_from_list(_GK.krell_meter_piximage_list, n);
	}

void
gkrellm_get_decal_alarm_piximage(GkrellmPiximage **im, gint *frames)
	{
	if (im)
		*im = _GK.decal_alarm_piximage;
	if (frames)
		*frames = _GK.decal_alarm_frames;
	}

void
gkrellm_get_decal_warn_piximage(GkrellmPiximage **im, gint *frames)
	{
	if (im)
		*im = _GK.decal_warn_piximage;
	if (frames)
		*frames = _GK.decal_warn_frames;
	}


void
gkrellm_monitor_height_adjust(gint h)
	{
	_GK.monitor_height += h;
	}

GdkPixmap *
gkrellm_decal_misc_pixmap(void)
	{
	return _GK.decal_misc_pixmap;
	}

GdkBitmap *
gkrellm_decal_misc_mask(void)
	{
	return _GK.decal_misc_mask;
	}

GdkPixmap **
gkrellm_data_in_pixmap(void)
	{
	return &_GK.data_in_pixmap;
	}

GdkPixmap *
gkrellm_data_in_grid_pixmap(void)
	{
	return _GK.data_in_grid_pixmap;
	}

GdkPixmap **
gkrellm_data_out_pixmap(void)
	{
	return &_GK.data_out_pixmap;
	}

GdkPixmap *
gkrellm_data_out_grid_pixmap(void)
	{
	return _GK.data_out_grid_pixmap;
	}

GkrellmPiximage *
gkrellm_krell_slider_piximage(void)
	{
	return _GK.krell_slider_piximage;
	}

GkrellmStyle *
gkrellm_krell_slider_style(void)
	{
	return _GK.krell_slider_style;
	}

GkrellmPiximage *
gkrellm_krell_mini_piximage(void)
	{
	return _GK.krell_mini_piximage;
	}

GkrellmStyle *
gkrellm_krell_mini_style(void)
	{
	return _GK.krell_mini_style;
	}

GdkGC *
gkrellm_draw_GC(gint n)
	{
	GdkGC	*gc;

	if (n == 0)
		return _GK.draw_stencil_GC;
	else if (n == 1)
		gc = _GK.draw1_GC;
	else if (n == 2)
		gc = _GK.draw2_GC;
	else
		gc = _GK.draw3_GC;
	return gc;
	}

GdkGC *
gkrellm_bit_GC(gint n)
	{
	if (n == 0)
		return _GK.bit0_GC;
	else
		return _GK.bit1_GC;
	}

GkrellmTextstyle *
gkrellm_chart_textstyle(gint n)
	{
	GkrellmStyle		*style;
	GkrellmTextstyle	*ts;

	style = get_style_from_list(_GK.chart_style_list, n);
	ts	= &style->label_tsA;
	ts->font = *(ts->font_seed);
	return ts;
	}

GkrellmTextstyle *
gkrellm_panel_textstyle(gint n)
	{
	GkrellmStyle		*style;
	GkrellmTextstyle 	*ts;

	style = get_style_from_list(_GK.panel_style_list, n);
	ts	= &style->label_tsA;
	ts->font = *(ts->font_seed);
	return ts;
	}

GkrellmTextstyle *
gkrellm_meter_textstyle(gint n)
	{
	GkrellmStyle		*style;
	GkrellmTextstyle 	*ts;

	style = get_style_from_list(_GK.meter_style_list, n);
	ts	= &style->label_tsA;
	ts->font = *(ts->font_seed);
	return ts;
	}


GkrellmTextstyle *
gkrellm_chart_alt_textstyle(gint n)
	{
	GkrellmStyle		*style;
	GkrellmTextstyle 	*ts;

	style = get_style_from_list(_GK.chart_style_list, n);
	ts	= &style->label_tsB;
	ts->font = *(ts->font_seed);
	return ts;
	}

GkrellmTextstyle *
gkrellm_panel_alt_textstyle(gint n)
	{
	GkrellmStyle		*style;
	GkrellmTextstyle 	*ts;

	style = get_style_from_list(_GK.panel_style_list, n);
	ts	= &style->label_tsB;
	ts->font = *(ts->font_seed);
	return ts;
	}

GkrellmTextstyle *
gkrellm_meter_alt_textstyle(gint n)
	{
	GkrellmStyle		*style;
	GkrellmTextstyle 	*ts;

	style = get_style_from_list(_GK.meter_style_list, n);
	ts	= &style->label_tsB;
	ts->font = *(ts->font_seed);
	return ts;
	}

PangoFontDescription *
gkrellm_default_font(gint n)
	{
	if (n == 0)
		return _GK.small_font;
	else if (n == 1)
		return _GK.normal_font;
	else
		return _GK.large_font;
	}

GdkColor *
gkrellm_white_color(void)
	{
	return &_GK.white_color;
	}

GdkColor *
gkrellm_black_color(void)
	{
	return &_GK.background_color;
	}

GdkColor *
gkrellm_in_color(void)
	{
	return &_GK.in_color;
	}

GdkColor *
gkrellm_out_color(void)
	{
	return &_GK.out_color;
	}

gboolean
gkrellm_demo_mode(void)
	{
	return _GK.demo;
	}

gint
gkrellm_update_HZ(void)
	{
	return _GK.update_HZ;
	}

gint
gkrellm_chart_width(void)
	{
	return _GK.chart_width;
	}

void
gkrellm_allow_scaling(gboolean *allow, gint *width_ref)
	{
	if (allow)
		*allow = _GK.allow_scaling;
	if (width_ref)
		*width_ref = _GK.chart_width_ref;
	}

gint
gkrellm_plugin_debug(void)
	{
	return _GK.debug;
	}

/* ======================================================================= */
typedef struct
	{
	GkrellmMonitor	*mon_plugin,
					*mon_target;
	gchar			key;
	gchar			*(*func)(gchar key, gchar *which);
	gint			id;
	}
	ExportLabel;

static GList	*export_label_list;

gint
gkrellm_plugin_export_label(GkrellmMonitor *mon_plugin, gchar *mon_name,
			gchar key, gchar *(*func)(gchar key, gchar *which))
	{
	GkrellmMonitor	*mon;
	ExportLabel		*el;
	GList			*list;
	static gint		id;

	if (!mon_name || !mon_plugin || !func || !key)
		return -1;
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (mon->name && (_GK.debug_level & DEBUG_PLUGIN))
			g_print("    %s %s\n", mon->name, mon_name);
		if (mon->name && !strcmp(mon->name, mon_name))
			break;
		}
	if (!list)
		return -1;

	el = g_new0(ExportLabel, 1);
	export_label_list = g_list_append(export_label_list, el);
	el->mon_target = mon;
	el->mon_plugin = mon_plugin;
	el->key = key;
	el->func = func;
	el->id = ++id;

	return id;
	}

void
gkrellm_plugin_cancel_label(GkrellmMonitor *mon_plugin, gint id)
	{
	GList		*list;
	ExportLabel	*el;

	if (!mon_plugin || id < 1)
		return;
	for (list = export_label_list; list; list = list->next)
		{
		el = (ExportLabel *) list->data;
		if (el->id == id)
			{
			export_label_list = g_list_remove(export_label_list, el);
			g_free(el);
			break;
			}
		}
	}

gchar *
gkrellm_plugin_get_exported_label(GkrellmMonitor *mon, gchar key, gchar *which)
	{
	GList		*list;
	ExportLabel	*el;
	gchar		*result = NULL;

	for (list = export_label_list; list; list = list->next)
		{
		el = (ExportLabel *) list->data;
		if (   el->mon_target == mon && el->key == key
			&& el->mon_plugin->privat->enabled && el->func
		   )
			{
			result = (*el->func)(key, which);
			break;
			}
		}
	return result;
	}

/* ======================================================================= */

static GkrellmMonitor		*place_plugin;

enum
	{
	NAME_COLUMN,
	ENABLE_COLUMN,
	MON_COLUMN,
	N_COLUMNS
	};

static GtkTreeView		*treeview;
static GtkTreeRowReference *row_reference;

static GtkWidget	*place_label,
					*place_button,
					*after_button,
					*before_button,
					*gravity_spin_button,
					*builtin_button[N_BUILTIN_MONITORS];

static GList		*plugins_list,
					*plugins_enable_list,
					*plugins_place_list;

static gboolean		plugin_enable_list_modified,
					plugin_placement_modified;

gchar				*plugin_install_log;


GkrellmMonitor *
gkrellm_monitor_from_id(gint id)
	{
	GList			*list;
	GkrellmMonitor	*mon;

	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (mon->id == id)
			return mon;
		}
	return NULL;
	}

GkrellmMonitor *
lookup_monitor_from_name(gchar *name)
	{
	GkrellmMonitor	*mon;
	GList			*list;

	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (!mon->name || mon->id < 0)
			continue;
		if (!strcmp(name, mon->name))
			return mon;
		}
	return NULL;
	}

GkrellmMonitor *
gkrellm_monitor_from_style_name(gchar *style_name)
	{
	GkrellmMonitor	*mon;
	GList			*list;

	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (!mon->privat || !mon->privat->style_name)
			continue;
		if (!strcmp(style_name, mon->privat->style_name))
			return mon;
		}
	return NULL;
	}

static void
plugin_log(gchar *string1, ...)
	{
	va_list		args;
	gchar		*s, *old_log;

	if (!string1)
		return;
	va_start(args, string1);
	s = string1;
	while (s)
		{
		old_log = plugin_install_log;
		if (plugin_install_log)
			plugin_install_log = g_strconcat(plugin_install_log, s, NULL);
		else
			plugin_install_log = g_strconcat(s, NULL);
		g_free(old_log);
		s = va_arg(args, gchar *);
		}
	va_end(args);
	}

static gboolean
user_placement(GkrellmMonitor *plugin)
	{
	GList			*list;
	GkrellmMonitor	*mon;
	gchar			*line, *s, *plugin_name, *mon_name;
	gint			n, after, gravity;

	if (!plugin->name)
		return FALSE;
	for (list = plugins_place_list; list; list = list->next)
		{
		line = g_strconcat((gchar *) list->data, NULL);
		plugin_name = gkrellm_cut_quoted_string(line, &s);
		mon_name = gkrellm_cut_quoted_string(s, &s);
		n = sscanf(s, "%d %d", &after, &gravity);
		if (n == 2)
			{
			if (after)
				gravity = 0;
			if (   !strcmp(plugin_name, plugin->name)
				&& (mon = lookup_monitor_from_name(mon_name)) != NULL
			   )
				{
				plugin->privat->insert_before_id = mon->id;
				plugin->privat->gravity = gravity;
				plugin->privat->insert_after = after;
				g_free(line);
				return TRUE;
				}
			}
		g_free(line);
		}
	return FALSE;
	}

void
gkrellm_place_plugin(GList **monitor_list, GkrellmMonitor *plugin)
	{
	GkrellmMonitor	*mon;
	GList			*list, *plist, *waste;
	gint			n, gravity, after_flag;
	gchar			buf[120];

	if (plugin->create_monitor && !plugin->privat->main_vbox)
		{
		plugin->privat->main_vbox = gtk_vbox_new(FALSE, 0);
		plugin->privat->top_spacer.vbox = gtk_vbox_new(FALSE, 0);
		plugin->privat->vbox = gtk_vbox_new(FALSE, 0);
		plugin->privat->bottom_spacer.vbox = gtk_vbox_new(FALSE, 0);
		}
	for (plist = NULL, list = *monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;

		/* Save list position as plugins are encountered so we can later
		|  walk through them looking at gravity.
		*/
		if (MONITOR_ID(mon) == MON_PLUGIN && plist == NULL)
			plist = list;
		if (MONITOR_ID(mon) == plugin->privat->insert_before_id)
			{
			after_flag = plugin->privat->insert_after;
			gravity = plugin->privat->gravity;
			snprintf(buf, sizeof(buf), _("\t%s: placement is %s %s  G:%d\n"),
					plugin->name,
					after_flag ? _("after") : _("before"), mon->name, gravity);
			plugin_log(buf, NULL);
			if (after_flag)
				{
				if ((n = g_list_position(*monitor_list, list)) < 0)
					n = 0;
				*monitor_list = g_list_insert(*monitor_list, plugin, n + 1);
				}
			else
				{
				/* If there are plugins already above this builtin, then place
				|  based on gravity.  Insert above the first plugin found that
				|  has greater gravity than the current placing plugin.
				*/
				if (plist)
					{
					for ( ; plist != list; plist = plist->next)
						{
						mon = (GkrellmMonitor *) plist->data;
						if (mon->privat->gravity > gravity)
							break;
						}
					list = plist;
					}
				if (list == *monitor_list)
					*monitor_list = g_list_prepend(list, plugin);
				else
					waste = g_list_prepend(list, plugin);
				}
			return;
			}
		else if (MONITOR_ID(mon) != MON_PLUGIN)
			plist = NULL;
		}
	if (plugin->privat->insert_before_id != MON_UPTIME)
		{
		plugin->privat->insert_before_id = MON_UPTIME;
		gkrellm_place_plugin(monitor_list, plugin);
		return;
		}
	*monitor_list = g_list_append(*monitor_list, plugin);
	}

GkrellmMonitor *
install_plugin(gchar *plugin_name)
	{
	GList					*list;
	GModule					*module;
	GkrellmMonitor			*m, *mm;
	GkrellmMonitor			*(*init_plugin)();
	gchar					buf[256];
	static GkrellmMonitor	mon_tmp;

	if (!g_module_supported())
		return NULL;
	module = g_module_open(plugin_name, 0);
	plugin_log(plugin_name, "\n", NULL);

	if (! module)
		{
		snprintf(buf, sizeof(buf), _("\tError: %s\n"), g_module_error());
		plugin_log(buf, NULL);
		return NULL;
		}
	if (!g_module_symbol(module, "gkrellm_init_plugin",
				(gpointer) &init_plugin))
		{
		snprintf(buf, sizeof(buf), _("\tError: %s\n"), g_module_error());
		plugin_log(buf, NULL);
		g_module_close(module);
		return NULL;
		}
	_GK.no_messages = TRUE;		/* Enforce policy */

	mon_tmp.name = g_strdup(plugin_name);
	gkrellm_record_state(INIT_MONITOR, &mon_tmp);

#if defined(WIN32)
	{
		win32_plugin_callbacks ** plugin_cb = NULL;

		if (!g_module_symbol(module, "callbacks", (gpointer) &plugin_cb))
		{
			snprintf(buf, sizeof(buf), _("\tError: %s\n"), g_module_error());
			plugin_log(buf, NULL);
			g_module_close(module);
			return NULL;
		}
		*plugin_cb = &gkrellm_callbacks;
	}
#endif

	m = (*init_plugin)();

	_GK.no_messages = FALSE;

	g_free(mon_tmp.name);
	mon_tmp.name = NULL;
	gkrellm_record_state(INTERNAL, NULL);

	if (m == NULL)
		{
		plugin_log(_("\tOoops! plugin returned NULL, aborting\n"), NULL);
		g_module_close(module);
		return NULL;
		}
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mm = (GkrellmMonitor *) list->data;
		if (   !mm->privat || !mm->privat->style_name
			|| !m->privat  || !m->privat->style_name
			|| strcmp(mm->privat->style_name, m->privat->style_name)
		   )
			continue;
		plugin_log(_("\tWarning: style name \""), m->privat->style_name,
			_("\" already used by:\n\t\t"), mm->path, "\n", NULL);
		}
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mm = (GkrellmMonitor *) list->data;
		if (   !mm->config_keyword || !m->config_keyword
			|| strcmp(mm->config_keyword, m->config_keyword)
		   )
			continue;
		plugin_log(_("\tWarning: config keyword \""), m->config_keyword,
			_("\" already used by:\n\t\t"), mm->path, "\n", NULL);
		}
	m->handle = module;
	m->path = plugin_name;
	if (!m->name)
		m->name = g_path_get_basename(m->path);
	if (m->privat == NULL)		/* Won't be null if style was added */
		m->privat = g_new0(GkrellmMonprivate, 1);
	m->privat->enabled = TRUE;

	/* Enforce some id fields.
	*/
	m->id &= ~(MON_ID_MASK | MON_CONFIG_MASK);
	m->id |= MON_PLUGIN;
	if (PLUGIN_INSERT_BEFORE_ID(m) >= N_BUILTIN_MONITORS)
		m->insert_before_id = MON_UPTIME;
	if (!user_placement(m))
		{
		m->privat->insert_before_id = PLUGIN_INSERT_BEFORE_ID(m);
		m->privat->gravity = PLUGIN_GRAVITY(m);
		m->privat->insert_after = PLUGIN_INSERT_AFTER(m);
		}
	gkrellm_place_plugin(&gkrellm_monitor_list, m);
	return m;
	}

pid_t
gkrellm_get_pid(void)
	{
	return getpid();
	}

void
gkrellm_disable_plugin_connect(GkrellmMonitor *mon, void (*cb_func)())
	{
	if (!mon || !mon->privat || !cb_func)
		return;
	mon->privat->cb_disable_plugin = cb_func;
	}

static void
disable_plugin(GkrellmMonitor *plugin)
	{
	GkrellmMonprivate	*mp = plugin->privat;

	if (!mp->enabled)
		return;
	if (mp->top_spacer.image)
		gtk_widget_destroy(mp->top_spacer.image);
	mp->top_spacer.image = NULL;
	if (mp->bottom_spacer.image)
		gtk_widget_destroy(mp->bottom_spacer.image);
	mp->bottom_spacer.image = NULL;

	/* It is not safe to destroy a plugin widget tree.
	|  They have already done one time first_create stuff.
	*/
	gtk_widget_hide(mp->vbox);
	gtk_widget_hide(mp->main_vbox);
	mp->enabled = FALSE;
	if (mp->cb_disable_plugin)
		(*mp->cb_disable_plugin)();
	gkrellm_build();
	gkrellm_remove_plugin_config_page(plugin);
	plugin_enable_list_modified = TRUE;
	}

static void
enable_plugin(GkrellmMonitor *plugin)
	{
	GkrellmMonprivate	*mp = plugin->privat;

	if (mp->enabled)
		return;
	gtk_widget_show(mp->vbox);
	gtk_widget_show(mp->main_vbox);
	mp->enabled = TRUE;
	gkrellm_load_user_config(plugin, TRUE);
	gkrellm_build();
	gkrellm_add_plugin_config_page(plugin);
	plugin_enable_list_modified = TRUE;
	}

static void
load_plugins_placement_file(void)
	{
	FILE	*f;
	gchar	*path, *s, buf[256];

	gkrellm_free_glist_and_data(&plugins_place_list);
	path = gkrellm_make_config_file_name(gkrellm_homedir(),
				PLUGIN_PLACEMENT_FILE);
	if ((f = g_fopen(path, "r")) != NULL)
		{
		while ((fgets(buf, sizeof(buf), f)) != NULL)
			{
			if ((s = strchr(buf, '\n')) != NULL)
				*s = '\0';
			s = g_strdup(buf);
			plugins_place_list = g_list_append(plugins_place_list, s);
			}
		fclose(f);
		}
	g_free(path);
	}

static void
save_plugins_placement_file(void)
	{
	FILE				*f;
	GList				*list;
	GkrellmMonitor		*builtin, *plugin;
	GkrellmMonprivate	*mp;
	gchar				*path;

	if (!plugin_placement_modified || _GK.demo || _GK.no_config)
		return;
	path = gkrellm_make_config_file_name(gkrellm_homedir(),
				PLUGIN_PLACEMENT_FILE);
	if ((f = g_fopen(path, "w")) != NULL)
		{
		for (list = plugins_list; list; list = list->next)
			{
			plugin = (GkrellmMonitor *) list->data;
			mp  = plugin->privat;
			if (   mp->enabled && !mp->from_command_line
				&& (   mp->insert_before_id != PLUGIN_INSERT_BEFORE_ID(plugin)
					|| mp->insert_after != PLUGIN_INSERT_AFTER(plugin)
					|| mp->gravity != PLUGIN_GRAVITY(plugin)
				   )
			   )
				{
				builtin = gkrellm_monitor_from_id(mp->insert_before_id);
				if (!builtin)
					continue;
				fprintf(f, "\"%s\" \"%s\" %d %d\n", plugin->name,
						builtin->name, mp->insert_after, mp->gravity);
				}
			}
		fclose(f);
		}
	plugin_placement_modified = FALSE;
	g_free(path);
	}

static void
load_plugins_enable_file(void)
	{
	FILE	*f;
	gchar	*path, *s, buf[256];

	gkrellm_free_glist_and_data(&plugins_enable_list);
	path = gkrellm_make_config_file_name(gkrellm_homedir(),PLUGIN_ENABLE_FILE);
	if ((f = g_fopen(path, "r")) != NULL)
		{
		while ((fgets(buf, sizeof(buf), f)) != NULL)
			{
			if ((s = strchr(buf, '\n')) != NULL)
				*s = '\0';
			if (!buf[0])
				continue;
			s = g_path_get_basename(buf);
			plugins_enable_list = g_list_append(plugins_enable_list, s);
			}
		fclose(f);
		}
	g_free(path);
	}

static void
save_plugins_enable_file(void)
	{
	FILE			*f;
	GList			*list;
	GkrellmMonitor	*m;
	gchar			*path, *s;

	if (!plugin_enable_list_modified || _GK.demo)
		return;
	path = gkrellm_make_config_file_name(gkrellm_homedir(),
				PLUGIN_ENABLE_FILE);
	if ((f = g_fopen(path, "w")) != NULL)
		{
		for (list = plugins_list; list; list = list->next)
			{
			m = (GkrellmMonitor *) list->data;
			if (m->privat->enabled && !m->privat->from_command_line)
				{
				s = g_path_get_basename(m->path);
				fprintf(f, "%s\n", s);
				g_free(s);
				}
			}
		fclose(f);
		}
	plugin_enable_list_modified = FALSE;
	g_free(path);
	}

static gchar *
string_suffix(gchar *string, gchar *suffix)
	{
	gchar	*dot;

	dot = strrchr(string, '.');
	if (dot && !strcmp(dot + 1, suffix))
		return dot + 1;
	return NULL;
	}

static void
scan_for_plugins(gchar *path)
	{
    GDir			*dir;
    gchar			*name, *filename;
	GList			*list;
	GkrellmMonitor	*m = NULL;
	gchar			*s;
	gboolean		exists;

	if (!path || !*path || (dir = g_dir_open(path, 0, NULL)) == NULL)
		return;
	while ((name = (gchar *) g_dir_read_name(dir)) != NULL)
		{
		if (   !string_suffix(name, "so")
			&& !string_suffix(name, "la")
			&& !string_suffix(name, "dll")
		   )
			continue;

		/* If there's a libtool .la archive, won't want to load this .so
		*/
		if (   !string_suffix(name, "la")
			&& (s = strrchr(name, '.')) != NULL
		   )
			{
			s = g_strndup(name, s - name);
			filename = g_strconcat(path, G_DIR_SEPARATOR_S, s, ".la", NULL);
			exists = g_file_test(filename, G_FILE_TEST_EXISTS);
			g_free(s);
			g_free(filename);
			if (exists)
				continue;
			}
		for (list = plugins_list; list; list = list->next)
			{
			m = (GkrellmMonitor *) list->data;
			s = g_path_get_basename(m->path);
			exists = !strcmp(s, name);
			g_free(s);
			if (exists)
				break;
			m = NULL;
			}
		s = g_strconcat(path, G_DIR_SEPARATOR_S, name, NULL);
		if (m)
			{
			plugin_log(_("Ignoring duplicate plugin "), s, "\n", NULL);
			g_free(s);
			continue;
			}
		m = install_plugin(s);
		if (m)	/* s is saved for use */
			{
			plugins_list = g_list_append(plugins_list, m);
			s = g_path_get_basename(m->path);
			if (! gkrellm_string_in_list(plugins_enable_list, s))
				m->privat->enabled = FALSE;
			}
		g_free(s);
		}
	g_dir_close(dir);
	}

void
gkrellm_plugins_load(void)
	{
	GkrellmMonitor	*m;
	gchar			*path;

	if (_GK.command_line_plugin)
		{
		if (   *_GK.command_line_plugin != '.'
			&& !strchr(_GK.command_line_plugin, G_DIR_SEPARATOR)
		   )
			path = g_strconcat(".", G_DIR_SEPARATOR_S,
						_GK.command_line_plugin, NULL);
		else
			path = g_strdup(_GK.command_line_plugin);
		plugin_log(_("*** Command line plugin:\n"), NULL);
		if ((m = install_plugin(path)) == NULL)
			g_free(path);
		else
			{
			m->privat->from_command_line = TRUE;
			plugins_list = g_list_append(plugins_list, m);
			}
		plugin_log("\n", NULL);
		}
	load_plugins_enable_file();
	load_plugins_placement_file();
	path = g_strconcat(gkrellm_homedir(), G_DIR_SEPARATOR_S,
				GKRELLM_PLUGINS_DIR, NULL);
	scan_for_plugins(path);
	g_free(path);

#if defined(WIN32)
	path = NULL;
#if GLIB_CHECK_VERSION(2,16,0)
	gchar *install_path;
	install_path = g_win32_get_package_installation_directory_of_module(NULL);
	if (install_path != NULL)
		{
		path = g_build_filename(install_path, "lib", "gkrellm2", "plugins", NULL);
		g_free(install_path);
		}
#else
	// deprecated since glib 2.16.0
	path = g_win32_get_package_installation_subdirectory(NULL, NULL, "lib/gkrellm2/plugins");
#endif
	if (path)
		{
		scan_for_plugins(path);
		g_free(path);
		}
#endif


#if defined(LOCAL_PLUGINS_DIR)
	scan_for_plugins(LOCAL_PLUGINS_DIR);
#endif

#if defined(SYSTEM_PLUGINS_DIR)
	scan_for_plugins(SYSTEM_PLUGINS_DIR);
#endif
	}

#if 0
void
shuffle_test(void)
	{
	GList			*list;
	GkrellmMonitor	*mon;
	gint			i;
	static gboolean	srand_done;
	extern GtkWidget *gkrellm_monitor_vbox();

	if (!srand_done)
		{
		srandom((unsigned int) time(0));
		srand_done = TRUE;
		}
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		i = (gint) random();
		i = (i % (g_list_length(gkrellm_monitor_list) - 2)) + 1;
		mon = (GkrellmMonitor *) list->data;
		if (mon->privat->main_vbox && mon != gkrellm_mon_host())
			{
			gtk_box_reorder_child(GTK_BOX(gkrellm_monitor_vbox()),
						mon->privat->main_vbox, i);
			}
		}
	}
#endif

static void
replace_plugins()
	{
	GList			*new_monitor_list, *list;
	GkrellmMonitor	*mon;
	extern GtkWidget *gkrellm_monitor_vbox();

	new_monitor_list = NULL;
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (mon->privat->main_vbox && mon != gkrellm_mon_host())
			{
			gtk_widget_ref(mon->privat->main_vbox);
			gtk_container_remove(GTK_CONTAINER(gkrellm_monitor_vbox()),
					mon->privat->main_vbox);
			}
		if (MONITOR_ID(mon) != MON_PLUGIN)
			new_monitor_list = g_list_append(new_monitor_list, mon);
		}
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (MONITOR_ID(mon) == MON_PLUGIN)
			gkrellm_place_plugin(&new_monitor_list, mon);
		}
	g_list_free(gkrellm_monitor_list);
	gkrellm_monitor_list = new_monitor_list;
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (mon->privat->main_vbox && mon != gkrellm_mon_host())
			{
			gtk_box_pack_start(GTK_BOX(gkrellm_monitor_vbox()),
					mon->privat->main_vbox, FALSE, FALSE, 0);
			gtk_widget_unref(mon->privat->main_vbox);
			}
		}
	}


static GtkWidget	*place_plugin_window;
static GtkWidget	*place_plugin_vbox;
static gboolean		setting_place_buttons;

static void
apply_place(void)
	{
	GList			*list;
	GkrellmMonitor	*mon;
	gint			i;

	if (!place_plugin || setting_place_buttons)
		return;
	for (i = 0; i < N_BUILTIN_MONITORS; ++i)
		if (GTK_TOGGLE_BUTTON(builtin_button[i])->active)
			break;
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (MONITOR_ID(mon) == MON_PLUGIN || i != mon->privat->button_id)
			continue;
		place_plugin->privat->insert_before_id = mon->id;
		place_plugin->privat->insert_after =
				GTK_TOGGLE_BUTTON(after_button)->active;
		if (place_plugin->privat->insert_after)
			place_plugin->privat->gravity = 0;
		else
			place_plugin->privat->gravity = gtk_spin_button_get_value_as_int(
						GTK_SPIN_BUTTON(gravity_spin_button));

		replace_plugins();
		plugin_placement_modified = TRUE;
		break;
		}
	}

static void
cb_close_place(void)
	{
	if (place_plugin_window)
		gtk_widget_destroy(place_plugin_window);
	place_plugin_window = NULL;
	place_plugin_vbox = NULL;
	}

static gint
place_plugin_window_delete_event(GtkWidget *widget, GdkEvent *ev,gpointer data)
	{
	cb_close_place();
	return FALSE;
	}

static void
cb_place_default(GtkWidget *widget, gpointer data)
	{
	GkrellmMonitor	*mon;

	if (!place_plugin)
		return;
	if (PLUGIN_INSERT_AFTER(place_plugin))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(after_button),
				TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(before_button),
				TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(gravity_spin_button),
			(gfloat) PLUGIN_GRAVITY(place_plugin));
	mon = gkrellm_monitor_from_id(PLUGIN_INSERT_BEFORE_ID(place_plugin));
	if (mon && mon->privat->button_id >= 0)
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(builtin_button[mon->privat->button_id]), TRUE);
	}

static void
place_button_sensitivity(GkrellmMonitor *plugin, gboolean state)
	{
	GkrellmMonitor	*mon = NULL;

	if (gkrellm_demo_mode())
		{
		gtk_widget_set_sensitive(place_button, FALSE);
		return;
		}
	place_plugin = plugin;
	if (state && plugin && plugin->create_monitor)
		{
		gtk_widget_set_sensitive(place_button, TRUE);
		if (place_plugin_vbox)
			gtk_widget_set_sensitive(place_plugin_vbox, TRUE);
		}
	else
		{
		gtk_widget_set_sensitive(place_button, FALSE);
		if (place_plugin_vbox)
			gtk_widget_set_sensitive(place_plugin_vbox, FALSE);
		return;
		}
	if (!place_plugin_window)
		return;

	setting_place_buttons = TRUE;
	gtk_label_set_text(GTK_LABEL(place_label), plugin->name);
	if (plugin->privat->insert_after)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(after_button), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(before_button), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(gravity_spin_button),
			(gfloat) plugin->privat->gravity);
	mon = gkrellm_monitor_from_id(plugin->privat->insert_before_id);
	if (mon && mon->privat->button_id >= 0)
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(builtin_button[mon->privat->button_id]), TRUE);
	setting_place_buttons = FALSE;
	}

static gboolean
cb_place(GtkToggleButton *button, gpointer data)
	{
	if (!button->active)
		return FALSE;
	apply_place();
	return TRUE;
	}

static void
cb_place_spin(GtkWidget *widget, GtkSpinButton *spin)
	{
	if (!place_plugin)
		return;
	if (place_plugin->privat->insert_after)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(gravity_spin_button), 0.0);
	else
		apply_place();
	return;
	}

static void
cb_place_button(GtkWidget *widget, gpointer data)
	{
	GtkWidget		*main_vbox, *vbox, *vbox1, *vbox2, *vbox3, *hbox;
	GtkWidget		*button;
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	GSList			*group;
	GList			*list;
	GkrellmMonitor	*mon;
	GkrellmMonprivate *mp;
	gint			i;

	if (gkrellm_demo_mode())
		return;
	if (!place_plugin_window)
		{
		place_plugin_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		g_signal_connect(G_OBJECT(place_plugin_window), "delete_event",
				G_CALLBACK(place_plugin_window_delete_event), NULL);
		gtk_window_set_title(GTK_WINDOW(place_plugin_window),
				_("GKrellM Place Plugin"));
		gtk_window_set_wmclass(GTK_WINDOW(place_plugin_window),
				"Gkrellm_conf", "Gkrellm");

		main_vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(place_plugin_window), main_vbox);
		place_plugin_vbox = main_vbox;
		vbox = gkrellm_gtk_framed_vbox(main_vbox, NULL, 3, FALSE, 4, 3);
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		vbox1 = gkrellm_gtk_framed_vbox(hbox, _("Builtin Monitors"),
				3, FALSE, 4, 3);
		group = NULL;
		for (i = 0, list = gkrellm_monitor_list; list; list = list->next)
			{
			mon = (GkrellmMonitor *) list->data;
			mp  = mon->privat;
			mon->privat->button_id = -1;
			if (MONITOR_ID(mon) != MON_PLUGIN)
				{
				if (   !mon->name || !mon->create_monitor
					|| mon == gkrellm_mon_host()
				   )
					continue;
				button = gtk_radio_button_new_with_label(group, mon->name);
				g_signal_connect(G_OBJECT(button), "clicked",
						G_CALLBACK(cb_place), NULL);
				gtk_box_pack_start(GTK_BOX(vbox1), button, FALSE, FALSE, 0);
				if (i < N_BUILTIN_MONITORS)
					{
					builtin_button[i] = button;
					mon->privat->button_id = i++;
					}
				group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
				}
			}
		vbox1 = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), vbox1, FALSE, FALSE, 0);
		place_label = gtk_label_new("");
		gtk_box_pack_start(GTK_BOX(vbox1), place_label, FALSE, FALSE, 10);
		vbox2 = gkrellm_gtk_framed_vbox(vbox1, _("Place Plugin"),
				3, FALSE, 4, 3);

		group = NULL;
		vbox3 = gkrellm_gtk_framed_vbox(vbox2, NULL, 3, FALSE, 4, 3);
		before_button = gtk_radio_button_new_with_label(group,
				_("Before selected builtin monitor"));
		g_signal_connect(G_OBJECT(before_button), "clicked",
				G_CALLBACK(cb_place), NULL);
		gtk_box_pack_start(GTK_BOX(vbox3), before_button, FALSE, FALSE, 0);
		gkrellm_gtk_spin_button(vbox3, &gravity_spin_button, 8,
				0.0, 15.0, 1, 1, 0, 50,
				cb_place_spin, NULL, FALSE,
				_("With gravity"));

		vbox3 = gkrellm_gtk_framed_vbox(vbox2, NULL, 3, FALSE, 4, 3);
		group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(before_button));
		after_button = gtk_radio_button_new_with_label(group,
				_("After selected builtin monitor"));
		g_signal_connect(G_OBJECT(after_button), "clicked",
				G_CALLBACK(cb_place), NULL);
		gtk_box_pack_start(GTK_BOX(vbox3), after_button, FALSE, FALSE, 0);

		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
		gkrellm_gtk_button_connected(hbox, NULL, TRUE, FALSE, 0,
				cb_place_default, NULL, _("Plugin Defaults"));

		hbox = gtk_hbutton_box_new();
		gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
		gtk_box_set_spacing(GTK_BOX(hbox), 5);
		gtk_box_pack_end(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);

		button = gtk_button_new_from_stock(GTK_STOCK_OK);
		GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
		g_signal_connect(G_OBJECT(button), "clicked",
				G_CALLBACK(cb_close_place), NULL);
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

		gtk_widget_show_all(place_plugin_window);
		}
	else
		gtk_window_present(GTK_WINDOW(place_plugin_window));
//		gdk_window_raise(place_plugin_window->window);

	if (row_reference)
		{
		model = gtk_tree_view_get_model(treeview);
		path = gtk_tree_row_reference_get_path(row_reference);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get(model, &iter, MON_COLUMN, &mon, -1);
		place_button_sensitivity(mon, mon->privat->enabled);
		}
	}

static void
cb_enable_plugin(GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
	{
	GtkTreeModel	*model = (GtkTreeModel *)data;
	GtkTreeIter		iter;
	GtkTreePath		*path;
	GkrellmMonitor	*mon;
	gboolean		enable;

	path = gtk_tree_path_new_from_string(path_str);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, &iter,
				ENABLE_COLUMN, &enable,
				MON_COLUMN, &mon,
				-1);
	if (mon->privat->from_command_line)
		enable = TRUE;
	else
		enable = !enable;
	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
				ENABLE_COLUMN, enable, -1);
	if (enable)
		enable_plugin(mon);
	else
		disable_plugin(mon);
	place_button_sensitivity(mon, enable);
	}

static void
cb_tree_selection_changed(GtkTreeSelection *selection, gpointer data)
	{
	GtkTreeIter		iter;
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GkrellmMonitor	*mon;

	gtk_tree_row_reference_free(row_reference);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		{
		row_reference = NULL;
		place_button_sensitivity(NULL, FALSE);
		return;
		}
	path = gtk_tree_model_get_path(model, &iter);
	row_reference = gtk_tree_row_reference_new(model, path);
	gtk_tree_model_get(model, &iter, MON_COLUMN, &mon, -1);
	place_button_sensitivity(mon, mon->privat->enabled);
	}

void
gkrellm_plugins_config_close(void)
	{
	save_plugins_enable_file();
	save_plugins_placement_file();
	cb_close_place();
	}

static GtkTreeModel *
create_model(void)
	{
	GtkListStore	*store;
	GtkTreeIter		iter;
	GkrellmMonitor	*mon;
	GList			*list;
	gchar			*buf;

	store = gtk_list_store_new(N_COLUMNS,
				G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);
	for (list = plugins_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (mon->privat->from_command_line)
			buf = g_strdup_printf("%s  (%s)", mon->name,
					_("from command line"));
		else
			buf = NULL;

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				NAME_COLUMN, buf ? buf : mon->name,
				ENABLE_COLUMN, mon->privat->enabled,
				MON_COLUMN, mon,
				-1);
		g_free(buf);
		}
	return GTK_TREE_MODEL(store);
	}

void
gkrellm_plugins_config_create(GtkWidget *tab_vbox)
	{
	GtkWidget			*tabs;
	GtkWidget			*vbox;
	GtkWidget			*hbox;
	GtkWidget			*scrolled;
	GtkWidget			*view;
	GtkTreeModel		*model;
	GtkCellRenderer		*renderer;
	GtkTreeSelection	*selection;
	GtkTreeViewColumn	*column;
	GtkTextIter			iter;
	GtkTextBuffer		*buffer;

	row_reference = NULL;
	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);


/* -- Plugins tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Plugins"));

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 2);

	model = create_model();
	treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(G_OBJECT(model));
	gtk_tree_view_set_rules_hint(treeview, TRUE);

	renderer = gtk_cell_renderer_toggle_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Enable"),
				renderer,
				"active", ENABLE_COLUMN, NULL);
	g_signal_connect (G_OBJECT(renderer), "toggled",
				G_CALLBACK(cb_enable_plugin), model);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes (_("Plugin"),
				renderer,
				"text", NAME_COLUMN, NULL);
	gtk_tree_view_append_column(treeview, column);
	gtk_tree_view_column_set_sort_column_id(column, NAME_COLUMN);
	gtk_tree_view_column_clicked(column);		/* Sort it */
/* gtk_tree_sortable_set_sort_column_id(model,
			NAME_COLUMN, GTK_ORDER_ASCENDING);
*/

	gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(treeview));
	selection = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed",
				G_CALLBACK(cb_tree_selection_changed), NULL);

	hbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_START);
	gtk_box_set_spacing(GTK_BOX(hbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

	place_button = gtk_button_new_with_label(_("Place"));
	gtk_box_pack_start(GTK_BOX(hbox), place_button, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(GTK_BUTTON(place_button)), "clicked",
			G_CALLBACK(cb_place_button), NULL);
	gtk_widget_set_sensitive(place_button, FALSE);

/* --Plugins detect log tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Install Log"));
	view = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	gtk_text_buffer_get_end_iter(buffer, &iter);
	if (plugin_install_log)
		gtk_text_buffer_insert(buffer, &iter, plugin_install_log, -1);
	else
		gtk_text_buffer_insert(buffer, &iter, _("No plugins found."), -1);
	}
