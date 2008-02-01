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


#define	DEFAULT_CLOCK_FORMAT \
	"%l:%M <span foreground=\"$A\"><small>%S</small></span>"
#define	DEFAULT_CAL_FORMAT \
	"%a <span foreground=\"$A\"><big><big>%e</big></big></span> %b"


static GkrellmMonitor
				*mon_clock,
				*mon_cal;

static GkrellmPanel
				*pclock,
				*pcal;

static GtkWidget
				*cal_vbox,
				*clock_vbox;

static GkrellmDecal
				*d_cal,
				*d_clock;


static GkrellmLauncher
				clock_launch,
				cal_launch;

static gboolean	cal_enable,
				clock_enable,
				loop_chime_enable;


static gchar	*cal_format,
				*clock_format;

static gchar	*cal_alt_color_string,
				*clock_alt_color_string;

static gint		clock_style_id,
				cal_style_id;


static gchar	*hour_chime_command,
				*quarter_chime_command;
static gboolean	chime_block;

typedef struct
	{
	gchar	*command;
	gint	count;
	}
	ChimeData;


struct tm		gkrellm_current_tm;



struct tm *
gkrellm_get_current_time(void)
	{
	return &gkrellm_current_tm;
	}


static gpointer
chime_func(gpointer data)
	{
	ChimeData	*chime = (ChimeData *)data;
	gint		counter;

	if (strlen(chime->command)) 
		{
		if (chime->count > 12) 
			chime->count -= 12;

		for (counter = 0; counter < chime -> count; counter ++)
			g_spawn_command_line_sync(chime->command,
						NULL, NULL, NULL, NULL /* GError */);
		}
	g_free(chime->command);
	g_free(chime);
	return NULL;
	}

static void
get_color_name(GdkColor *color, gchar **color_string)
	{
	gchar	*cstring;

	cstring = g_strdup_printf("#%2.2x%2.2x%2.2x",
			(color->red >> 8) & 0xff,
			(color->green >> 8) & 0xff,
			(color->blue >> 8) & 0xff );
	gkrellm_dup_string(color_string, cstring);
	}

static void
format_alt_color(gchar *src_string, gchar *buf, gint size, gchar *alt_color)
	{
	gint		len;
	gchar		*s;

	if (!buf || size < 1)
		return;
	--size;
	*buf = '\0';
	if (!src_string)
		return;

	for (s = src_string; *s != '\0' && size > 0; ++s)
		{
		len = 1;
		if (*s == '$' && *(s + 1) != '\0')
			{
			switch(*(s + 1))
				{
				case 'A':
					len = snprintf(buf, size, "%s", alt_color);
					break;
				default:
					*buf = *s;
					if (size > 1)
						{
						*(buf + 1) = *(s + 1);
						++len;
						}
					break;
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

static gchar *
strftime_format(gchar *format, gchar *alt_color)
	{
	struct tm	*t;
	gchar		buf1[512], buf2[512];

	if (_GK.client_mode)
		t = gkrellm_client_server_time();
	else
		t = &gkrellm_current_tm;

	strftime(buf1, sizeof(buf1), format, t);
	format_alt_color(buf1, buf2, sizeof(buf2), alt_color);

// printf("%s\n", buf2);

	return g_strdup(buf2);
	}

static gint
expose_event(GtkWidget *widget, GdkEventExpose *ev)
	{
	GdkPixmap	*pixmap = NULL;

	if (widget == pcal->drawing_area)
		pixmap = pcal->pixmap;
	else if (widget == pclock->drawing_area)
		pixmap = pclock->pixmap;
	if (pixmap)
		gdk_draw_drawable(widget->window, gkrellm_draw_GC(1), pixmap,
				ev->area.x, ev->area.y, ev->area.x, ev->area.y,
				ev->area.width, ev->area.height);
	return FALSE;
	}

static gint
cb_panel_press(GtkWidget *widget, GdkEventButton *ev)
	{
	if (ev->button == 3)
		gkrellm_open_config_window(mon_clock);
	return FALSE;
	}

static void
cal_visibility(void)
	{
	if (cal_enable)
		{
		gkrellm_panel_show(pcal);
		gkrellm_spacers_show(mon_cal);
		}
	else
		{
		gkrellm_panel_hide(pcal);
		gkrellm_spacers_hide(mon_cal);
		}
	}

static void
create_calendar_panel(GtkWidget *vbox, gint first_create)
	{
	GkrellmStyle		*style;
	GkrellmTextstyle	*ts;
	gchar				*cal_string;

	if (first_create)
		pcal = gkrellm_panel_new0();
	style = gkrellm_meter_style(cal_style_id);

	ts = gkrellm_meter_alt_textstyle(cal_style_id);
	get_color_name(&ts->color, &cal_alt_color_string);

	cal_string = strftime_format(cal_format, cal_alt_color_string);
	d_cal = gkrellm_create_decal_text_markup(pcal, cal_string,
				gkrellm_meter_textstyle(cal_style_id), style, -1, -1, -1);

	gkrellm_panel_configure(pcal, NULL, style);
	gkrellm_panel_create(vbox, mon_cal, pcal);

	if (first_create)
		{
		/* Help the motion out hack. If starting a move in host panel and mouse
		|  jerks into the pclock/pcal drawing areas, we stop moving unless:
		*/
		extern void gkrellm_motion(GtkWidget *, GdkEventMotion *, gpointer);

		g_signal_connect(G_OBJECT(pcal->drawing_area), "motion_notify_event",
				G_CALLBACK(gkrellm_motion), NULL);

		g_signal_connect(G_OBJECT (pcal->drawing_area), "expose_event",
				G_CALLBACK(expose_event), NULL);
		g_signal_connect(G_OBJECT(pcal->drawing_area), "button_press_event",
				G_CALLBACK(cb_panel_press), NULL);
		}
	gkrellm_setup_launcher(pcal, &cal_launch, METER_PANEL_TYPE, 0);

	cal_visibility();
	}

static void
draw_cal(void)
	{
	gchar			*utf8, *cal_string;
	gint			w, h;

	if (!cal_enable)
		return;

	cal_string = strftime_format(cal_format, cal_alt_color_string);
	if (!g_utf8_validate(cal_string, -1, NULL))
		{
	    if ((utf8 = g_locale_to_utf8(cal_string, -1, NULL, NULL, NULL))
				!= NULL)
			{
			g_free(cal_string);
			cal_string = utf8;
			}
		}

	/* Get string extents in case a string change => need to resize the
	|  panel.  A string change can also change the decal y_ink (possibly
	|  without changing decal size).
	*/
	gkrellm_text_markup_extents(d_cal->text_style.font,
			cal_string, strlen(cal_string), &w, &h, NULL, &d_cal->y_ink);

	w += d_cal->text_style.effect;
	if (h + d_cal->text_style.effect != d_cal->h)
		{
		gkrellm_panel_destroy(pcal);
		create_calendar_panel(cal_vbox, TRUE);
		}

	gkrellm_draw_decal_markup(pcal, d_cal, cal_string);
	gkrellm_decal_text_set_offset(d_cal, (d_cal->w - w) / 2, 0);
	
	gkrellm_draw_panel_layers(pcal);
	}

static void
create_calendar(GtkWidget *vbox, gint first_create)
	{
	cal_vbox = vbox;
	create_calendar_panel(vbox, first_create);
	draw_cal();
	}

static void
clock_visibility(void)
	{
	if (clock_enable)
		{
		gkrellm_panel_show(pclock);
		gkrellm_spacers_show(mon_clock);
		}
	else
		{
		gkrellm_panel_hide(pclock);
		gkrellm_spacers_hide(mon_clock);
		}
	}

static void
create_clock_panel(GtkWidget *vbox, gint first_create)
	{
	GkrellmStyle		*style;
	GkrellmTextstyle	*ts;
	gchar				*clock_string;

	if (first_create)
		pclock = gkrellm_panel_new0();
	style = gkrellm_meter_style(clock_style_id);

	ts = gkrellm_meter_alt_textstyle(clock_style_id);
	get_color_name(&ts->color, &clock_alt_color_string);

	clock_string = strftime_format(clock_format, clock_alt_color_string);
	d_clock = gkrellm_create_decal_text_markup(pclock, clock_string,
				gkrellm_meter_textstyle(clock_style_id), style, -1, -1, -1);

	gkrellm_panel_configure(pclock, NULL, style);
	gkrellm_panel_create(vbox, mon_clock, pclock);

	if (first_create)
		{
		g_signal_connect(G_OBJECT (pclock->drawing_area), "expose_event",
				G_CALLBACK(expose_event), NULL);
		g_signal_connect(G_OBJECT(pclock->drawing_area), "button_press_event",
				G_CALLBACK(cb_panel_press), NULL);
		}
	gkrellm_setup_launcher(pclock, &clock_launch, METER_PANEL_TYPE, 0);

	clock_visibility();
	}

static void
draw_clock(gboolean check_size)
	{
	gchar	*utf8, *clock_string;
	gint	w,h;

	if (!clock_enable)
		return;

	clock_string = strftime_format(clock_format, clock_alt_color_string);
	if (!g_utf8_validate(clock_string, -1, NULL))
		{
		if ((utf8 = g_locale_to_utf8(clock_string, -1, NULL, NULL, NULL))
				!= NULL)
			{
			g_free(clock_string);
			clock_string = utf8;
			}
		}

	if (check_size)
		{
		/* Get string extents in case a string change => need to resize the
		|  panel.  A string change can also change the decal y_ink (possibly
		|  without changing decal size).
		*/
		gkrellm_text_markup_extents(d_clock->text_style.font,
				clock_string, strlen(clock_string),
				&w, &h, NULL, &d_clock->y_ink);
		w += d_clock->text_style.effect;
		if (h + d_clock->text_style.effect != d_clock->h)
			{
			gkrellm_panel_destroy(pclock);
			create_clock_panel(clock_vbox, TRUE);
			}
		gkrellm_decal_text_set_offset(d_clock, (d_clock->w - w) / 2, 0);
		}
	gkrellm_draw_decal_markup(pclock, d_clock, clock_string);
	gkrellm_draw_panel_layers(pclock);
	}


static void
create_clock(GtkWidget *vbox, gint first_create)
	{
	clock_vbox = vbox;
	create_clock_panel(vbox, first_create);
	draw_clock(TRUE);
	}

static void
update_clock(void)
	{
	struct tm		*ptm;
	static gint		min_prev, hour_prev = -1, sec_prev = -1;
	ChimeData		*chime;
	gboolean		hour_tick;

	if (_GK.client_mode)
		{
		ptm = gkrellm_client_server_time();
		if (sec_prev == ptm->tm_sec)
			return;
		sec_prev = ptm->tm_sec;
		}
	else
		{
		if (!GK.second_tick)
			return;
		ptm = &gkrellm_current_tm;
		}

	hour_tick = (ptm->tm_hour != hour_prev);
	if (   ptm->tm_min != min_prev
		|| hour_tick
	   )
		{
		draw_cal();
		if (ptm->tm_hour != hour_prev && hour_prev != -1)
			{
			if (!chime_block && hour_chime_command && *hour_chime_command) 
				{
				chime = g_new0(ChimeData, 1);
				chime -> command = strdup(hour_chime_command);
				chime -> count = loop_chime_enable ? ptm->tm_hour : 1;
				g_thread_create(chime_func, chime, FALSE, NULL);
				}
			}
		else
			{
			if (   !chime_block && (ptm->tm_min % 15) == 0
				&& quarter_chime_command && *quarter_chime_command
			   ) 
				{
				chime = g_new0(ChimeData, 1);
				chime -> command = strdup(quarter_chime_command);
				chime -> count = 1;
				g_thread_create(chime_func, chime, FALSE, NULL);
				}
			}
		}
	draw_clock(hour_tick);
	min_prev = ptm->tm_min;
	hour_prev = ptm->tm_hour;
	}


#define	CLOCK_CONFIG_KEYWORD	"clock_cal"

static void
save_clock_cal_config(FILE *f)
	{
	fprintf(f, "%s clock_launch %s\n", CLOCK_CONFIG_KEYWORD,
				clock_launch.command);
	fprintf(f, "%s clock_tooltip %s\n", CLOCK_CONFIG_KEYWORD,
				clock_launch.tooltip_comment);
	fprintf(f, "%s cal_launch %s\n", CLOCK_CONFIG_KEYWORD,
				cal_launch.command);
	fprintf(f, "%s cal_tooltip %s\n", CLOCK_CONFIG_KEYWORD,
				cal_launch.tooltip_comment);
	fprintf(f, "%s hour_chime_command %s\n", CLOCK_CONFIG_KEYWORD,
				hour_chime_command);
	fprintf(f, "%s quarter_chime_command %s\n", CLOCK_CONFIG_KEYWORD,
				quarter_chime_command);
    fprintf(f, "%s loop_chime_enable %d\n", CLOCK_CONFIG_KEYWORD,
               loop_chime_enable);
	fprintf(f, "%s clock_options %d\n", CLOCK_CONFIG_KEYWORD,
				clock_enable);
	fprintf(f, "%s cal_options %d\n", CLOCK_CONFIG_KEYWORD,
				cal_enable);
	fprintf(f, "%s cal_format %s\n", CLOCK_CONFIG_KEYWORD,
				cal_format);
	fprintf(f, "%s clock_format %s\n", CLOCK_CONFIG_KEYWORD,
				clock_format);
	}

static void
load_clock_cal_config(gchar *arg)
	{
	gchar	config[32], item[CFG_BUFSIZE];
	gint	n;

	n = sscanf(arg, "%31s %[^\n]", config, item);
	if (n == 2)
		{
		if (!strcmp(config, "clock_launch"))
			clock_launch.command = g_strdup(item);
		else if (!strcmp(config, "clock_tooltip"))
			clock_launch.tooltip_comment = g_strdup(item);
		else if (!strcmp(config, "cal_launch"))
			cal_launch.command = g_strdup(item);
		else if (!strcmp(config, "cal_tooltip"))
			cal_launch.tooltip_comment = g_strdup(item);
		else if (!strcmp(config, "hour_chime_command"))
			gkrellm_dup_string(&hour_chime_command, item);
		else if (!strcmp(config, "quarter_chime_command"))
			gkrellm_dup_string(&quarter_chime_command, item);
		else if (!strcmp(config, "loop_chime_enable"))
			sscanf(item, "%d", &loop_chime_enable);
		else if (!strcmp(config, "clock_options"))
			sscanf(item, "%d", &clock_enable);
		else if (!strcmp(config, "cal_options"))
			sscanf(item, "%d", &cal_enable);
		else if (!strcmp(config, "cal_format"))
			gkrellm_dup_string(&cal_format, item);
		else if (!strcmp(config, "clock_format"))
			gkrellm_dup_string(&clock_format, item);
		}
	}

/* --------------------------------------------------------------------- */
static GtkWidget	*cal_launch_entry,
					*cal_tooltip_entry,
					*clock_launch_entry,
					*clock_tooltip_entry,
                    *hour_chime_entry,
                    *quarter_chime_entry,
					*clock_enable_button,
					*cal_enable_button,
					*loop_chime_button;

static GtkWidget	*cal_format_combo,
					*clock_format_combo;


static void
cb_clock_cal(GtkWidget *widget, gpointer data)
	{
	loop_chime_enable = GTK_TOGGLE_BUTTON(loop_chime_button)->active;

	clock_enable = GTK_TOGGLE_BUTTON(clock_enable_button)->active;
	clock_visibility();

	cal_enable = GTK_TOGGLE_BUTTON(cal_enable_button)->active;
	cal_visibility();

	draw_cal();
	draw_clock(TRUE);
	}

static void
cb_launch_entry(GtkWidget *widget, gpointer data)
	{
	gint	which = GPOINTER_TO_INT(data);

	if (which)
		gkrellm_apply_launcher(&cal_launch_entry, &cal_tooltip_entry,
					pcal, &cal_launch, gkrellm_launch_button_cb);
	else
		gkrellm_apply_launcher(&clock_launch_entry, &clock_tooltip_entry,
					pclock, &clock_launch, gkrellm_launch_button_cb);
	}

static void
cb_chime_entry(GtkWidget *widget, gpointer data)
	{
	gint	which    = (GPOINTER_TO_INT(data)) & 0x1;
	gint	activate = (GPOINTER_TO_INT(data)) & 0x10;

	/* If editing the chime commands, block them until config is destroyed
	|  or we get a "activate".
	*/
	chime_block = activate ? FALSE : TRUE;
	if (which)
	    gkrellm_dup_string(&hour_chime_command,
				gkrellm_gtk_entry_get_text(&hour_chime_entry));
	else
	    gkrellm_dup_string(&quarter_chime_command,
				gkrellm_gtk_entry_get_text(&quarter_chime_entry));
	}

static void
cal_format_cb(GtkWidget *widget, gpointer data)
	{
	gchar	*s, *check;

	s = gkrellm_gtk_entry_get_text(&(GTK_COMBO(cal_format_combo)->entry));

	check = strftime_format(s, cal_alt_color_string);

	/* In case Pango markup tags, don't accept line unless valid markup.
	|  Ie, markup like <span ...> xxx </span> or <b> xxx </b>
	*/
	if (   strchr(check, '<') != NULL
	    && !pango_parse_markup(check, -1, 0, NULL, NULL, NULL, NULL)
	   )
		{
		g_free(check);
		return;
		}

	g_free(check);
	if (gkrellm_dup_string(&cal_format, s))
		draw_cal();
	}

static void
clock_format_cb(GtkWidget *widget, gpointer data)
	{
	gchar	*s, *check;

	s = gkrellm_gtk_entry_get_text(&(GTK_COMBO(clock_format_combo)->entry));

	check = strftime_format(s, clock_alt_color_string);

	if (   strchr(check, '<') != NULL
	    && !pango_parse_markup(check, -1, 0, NULL, NULL, NULL, NULL)
	   )
		{
		g_free(check);
		return;
		}

	g_free(check);
	if (gkrellm_dup_string(&clock_format, s))
		draw_clock(TRUE);
	}

static void
config_destroyed(void)
	{
	chime_block = FALSE;
	}


static gchar    *clock_info_text[] =
{
N_("<h>Clock/Calendar Format Strings\n"),
N_("The display format strings should contain strftime conversion\n"
"characters and Pango text attribute markup strings.\n"),
"\n",
N_("For the clock, the provided default strings will display a 12 hour\n"
"clock with seconds, a 12 hour clock with AM/PM indicator, or a 24 hour\n"
"clock with seconds.\n"),
"\n",
N_("The special $A substitution variable expands to the current theme\n"
"alternate color and is for use with the Pango \"foreground\" attribute.\n")
};


static void
create_clock_tab(GtkWidget *tab_vbox)
	{
	GtkWidget	*tabs;
	GtkWidget	*table, *vbox, *vbox1, *hbox, *label, *text;
	GList		*list;
	gint		i;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(tabs),"destroy",
				G_CALLBACK(config_destroyed), NULL);

	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Options"));

	vbox1 = gkrellm_gtk_category_vbox(vbox, _("Calendar"), 4, 0, TRUE);
	gkrellm_gtk_check_button_connected(vbox1, &cal_enable_button,
				cal_enable, FALSE, FALSE, 2,
				cb_clock_cal, NULL,
				_("Enable"));
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 6);
	label = gtk_label_new(_("Display format string:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	cal_format_combo = gtk_combo_new();
	gtk_box_pack_start(GTK_BOX(vbox1), cal_format_combo, TRUE, TRUE, 0);
	list = NULL;
	list = g_list_append(list, cal_format);
	list = g_list_append(list, DEFAULT_CAL_FORMAT);
	list = g_list_append(list,
"<big>%a %b <span foreground=\"$A\">%e</span></big>");
	list = g_list_append(list,
"%a <span foreground=\"cyan2\"><span font_desc=\"16.5\"><i>%e</i></big></span></span> %b");

	gtk_combo_set_popdown_strings(GTK_COMBO(cal_format_combo), list);
	gtk_combo_set_case_sensitive(GTK_COMBO(cal_format_combo), TRUE);
	g_list_free(list);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(cal_format_combo)->entry),
			cal_format);
	g_signal_connect(G_OBJECT(GTK_COMBO(cal_format_combo)->entry), "changed",
			G_CALLBACK(cal_format_cb), NULL);

	vbox1 = gkrellm_gtk_category_vbox(vbox, _("Clock"), 4, 0, TRUE);
	gkrellm_gtk_check_button_connected(vbox1, &clock_enable_button,
				clock_enable, FALSE, FALSE, 6,
				cb_clock_cal, NULL,
				_("Enable"));
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 6);
	label = gtk_label_new(_("Display format string:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	clock_format_combo = gtk_combo_new();
	gtk_box_pack_start(GTK_BOX(vbox1), clock_format_combo, TRUE, TRUE, 0);
	list = NULL;
	list = g_list_append(list, clock_format);
	list = g_list_append(list, DEFAULT_CLOCK_FORMAT);
	list = g_list_append(list,
		"%l:%M <span foreground=\"$A\"><small>%p</small></span>");
	list = g_list_append(list,
		"%k:%M <span foreground=\"$A\"><small>%S</small></span>");

	gtk_combo_set_popdown_strings(GTK_COMBO(clock_format_combo), list);
	gtk_combo_set_case_sensitive(GTK_COMBO(clock_format_combo), TRUE);
	g_list_free(list);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(clock_format_combo)->entry),
			clock_format);
	g_signal_connect(G_OBJECT(GTK_COMBO(clock_format_combo)->entry), "changed",
			G_CALLBACK(clock_format_cb), NULL);

/* -- Setup tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));

	vbox1 = gkrellm_gtk_category_vbox(vbox, _("Clock Chime Commands"),
				4, 0, TRUE);
	table = gkrellm_gtk_launcher_table_new(vbox1, 2);

	gkrellm_gtk_config_launcher(table, 0, &hour_chime_entry, NULL,
			_("Hour"), NULL);
	gtk_entry_set_text(GTK_ENTRY(hour_chime_entry), hour_chime_command);
	g_signal_connect(G_OBJECT(hour_chime_entry), "changed",
			G_CALLBACK(cb_chime_entry), GINT_TO_POINTER(1));
	g_signal_connect(G_OBJECT(hour_chime_entry), "activate",
			G_CALLBACK(cb_chime_entry), GINT_TO_POINTER(0x11));

	gkrellm_gtk_config_launcher(table, 1, &quarter_chime_entry, NULL,
			_("Quarter hour"), NULL);
    gtk_entry_set_text(GTK_ENTRY(quarter_chime_entry), quarter_chime_command);
	g_signal_connect(G_OBJECT(quarter_chime_entry), "changed",
			G_CALLBACK(cb_chime_entry), GINT_TO_POINTER(0));
	g_signal_connect(G_OBJECT(quarter_chime_entry), "activate",
			G_CALLBACK(cb_chime_entry), GINT_TO_POINTER(0x10));

	gkrellm_gtk_check_button_connected(vbox1, &loop_chime_button,
			loop_chime_enable, FALSE, FALSE, 6,
			cb_clock_cal, NULL,
			_("Loop hour chime command"));

	vbox = gkrellm_gtk_category_vbox(vbox, _("Launch Commands"),
				4, 0, TRUE);
	table = gkrellm_gtk_launcher_table_new(vbox, 2);

	gkrellm_gtk_config_launcher(table, 0,  &cal_launch_entry,
				&cal_tooltip_entry, _("Calendar"), &cal_launch);

	/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	for (i = 0; i < sizeof(clock_info_text)/sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(clock_info_text[i]));


	g_signal_connect(G_OBJECT(cal_launch_entry), "changed",
			G_CALLBACK(cb_launch_entry), GINT_TO_POINTER(1));
	g_signal_connect(G_OBJECT(cal_tooltip_entry), "changed",
			G_CALLBACK(cb_launch_entry), GINT_TO_POINTER(1));
	
	gkrellm_gtk_config_launcher(table, 1,  &clock_launch_entry,
				&clock_tooltip_entry, _("Clock"), &clock_launch);
	g_signal_connect(G_OBJECT(clock_launch_entry), "changed",
			G_CALLBACK(cb_launch_entry), GINT_TO_POINTER(0));
	g_signal_connect(G_OBJECT(clock_tooltip_entry), "changed",
			G_CALLBACK(cb_launch_entry), GINT_TO_POINTER(0));
	}


static GkrellmMonitor	monitor_clock =
	{
	N_("Clock"),		/* Name, for config tab.	*/
	MON_CLOCK,			/* Id,  0 if a plugin		*/
	create_clock,		/* The create function		*/
	update_clock,		/* The update function		*/
	create_clock_tab,	/* The config tab create function	*/
	NULL,				/* Instant apply	*/

	save_clock_cal_config,	/* Save user conifg			*/
	load_clock_cal_config,	/* Load user config			*/
	CLOCK_CONFIG_KEYWORD, /* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_clock_monitor(void)
	{
	clock_enable = TRUE;
    monitor_clock.name = _(monitor_clock.name);
    loop_chime_enable   = FALSE;
    hour_chime_command = g_strdup("");
    quarter_chime_command = g_strdup("");

	gkrellm_dup_string(&clock_format, DEFAULT_CLOCK_FORMAT);

	clock_style_id = gkrellm_add_meter_style(&monitor_clock, CLOCK_STYLE_NAME);
	mon_clock = &monitor_clock;
	return &monitor_clock;
	}

static GkrellmMonitor	monitor_cal =
	{
	N_("Calendar"),	/* Name, for config tab.	*/
	MON_CAL,		/* Id,  0 if a plugin		*/
	create_calendar, /* The create function		*/
	NULL,			/* The update function		*/
	NULL,			/* The config tab create function	*/
	NULL,			/* Apply the config function		*/

	NULL,			/* Save user conifg			*/
	NULL,			/* Load user config			*/
	NULL,			/* config keyword			*/

	NULL,			/* Undef 2	*/
	NULL,			/* Undef 1	*/
	NULL,			/* Undef 0	*/

	0,				/* insert_before_id - place plugin before this mon */

	NULL,			/* Handle if a plugin, filled in by GKrellM		*/
	NULL			/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_cal_monitor(void)
	{
	cal_enable = TRUE;
    monitor_cal.name = _(monitor_cal.name);

	gkrellm_dup_string(&cal_format, DEFAULT_CAL_FORMAT);

	cal_style_id = gkrellm_add_meter_style(&monitor_cal, CAL_STYLE_NAME);
	mon_cal = &monitor_cal;
	return &monitor_cal;
	}

