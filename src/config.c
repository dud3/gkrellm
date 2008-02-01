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

#include "gkrellm.h"
#include "gkrellm-private.h"

#if !defined(WIN32)
#include <unistd.h> /* needed for gethostname() */
#else
#include <winsock2.h> /* needed for gethostname() */
#endif

#include	"pixmaps/frame_top.xpm"
#include	"pixmaps/frame_bottom.xpm"
#include	"pixmaps/frame_left.xpm"
#include	"pixmaps/frame_right.xpm"

#include	"pixmaps/button_panel_out.xpm"
#include	"pixmaps/button_panel_in.xpm"
#include	"pixmaps/button_meter_out.xpm"
#include	"pixmaps/button_meter_in.xpm"

#include	"pixmaps/bg_chart.xpm"
#include	"pixmaps/bg_grid.xpm"
#include	"pixmaps/bg_panel.xpm"
#include	"pixmaps/bg_separator.xpm"

#include	"pixmaps/bg_meter.xpm"

#include	"pixmaps/decal_alarm.xpm"
#include	"pixmaps/decal_warn.xpm"
//#include	"pixmaps/krell_alarm.xpm"
//#include	"pixmaps/krell_warn.xpm"

  /* These data images are used only for the default theme
  */
#include	"pixmaps/data_in.xpm"
#include	"pixmaps/data_in_grid.xpm"
#include	"pixmaps/data_out.xpm"
#include	"pixmaps/data_out_grid.xpm"

#include	"pixmaps/decal_misc.xpm"
#include	"pixmaps/decal_button.xpm"

#include	"pixmaps/krell_panel.xpm"
#include	"pixmaps/krell_meter.xpm"
#include	"pixmaps/krell_slider.xpm"
#include	"pixmaps/krell_mini.xpm"

  /* Theme images for builtin monitors.
  */
#include    "pixmaps/cal/bg_panel.xpm"
#include    "pixmaps/clock/bg_panel.xpm"
#include	"pixmaps/cpu/nice.xpm"
#include	"pixmaps/cpu/nice_grid.xpm"
#include    "pixmaps/fs/bg_panel.xpm"
#include    "pixmaps/fs/bg_panel_1.xpm"
#include    "pixmaps/fs/bg_panel_2.xpm"
#include	"pixmaps/fs/spacer_top.xpm"
#include	"pixmaps/fs/spacer_bottom.xpm"
#include	"pixmaps/host/bg_panel.xpm"
#ifdef BSD
#include	"pixmaps/mail/krell_mail_daemon.xpm"
#else
#include	"pixmaps/mail/krell_mail.xpm"
#endif
#include	"pixmaps/mem/bg_panel.xpm"
#include	"pixmaps/mem/krell.xpm"
#include    "pixmaps/sensors/bg_panel.xpm"
#include    "pixmaps/sensors/bg_panel_1.xpm"
#include    "pixmaps/sensors/bg_panel_2.xpm"
#include	"pixmaps/swap/bg_panel.xpm"
#include	"pixmaps/swap/krell.xpm"
#include	"pixmaps/uptime/bg_panel.xpm"
#include	"pixmaps/timer/bg_panel.xpm"

  /* Default theme images for various plugins
  */
#include	"pixmaps/gkrellmms/krell.xpm"
#include	"pixmaps/gkrellmms/bg_scroll.xpm"
#include	"pixmaps/gkrellmms/bg_panel.xpm"
#include	"pixmaps/gkrellmms/bg_panel_1.xpm"
#include	"pixmaps/gkrellmms/bg_panel_2.xpm"
#include	"pixmaps/gkrellmms/spacer_top.xpm"
#include	"pixmaps/gkrellmms/spacer_bottom.xpm"
#include	"pixmaps/gkrellmms/play_button.xpm"
#include	"pixmaps/gkrellmms/prev_button.xpm"
#include	"pixmaps/gkrellmms/stop_button.xpm"
#include	"pixmaps/gkrellmms/next_button.xpm"
#include	"pixmaps/gkrellmms/eject_button.xpm"
#include	"pixmaps/gkrellmms/led_indicator.xpm"

#include	"pixmaps/timers/bg_panel.xpm"
#include	"pixmaps/timers/bg_panel_1.xpm"
#include	"pixmaps/timers/bg_panel_2.xpm"
#include	"pixmaps/timers/spacer_top.xpm"
#include	"pixmaps/timers/spacer_bottom.xpm"

#include	"pixmaps/volume/bg_panel.xpm"
#include	"pixmaps/volume/bg_panel_1.xpm"
#include	"pixmaps/volume/bg_panel_2.xpm"
#include	"pixmaps/volume/spacer_top.xpm"
#include	"pixmaps/volume/spacer_bottom.xpm"

#include	"pixmaps/pmu/bg_panel.xpm"
#include	"pixmaps/pmu/bg_panel_1.xpm"
#include	"pixmaps/pmu/bg_panel_2.xpm"
#include	"pixmaps/pmu/spacer_top.xpm"
#include	"pixmaps/pmu/spacer_bottom.xpm"

#define	SET_ALL_MARGINS	0x1000000
#define	OLD_SET_MARGIN	0x2000000

static gchar	*image_type[] =
	{
	".png", ".jpg", ".xpm", ".gif"
	};

gchar *
gkrellm_theme_file_exists(char *name, gchar *subdir)
	{
	gint			i;
	static gchar	*path;
	struct stat		st;

	if (gkrellm_using_default_theme())
		return NULL;
	if (path)
		g_free(path);
	if (_GK.theme_alternative > 0)
		{
		for (i = 0; i < sizeof(image_type) / sizeof(char *); ++i)
			{
			if (subdir)
				path = g_strdup_printf("%s/%s/%s_%d%s", _GK.theme_path, subdir,
						name, _GK.theme_alternative, image_type[i]);
			else
				path = g_strdup_printf("%s/%s_%d%s", _GK.theme_path,
						name, _GK.theme_alternative, image_type[i]);
#ifdef WIN32
			if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
#else
			if (   stat(path, &st) == 0
				&& (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))
			   )
#endif
				return path;
			g_free(path);
			path = NULL;
			}
		}
	for (i = 0; i < sizeof(image_type) / sizeof(char *); ++i)
		{
		if (subdir)
			path = g_strdup_printf("%s/%s/%s%s", _GK.theme_path, subdir, name,
					image_type[i]);
		else
			path = g_strdup_printf("%s/%s%s", _GK.theme_path, name,
					image_type[i]);
#ifdef WIN32
			if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
#else
		if (   stat(path, &st) == 0
			&& (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))
		   )
#endif
			return path;
		g_free(path);
		path = NULL;
		}
	return NULL;
	}

static void
set_border(GkrellmBorder *border, char *string)
	{
	if (!border)
		return;
	border->left = 0;
	border->right = 0;
	border->top = 0;
	border->bottom = 0;
	if (string == NULL)
		return;
	sscanf(string, "%d,%d,%d,%d", &border->left, &border->right,
				&border->top, &border->bottom);
	}

static void
set_margins(GkrellmStyle *style, char *string)
	{
	GkrellmMargin	*m;

	_GK.use_top_bottom_margins = TRUE;
	if (!style)
		return;
	m = &style->margin;
	m->left = 0;
	m->right = 0;
	m->top = 0;
	m->bottom = 0;
	if (string == NULL)
		return;
	sscanf(string, "%d,%d,%d,%d", &m->left, &m->right, &m->top, &m->bottom);
	m->left = m->left * _GK.theme_scale / 100;
	m->right = m->right * _GK.theme_scale / 100;
	m->top = m->top * _GK.theme_scale / 100;
	m->bottom = m->bottom * _GK.theme_scale / 100;
	}


static void
assign_font(GkrellmStyle *style, gchar *fontname, gint AorB)
	{
	GkrellmTextstyle	*ts;

	ts = (AorB == GKRELLMSTYLE_TEXTFONT_A)
				? &style->label_tsA : &style->label_tsB;

	if (strcmp(fontname, "large_font") == 0)
		ts->font_seed = &_GK.large_font;
	else if (strcmp(fontname, "normal_font") == 0)
		ts->font_seed = &_GK.normal_font;
	else if (strcmp(fontname, "small_font") == 0)
		ts->font_seed = &_GK.small_font;
	}

static void
assign_textcolor(GkrellmStyle *style, gchar *arg, gint AorB)
	{
	GkrellmTextstyle *ts;
	gchar			*values, *s;
	gchar			*color, *shadowcolor, *effect;

	values = g_strconcat(arg, NULL);

	color = gkrellm_cut_quoted_string(values, &s);
	shadowcolor = gkrellm_cut_quoted_string(s, &s);
	effect = gkrellm_cut_quoted_string(s, &s);
	if (*color == '\0' || *shadowcolor == '\0' || *effect == '\0')
		{
		printf(_("Bad textcolor line %s\n"), arg);
		g_free(values);
		return;
		}
	ts = (AorB == GKRELLMSTYLE_TEXTCOLOR_A)
				? &style->label_tsA : &style->label_tsB;
	gkrellm_map_color_string(color, &(ts->color));
	gkrellm_map_color_string(shadowcolor, &(ts->shadow_color));
	ts->effect = gkrellm_effect_string_value(effect);
	g_free(values);
	}

gboolean
gkrellm_style_is_themed(GkrellmStyle *style, gint query)
	{
	if (query == 0)
		query = ~0;
	return (style->themed & query) ? TRUE : FALSE;
	}

void
gkrellm_set_style_krell_values(GkrellmStyle *s, gint yoff, gint depth,
		gint x_hot, gint expand, gint ema, gint left_margin, gint right_margin)
	{
	if (!s)
		return;
	if (yoff >= -3)
		{
		s->krell_yoff = yoff;
		if (yoff > 0)
			s->krell_yoff_not_scalable = TRUE;
		}
	if (left_margin >= 0)
		s->krell_left_margin = left_margin;
	if (right_margin >= 0)
		s->krell_right_margin = right_margin;
	if (depth > 0)
		s->krell_depth = depth;
	if (x_hot >= -1)
		s->krell_x_hot = x_hot;
	if (expand >= 0)
		s->krell_expand = expand;
	if (ema > 0)
		s->krell_ema_period = ema;
	}

void
gkrellm_set_style_krell_values_default(GkrellmStyle *s, gint yoff, gint depth,
		gint x_hot, gint expand, gint ema, gint left_margin, gint right_margin)
	{
	if (!s)
		return;
	if (yoff >= -3 && !(s->themed & GKRELLMSTYLE_KRELL_YOFF))
		{
		s->krell_yoff = yoff;
		if (yoff > 0)
			s->krell_yoff_not_scalable = TRUE;
		}
	if (left_margin >= 0 && !(s->themed & GKRELLMSTYLE_KRELL_LEFT_MARGIN))
		s->krell_left_margin = left_margin;
	if (right_margin >= 0 && !(s->themed & GKRELLMSTYLE_KRELL_RIGHT_MARGIN))
		s->krell_right_margin = right_margin;
	if (depth > 0 && !(s->themed & GKRELLMSTYLE_KRELL_DEPTH))
		s->krell_depth = depth;
	if (x_hot >= -1 && !(s->themed & GKRELLMSTYLE_KRELL_X_HOT))
		s->krell_x_hot = x_hot;
	if (expand >= 0 && !(s->themed & GKRELLMSTYLE_KRELL_EXPAND))
		s->krell_expand = expand;
	if (ema > 0 && !(s->themed & GKRELLMSTYLE_KRELL_EMA_PERIOD))
		s->krell_ema_period = ema;
	}

void
gkrellm_set_style_slider_values_default(GkrellmStyle *s, gint yoff,
		gint left_margin, gint right_margin)
	{
	gint	themed, y, left, right;

	if (!s)
		return;
	themed = s->themed;
	y = s->krell_yoff;
	left = s->krell_left_margin;
	right = s->krell_right_margin;
	gkrellm_copy_style_values(s, _GK.krell_slider_style);
	s->themed = themed;
	s->krell_yoff = y;
	s->krell_left_margin = left;
	s->krell_right_margin = right;

	if (yoff >= - 3 && !(s->themed & GKRELLMSTYLE_KRELL_YOFF))
		{
		s->krell_yoff = yoff;
		if (yoff > 0)
			s->krell_yoff_not_scalable = TRUE;
		}
	if (left_margin >= 0 && !(s->themed & GKRELLMSTYLE_KRELL_LEFT_MARGIN))
		s->krell_left_margin = left_margin;
	if (right_margin >= 0 && !(s->themed & GKRELLMSTYLE_KRELL_RIGHT_MARGIN))
		s->krell_right_margin = right_margin;
	}

void
gkrellm_set_krell_expand(GkrellmStyle *style, gchar *value)
	{
	gint	expand	= KRELL_EXPAND_NONE;

	if (!style)
		return;
	if (value)
		{
		if (!strcmp(value, "left"))
			expand = KRELL_EXPAND_LEFT;
		else if (!strcmp(value, "left-scaled"))
			expand = KRELL_EXPAND_LEFT_SCALED;
		else if (!strcmp(value, "right"))
			expand = KRELL_EXPAND_RIGHT;
		else if (!strcmp(value, "right-scaled"))
			expand = KRELL_EXPAND_RIGHT_SCALED;
		else if (!strcmp(value, "bar-mode"))
			expand = KRELL_EXPAND_BAR_MODE;
		else if (!strcmp(value, "bar-mode-scaled"))
			expand = KRELL_EXPAND_BAR_MODE_SCALED;
		}
	style->krell_expand = expand;
	}

static gboolean
parse_boolean(gchar *value)
	{
	if (   !strcmp("1", value)
		|| !strcasecmp("true", value)
		|| !strcasecmp("on", value)
		|| !strcasecmp("yes", value)
	   )
		return TRUE;
	return FALSE;
	}

static void
assign_style_entry(GkrellmStyle *style, gchar *value, gint entry_flag)
	{
	if (entry_flag == GKRELLMSTYLE_KRELL_YOFF)
		style->krell_yoff = atoi(value);
	else if (entry_flag == GKRELLMSTYLE_KRELL_YOFF_NOT_SCALABLE)
		style->krell_yoff_not_scalable = parse_boolean(value);
	else if (entry_flag == GKRELLMSTYLE_KRELL_EXPAND)
		gkrellm_set_krell_expand(style, value);
	else if (entry_flag == GKRELLMSTYLE_KRELL_X_HOT)
		style->krell_x_hot = atoi(value);
	else if (entry_flag == GKRELLMSTYLE_KRELL_EMA_PERIOD)
		style->krell_ema_period = atoi(value);
	else if (entry_flag == GKRELLMSTYLE_KRELL_DEPTH)
		style->krell_depth = atoi(value);
	else if (entry_flag == GKRELLMSTYLE_KRELL_LEFT_MARGIN)
		style->krell_left_margin = atoi(value);
	else if (entry_flag == GKRELLMSTYLE_KRELL_RIGHT_MARGIN)
		style->krell_right_margin = atoi(value);
	else if (entry_flag == GKRELLMSTYLE_LABEL_POSITION)
		{
		if (strcmp(value, "center") == 0)
			style->label_position = GKRELLM_LABEL_CENTER;
		else if (isdigit((unsigned char)*value))
			style->label_position = atoi(value);
		else
			style->label_position = GKRELLM_LABEL_NONE;
		}
	else if (entry_flag == GKRELLMSTYLE_LABEL_YOFF)
		style->label_yoff = atoi(value) * _GK.theme_scale / 100;
	else if (entry_flag == OLD_SET_MARGIN)	/* Deprecated as of 1.2.9 */
		{
		style->margin.left = atoi(value) * _GK.theme_scale / 100;
		style->margin.right = style->margin.left;
		}
	else if (entry_flag == GKRELLMSTYLE_TOP_MARGIN)
		{
		style->margin.top = atoi(value)  * _GK.theme_scale / 100;
		_GK.use_top_bottom_margins = TRUE;	/* Allow themes to adapt. */
		}
	else if (entry_flag == GKRELLMSTYLE_BOTTOM_MARGIN)
		{
		style->margin.bottom = atoi(value) * _GK.theme_scale / 100;
		_GK.use_top_bottom_margins = TRUE;	/* Allow themes to adapt. */
		}
	else if (entry_flag == GKRELLMSTYLE_LEFT_MARGIN)
		style->margin.left = atoi(value) * _GK.theme_scale / 100;
	else if (entry_flag == GKRELLMSTYLE_RIGHT_MARGIN)
		style->margin.right = atoi(value) * _GK.theme_scale / 100;
	else if (entry_flag == GKRELLMSTYLE_TRANSPARENCY)
		style->transparency = atoi(value);
	else if (entry_flag == GKRELLMSTYLE_SCROLL_TEXT_CACHE_OFF)
		style->scroll_text_cache_off = parse_boolean(value);
	else if (entry_flag == GKRELLMSTYLE_TEXTCOLOR_A)
		assign_textcolor(style, value, GKRELLMSTYLE_TEXTCOLOR_A);
	else if (entry_flag == GKRELLMSTYLE_TEXTCOLOR_B)
		assign_textcolor(style, value, GKRELLMSTYLE_TEXTCOLOR_B);
	else if (entry_flag == GKRELLMSTYLE_TEXTFONT_A)
		assign_font(style, value, GKRELLMSTYLE_TEXTFONT_A);
	else if (entry_flag == GKRELLMSTYLE_TEXTFONT_B)
		assign_font(style, value, GKRELLMSTYLE_TEXTFONT_B);
	else if (entry_flag == GKRELLMSTYLE_BORDER)
		set_border(&style->border, value);
	else if (entry_flag == SET_ALL_MARGINS)
		set_margins(style, value);
	}

static void
set_themed(GkrellmStyle *s, gint flag)
	{
	if (flag == OLD_SET_MARGIN)
		flag = (GKRELLMSTYLE_LEFT_MARGIN | GKRELLMSTYLE_RIGHT_MARGIN);
	else if (flag == SET_ALL_MARGINS)
		flag = (  GKRELLMSTYLE_LEFT_MARGIN | GKRELLMSTYLE_RIGHT_MARGIN
				| GKRELLMSTYLE_TOP_MARGIN | GKRELLMSTYLE_BOTTOM_MARGIN);
	s->themed |= flag;
	}

static void
assign_style(gchar *debug_name, GList *style_list, gint index,
				gchar *arg, gint entry_flag, gint override)
	{
	GkrellmStyle	*style;
	GList			*list;

	style = (GkrellmStyle *) g_list_nth_data(style_list, index);
	if (!style)
		return;

	/* If this is not an override assignment and this entry has already had
	|  an override assignment, then we do not assign.
	*/
	if (! override && (style->override & entry_flag))
		return;
	if (override)
		style->override |= entry_flag;
	assign_style_entry(style, arg, entry_flag);
	if (index > 0)	/* Theme has custom setting for this style */
		set_themed(style, entry_flag);

	if (index++ == 0)		/* style == style_list */
		{
		if (override)
			printf("Bad override on DEFAULT: %s %s %d\n",
					debug_name, arg, entry_flag);
		for (list = style_list->next; list; list = list->next, ++index)
			{
			style = (GkrellmStyle *) list->data;
			if (style && !(style->override & entry_flag))
				assign_style_entry(style, arg, entry_flag);
			}
		}
	}

#if 0
static void
assign_chart_style(gint index, gchar *arg, gint entry_flag, gint override)
	{
	assign_style("StyleChart", _GK.chart_style_list, index, arg,
				entry_flag, override);
	}

static void
assign_panel_style(gint index, gchar *arg, gint entry_flag, gint override)
	{
	assign_style("StylePanel", _GK.panel_style_list, index, arg,
				entry_flag, override);
	}
#endif

static void
assign_meter_style(gint index, gchar *arg, gint entry_flag, gint override)
	{
	assign_style("StyleMeter", _GK.meter_style_list, index, arg,
				entry_flag, override);
	}

static void
assign_custom_style(gchar *debug_name, GList *style_list, gint index,
				gchar *arg, gint entry_flag, gchar *custom_name)
	{
	GkrellmStyle	*style, *custom_style;
	gint	i;

	if ((i = gkrellm_string_position_in_list(_GK.custom_name_list, custom_name)) < 0)
		{
		style = (GkrellmStyle *) g_list_nth_data(style_list, index);
		if (!style)
			return;
		custom_style = gkrellm_copy_style(style);
		_GK.custom_name_list = g_list_append(_GK.custom_name_list,
				g_strdup(custom_name));
		_GK.custom_style_list = g_list_append(_GK.custom_style_list,
				custom_style);
		}
	else
		custom_style =
				(GkrellmStyle *) g_list_nth_data(_GK.custom_style_list, i);

//printf("assign_custom_style(%s, %s, %d, %s) %d\n",
//debug_name, custom_name, entry_flag, arg, i);
	assign_style_entry(custom_style, arg, entry_flag);
	set_themed(custom_style, entry_flag);
	}

static struct string_map
	{
	gchar	*string;
	gint	flag;
	}
	entry_map[] =
	{
	{ "krell_yoff",			GKRELLMSTYLE_KRELL_YOFF },
	{ "krell_yoff_not_scalable",GKRELLMSTYLE_KRELL_YOFF_NOT_SCALABLE },
	{ "krell_expand",		GKRELLMSTYLE_KRELL_EXPAND },
	{ "krell_x_hot",		GKRELLMSTYLE_KRELL_X_HOT },
	{ "krell_ema_period",	GKRELLMSTYLE_KRELL_EMA_PERIOD },
	{ "krell_depth",		GKRELLMSTYLE_KRELL_DEPTH },
	{ "krell_left_margin",	GKRELLMSTYLE_KRELL_LEFT_MARGIN },
	{ "krell_right_margin",	GKRELLMSTYLE_KRELL_RIGHT_MARGIN },
	{ "label_position",		GKRELLMSTYLE_LABEL_POSITION },
	{ "label_yoff",			GKRELLMSTYLE_LABEL_YOFF },
	{ "margins",			SET_ALL_MARGINS },
	{ "left_margin",		GKRELLMSTYLE_LEFT_MARGIN },
	{ "right_margin",		GKRELLMSTYLE_RIGHT_MARGIN },
	{ "top_margin",			GKRELLMSTYLE_TOP_MARGIN },
	{ "bottom_margin",		GKRELLMSTYLE_BOTTOM_MARGIN },
	{ "textcolor",			GKRELLMSTYLE_TEXTCOLOR_A },
	{ "alt_textcolor",		GKRELLMSTYLE_TEXTCOLOR_B },
	{ "font",				GKRELLMSTYLE_TEXTFONT_A },
	{ "alt_font",			GKRELLMSTYLE_TEXTFONT_B },
	{ "border",				GKRELLMSTYLE_BORDER },
	{ "transparency",		GKRELLMSTYLE_TRANSPARENCY },
	{ "scroll_text_cache_off",	GKRELLMSTYLE_SCROLL_TEXT_CACHE_OFF },
	{ "margin",				OLD_SET_MARGIN },	/* deprecated */
	};


static gint
get_entry_flag(gchar *entry)
	{
	struct string_map	*sm;

	for (sm = &entry_map[0];
		sm < &entry_map[sizeof(entry_map) / sizeof(struct string_map)]; ++sm)
		if (!strcmp(entry, sm->string))
			return sm->flag;
	return -1;
	}

static void
assign_gkrellmrc_style(gchar *source_line, gchar *area, gchar *string)
	{
	GList	*style_list = NULL, *name_list = NULL;
	gchar	*s;
	gchar	*arg = NULL, *mon_name = NULL, *custom_name = NULL, *entry = NULL;
	gint	index, entry_flag, override;

	/* string starts out in format "*.yyy arg" or "foo.yyy arg"
	*/
	mon_name = strtok(string, " \t=:");	/* "*.yyy" or "foo.yyy" */
	if (mon_name && (arg = strtok(NULL, "\n")) != NULL)	/* arg is "arg" part */
		{
		while (*arg == ' ' || *arg == '\t' || *arg == '=' || *arg == ':')
			++arg;
		entry = strrchr(mon_name, '.');
		if (entry)
			*entry++ = '\0';
		if ((s = strchr(mon_name, '.')) != NULL)
			{
			custom_name = g_strdup(mon_name);
			*s = '\0';
			}
		}
	if (!mon_name || !entry || !*entry || !arg)
		{
		printf("StyleXXX ?: %s\n", source_line);
		g_free(custom_name);
		return;
		}
	override = TRUE;
	entry_flag = get_entry_flag(entry);
	if (!strcmp(area, "StyleChart"))
		{
		name_list = _GK.chart_name_list;
		style_list = _GK.chart_style_list;
		}
	else if (!strcmp(area, "StylePanel"))
		{
		name_list = _GK.chart_name_list;
		style_list = _GK.panel_style_list;
		}
	else if (!strcmp(area, "StyleMeter"))
		{
		name_list = _GK.meter_name_list;
		style_list = _GK.meter_style_list;
		}
	else
		{
		printf("StyleXXX ?: %s\n", source_line);
		g_free(custom_name);
		return;
		}
	index = gkrellm_string_position_in_list(name_list, mon_name);
	if (index == DEFAULT_STYLE_ID)
		override = FALSE;

	if (entry_flag >= 0 && index >= 0)
		{
		if (custom_name)
			assign_custom_style(area, style_list, index, arg, entry_flag,
					custom_name);
		else
			assign_style(area, style_list, index, arg, entry_flag, override);
		}
	g_free(custom_name);
	}

gint
gkrellm_add_chart_style(GkrellmMonitor *mon, gchar *name)
	{
	GkrellmStyle	*panel_style, *chart_style;
	gint			id;
	static gint		style_id;

	if (!name)
		return 0;
	id = style_id++;
	chart_style = gkrellm_style_new0();
	panel_style = gkrellm_style_new0();
	if (mon)
		{
		if (mon->privat == NULL)
			mon->privat = g_new0(GkrellmMonprivate, 1);
		mon->privat->panel_style = panel_style;
		mon->privat->chart_style = chart_style;
		mon->privat->style_name = name;
		mon->privat->style_type = CHART_PANEL_TYPE;
		mon->privat->style_id = id;
		}
	_GK.chart_name_list = g_list_append(_GK.chart_name_list, (gchar *) name);
	_GK.chart_style_list = g_list_append(_GK.chart_style_list, chart_style);
	_GK.panel_style_list = g_list_append(_GK.panel_style_list, panel_style);
	_GK.bg_chart_piximage_list =
				g_list_append(_GK.bg_chart_piximage_list, NULL);
	_GK.bg_grid_piximage_list =
				g_list_append(_GK.bg_grid_piximage_list, NULL);
	_GK.bg_panel_piximage_list =
				g_list_append(_GK.bg_panel_piximage_list, NULL);
	_GK.krell_panel_piximage_list =
				g_list_append(_GK.krell_panel_piximage_list, NULL);
	return id;
	}

gint
gkrellm_add_meter_style(GkrellmMonitor *mon, gchar *name)
	{
	GkrellmStyle	*style;
	gint			id;
	static gint		style_id;

	if (!name)
		return 0;
	id = style_id++;
	style = gkrellm_style_new0();
	if (mon)
		{
		if (mon->privat == NULL)
			mon->privat = g_new0(GkrellmMonprivate, 1);
		mon->privat->panel_style = style;
		mon->privat->style_name = name;
		mon->privat->style_type = METER_PANEL_TYPE;
		mon->privat->style_id = id;
		}
	_GK.meter_name_list = g_list_append(_GK.meter_name_list, (gchar *) name);
	_GK.meter_style_list = g_list_append(_GK.meter_style_list, style);
	_GK.bg_meter_piximage_list =
				g_list_append(_GK.bg_meter_piximage_list, NULL);
	_GK.krell_meter_piximage_list =
				g_list_append(_GK.krell_meter_piximage_list, NULL);
	return id;
	}


static void
set_piximage_borders_in_list(GList *st_list, GList *im_list, GList *nm_list)
	{
	GkrellmStyle	*style;
	GkrellmPiximage	*image;

	for ( ; st_list && im_list && nm_list;
			st_list = st_list->next, im_list = im_list->next,
			nm_list = nm_list->next)
		{
		style = (GkrellmStyle *) st_list->data;
		image = (GkrellmPiximage *) im_list->data;
		if (style && image)
			gkrellm_set_piximage_border(image, &style->border);
		}
	}

static void
setup_piximages(void)
	{
	GList				*list;
	GkrellmMonitor		*mon;
	GkrellmMonprivate	*mp;
	gint				h;

	gkrellm_set_piximage_border(_GK.frame_top_piximage, &_GK.frame_top_border);
	gkrellm_set_piximage_border(_GK.frame_bottom_piximage,
						&_GK.frame_bottom_border);

	if (_GK.frame_left_width == 0)
		_GK.frame_left_width =
				gdk_pixbuf_get_width(_GK.frame_left_piximage->pixbuf);
//	_GK.frame_left_width =	_GK.frame_left_width * _GK.theme_scale / 100;
	gkrellm_set_piximage_border(_GK.frame_left_piximage,
						&_GK.frame_left_border);

	if (_GK.frame_right_width == 0)
		_GK.frame_right_width =
				gdk_pixbuf_get_width(_GK.frame_right_piximage->pixbuf);
//	_GK.frame_right_width = _GK.frame_right_width * _GK.theme_scale / 100;

	gkrellm_set_piximage_border(_GK.frame_right_piximage,
						&_GK.frame_right_border);

	gkrellm_set_piximage_border(_GK.button_panel_out_piximage,
						&_GK.button_panel_border);
	gkrellm_set_piximage_border(_GK.button_panel_in_piximage,
						&_GK.button_panel_border);

	gkrellm_set_piximage_border(_GK.button_meter_out_piximage,
						&_GK.button_meter_border);
	gkrellm_set_piximage_border(_GK.button_meter_in_piximage,
						&_GK.button_meter_border);

	set_piximage_borders_in_list(_GK.chart_style_list,
						_GK.bg_chart_piximage_list, _GK.chart_name_list);
	set_piximage_borders_in_list(_GK.panel_style_list,
						_GK.bg_panel_piximage_list, _GK.chart_name_list);
	set_piximage_borders_in_list(_GK.meter_style_list,
						_GK.bg_meter_piximage_list, _GK.meter_name_list);

	h = gdk_pixbuf_get_height(_GK.decal_misc_piximage->pixbuf) /
				N_MISC_DECALS * _GK.theme_scale / 100;
	gkrellm_scale_piximage_to_pixmap(_GK.decal_misc_piximage,
						&_GK.decal_misc_pixmap, &_GK.decal_misc_mask, -1,
						h * N_MISC_DECALS);

	if (!_GK.spacer_top_chart_piximage)
		_GK.spacer_top_chart_piximage =
					gkrellm_clone_piximage(_GK.spacer_top_piximage);
	if (!_GK.spacer_bottom_chart_piximage)
		_GK.spacer_bottom_chart_piximage =
					gkrellm_clone_piximage(_GK.spacer_bottom_piximage);

	if (!_GK.spacer_top_meter_piximage)
		_GK.spacer_top_meter_piximage =
					gkrellm_clone_piximage(_GK.spacer_top_piximage);
	if (!_GK.spacer_bottom_meter_piximage)
		_GK.spacer_bottom_meter_piximage =
					gkrellm_clone_piximage(_GK.spacer_bottom_piximage);

	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		GkrellmPiximage	*top_pix, *bot_pix;

		mon = (GkrellmMonitor *) list->data;
		mp = mon->privat;
		if ((!mon->name || !mon->create_monitor) && mon != gkrellm_mon_host())
			continue;
		if (mp->style_type == CHART_PANEL_TYPE)
			{
			top_pix = _GK.spacer_top_chart_piximage;
			bot_pix = _GK.spacer_bottom_chart_piximage;
			mp->top_type = mp->bottom_type = GKRELLM_SPACER_CHART;
			}
		else
			{
			top_pix = _GK.spacer_top_meter_piximage;
			bot_pix = _GK.spacer_bottom_meter_piximage;
			mp->top_type = mp->bottom_type = GKRELLM_SPACER_METER;
			}

		if (!mp->top_spacer.piximage)
			mp->top_spacer.piximage = gkrellm_clone_piximage(top_pix);
		gkrellm_set_piximage_border(mp->top_spacer.piximage,
								&_GK.spacer_top_border);

		if (!mp->bottom_spacer.piximage)
			mp->bottom_spacer.piximage = gkrellm_clone_piximage(bot_pix);
		gkrellm_set_piximage_border(mp->bottom_spacer.piximage,
								&_GK.spacer_bottom_border);
		}
	}


typedef struct
	{
	gchar			*name;
	gchar			**xpm;
	GkrellmPiximage	**im;
	GList			**image_list;
	gchar			*name_in_list;
	}
	ImageTable;

static ImageTable base_theme_piximages[]	=
	{
	/* Images in this table which have a non NULL _xpm default form the
	|  minimal set of required images for a complete theme change.
	|  If there is a NULL xpm, the image will be somehow constructed to
	|  a default image in the code.
	*/
{ "frame_top",		frame_top_xpm,		&_GK.frame_top_piximage,	NULL, NULL},
{ "frame_bottom",	frame_bottom_xpm,	&_GK.frame_bottom_piximage,	NULL, NULL},
{ "frame_left",  	frame_left_xpm,		&_GK.frame_left_piximage,	NULL, NULL},
{ "frame_right", 	frame_right_xpm,	&_GK.frame_right_piximage,	NULL, NULL},

{ "button_panel_out",	NULL,	&_GK.button_panel_out_piximage,		NULL, NULL},
{ "button_panel_in",	NULL,	&_GK.button_panel_in_piximage,		NULL, NULL},
{ "button_meter_out",	NULL,	&_GK.button_meter_out_piximage,		NULL, NULL},
{ "button_meter_in",	NULL,	&_GK.button_meter_in_piximage,		NULL, NULL},

{ "bg_chart",	 	bg_chart_xpm, NULL,	&_GK.bg_chart_piximage_list,	"*"	},
{ "bg_grid", 		bg_grid_xpm, NULL, &_GK.bg_grid_piximage_list,		"*"},
{ "bg_panel",		bg_panel_xpm, NULL, &_GK.bg_panel_piximage_list,	"*" },
{ "bg_meter",		bg_meter_xpm, NULL, &_GK.bg_meter_piximage_list, 	"*" },

{ "decal_alarm",	decal_alarm_xpm,	&_GK.decal_alarm_piximage, NULL, NULL},
{ "decal_warn",		decal_warn_xpm,		&_GK.decal_warn_piximage, NULL,	NULL},

{ "decal_misc",		decal_misc_xpm,		&_GK.decal_misc_piximage, NULL, NULL},
{ "decal_button",	decal_button_xpm,	&_GK.decal_button_piximage, NULL, NULL},

{ "data_in",	 	NULL,		&_GK.data_in_piximage,			NULL, 	NULL},
{ "data_in_grid", 	NULL,		&_GK.data_in_grid_piximage,		NULL,	NULL},
{ "data_out",	 	NULL,		&_GK.data_out_piximage,			NULL,	NULL},
{ "data_out_grid", 	NULL,		&_GK.data_out_grid_piximage,	NULL,	NULL},

{ "bg_separator", NULL,		&_GK.bg_separator_piximage,			NULL,	NULL},
{ "spacer_top",		NULL,	&_GK.spacer_top_piximage,			NULL,	NULL},
{ "spacer_bottom",	NULL,	&_GK.spacer_bottom_piximage,		NULL,	NULL},
{ "spacer_top_chart",	NULL, &_GK.spacer_top_chart_piximage,	NULL,	NULL},
{ "spacer_bottom_chart",NULL, &_GK.spacer_bottom_chart_piximage,NULL,	NULL},
{ "spacer_top_meter",	NULL, &_GK.spacer_top_meter_piximage,	NULL,	NULL},
{ "spacer_bottom_meter",NULL, &_GK.spacer_bottom_meter_piximage,NULL,	NULL},

{ "cap_top_left_chart", NULL, &_GK.cap_top_left_chart_piximage, NULL, NULL},
{ "cap_bottom_left_chart", NULL,&_GK.cap_bottom_left_chart_piximage,NULL,NULL},
{ "cap_top_right_chart",   NULL, &_GK.cap_top_right_chart_piximage, NULL,NULL},
{"cap_bottom_right_chart",NULL,&_GK.cap_bottom_right_chart_piximage,NULL,NULL},
{ "cap_top_left_meter", NULL, &_GK.cap_top_left_meter_piximage, NULL, NULL},
{ "cap_bottom_left_meter", NULL,&_GK.cap_bottom_left_meter_piximage,NULL,NULL},
{ "cap_top_right_meter",   NULL, &_GK.cap_top_right_meter_piximage, NULL,NULL},
{"cap_bottom_right_meter",NULL,&_GK.cap_bottom_right_meter_piximage,NULL,NULL},

{ "krell_panel",	krell_panel_xpm, NULL, &_GK.krell_panel_piximage_list, "*"},
{ "krell_meter",	krell_meter_xpm, NULL, &_GK.krell_meter_piximage_list, "*"},
{ "krell_mail", krell_mail_xpm, NULL,
								&_GK.krell_meter_piximage_list, MAIL_STYLE_NAME },

{ "krell_slider",	krell_slider_xpm,	&_GK.krell_slider_piximage,	NULL,	NULL},
{ "krell_mini", 	krell_mini_xpm,		&_GK.krell_mini_piximage,	NULL,	NULL}
	};


static ImageTable	default_theme_piximages[] =
	{
{ NULL, button_panel_out_xpm, &_GK.button_panel_out_piximage,NULL, NULL},
{ NULL, button_panel_in_xpm, &_GK.button_panel_in_piximage, NULL, NULL},
{ NULL, button_meter_out_xpm,&_GK.button_meter_out_piximage, NULL, NULL},
{ NULL, button_meter_in_xpm, &_GK.button_meter_in_piximage, NULL, NULL},

{ NULL,	bg_separator_xpm,	&_GK.bg_separator_piximage, NULL, NULL},

{ NULL,	bg_panel_cal_xpm, NULL, &_GK.bg_meter_piximage_list, CAL_STYLE_NAME},
{ NULL,	bg_panel_clock_xpm, NULL, &_GK.bg_meter_piximage_list, CLOCK_STYLE_NAME},
{ NULL, bg_panel_mem_xpm, NULL, &_GK.bg_meter_piximage_list, MEM_STYLE_NAME},
{ NULL,	bg_panel_host_xpm, NULL, &_GK.bg_meter_piximage_list, HOST_STYLE_NAME},
{ NULL, bg_panel_uptime_xpm, NULL, &_GK.bg_meter_piximage_list, UPTIME_STYLE_NAME},
{ NULL, bg_panel_timer_xpm, NULL, &_GK.bg_meter_piximage_list, TIMER_STYLE_NAME},
{ NULL, bg_panel_swap_xpm, NULL, &_GK.bg_meter_piximage_list, SWAP_STYLE_NAME},

{ NULL,	data_in_xpm,		&_GK.data_in_piximage,		NULL, NULL},
{ NULL,	data_in_grid_xpm,	&_GK.data_in_grid_piximage,	NULL, NULL},
{ NULL,	data_out_xpm,		&_GK.data_out_piximage,		NULL, NULL},
{ NULL,	data_out_grid_xpm,	&_GK.data_out_grid_piximage, NULL, NULL},

{ NULL,	krell_mem_xpm, NULL, &_GK.krell_meter_piximage_list, MEM_STYLE_NAME},
{ NULL,	krell_swap_xpm, NULL, &_GK.krell_meter_piximage_list, SWAP_STYLE_NAME},
	};

static ImageTable	default_theme_alt0_piximages[] =
	{
{ NULL,	bg_panel_fs_xpm, NULL, &_GK.bg_meter_piximage_list, FS_STYLE_NAME},
{ NULL,	bg_panel_sensors_xpm, NULL,&_GK.bg_meter_piximage_list, "sensors" },

/* Plugins */
{ NULL,	bg_panel_timers_xpm,	NULL, &_GK.bg_meter_piximage_list, "timers"},
{ NULL,	bg_panel_volume_xpm,	NULL, &_GK.bg_meter_piximage_list, "volume"},
{ NULL,	krell_gkrellmms_xpm,	NULL, &_GK.krell_meter_piximage_list, "gkrellmms"},
{ NULL,	bg_panel_gkrellmms_xpm,	NULL, &_GK.bg_meter_piximage_list, "gkrellmms"},
{ NULL,	bg_panel_pmu_xpm,		NULL, &_GK.bg_meter_piximage_list, "pmu"},
	};

static ImageTable	default_theme_alt1_piximages[] =
	{
{ NULL,	bg_panel_fs_1_xpm, NULL, &_GK.bg_meter_piximage_list, FS_STYLE_NAME},
{ NULL,	bg_panel_sensors_1_xpm, NULL, &_GK.bg_meter_piximage_list,
			"sensors"},

/* Plugins */
{ NULL,	bg_panel_timers_1_xpm,	NULL, &_GK.bg_meter_piximage_list, "timers"},
{ NULL,	bg_panel_volume_1_xpm,	NULL, &_GK.bg_meter_piximage_list, "volume"},
{ NULL,	bg_panel_gkrellmms_1_xpm, NULL, &_GK.bg_meter_piximage_list, "gkrellmms"},
{ NULL,	bg_panel_pmu_1_xpm,		NULL, &_GK.bg_meter_piximage_list, "pmu"},
	};

static ImageTable	default_theme_alt2_piximages[] =
	{
{ NULL,	bg_panel_fs_2_xpm, NULL, &_GK.bg_meter_piximage_list, FS_STYLE_NAME},
{ NULL,	bg_panel_sensors_2_xpm, NULL, &_GK.bg_meter_piximage_list,
			"sensors"},

/* Plugins */
{ NULL,	bg_panel_timers_2_xpm,	NULL, &_GK.bg_meter_piximage_list, "timers"},
{ NULL,	bg_panel_volume_2_xpm,	NULL, &_GK.bg_meter_piximage_list, "volume"},
{ NULL,	bg_panel_gkrellmms_2_xpm, NULL, &_GK.bg_meter_piximage_list, "gkrellmms"},
{ NULL,	bg_panel_pmu_2_xpm,		NULL, &_GK.bg_meter_piximage_list, "pmu"},
	};


  /* Need a trap to look for extra custom and extension images I've made for
  |  the default theme.
  */
static GkrellmPiximage *
default_theme_extension_piximage(gchar *name, gchar *subdir)
	{
	GkrellmPiximage	*im		= NULL;

	if (!strcmp(subdir, "gkrellmms"))
		{
		if (!strcmp(name, "bg_scroll"))
			im = gkrellm_piximage_new_from_xpm_data(bg_scroll_gkrellmms_xpm);
		if (!strcmp(name, "play_button"))
			im = gkrellm_piximage_new_from_xpm_data(gkrellmms_play_button_xpm);
		if (!strcmp(name, "prev_button"))
			im = gkrellm_piximage_new_from_xpm_data(gkrellmms_prev_button_xpm);
		if (!strcmp(name, "stop_button"))
			im = gkrellm_piximage_new_from_xpm_data(gkrellmms_stop_button_xpm);
		if (!strcmp(name, "next_button"))
			im = gkrellm_piximage_new_from_xpm_data(gkrellmms_next_button_xpm);
		if (!strcmp(name, "eject_button"))
			im =gkrellm_piximage_new_from_xpm_data(gkrellmms_eject_button_xpm);
		if (!strcmp(name, "led_indicator"))
			im=gkrellm_piximage_new_from_xpm_data(gkrellmms_led_indicator_xpm);
		}
	else if (!strcmp(subdir, CPU_STYLE_NAME) && !strcmp(name, "nice"))
		im = gkrellm_piximage_new_from_xpm_data(nice_xpm);
	else if (!strcmp(subdir, CPU_STYLE_NAME) && !strcmp(name,"nice_grid"))
		im = gkrellm_piximage_new_from_xpm_data(nice_grid_xpm);
	return im;
	}

gboolean
gkrellm_load_piximage_from_inline(gchar *name, const guint8 *data,
			GkrellmPiximage **image, gchar *subdir, gboolean copy_pixels)
	{
	GkrellmPiximage	*im		= NULL;
	gchar			*fname;

	if (gkrellm_using_default_theme() && name && subdir)
		im = default_theme_extension_piximage(name, subdir);
	else if (name && (fname = gkrellm_theme_file_exists(name, subdir)) != NULL)
		{
		name = fname;
		im = gkrellm_piximage_new_from_file(fname);
		if (im == NULL)
			printf(_("  Cannot load file image: %s\n"), fname);
		}
	if (im == NULL && data)
		{
		im = gkrellm_piximage_new_from_inline(data, copy_pixels);
		if (im == NULL)
			printf(_("  Cannot load GdkPixbuf inline data.\n"));
		}
	if (im && image)
		{
		if (*image)
			gkrellm_destroy_piximage(*image);
		*image = im;
		}
	return (im ? TRUE : FALSE);
    }

gboolean
gkrellm_load_piximage(gchar *name, gchar **xpm, GkrellmPiximage **image,
			gchar *subdir)
	{
	GkrellmPiximage	*im		= NULL;
	gchar			*fname;

	if (gkrellm_using_default_theme() && name && subdir)
		im = default_theme_extension_piximage(name, subdir);
	else if (name && (fname = gkrellm_theme_file_exists(name, subdir)) != NULL)
		{
		name = fname;
		im = gkrellm_piximage_new_from_file(fname);
		if (im == NULL)
			printf(_("  Cannot load file image: %s\n"), fname);
		}
	if (im == NULL && xpm)
		{
		im = gkrellm_piximage_new_from_xpm_data(xpm);
		if (im == NULL)
			printf(_("  Cannot load xpm: %s\n"), name);
		}
	if (im && image)
		{
		if (*image)
			gkrellm_destroy_piximage(*image);
		*image = im;
		}
	return (im ? TRUE : FALSE);
    }

static void
load_from_piximage_list(gchar *name, GList *image_list,
				gint index, gchar *subdir)
	{
	GList			*list;
	GkrellmPiximage	*im;

	list = g_list_nth(image_list, index);
	if (list)
		{
		im = (GkrellmPiximage *) list->data;
		gkrellm_load_piximage(name, NULL, &im, subdir);
		list->data = (gpointer) im;
		}
	else
		printf("Bad index %d for image list (meter/panel problem?)\n", index);
	}

static void
load_monitor_specific_piximages(void)
	{
	GkrellmMonitor	*mon;
	GList			*list;
	gchar			*subdir;
	gint			i;

	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if ((subdir = mon->privat->style_name) == NULL)
			continue;
		i = mon->privat->style_id;
		if (mon->privat->style_type == CHART_PANEL_TYPE)
			{
			load_from_piximage_list("bg_chart",
						_GK.bg_chart_piximage_list, i, subdir);
			load_from_piximage_list("bg_grid",
						_GK.bg_grid_piximage_list, i, subdir);
			load_from_piximage_list("bg_panel",
						_GK.bg_panel_piximage_list, i, subdir);
			load_from_piximage_list("krell",
						_GK.krell_panel_piximage_list, i, subdir);
			}
		else
			{
			load_from_piximage_list("krell",
						_GK.krell_meter_piximage_list, i, subdir);
			load_from_piximage_list("bg_panel",
						_GK.bg_meter_piximage_list, i, subdir);
			load_from_piximage_list("bg_meter",
						_GK.bg_meter_piximage_list, i, subdir);
			}
		gkrellm_load_piximage("spacer_top", NULL,
					&mon->privat->top_spacer.piximage, subdir);
		gkrellm_load_piximage("spacer_bottom", NULL,
					&mon->privat->bottom_spacer.piximage, subdir);
		}
	}

static void
assign_gkrellmrc_spacer(gchar *source_line, gchar *area, gchar *string)
	{
	GkrellmMonitor	*mon = NULL;
	gchar			style_name[32], arg[32], *s;
	gint			n;

	if ((n = sscanf(string, "%31s %31[^\n]", style_name, arg)) < 1)
		return;
	if (n == 1)
		strcpy(arg, style_name);
	else if ((mon = gkrellm_monitor_from_style_name(style_name)) == NULL)
		return;

	for (s = arg; *s == ' ' || *s == '=' || *s == '\t'; ++s)
		;

	if (!strncmp(area, "spacer_top_height", 17))
		{
		if (n == 1 && !strcmp(area, "spacer_top_height_chart"))
			_GK.spacer_top_height_chart = atoi(s);
		else if (n == 1 && !strcmp(area, "spacer_top_height_meter"))
			_GK.spacer_top_height_meter = atoi(s);
		else if (n == 1)
			{
			_GK.spacer_top_height_chart = atoi(s);
			_GK.spacer_top_height_meter = _GK.spacer_top_height_chart;
			}
		else
			mon->privat->top_spacer.height = atoi(s);
		}
	else if (!strncmp(area, "spacer_bottom_height", 20))
		{
		if (n == 1 && !strcmp(area, "spacer_bottom_height_chart"))
			_GK.spacer_bottom_height_chart = atoi(s);
		else if (n == 1 && !strcmp(area, "spacer_bottom_height_meter"))
			_GK.spacer_bottom_height_meter = atoi(s);
		else if (n == 1)
			{
			_GK.spacer_bottom_height_chart = atoi(s);
			_GK.spacer_bottom_height_meter = _GK.spacer_bottom_height_chart;
			}
		else
			mon->privat->bottom_spacer.height = atoi(s);
		}
	}


  /* I have to do something about separate chart/meter lists.
  */
static GList *
lookup_piximage_from_name(GList *image_list, gchar *name)
	{
	GList	*n_list, *i_list;

	for (n_list = _GK.chart_name_list, i_list = image_list;
			n_list && i_list; n_list = n_list->next, i_list = i_list->next)
		if (!strcmp(name, (gchar *) n_list->data))
			return i_list;
	for (n_list = _GK.meter_name_list, i_list = image_list;
			n_list && i_list; n_list = n_list->next, i_list = i_list->next)
		if (!strcmp(name, (gchar *) n_list->data))
			return i_list;
	return NULL;
	}

static void
load_piximage_table(ImageTable *ti, gint n_piximages, gchar *subdir)
	{
	GkrellmPiximage	*im;
	GList			*list;
	gint			i;

	for (i = 0; i < n_piximages; ++i, ++ti)
		{
		if (ti->image_list)
			{
/*			list = g_list_nth(*(ti->image_list), ti->list_index); */
			list = lookup_piximage_from_name(*(ti->image_list),
							ti->name_in_list);
			if (list)
				{
				im = (GkrellmPiximage *) list->data;
				gkrellm_load_piximage(ti->name, ti->xpm, &im, subdir);
				list->data = (gpointer) im;
				}
			}
		else
			gkrellm_load_piximage(ti->name, ti->xpm, ti->im, subdir);
		}
    }

  /* When loading a new theme, required base level images are not cleaned
  |  so the program will not crash if a theme does not have all images yet.
  |  It will just look funny.  But all optional base level images are cleaned
  |  so they will not carry over to the new theme.  There are no optional
  |  base level images in the image_lists.
  */
static void
clean_base_piximage_table(void)
	{
	ImageTable	*ti;
	gint		i;

	ti = &base_theme_piximages[0];
	for (i = 0; i < sizeof(base_theme_piximages) / sizeof(ImageTable);
					++i, ++ti)
		if (ti->xpm == NULL && ti->im && *(ti->im))	/* Is an optional image */
			{
			gkrellm_destroy_piximage(*(ti->im));
			*(ti->im) = NULL;
			}
	}

static void
destroy_piximage(GkrellmPiximage **im)
	{
	if (im && *im)
		{
		gkrellm_destroy_piximage(*im);
		*im = NULL;
		}
	}

static void
destroy_piximage_list(GList *list, GList *name_list, gchar *debug_name)
	{
	GkrellmPiximage *im;

	for ( ; list; list = list->next, name_list = name_list->next)
		{
		im = (GkrellmPiximage *) list->data;
		destroy_piximage(&im);
		list->data = NULL;
		}
	}

static void
destroy_monitor_specific_piximages(void)
	{
	GkrellmMonitor	*mon;
	GList			*list;

	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		destroy_piximage(&mon->privat->top_spacer.piximage);
		destroy_piximage(&mon->privat->bottom_spacer.piximage);
		}
	}

void
gkrellm_load_theme_piximages(void)
	{
	GkrellmMonitor	*mon;
	gint			n_base, n_default;

	/* Free up all custom images from old theme.
	*/
	destroy_piximage_list(_GK.bg_chart_piximage_list, _GK.chart_name_list,
				"bg_chart");
	destroy_piximage_list(_GK.bg_grid_piximage_list, _GK.chart_name_list,
				"bg_grid");
	destroy_piximage_list(_GK.bg_panel_piximage_list, _GK.chart_name_list,
				"bg_panel");
	destroy_piximage_list(_GK.bg_meter_piximage_list, _GK.meter_name_list,
				"bg_meter");
	destroy_piximage_list(_GK.krell_panel_piximage_list, _GK.chart_name_list,
				"krell_panel");
	destroy_piximage_list(_GK.krell_meter_piximage_list, _GK.meter_name_list,
				"krell_meter");
	destroy_monitor_specific_piximages();

	clean_base_piximage_table();

	/* This loads the base images in the top level of the theme directory.
	|  For backward compatibility, it also loads monitor specific name
	|  qualified images in the top level directory.  The new way is for
	|  monitor specific images to be in subdirectories, loaded below.
	*/
	n_base = sizeof(base_theme_piximages) / sizeof(ImageTable);
	load_piximage_table(&base_theme_piximages[0], n_base, NULL);

	if (gkrellm_using_default_theme())
		{
		n_default = sizeof(default_theme_piximages) / sizeof(ImageTable);
		load_piximage_table(&default_theme_piximages[0], n_default, NULL);

		if (_GK.theme_alternative == 2 || _GK.theme_alternative == 5)
			{
			n_default =
					sizeof(default_theme_alt2_piximages) / sizeof(ImageTable);
			load_piximage_table(&default_theme_alt2_piximages[0],
					n_default, NULL);
			}
		else if (_GK.theme_alternative == 1 || _GK.theme_alternative == 4)
			{
			n_default =
					sizeof(default_theme_alt1_piximages) / sizeof(ImageTable);
			load_piximage_table(&default_theme_alt1_piximages[0],
					n_default, NULL);
			}
		else
			{
			n_default =
					sizeof(default_theme_alt0_piximages) / sizeof(ImageTable);
			load_piximage_table(&default_theme_alt0_piximages[0],
					n_default, NULL);
			}

		if ((mon = gkrellm_monitor_from_style_name("timers")) != NULL)
			{
			gkrellm_load_piximage(NULL, spacer_top_timers_xpm,
						&mon->privat->top_spacer.piximage, NULL);
			gkrellm_load_piximage(NULL, spacer_bottom_timers_xpm,
						&mon->privat->bottom_spacer.piximage, NULL);
			}
		if ((mon = gkrellm_monitor_from_style_name("volume")) != NULL)
			{
			gkrellm_load_piximage(NULL, spacer_top_volume_xpm,
						&mon->privat->top_spacer.piximage, NULL);
			gkrellm_load_piximage(NULL, spacer_bottom_volume_xpm,
						&mon->privat->bottom_spacer.piximage, NULL);
			}
		if ((mon = gkrellm_monitor_from_style_name("gkrellmms")) != NULL)
			{
			gkrellm_load_piximage(NULL, spacer_top_gkrellmms_xpm,
						&mon->privat->top_spacer.piximage, NULL);
			gkrellm_load_piximage(NULL, spacer_bottom_gkrellmms_xpm,
						&mon->privat->bottom_spacer.piximage, NULL);
			}
		if ((mon = gkrellm_monitor_from_style_name("pmu")) != NULL)
			{
			gkrellm_load_piximage(NULL, spacer_top_pmu_xpm,
						&mon->privat->top_spacer.piximage, NULL);
			gkrellm_load_piximage(NULL, spacer_bottom_pmu_xpm,
						&mon->privat->bottom_spacer.piximage, NULL);
			}
		if ((mon = gkrellm_monitor_from_style_name(FS_STYLE_NAME)) != NULL)
			{
			gkrellm_load_piximage(NULL, spacer_top_fs_xpm,
						&mon->privat->top_spacer.piximage, NULL);
			gkrellm_load_piximage(NULL, spacer_bottom_fs_xpm,
						&mon->privat->bottom_spacer.piximage, NULL);
			}
		}
	else
		{
		load_monitor_specific_piximages();
		}
	setup_piximages();
	}

  /* Borders for things that are not primary background parts of a monitor,
  |  and so are not set by a style line.
  */
static gchar
			*frame_top_border,
			*frame_bottom_border,
			*frame_left_border,
			*frame_right_border,
			*button_panel_border,
			*button_meter_border,
			*krell_slider_expand,
			*frame_left_chart_border,
			*frame_right_chart_border,
			*frame_left_panel_border,
			*frame_right_panel_border,
			*spacer_top_border,
			*spacer_bottom_border;

gint		krell_slider_depth,
			krell_slider_x_hot;

static struct	_config
	{
	gchar	*option;
	gint	*value;
	gchar	**arg;
	gint	minimum;
	}
	theme_config []	=
	{
	{"author",				NULL,		NULL,			-100 },

	{"theme_alternatives",	&_GK.theme_n_alternatives,	NULL,		0  },

	{"frame_top_height",	&_GK.frame_top_height,		NULL,		0  },
	{"frame_bottom_height",	&_GK.frame_bottom_height,	NULL,		0  },
	{"frame_left_width",	&_GK.frame_left_width,		NULL,		0  },
	{"frame_right_width",	&_GK.frame_right_width,		NULL,		0  },
	{"frame_left_chart_overlap",  &_GK.frame_left_chart_overlap,   NULL, 0  },
	{"frame_right_chart_overlap", &_GK.frame_right_chart_overlap,  NULL, 0  },
	{"frame_left_panel_overlap",  &_GK.frame_left_panel_overlap,   NULL, 0  },
	{"frame_right_panel_overlap", &_GK.frame_right_panel_overlap,  NULL, 0  },
	{"frame_left_spacer_overlap",  &_GK.frame_left_spacer_overlap,   NULL, 0 },
	{"frame_right_spacer_overlap", &_GK.frame_right_spacer_overlap,  NULL, 0 },
	{"chart_width_ref",		&_GK.chart_width_ref,		NULL,		30 },
	{"chart_height_min",	&_GK.chart_height_min,		NULL,		2 },
	{"chart_height_max",	&_GK.chart_height_max,		NULL,		20 },
	{"bg_separator_height", &_GK.bg_separator_height,	NULL,		0  },
	{"allow_scaling",		&_GK.allow_scaling,			NULL,		0 },

	{"rx_led_x",			&_GK.rx_led_x,				NULL,		-99 },
	{"rx_led_y",			&_GK.rx_led_y,				NULL,		0   },
	{"tx_led_x",			&_GK.tx_led_x,				NULL,		-99 },
	{"tx_led_y",			&_GK.tx_led_y,				NULL,		0   },

	/* These two are handled as a service for mail.c because of historical
	|  reasons.  They should be set with set_integer in the gkrellmrc.
	*/
	{"decal_mail_frames",	&_GK.decal_mail_frames,	NULL,			1  },
	{"decal_mail_delay",	&_GK.decal_mail_delay,	NULL,			1  },

	{"decal_alarm_frames",	&_GK.decal_alarm_frames,	NULL,			1  },
	{"decal_warn_frames",	&_GK.decal_warn_frames,	NULL,			1  },

	{"chart_in_color",		NULL,		&_GK.chart_in_color,		-100 },
	{"chart_in_color_grid",	NULL,		&_GK.chart_in_color_grid,	-100 },
	{"chart_out_color",		NULL,		&_GK.chart_out_color,		-100 },
	{"chart_out_color_grid",NULL,		&_GK.chart_out_color_grid,	-100 },

	{"chart_text_no_fill",	&_GK.chart_text_no_fill,	NULL,			0  },

	{"bg_grid_mode",		&_GK.bg_grid_mode,		NULL,			0  },

	{"frame_top_border",	NULL,		&frame_top_border,			-100 },
	{"frame_bottom_border",	NULL,		&frame_bottom_border,		-100 },
	{"frame_left_border",	NULL,		&frame_left_border,			-100 },
	{"frame_right_border",	NULL,		&frame_right_border,		-100 },
	{"button_panel_border", NULL,		&button_panel_border,		-100 },
	{"button_meter_border", NULL,		&button_meter_border,		-100 },
	{"frame_left_chart_border", NULL,	&frame_left_chart_border,	-100 },
	{"frame_right_chart_border", NULL,	&frame_right_chart_border,	-100 },
	{"frame_left_panel_border", NULL,	&frame_left_panel_border,	-100 },
	{"frame_right_panel_border", NULL,	&frame_right_panel_border,	-100 },
	{"spacer_top_border",	NULL,		&spacer_top_border,			-100 },
	{"spacer_bottom_border",	NULL,	&spacer_bottom_border,		-100 },

	{"krell_slider_depth",	&krell_slider_depth,	NULL,			1  },
	{"krell_slider_x_hot",	&krell_slider_x_hot,	NULL,			-1  },
	{"krell_slider_expand",	NULL, 		&krell_slider_expand,		-1  },
	};


  /* Handle borders set in gkrellmrc which are not set by a style line.
  */
static void
cleanup_gkrellmrc(void)
	{
	set_border(&_GK.frame_top_border, frame_top_border);
	set_border(&_GK.frame_bottom_border, frame_bottom_border);
	set_border(&_GK.frame_left_border, frame_left_border);
	set_border(&_GK.frame_right_border, frame_right_border);

	set_border(&_GK.frame_left_chart_border, frame_left_chart_border);
	set_border(&_GK.frame_right_chart_border, frame_right_chart_border);
	set_border(&_GK.frame_left_panel_border, frame_left_panel_border);
	set_border(&_GK.frame_right_panel_border, frame_right_panel_border);

	set_border(&_GK.spacer_top_border, spacer_top_border);
	set_border(&_GK.spacer_bottom_border, spacer_bottom_border);

	set_border(&_GK.button_panel_border, button_panel_border);
	set_border(&_GK.button_meter_border, button_meter_border);

	_GK.krell_slider_style->krell_x_hot  = krell_slider_x_hot;
	_GK.krell_slider_style->krell_depth  = krell_slider_depth;
	gkrellm_set_krell_expand(_GK.krell_slider_style, krell_slider_expand);
	_GK.rx_led_x = _GK.rx_led_x * _GK.theme_scale / 100;
	_GK.rx_led_y = _GK.rx_led_y * _GK.theme_scale / 100;
	_GK.tx_led_x = _GK.tx_led_x * _GK.theme_scale / 100;
	_GK.tx_led_y = _GK.tx_led_y * _GK.theme_scale / 100;
	}

static GList	*gkrellmrc_border_list,
				*gkrellmrc_integer_list,
				*gkrellmrc_string_list;


static GkrellmBorder	zero_border;


gboolean
gkrellm_set_gkrellmrc_piximage_border(gchar *image_name,
			GkrellmPiximage *image, GkrellmStyle *style)
	{
	static GkrellmBorder	b;
	GList					*list;
	gchar					name[64], border_string[32];
	gchar					*s, *r;

	if (style)
		style->border = zero_border;
	if (!image || !image_name)
		return FALSE;
	for (list = gkrellmrc_border_list; list; list = list->next)
		{
		s = list->data;
		if ((r = strchr(s, '=')) != NULL)
			*r = ' ';
		sscanf(s, "%63s %31s", name, border_string);
		if (!strcmp(name, image_name))
			{
			set_border(&b, border_string);
			gkrellm_set_piximage_border(image, &b);
			if (style)
				style->border = b;
			return TRUE;
			}
		}
	return FALSE;
	}

gboolean
gkrellm_get_gkrellmrc_piximage_border(gchar *image_name, GkrellmPiximage *image,
				GkrellmBorder *border)
	{
	GkrellmBorder	b;
	GList			*list;
	gchar			name[64], border_string[32];
	gchar			*s, *r;

	if (!image || !image_name)
		return FALSE;
	for (list = gkrellmrc_border_list; list; list = list->next)
		{
		s = list->data;
		if ((r = strchr(s, '=')) != NULL)
			*r = ' ';
		sscanf(s, "%63s %31s", name, border_string);
		if (!strcmp(name, image_name))
			{
			set_border(&b, border_string);
			gkrellm_set_piximage_border(image, &b);
			if (border)
				*border = b;
			return TRUE;
			}
		}
	return FALSE;
	}

gboolean
gkrellm_get_gkrellmrc_integer(gchar *int_name, gint *result)
	{
	GList		*list;
	gchar		name[64], string[64];
	gchar		*s, *r;
	gboolean	found = FALSE;

	if (!int_name || !result)
		return FALSE;
	for (list = gkrellmrc_integer_list; list; list = list->next)
		{
		s = list->data;
		if ((r = strchr(s, '=')) != NULL)
			*r = ' ';
		sscanf(s, "%63s %63s", name, string);
		if (!strcmp(name, int_name) && sscanf(string, "%d", result) == 1)
			found = TRUE;
		}
	return found;
	}

gchar *
gkrellm_get_gkrellmrc_string(gchar *string_name)
	{
	GList	*list;
	gchar	name[64], string[CFG_BUFSIZE];
	gchar	*s, *r;

	if (!string_name)
		return NULL;
	for (list = gkrellmrc_string_list; list; list = list->next)
		{
		s = list->data;
		if ((r = strchr(s, '=')) != NULL)
			*r = ' ';
		sscanf(s, "%63s %[^\n]", name, string);
		if (!strcmp(name, string_name))
			{
			if ((s = gkrellm_cut_quoted_string(string, NULL)) != NULL)
				return g_strdup(s);
			break;
			}
		}
	return NULL;
	}

static gboolean
parse_monitor_config_keyword(GkrellmMonitor *mon_only, gchar *line)
	{
	GList			*list;
	GkrellmMonitor	*mon;
	gchar			*keyword;
	gboolean		result = FALSE;

	keyword = gkrellm_dup_token(&line, NULL);
	if (!*keyword)
		{
		g_free(keyword);
		return FALSE;
		}

	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (mon_only && mon != mon_only)
			continue;
		if (!mon->config_keyword || strcmp(mon->config_keyword, keyword))
			continue;
		while (*line == ' ' || *line == '\t')
			++line;
		if (*line && mon->load_user_config && mon->privat->enabled)
			{
			gkrellm_record_state(LOAD_CONFIG, mon);
			(*(mon->load_user_config))(line);
			gkrellm_record_state(INTERNAL, NULL);
			}
		result = TRUE;
		}
	g_free(keyword);
	return result;
	}

static gboolean
parse_gkrellmrc_keyword(gchar *line)
	{
	GkrellmMonitor	*mon;
	gchar			buf[CFG_BUFSIZE];
	gchar			*s, *arg;

	strncpy(buf, line, CFG_BUFSIZE);	/* strtok() is destructive */
	buf[CFG_BUFSIZE - 1] = '\0';
	s = strtok(buf, " \t:=\n");
	if (!s || *s == '#' || *s == '\n' || *s == '\0')
		return FALSE;
	arg = strtok(NULL, "\n");

	if (strncmp(line, "Style", 5) == 0)	/* StyleChart ... */
		assign_gkrellmrc_style(line, s, arg);
	else if (   !strncmp(s, "spacer_top_height", 17)
			 || !strncmp(s, "spacer_bottom_height", 20)
			)
		assign_gkrellmrc_spacer(line, s, arg);
	else if (!strcmp(s, "cap_images_off"))
		{
		mon = gkrellm_monitor_from_style_name(arg);
		if (mon)
			mon->privat->cap_images_off = TRUE;
		}
	else if (!strcmp(s, "spacer_overlap_off"))
		{
		mon = gkrellm_monitor_from_style_name(arg);
		if (mon)
			mon->privat->spacer_overlap_off = TRUE;
		}
	else if (strcmp(s, "set_image_border") == 0)
		gkrellmrc_border_list = g_list_append(gkrellmrc_border_list,
				g_strdup(arg));
	else if (strcmp(s, "set_integer") == 0)
		gkrellmrc_integer_list = g_list_append(gkrellmrc_integer_list,
				g_strdup(arg));
	else if (strcmp(s, "set_string") == 0)
		gkrellmrc_string_list = g_list_prepend(gkrellmrc_string_list,
				g_strdup(arg));
	else
		return FALSE;
	return TRUE;
	}

static gboolean
parse_config_line(gchar *line, struct _config *cf, gint size)
	{
	struct _config	*conf;
	gchar			*s, *ss;
	gint			i, val, len;

	s = line;
	while (*s && *s != ' ' && *s != '\t' && *s != '=')
		++s;
	if (s == line || !*s || *s == '\n')
		return FALSE;
	for (i = 0; i < size; ++i)
		{
		conf = cf + i;
		len = strlen(conf->option);
		if (strncmp(conf->option, line, len) || len != s - line)
			continue;
		while (*s == ' ' || *s == '\t' || *s == '=')
			++s;
		if (!*s || *s == '\n')
			{
			printf(_("Incomplete config line:\n    %s\n"), line);
			continue;
			}
		if (conf->value)
			{
			if (isdigit((unsigned char)*s) || *s == '-')
				val = atoi(s);
			else if (!strcmp(s, "on") || !strcmp(s, "true"))
				val = 1;
			else
				val = 0;
			if (conf->minimum > -100 && val < conf->minimum)
				val = conf->minimum;
			*(conf->value) = val;
			}
		else if (conf->arg)
			{
			if (*s == '"')
				{
				++s;
				ss = strchr(s, '"');
				}
			else
				{
				ss = s;
				while (*ss && *ss != ' ' && *ss != '\t' && *ss != '\n')
					++ss;
				if (*ss == '\0')
					ss = NULL;
				}
			if (ss)
				*(conf->arg) = g_strndup(s, ss - s);
			else
				*(conf->arg) = g_strdup(s);
			}
		return TRUE;
		}
	return FALSE;
	}

static void
parse_gkrellmrc_line(gchar *line)
	{
	if (!parse_gkrellmrc_keyword(line))
		parse_config_line(line, &theme_config[0],
					sizeof(theme_config) / sizeof (struct _config));

	}

static void
clear_style_list(GList *list, GList *name_list)
	{
	GkrellmStyle	*style;

	for ( ; list; list = list->next, name_list = name_list->next)
		{
		style = (GkrellmStyle *) list->data;
		if (style)
			memset(style, 0, sizeof(GkrellmStyle));
		}
	}

static gchar	*base_theme[]	=
	{
	"StyleChart *.border = 0,0,0,0",
	"StyleChart *.margins = 0,0,0,0",
	"StyleChart *.font = normal_font",
	"StyleChart *.alt_font = small_font",
	"StyleChart *.textcolor = #efd097 #000000 shadow",
	"StyleChart *.alt_textcolor = #c0c0c0 #181818 shadow",

	"StylePanel *.border = 0,0,2,1",
	"StylePanel *.font = normal_font",
	"StylePanel *.alt_font = normal_font",
	"StylePanel *.textcolor = white #000000 shadow",
	"StylePanel *.alt_textcolor = #d8e0c8 #000000 shadow",
	"StylePanel *.label_position = 50",
	"StylePanel *.margins = 1,1,2,2",
	"StylePanel *.krell_yoff = 0",
	"StylePanel *.krell_yoff_not_scalable = false",
	"StylePanel *.krell_x_hot = 3",
	"StylePanel *.krell_depth = 4",
	"StylePanel *.krell_expand = 0",
	"StylePanel *.krell_ema_period = 3",

	"StyleMeter *.border = 3,3,3,2",
	"StyleMeter *.font = normal_font",
	"StyleMeter *.alt_font = small_font",
	"StyleMeter *.textcolor = #ffeac4 #000000 shadow",
	"StyleMeter *.alt_textcolor = wheat #000000 shadow",
	"StyleMeter *.label_position = 50",
	"StyleMeter *.margins = 2,2,2,2",
	"StyleMeter *.krell_yoff = 1",
	"StyleMeter *.krell_yoff_not_scalable = false",
	"StyleMeter *.krell_expand = 0",
	"StyleMeter *.krell_x_hot = -1",
	"StyleMeter *.krell_depth = 1",
	"StyleMeter *.krell_ema_period = 1",

	/* These have an override effect */
	"StyleMeter apm.bottom_margin = 2",
	"StyleMeter mail.krell_depth = 15",
	"StyleMeter mail.krell_yoff = 0",
	"StyleMeter mail.krell_x_hot = -1",
	"StyleMeter mail.krell_expand = 0",
	"StyleMeter mail.label_position = 70",
	"StyleChart net.alt_font = small_font",
	};


static void
gkrellm_init_theme(void)
	{
	GkrellmMonitor	*mon;
	GList			*list;
	GkrellmStyle	*style;
	gint			i, style_id;

	_GK.theme_n_alternatives = 0;
	_GK.frame_top_height = 0;		/* use image height. */
	_GK.frame_bottom_height = 0;	/* use image height. */
	_GK.frame_left_width = 0;
	_GK.frame_right_width = 0;

	_GK.frame_left_chart_overlap = 0;
	_GK.frame_right_chart_overlap = 0;
	_GK.frame_left_panel_overlap = 0;
	_GK.frame_right_panel_overlap = 0;
	_GK.frame_left_spacer_overlap = 0;
	_GK.frame_right_spacer_overlap = 0;

	_GK.chart_height_min = 5;
	_GK.chart_height_max = 200;
	_GK.chart_width_ref = 60;
	_GK.chart_text_no_fill = FALSE;
	_GK.bg_separator_height = 2;
	_GK.allow_scaling = FALSE;
	_GK.bg_grid_mode = GRID_MODE_NORMAL;
	_GK.spacer_top_height_chart = 3;
	_GK.spacer_bottom_height_chart = 3;
	_GK.spacer_top_height_meter = 3;
	_GK.spacer_bottom_height_meter = 3;

	_GK.decal_mail_frames = 18;
	_GK.decal_mail_delay = 1;

	_GK.decal_alarm_frames = 10;
	_GK.decal_warn_frames = 10;

	_GK.rx_led_x = 3;
	_GK.rx_led_y = 6;
	_GK.tx_led_x = -3;
	_GK.tx_led_y = 6;

	gkrellm_dup_string(&_GK.large_font_string,
				gkrellm_get_large_font_string());
	gkrellm_dup_string(&_GK.normal_font_string,
				gkrellm_get_normal_font_string());
	gkrellm_dup_string(&_GK.small_font_string,
				gkrellm_get_small_font_string());

	gkrellm_dup_string(&_GK.chart_in_color, "#10d3d3");
	gkrellm_dup_string(&_GK.chart_in_color_grid, "#00a3a3");
	gkrellm_dup_string(&_GK.chart_out_color, "#f4ac4a");
	gkrellm_dup_string(&_GK.chart_out_color_grid, "#b47c20");


	/* Setup the default styles.  Substyles may be set in gkrellmrc.  If
	|  they are not, then they will be set to point to the default after
	|  parsing the gkrellmrc file.
	*/
	clear_style_list(_GK.chart_style_list, _GK.chart_name_list);
	clear_style_list(_GK.panel_style_list, _GK.chart_name_list);
	clear_style_list(_GK.meter_style_list, _GK.meter_name_list);
	gkrellm_free_glist_and_data(&_GK.custom_name_list);
	gkrellm_free_glist_and_data(&_GK.custom_style_list);

	for (i = 0; i < sizeof(base_theme) / sizeof(gchar *); ++i)
		parse_gkrellmrc_line(base_theme[i]);

	/* Allow themes to transition to using top/bottom margins. */
	_GK.use_top_bottom_margins = FALSE;

	/* I set some base theme parameters with no override.  The idea is if
	|  a theme does not bother to set anything, these will remain in effect,
	|  but if the theme sets any "*" settings, they can wipe these out.
	|  This is probably a mistake, I am contributing to theme author
	|  laziness and should move these to the default theme.
	*/
	style_id = gkrellm_lookup_meter_style_id(BATTERY_STYLE_NAME);
	assign_meter_style(style_id, "3,3,2,2", GKRELLMSTYLE_BORDER, 0);

	style_id = gkrellm_lookup_meter_style_id(CAL_STYLE_NAME);
	assign_meter_style(style_id,  "small_font", GKRELLMSTYLE_TEXTFONT_A, 0);
	assign_meter_style(style_id,  "large_font", GKRELLMSTYLE_TEXTFONT_B, 0);

	style_id = gkrellm_lookup_meter_style_id(CLOCK_STYLE_NAME);
	assign_meter_style(style_id,"large_font", GKRELLMSTYLE_TEXTFONT_A, 0);

	style_id = gkrellm_lookup_meter_style_id(FS_STYLE_NAME);
	assign_meter_style(style_id, "0", GKRELLMSTYLE_LABEL_POSITION, 0);

	if (_GK.krell_slider_style)
		g_free(_GK.krell_slider_style);
	_GK.krell_slider_style = gkrellm_style_new0();
	style = (GkrellmStyle *) _GK.meter_style_list->data;
	*_GK.krell_slider_style = *style;

	if (_GK.krell_mini_style)
		g_free(_GK.krell_mini_style);
	_GK.krell_mini_style = gkrellm_style_new0();
	*_GK.krell_mini_style = *style;

	gkrellm_dup_string(&frame_top_border, "0,0,0,0");
	gkrellm_dup_string(&frame_bottom_border, "0,0,0,0");
	gkrellm_dup_string(&frame_left_border, "0,0,0,0");
	gkrellm_dup_string(&frame_right_border, "0,0,0,0");
	gkrellm_dup_string(&button_panel_border, "2,2,2,2");
	gkrellm_dup_string(&button_meter_border, "2,2,2,2");
	gkrellm_dup_string(&frame_left_chart_border, "0,0,0,0");
	gkrellm_dup_string(&frame_right_chart_border, "0,0,0,0");
	gkrellm_dup_string(&frame_left_panel_border, "0,0,0,0");
	gkrellm_dup_string(&frame_right_panel_border, "0,0,0,0");
	gkrellm_dup_string(&spacer_top_border, "0,0,0,0");
	gkrellm_dup_string(&spacer_bottom_border, "0,0,0,0");

	krell_slider_x_hot = -1;
	krell_slider_depth = 6;
	gkrellm_dup_string(&krell_slider_expand, "none");

	gkrellm_free_glist_and_data(&gkrellmrc_border_list);
	gkrellm_free_glist_and_data(&gkrellmrc_integer_list);
	gkrellm_free_glist_and_data(&gkrellmrc_string_list);
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		mon->privat->top_spacer.height = -1;
		mon->privat->bottom_spacer.height = -1;
		mon->privat->cap_images_off = FALSE;
		mon->privat->spacer_overlap_off = FALSE;
		}
	}

static gchar	*default_theme[]	=
	{
	"frame_left_border = 0,0,42,16",
	"frame_right_border = 0,0,42,16",

	"StyleChart *.textcolor = #efd097 #000000 shadow",
	"StyleChart *.alt_textcolor = #a8e4e2 #000000 shadow",

	"StylePanel *.margins = 1,1,2,1",
	"StylePanel *.textcolor = white #000000 shadow",
	"StylePanel *.alt_textcolor = #60fff0 #000000 shadow",

	"StylePanel cpu.margins = 0,0,2,1",

	"StyleMeter *.border = 4,4,3,2",
	"StyleMeter *.margins = 2,2,2,2",
	"StyleMeter *.krell_yoff = 1",

	"StyleMeter *.textcolor = #c8e4e2 #000000 shadow",
	"StyleMeter *.alt_textcolor = #e8e4d2 #000000 shadow",

	"StyleMeter cal.textcolor = #c8e4e2 #000000 shadow",

	"StyleMeter clock.textcolor = #e8e4d2 #000000 shadow",
	"StyleMeter clock.alt_textcolor = #c8e4e2 #000000 shadow",

	"StyleMeter sensors.textcolor = #60fff0 #000000 shadow",
	"StyleMeter sensors.alt_textcolor = #c8e4e2 #000000 shadow",

	"StyleMeter fs.border = 4,4,3,1",
	"StyleMeter fs.bottom_margin = 1",
	"StyleMeter fs.label_position = 0",
	"StyleMeter fs.alt_font = normal_font",

	"StyleMeter fs.alt_textcolor = #c8e4e2 #000000 shadow",
	"StyleMeter mem.alt_textcolor = #c8e4e2 #000000 shadow",
	"StyleMeter swap.alt_textcolor = #c8e4e2 #000000 shadow",


	"StyleMeter host.textcolor = #c8d4d2 #000000 shadow",
	"StyleMeter host.top_margin = 1",
	"StyleMeter host.bottom_margin = 1",

	"StyleMeter mail.alt_textcolor = #ffc0c0 #000000 shadow",

	"StyleMeter mem.krell_yoff = 0",
	"StyleMeter mem.alt_font = normal_font",
	"StyleMeter mem.top_margin = 2",
	"StyleMeter mem.bottom_margin = 0",
	"StyleMeter swap.top_margin = 2",
	"StyleMeter swap.bottom_margin = 1",

	"StyleMeter swap.krell_yoff = -2",		/* Bottom justify */
	"StyleMeter swap.alt_font = normal_font",

	"StyleMeter sensors.alt_textcolor = #d8e0c8 #000000 shadow",
	"StyleMeter sensors.top_margin = 3",
	"StyleMeter sensors.bottom_margin = 3",
	"set_image_border sensors_bg_volt 1,1,1,1",

	"StyleMeter timer.textcolor = #e8e4d2 #000000 shadow",
	"StyleMeter timer.alt_textcolor = #c8e4e2 #000000 shadow",
	"StyleMeter timer.margins = 1,1,1,2",
	"set_image_border timer_bg_timer 1,1,2,2",

	"StyleMeter uptime.textcolor = #e8e4d2 #000000 shadow",
	"StyleMeter uptime.border = 3,3,2,2",
	"StyleMeter uptime.bottom_margin = 1",


	/* ---- plugins ---- */

	"spacer_top_height pmu 3",
	"spacer_bottom_height pmu 2",

	/* GKrellMMS scroll bar panel */
	"spacer_top_height gkrellmms 3",
	"spacer_bottom_height gkrellmms 3",
	"set_image_border gkrellmms_bg_scroll 3,3,2,2",
	"set_integer gkrellmms_scroll_margin 3",
	"set_integer gkrellmms_scroll_top_margin 2",
	"set_integer gkrellmms_scroll_bottom_margin 1",
	/* GKrellMMS control (button) bar panel*/
	"StyleMeter gkrellmms.alt_textcolor = black #dcdccc shadow",
	"StyleMeter gkrellmms.margins = 2,2,2,0",
	"StyleMeter gkrellmms.border = 2,2,4,0",
	"StyleMeter gkrellmms.krell_yoff = 0",
	"StyleMeter gkrellmms.krell_x_hot = 59",
	"StyleMeter gkrellmms.krell_expand = left",
	"StyleMeter gkrellmms.krell_left_margin = 3",
	"StyleMeter gkrellmms.krell_right_margin = 2",

	"set_string gkrellmms_play_button_position \"-27 4 0 0 c\"",
	"set_string gkrellmms_prev_button_position \"-25 20 0 0 c\"",
	"set_string gkrellmms_stop_button_position \"-13 21 0 0 c\"",
	"set_string gkrellmms_next_button_position \"9 20 0 0 c\"",

	"set_string gkrellmms_eject_button_position \"17 12 0 0 c\"",
	"set_string gkrellmms_led_position \"7 7 c\"",
	"set_string gkrellmms_label_position \"-25 7 c\"",



	/* Timers panels */
	"spacer_top_height timers 3",
	"spacer_bottom_height timers 3",
	"StyleMeter timers.border = 6,6,2,2",
	"StyleMeter timers.font = large_font",
	"StyleMeter timers.textcolor = #d8e4d2 #000000 shadow",
	"StyleMeter timers.alt_textcolor = #c8e4e2 #000000 shadow",

	/* All volume panels */
	"spacer_top_height volume 3",
	"spacer_bottom_height volume 3",
	"StyleMeter volume.label_position = 0",
	"StyleMeter volume.border = 26,3,0,0",
	"StyleMeter volume.top_margin = 1",
	"StyleMeter volume.bottom_margin = 0",
	};

static gchar	*default_theme_1[]	=
	{
	"StyleChart *.textcolor #efd097 #000000 shadow",
	"StyleChart *.alt_textcolor #e4e4e2 #000000 shadow",
	"StylePanel *.textcolor white #000000 shadow",
	"StylePanel *.alt_textcolor #f0f080 #000000 shadow",
	"StyleMeter *.textcolor = #f2f4d8 #000000 shadow",
	"StyleMeter *.alt_textcolor #e8e4b2 #000000 shadow",
	"StyleMeter cal.textcolor #f2f4d8 #000000 shadow",
	"StyleMeter clock.textcolor #e8e4b2 #000000 shadow",
	"StyleMeter clock.alt_textcolor #f2f4d8 #000000 shadow",
	"StyleMeter fs.alt_textcolor = #f2f4d8 #000000 shadow",
	"StyleMeter host.textcolor #b8c4c2 #000000 shadow",
	"StyleMeter mail.alt_textcolor #e0c0c0 #000000 shadow",
	"StyleMeter mem.alt_textcolor = #f2f4d8 #000000 shadow",
	"StyleMeter swap.alt_textcolor = #f2f4d8 #000000 shadow",
	"StyleMeter sensors.textcolor = #f0f080 #000000 shadow",
	"StyleMeter sensors.alt_textcolor = #f2f4d8 #000000 shadow",
	"StyleMeter timer.textcolor #e8e4b2 #000000 shadow",
	"StyleMeter timer.alt_textcolor #f2f4d8 #000000 shadow",
	"StyleMeter uptime.textcolor #e8e4b2 #000000 shadow",
	"StyleMeter gkrellmms.alt_textcolor = black #dcdccc shadow",
	"StyleMeter timers.textcolor #d2d8c0 #000000 shadow",
	"StyleMeter timers.alt_textcolor = #f2f4d8 #000000 shadow",
	};

static gchar	*default_theme_2[]	=
	{
	"StyleChart *.textcolor #f8b080 #000000 shadow",
	"StyleChart *.alt_textcolor #f0e8f0 #000000 shadow",
	"StylePanel *.textcolor white #000000 shadow",
	"StylePanel *.alt_textcolor #f0d0f0 #000000 shadow",
	"StyleMeter *.textcolor = #f0e8f0 #000000 shadow",
	"StyleMeter *.alt_textcolor #f0c0a0 #000000 shadow",
	"StyleMeter cal.textcolor #f0e8f0 #000000 shadow",
	"StyleMeter clock.textcolor #f0c0a0 #000000 shadow",
	"StyleMeter clock.alt_textcolor #f0e8f0 #000000 shadow",
	"StyleMeter fs.alt_textcolor = #f0e8f0 #000000 shadow",
	"StyleMeter host.textcolor #b8c4c2 #000000 shadow",
	"StyleMeter mail.alt_textcolor #e0c0c0 #000000 shadow",
	"StyleMeter mem.alt_textcolor = #f0e8f0 #000000 shadow",
	"StyleMeter swap.alt_textcolor = #f0e8f0 #000000 shadow",
	"StyleMeter sensors.textcolor = #f0d0f0 #000000 shadow",
	"StyleMeter sensors.alt_textcolor = #f0e8f0 #000000 shadow",
	"StyleMeter timer.textcolor #f0c0a0 #000000 shadow",
	"StyleMeter timer.alt_textcolor #f0e8f0 #000000 shadow",
	"StyleMeter uptime.textcolor #f0c0a0 #000000 shadow",
	"StyleMeter gkrellmms.alt_textcolor = black #dcdccc shadow",
	"StyleMeter timers.textcolor #f0d0b0 #000000 shadow",
	"StyleMeter timers.alt_textcolor = #f0e8f0 #000000 shadow",
	};

static void
default_theme_config(void)
	{
	gint	i;

	_GK.theme_n_alternatives = 5;
	if (_GK.theme_alternative > _GK.theme_n_alternatives)
		_GK.theme_alternative = _GK.theme_n_alternatives;

	for (i = 0; i < sizeof(default_theme) / sizeof(gchar *); ++i)
		parse_gkrellmrc_line(default_theme[i]);

	if (_GK.theme_alternative == 1 || _GK.theme_alternative == 4)
		{
		for (i = 0; i < sizeof(default_theme_1) / sizeof(gchar *); ++i)
			parse_gkrellmrc_line(default_theme_1[i]);
		}
	if (_GK.theme_alternative == 2 || _GK.theme_alternative == 5)
		{
		for (i = 0; i < sizeof(default_theme_2) / sizeof(gchar *); ++i)
			parse_gkrellmrc_line(default_theme_2[i]);
		}
	if (_GK.theme_alternative > 2)
		parse_gkrellmrc_keyword("StyleChart *.transparency = 1");
	}

static void
parse_gkrellmrc(gint alternative)
	{
	FILE	*f;
	gchar	*path, *s, *ss, lbuf[16];
	gchar	buf[CFG_BUFSIZE];

	lbuf[0] = '\0';
	if (alternative > 0)
		snprintf(lbuf, sizeof(lbuf), "_%d", alternative);
	path = g_strdup_printf("%s/%s%s", _GK.theme_path, GKRELLMRC, lbuf);
	if ((f = fopen(path, "r")) != NULL)
		{
		while (fgets(buf, sizeof(buf), f))
			{
			s = buf;
			while (*s == ' ' || *s == '\t')
				++s;
			if (!*s || *s == '\n' || *s == '#')
				continue;
			ss = strchr(s, '\n');
			if (ss)
				*ss = '\0';
			parse_gkrellmrc_line(s);
			}
		fclose(f);
		}
	g_free(path);
	}

gboolean
gkrellm_using_default_theme(void)
	{
	return (*(_GK.theme_path) == '\0') ? TRUE : FALSE;
	}

void
gkrellm_theme_config(void)
	{
	gkrellm_load_theme_config();
	gkrellm_init_theme();

	if (gkrellm_using_default_theme())
		default_theme_config();
	else
		{
		parse_gkrellmrc(0);
		if (_GK.theme_alternative > _GK.theme_n_alternatives)
			_GK.theme_alternative = _GK.theme_n_alternatives;
		if (_GK.theme_alternative > 0)
			parse_gkrellmrc(_GK.theme_alternative);
		}
	cleanup_gkrellmrc();
	gkrellm_set_theme_alternatives_label();

	/* Warn theme developers!
	*/
	if (!_GK.use_top_bottom_margins && _GK.command_line_theme)
		fprintf(stderr,
				"Warning: Top and bottom meter/panel margins are not set.\n"
				"         Do not depend on borders!!\n");
	}


/* --------------------------------------------------------------*/

struct	_config	user_config[] =
	{
	{"enable_hostname",	&_GK.enable_hostname,		NULL,	0  },
	{"hostname_short",	&_GK.hostname_short,		NULL,	0  },
	{"enable_sysname",	&_GK.enable_system_name,	NULL,	0  },
	{"mbmon_port",		&_GK.mbmon_port,			NULL,	0  },

#if !defined(WIN32)
	{"sticky_state",	&_GK.sticky_state,		NULL, 	0 },
	{"dock_type",		&_GK.dock_type,			NULL,	0 },
	{"decorated",		&_GK.decorated,			NULL,	0 },
	{"skip_taskbar",	&_GK.state_skip_taskbar, NULL,	0 },
	{"skip_pager",		&_GK.state_skip_pager,	NULL,	0 },
	{"above",			&_GK.state_above,	NULL,	0 },
	{"below",			&_GK.state_below,	NULL,	0 },
#else
	{"on_top",			&_GK.on_top,		NULL,		0  },
#endif

	{"track_gtk_theme_name", &_GK.track_gtk_theme_name, NULL,	0  },
	{"default_track_theme", NULL, &_GK.default_track_theme,	0  },
	{"save_position",	&_GK.save_position,		NULL,	0  },

	{"chart_width",		&_GK.chart_width,		NULL,	0  },

	{"update_HZ",		&_GK.update_HZ,			NULL,	0  },
	{"allow_multiple_instances", &_GK.allow_multiple_instances,	NULL,	0  },
	};

  /* The user_config is read twice.  Early to load _GK config values and then
  |  again when when building gkrellm for the first time to load monitor user
  |  config values.  It's also read at plugin enables in case existing configs.
  */
void
gkrellm_load_user_config(GkrellmMonitor *mon_only, gboolean monitor_values)
	{
	FILE	*f;
	gchar	*s, *ss, *config;
	gchar	buf[CFG_BUFSIZE];

	if (!monitor_values)
		{
		_GK.enable_hostname = TRUE;
		_GK.hostname_short = FALSE;
		_GK.enable_system_name = FALSE;
		_GK.chart_width = 60;
		_GK.update_HZ = 10;
		_GK.theme_scale = 100;
		_GK.float_factor = 1.0;
		_GK.default_track_theme = g_strdup("Default");
		}
	config = gkrellm_make_config_file_name(gkrellm_homedir(),
				GKRELLM_USER_CONFIG);
	f = fopen(config, "r");
	g_free(config);

	if (f)
		{
		while (fgets(buf, sizeof(buf), f))
			{
			s = buf;
			while (*s == ' ' || *s == '\t')
				++s;
			if (!*s || *s == '\n' || *s == '#')
				continue;
			ss = strchr(s, '\n');
			if (ss)
				*ss = '\0';
			if (!strncmp(s, "float_factor ", 13))
				sscanf(s + 13, "%f", &_GK.float_factor);
			else if (monitor_values)
				parse_monitor_config_keyword(mon_only, s);
			else
				parse_config_line(s, &user_config[0],
						sizeof(user_config) / sizeof (struct _config));
			}
		fclose(f);
		}
	if (_GK.chart_width < CHART_WIDTH_MIN)
		_GK.chart_width = CHART_WIDTH_MIN;
	if (_GK.chart_width > CHART_WIDTH_MAX)
		_GK.chart_width = CHART_WIDTH_MAX;
	}

void
gkrellm_config_modified(void)
	{
	if (_GK.no_config)
		return;
	_GK.config_modified = TRUE;
	}

gchar *
gkrellm_make_data_file_name(gchar *subdir, gchar *name)
	{
	gchar	*dir, *path, *s;

	dir = gkrellm_make_config_file_name(NULL, GKRELLM_DATA_DIR);
	gkrellm_make_home_subdir(dir, &path);
	if (subdir)
		{
		g_free(path);
		s = g_strconcat(dir, G_DIR_SEPARATOR_S, subdir, NULL);
		g_free(dir);
		dir = s;
		gkrellm_make_home_subdir(dir, &path);
		}
	g_free(dir);
	if (name)
		{
		s = g_strconcat(path, G_DIR_SEPARATOR_S, name, NULL);
		g_free(path);
		path = s;
		}
	return path;
	}

gchar *
gkrellm_make_config_file_name(gchar *dir, gchar *config)
	{
	gchar	hostname[256],
			*fname, *d,
			*s		= NULL;

	if (_GK.client_mode)
		{
		s = g_strdup_printf("%s_S-%s", config, _GK.server_hostname);
		if (_GK.config_suffix)
			{
			d = g_strconcat(s, "-", _GK.config_suffix, NULL);
			g_free(s);
			s = d;
			}
		}
	else if (   _GK.config_suffix
			 || _GK.found_host_config || _GK.force_host_config
			)
		{
		if (_GK.config_suffix)
			s = _GK.config_suffix;
		else if (!gethostname(hostname, 256))
			s = hostname;
		if (s)
			s = g_strdup_printf("%s-%s", config, s);
		}
	if (!s)
		s = g_strdup(config);

	if (dir)
		{
		fname = g_strdup_printf("%s/%s", dir, s);
		g_free(s);
		}
	else
		fname = s;
	return fname;
	}

void
gkrellm_save_user_config(void)
	{
	FILE			*f, *ff;
	GList			*list;
	GkrellmMonitor	*mon;
	gint			i;
	mode_t			mode;
	gchar			*config, *config_new;

	if (_GK.demo || _GK.no_config)
		return;
	config = gkrellm_make_config_file_name(gkrellm_homedir(),
					GKRELLM_USER_CONFIG);
	config_new = g_strconcat(config, ".new", NULL);

	f = fopen(config_new, "w");
	if (f == NULL)
		{
		printf(_("Cannot open config file %s for writing.\n"), config_new);
		g_free(config_new);
		g_free(config);
		return;
		}

	fprintf(f,
		"### GKrellM user config.  Auto written, do not edit (usually) ###\n");
	fprintf(f, "### Version %d.%d.%d ###\n",
			GKRELLM_VERSION_MAJOR, GKRELLM_VERSION_MINOR, GKRELLM_VERSION_REV);
	for (i = 0; i < sizeof(user_config) / sizeof(struct _config); ++i)
		{
		if (user_config[i].value)
			fprintf(f, "%s %d\n", user_config[i].option,
							*(user_config[i].value));
		else if (user_config[i].arg)	/* Put quotes around strings */
			fprintf(f, "%s \"%s\"\n",user_config[i].option,
						*(user_config[i].arg));
		}
	fprintf(f, "float_factor %.0f\n", GKRELLM_FLOAT_FACTOR);
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (mon->save_user_config && mon->privat->enabled)
			{
			gkrellm_record_state(SAVE_CONFIG, mon);
			(*(mon->save_user_config))(f);
			gkrellm_record_state(INTERNAL, NULL);
			}
		}

	if (   !_GK.config_clean
	    && (ff = fopen(config, "r")) != NULL
	   )
		{
		gchar		buf[CFG_BUFSIZE], *keyword, *ch, tmp;
		struct _config
					*cf,
					*const end = user_config +
								sizeof user_config / sizeof *user_config;

		while (fgets(buf, sizeof(buf), ff))
			{
			for (ch = buf; *ch == ' ' || *ch == '\t'; ++ch)
				;
			if (*ch == '\n' || *ch == '#' || !*ch)
				continue;
			keyword = ch;
			while (*ch && *ch != ' ' && *ch != '\t' && *ch != '\n')
				++ch;
			tmp = *ch;
			*ch = 0;

			for (list = gkrellm_monitor_list; list; list = list->next)
				{
				mon = (GkrellmMonitor *) list->data;
				if (   mon->save_user_config && mon->privat->enabled
				    && mon->config_keyword
				    && !strcmp(mon->config_keyword, keyword)
				   )
					break;
				}
			if (list)
				continue;

			cf = user_config;
			while (cf != end && strcmp(cf->option, keyword))
				++cf;
			if (cf != end)
				continue;
			if (!strcmp("float_factor", keyword))
				continue;

			*ch = tmp;
			fputs(buf, f);
			}
		fclose(ff);
		}
	fclose(f);
	rename(config_new, config);

#if defined (S_IRUSR)
	mode = (S_IRUSR | S_IWUSR);
#elif defined (S_IREAD)
	mode = (S_IREAD | S_IWRITE);
#else
	mode = 0600;
#endif
	chmod(config, mode);

	g_free(config);
	g_free(config_new);

	_GK.config_modified = FALSE;
	}
