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
#include "log-private.h"

// GTK+-win32 automatically uses the application-icon
#if GTK_CHECK_VERSION(2,4,0) && !defined(WIN32)
#include "icon.xpm"
#endif

#include <signal.h>


struct GkrellmConfig	_GK;
GkrellmTicks			GK;

struct
	{
	GtkWidget	*window;		/* Top level window	*/
	GtkWidget	*vbox;

	GtkWidget	*top0_event_box;	/* Top frame event box */
	GtkWidget	*top0_vbox;			/* Top frame			*/

	GtkWidget	*middle_hbox;
	  GtkWidget	*left_event_box;	/* Holds left vbox.		*/
	  GtkWidget	*left_vbox;			/* Holds left frame.	*/

	  GtkWidget	*middle_vbox;		/* A handle for sliding shut */
	  GtkWidget	*monitor_vbox;		/* All monitors go in here.	*/
		GtkWidget	*top1_event_box;	/* First event box */
		GtkWidget	*top1_vbox;			/* hostname	*/

	  GtkWidget	*right_event_box;	/* Holds right vbox.	*/
	  GtkWidget	*right_vbox;		/* Holds right frame.	*/

	GtkWidget	*bottom_vbox;	/* Bottom frame	*/

	GdkPixmap	*frame_left_pixmap;
	GdkBitmap	*frame_left_mask;
	GdkPixmap	*frame_right_pixmap;
	GdkBitmap	*frame_right_mask;

	GdkPixmap	*frame_top_pixmap;
	GdkBitmap	*frame_top_mask;
	GdkPixmap	*frame_bottom_pixmap;
	GdkBitmap	*frame_bottom_mask;

	GdkPixmap	*window_transparency_mask;
	}
	gtree;


static GtkWidget *top_window;
GList			*gkrellm_monitor_list;
time_t			gkrellm_time_now;

static GtkUIManager	*ui_manager;

static gchar	*geometry;

static gint		y_pack;

static gint		monitor_previous_height;
static gint		monitors_visible	= TRUE;
static gint		mask_monitors_visible = TRUE;
static gint		check_rootpixmap_transparency;

static gboolean	no_transparency,
				do_intro,
				decorated,
				configure_position_lock;

static void		apply_frame_transparency(gboolean force);


#define	N_FALLBACK_FONTS	3

static gchar	*fail_large_font[N_FALLBACK_FONTS] =
{
"Serif 10",
"Ariel 10",
"fixed"
};

static gchar	*fail_normal_font[N_FALLBACK_FONTS] =
{
"Serif 9",
"Ariel 9",
"fixed"
};

static gchar	*fail_small_font[N_FALLBACK_FONTS] =
{
"Serif 8",
"Ariel 8",
"fixed"
};


static gchar	*intro_msg =
N_(	"You can configure your monitors by right clicking on\n"
	"the top frame of GKrellM or by hitting the F1 key\n"
	"with the mouse in the GKrellM window.\n\n"
	"Read the Info pages in the config for setup help.");


static void
load_font(gchar *font_string, PangoFontDescription **gk_font,
				gchar **fallback_fonts)
	{
	PangoFontDescription	*font_desc = NULL;
	gint		i;

	if (font_string)
		font_desc = pango_font_description_from_string(font_string);
	gkrellm_debug(DEBUG_GUI, "load_font: %s %p\n", font_string, font_desc);

	if (!font_desc)
		{
		for (i = 0; !font_desc && i < N_FALLBACK_FONTS; ++i)
			{
			font_desc = pango_font_description_from_string(fallback_fonts[i]);
			gkrellm_debug(DEBUG_GUI, "load_font trying fallback: %s\n",
						fallback_fonts[i]);
			}
		}
	if (*gk_font)
		pango_font_description_free(*gk_font);
	*gk_font = font_desc;
	}

static void
setup_fonts()
	{
	load_font(_GK.large_font_string, &_GK.large_font, fail_large_font);
	load_font(_GK.normal_font_string, &_GK.normal_font, fail_normal_font);
	load_font(_GK.small_font_string, &_GK.small_font, fail_small_font);
	if (!_GK.large_font || !_GK.normal_font || !_GK.small_font)
		{
		g_print(_("Error: Could not load all fonts.\n"));
		exit(0);
		}
	_GK.font_load_count += 1;
	}

void
gkrellm_map_color_string(gchar *color_string, GdkColor *color)
	{
	static GdkColormap	*colormap;

	if (colormap == NULL)
		colormap = gtk_widget_get_colormap(top_window);
	if (color->red || color->green || color->blue)
		gdk_colormap_free_colors(colormap, color, 1);
	if (!color_string)
		color_string = "black";
	gdk_color_parse(color_string, color);
	gdk_colormap_alloc_color(colormap, color, FALSE, TRUE);
	}

static void
setup_colors()
	{
	GtkWidget	*win	= top_window;

	gkrellm_map_color_string(_GK.chart_in_color, &_GK.in_color);
	gkrellm_map_color_string(_GK.chart_in_color_grid, &_GK.in_color_grid);

	gkrellm_map_color_string(_GK.chart_out_color, &_GK.out_color);
	gkrellm_map_color_string(_GK.chart_out_color_grid, &_GK.out_color_grid);

	gkrellm_map_color_string("#000000", &_GK.background_color);
	gkrellm_map_color_string("#FFFFFF", &_GK.white_color);

	if (_GK.draw1_GC == NULL)
		{
		_GK.draw1_GC = gdk_gc_new( win->window );
		gdk_gc_copy( _GK.draw1_GC, win->style->white_gc );
		}
	if (_GK.draw2_GC == NULL)
		{
		_GK.draw2_GC = gdk_gc_new( win->window );
		gdk_gc_copy( _GK.draw2_GC, win->style->white_gc );
		}
	if (_GK.draw3_GC == NULL)
		{
		_GK.draw3_GC = gdk_gc_new( win->window );
		gdk_gc_copy( _GK.draw3_GC, win->style->white_gc );
		}
	if (_GK.draw_stencil_GC == NULL)
		{
		_GK.draw_stencil_GC = gdk_gc_new( win->window );
		gdk_gc_copy(_GK.draw_stencil_GC, win->style->white_gc );
		}
	if (_GK.text_GC == NULL)
		{
		_GK.text_GC = gdk_gc_new( win->window );
		gdk_gc_copy( _GK.text_GC, win->style->white_gc );
		}

	/* Set up the depth 1 GCs
	*/
	/* g_print("white pixel = %ld\n", _GK.white_color.pixel); */
	if (_GK.bit1_GC == NULL)
		{
		GdkBitmap	*dummy_bitmap;
		GdkColor	bit_color;

		dummy_bitmap = gdk_pixmap_new(top_window->window, 16, 16, 1);
		_GK.bit1_GC = gdk_gc_new(dummy_bitmap);
		_GK.bit0_GC = gdk_gc_new(dummy_bitmap);
		bit_color.pixel = 1;
		gdk_gc_set_foreground(_GK.bit1_GC, &bit_color);
		bit_color.pixel = 0;
		gdk_gc_set_foreground(_GK.bit0_GC, &bit_color);
		g_object_unref(G_OBJECT(dummy_bitmap));
		}
	}


static gint	save_position_countdown;

static void
set_or_save_position(gint save)
	{
	FILE		*f;
	gchar		*path;
	gint		x, y;
	static gint	x_last = -1,
				y_last = -1;

	path = gkrellm_make_data_file_name(NULL, "startup_position");
	if (save)
		{
		if (!configure_position_lock && y_pack < 0)
			{	/* Most recent configure event may have happened before
				|  gkrellm has removed it's lock, so get gdk's cached values.
				|  Eg. gkrellm is moved < 2 sec after startup by the window
				|  manager.  See cb_configure_notify().
				|  But don't update _GK.y_position if current gkrellm position
				|  reflects a packed move (ie, not a user set position).
				*/
			gdk_window_get_position(top_window->window,
					&_GK.x_position, &_GK.y_position);
			}
		if (   !_GK.no_config
		    && (_GK.x_position != x_last || _GK.y_position != y_last)
			&& (f = g_fopen(path, "w")) != NULL
		   )
			{
			x_last = _GK.x_position;
			y_last = _GK.y_position;
			fprintf(f, "%d %d\n", _GK.x_position, _GK.y_position);
			fclose(f);
			gkrellm_debug(DEBUG_POSITION, "save_position: %d %d\n", x_last, y_last);
			}
		save_position_countdown = 0;
		}
	else if (!_GK.withdrawn)  /* In slit conflicts with setting position */
		{
		if ((f = g_fopen(path, "r")) != NULL)
			{
			x = y = 0;
			fscanf(f, "%d %d", &x, &y);
			fclose(f);

			if (   x >= 0 && x < _GK.w_display - 10
				&& y >= 0 && y < _GK.h_display - 25
			   )
				{
				_GK.x_position = x_last = x;
				_GK.y_position = y_last = y;
				_GK.position_valid = TRUE;
				gdk_window_move(gtree.window->window, x, y);
				gkrellm_debug(DEBUG_POSITION, "startup_position moveto %d %d (valid)\n", x, y);
				}
			}
		}
	g_free(path);
	}


static gint
update_monitors()
	{
	GList			*list;
	GkrellmMonitor			*mon;
	struct tm		*pCur, *pPrev;
	static time_t	time_prev;

	time(&_GK.time_now);
	GK.second_tick = (_GK.time_now == time_prev) ? FALSE : TRUE;
	time_prev = _GK.time_now;

	if (GK.second_tick)
		{
		pPrev = &gkrellm_current_tm;
		pCur = localtime(&_GK.time_now);
		GK.two_second_tick  = ((pCur->tm_sec % 2) == 0) ? TRUE : FALSE;
		GK.five_second_tick = ((pCur->tm_sec % 5) == 0) ? TRUE : FALSE;
		GK.ten_second_tick  = ((pCur->tm_sec % 10) == 0) ? TRUE : FALSE;
		GK.minute_tick = (pCur->tm_min  != pPrev->tm_min)  ? TRUE : FALSE;
		GK.hour_tick   = (pCur->tm_hour != pPrev->tm_hour) ? TRUE : FALSE;
		GK.day_tick    = (pCur->tm_mday != pPrev->tm_mday) ? TRUE : FALSE;

		/* Copy localtime() data to my global struct tm data so clock (or
		|  anybody) has access to current struct tm data.  Must copy so
		|  data is not munged by plugins which might call localtime().
		*/
		gkrellm_current_tm = *pCur;
		}
	else
		{
		GK.two_second_tick = FALSE;
		GK.five_second_tick = FALSE;
		GK.ten_second_tick = FALSE;
		GK.minute_tick = FALSE;
		GK.hour_tick = FALSE;
		GK.day_tick = FALSE;
		}

	gkrellm_alert_update();
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		gkrellm_record_state(UPDATE_MONITOR, mon);
		if (mon->update_monitor && mon->privat->enabled)
			(*(mon->update_monitor))();
		}
	gkrellm_record_state(INTERNAL, NULL);
	++GK.timer_ticks;

	if (save_position_countdown > 0 && --save_position_countdown == 0)
		set_or_save_position(1);
	if (configure_position_lock && (GK.timer_ticks / _GK.update_HZ) > 1)
		configure_position_lock = FALSE;


	/* Update if background has changed */
	if (   GK.second_tick && !no_transparency
		&& gkrellm_winop_updated_background()
		&& check_rootpixmap_transparency == 0
	   )
		check_rootpixmap_transparency = 1;

	if (_GK.need_frame_packing)
		{
		gkrellm_pack_side_frames();
		apply_frame_transparency(TRUE);
		if (!no_transparency)
			gkrellm_winop_apply_rootpixmap_transparency();
		check_rootpixmap_transparency = 0;
		}

	if (check_rootpixmap_transparency > 0)
		if (--check_rootpixmap_transparency == 0 && !no_transparency)
			gkrellm_winop_apply_rootpixmap_transparency();

	if (   GK.minute_tick && _GK.config_modified
		&& !gkrellm_config_window_shown()
	   )
		gkrellm_save_user_config();

	return TRUE;			/* restarts timeout */
	}

void
gkrellm_start_timer(gint Hz)
	{
	static guint	timeout_id	= 0;
	gint		interval;

	if (timeout_id)
		g_source_remove(timeout_id);
	timeout_id = 0;
	if (Hz > 0)
		{
		interval = 1000 / Hz;
		interval = interval * 60 / 63;	/* Compensate for overhead XXX */
		timeout_id = g_timeout_add(interval,
						(GtkFunction) update_monitors,NULL);
		}
	}

GtkWidget *
gkrellm_get_top_window()
	{
	return top_window;
	}

GkrellmTicks *
gkrellm_ticks(void)
	{
	return &GK;
	}

gint
gkrellm_get_timer_ticks(void)
	{
	return GK.timer_ticks;
	}


  /* Nice set of effects I have here...  Either shadow effect or none.
  |  Returning 1 means shadow effect and the return value is also used by
  |  callers to increment height fields to allow for the offset shadow draw.
  */
gint
gkrellm_effect_string_value(gchar *effect_string)
	{
	if (effect_string)
		return (strcmp(effect_string, "shadow") == 0) ? 1 : 0;
	return 0;
	}

  /* Before 1.0.3 text, decals, krells used top/bottom borders for placement.
  |  This is to allow themes to transition to using top/bottom_margin.
  */
void
gkrellm_get_top_bottom_margins(GkrellmStyle *style, gint *top, gint *bottom)
	{
	gint	t = 0,
			b = 0;

	if (style)
		{
		if (   _GK.use_top_bottom_margins		/* XXX */
			|| g_list_find(_GK.chart_style_list, style)
		   )
			{
			t = style->margin.top;
			b = style->margin.bottom;
			}
		else
			{
			t = style->border.top * _GK.theme_scale / 100;
			b = style->border.bottom * _GK.theme_scale / 100;
			}
		}
	if (top)
		*top = t;
	if (bottom)
		*bottom = b;
	}

GkrellmMargin *
gkrellm_get_style_margins(GkrellmStyle *style)
	{
	static GkrellmMargin	m_default = {0,0,0,0};

	if (!style)
		return &m_default;
	gkrellm_get_top_bottom_margins(style,
				&style->margin.top, &style->margin.bottom);		/* XXX */
	return &style->margin;
	}

void
gkrellm_set_style_margins(GkrellmStyle *style, GkrellmMargin *margin)
	{
	if (style && margin)
		style->margin = *margin;
	}

void
gkrellm_draw_string(GdkDrawable *drawable, GkrellmTextstyle *ts,
			gint x, gint y, gchar *s)
	{
	if (!drawable || !ts || !s)
		return;
	if (ts->effect)
		{
		gdk_gc_set_foreground(_GK.text_GC, &ts->shadow_color);
		gkrellm_gdk_draw_string(drawable, ts->font, _GK.text_GC,
					x + 1, y + 1, s);
		}
	gdk_gc_set_foreground(_GK.text_GC, &ts->color);
	gkrellm_gdk_draw_string(drawable, ts->font, _GK.text_GC, x, y, s);
	}

void
gkrellm_draw_text(GdkDrawable *drawable, GkrellmTextstyle *ts, gint x, gint y,
			gchar *s, gint len)
	{
	if (!drawable || !ts || !s)
		return;
	if (ts->effect)
		{
		gdk_gc_set_foreground(_GK.text_GC, &ts->shadow_color);
		gkrellm_gdk_draw_text(drawable, ts->font, _GK.text_GC,
					x + 1, y + 1, s, len);
		}
	gdk_gc_set_foreground(_GK.text_GC, &ts->color);
	gkrellm_gdk_draw_text(drawable, ts->font, _GK.text_GC, x, y, s, len);
	}

gint
gkrellm_label_x_position(gint position, gint w_field, gint w_label,
				gint margin)
	{
	gint	x;

	x = w_field * position / GKRELLM_LABEL_MAX;
	x -= w_label / 2;
	if (x > w_field - w_label - margin)
		x = w_field - w_label - margin;
	if (x < margin)
		x = margin;
	return x;
	}

/* ---------------------------------------------------------------------- */
#define	RESISTANCE_PIXELS	35

static gint		moving_gkrellm	= FALSE;
static gint		x_press_event, y_press_event;
static gint		gkrellm_width, gkrellm_height;

void
gkrellm_motion(GtkWidget *widget, GdkEventMotion *ev, gpointer data)
	{
	gint			x_pointer, y_pointer, x, y, right_zone, bottom_zone;
	GdkModifierType	m;

	if (moving_gkrellm)
		{
		m = ev->state;
		if (!(m & GDK_BUTTON1_MASK))
			{
			moving_gkrellm = FALSE;
			return;
			}
		/* Catch up to the pointer so GKrellM does not lag the pointer motion.
		*/
		gkrellm_winop_flush_motion_events();
		gdk_window_get_pointer(NULL, &x_pointer, &y_pointer, &m);

		/* Subtract press event coordinates to account for pointer offset
		|  into top_window.  Have edge resistance to the move.
		*/
		x = x_pointer - x_press_event;
		y = y_pointer - y_press_event;
		right_zone = _GK.w_display - gkrellm_width;
		bottom_zone = _GK.h_display - gkrellm_height;

		if (_GK.x_position >= 0 && x < 0 && x > -RESISTANCE_PIXELS)
			x = 0;
		if (   _GK.x_position <= right_zone
			&& x > right_zone && x < right_zone + RESISTANCE_PIXELS
		   )
			x = right_zone;
		if (_GK.y_position >= 0 && y < 0 && y > -RESISTANCE_PIXELS)
			y = 0;
		if (   _GK.y_position <= bottom_zone
			&& y > bottom_zone && y < bottom_zone + RESISTANCE_PIXELS
		   )
			y = bottom_zone;

		/* Moves to x,y position relative to root.
		*/
		_GK.y_position = y;
		_GK.x_position = x;
		_GK.position_valid = TRUE;
		gdk_window_move(top_window->window, x, y);
		}
	}

void
gkrellm_menu_popup(void)
	{
            gtk_menu_popup(GTK_MENU(gtk_ui_manager_get_widget(ui_manager, "/popup")), NULL, NULL, NULL, NULL,
					0, gtk_get_current_event_time());
	}

static void
top_frame_button_release(GtkWidget *widget, GdkEventButton *ev, gpointer data)
	{
	if (!no_transparency)
		gkrellm_winop_apply_rootpixmap_transparency();
	moving_gkrellm = FALSE;
	gkrellm_debug(DEBUG_POSITION, "gkrellm moveto: x_pos=%d y_pos=%d\n",
				_GK.x_position, _GK.y_position);
	}

static gboolean
top_frame_button_press(GtkWidget *widget, GdkEventButton *ev, gpointer data)
	{
	gint			x_pointer, y_pointer;
	GdkModifierType	m;
	time_t			time_check;

	gdk_window_get_pointer(NULL, &x_pointer, &y_pointer, &m);
	_GK.w_display = gdk_screen_get_width(gdk_screen_get_default());
	_GK.h_display = gdk_screen_get_height(gdk_screen_get_default());
	gkrellm_width = _GK.chart_width
				+ _GK.frame_left_width + _GK.frame_right_width;
	gkrellm_height = _GK.monitor_height + _GK.total_frame_height;

	/* If system time is changed, gtk_timeout_add() setting gets
	|  confused, so put some duct tape here ...
	*/
	time(&time_check);
	if (time_check > _GK.time_now + 2 || time_check < _GK.time_now)
		gkrellm_start_timer(_GK.update_HZ);

	if (ev->button == 3)
		{
		gtk_menu_popup(GTK_MENU(gtk_ui_manager_get_widget(ui_manager, "/popup")), NULL, NULL, NULL, NULL,
					ev->button, ev->time);
		return FALSE;
		}
	gtk_window_present(GTK_WINDOW(top_window));

	if (   _GK.client_mode
	    && gkrellm_client_server_connect_state() == DISCONNECTED
	   )
		gkrellm_client_mode_connect_thread();

	if (!_GK.withdrawn)		/* Move window unless in the slit */
		{
		/* I need pointer coords relative to top_window.  So, add in offsets
		|  to hostname window if that is where we pressed.
		*/
		x_press_event = ev->x;
		y_press_event = ev->y;
		if (widget == gtree.top1_event_box)
			{
			x_press_event += _GK.frame_left_width;
			y_press_event += _GK.frame_top_height;
			}
		moving_gkrellm = TRUE;
		configure_position_lock = FALSE;
		}
	return FALSE;
	}

#define	CLOSE_LEFT	0
#define	CLOSE_RIGHT	1


  /* If any frame images have transparency (masks != NULL) use the
  |  frame masks to construct a main window sized mask.
  */
static void
apply_frame_transparency(gboolean force)
	{
	GtkWidget			*win;
	gint				w, h;
	static gint			w_prev, h_prev;

	if (decorated)
		return;

	win = gtree.window;
	w = _GK.chart_width + _GK.frame_left_width + _GK.frame_right_width;
	h = _GK.monitor_height + _GK.total_frame_height;
	if (!gtree.window_transparency_mask || w != w_prev || h != h_prev)
		{
		if (gtree.window_transparency_mask)
			g_object_unref(G_OBJECT(gtree.window_transparency_mask));
		gtree.window_transparency_mask = gdk_pixmap_new(win->window, w, h, 1);
		w_prev = w;
		h_prev = h;
		}
	else if (!force && monitors_visible == mask_monitors_visible)
		return;

	/* Set entire shape mask to 1, then write in the frame masks.
	*/
	gdk_draw_rectangle(gtree.window_transparency_mask, _GK.bit1_GC, TRUE,
				0, 0, w, h);

	if (monitors_visible)
		{
		if (gtree.frame_top_mask)
			gdk_draw_drawable(gtree.window_transparency_mask, _GK.bit1_GC,
				gtree.frame_top_mask, 0, 0,  0, 0,  w, _GK.frame_top_height);
		if (gtree.frame_bottom_mask)
			gdk_draw_drawable(gtree.window_transparency_mask, _GK.bit1_GC,
				gtree.frame_bottom_mask,
				0, 0, 0, h - _GK.frame_bottom_height,
				w, _GK.frame_bottom_height);

		if (gtree.frame_left_mask)
			gdk_draw_drawable(gtree.window_transparency_mask, _GK.bit1_GC,
				gtree.frame_left_mask,
				0, 0,  0, _GK.frame_top_height,
				_GK.frame_left_width, _GK.monitor_height);

		if (gtree.frame_right_mask)
			gdk_draw_drawable(gtree.window_transparency_mask, _GK.bit1_GC,
				gtree.frame_right_mask,
				0, 0,  w - _GK.frame_right_width, _GK.frame_top_height,
				_GK.frame_right_width, _GK.monitor_height);
		}
	else	/* Top and bottom frames are not visible and GKrellM is shut */
		{
		if (gtree.frame_left_mask)
			gdk_draw_drawable(gtree.window_transparency_mask, _GK.bit1_GC,
				gtree.frame_left_mask,
				0, 0,  0, 0,
				_GK.frame_left_width, _GK.monitor_height);

		if (gtree.frame_right_mask)
			gdk_draw_drawable(gtree.window_transparency_mask, _GK.bit1_GC,
				gtree.frame_right_mask,
				0, 0,  _GK.frame_left_width, 0,
				_GK.frame_right_width, _GK.monitor_height);
		}

	gtk_widget_shape_combine_mask(gtree.window,
						gtree.window_transparency_mask, 0, 0);
	mask_monitors_visible = monitors_visible;
	}

static gboolean
side_frame_button_press(GtkWidget *widget, GdkEventButton *ev, gpointer data)
	{
	static gint		direction;
	gint			x_gkrell, y_gkrell;

	if (ev->button == 3)
		{
		gtk_menu_popup(GTK_MENU(gtk_ui_manager_get_widget(ui_manager, "/popup")), NULL, NULL, NULL, NULL,
					ev->button, ev->time);
		return FALSE;
		}
	if (decorated || _GK.withdrawn)
		return FALSE;
	gdk_window_get_origin(gtree.window->window, &x_gkrell, &y_gkrell);
	direction = (x_gkrell < _GK.w_display / 2) ? CLOSE_LEFT : CLOSE_RIGHT;

	if (ev->button == 2 || (_GK.m2 && ev->button == 1))
		{
		monitors_visible = !monitors_visible;
		if (monitors_visible)
			{
			gtk_widget_show(gtree.middle_vbox);
			gtk_widget_show(gtree.top0_vbox);
			gtk_widget_show(gtree.bottom_vbox);
			while (gtk_events_pending())
				gtk_main_iteration ();
			if (direction == CLOSE_RIGHT)
				gdk_window_move(gtree.window->window,
								x_gkrell - _GK.chart_width, y_gkrell);
			}
		else
			{
			gtk_widget_hide(gtree.top0_vbox);
			gtk_widget_hide(gtree.bottom_vbox);
			gtk_widget_hide(gtree.middle_vbox);
			while (gtk_events_pending())
				gtk_main_iteration ();
			if (direction == CLOSE_RIGHT)
				gdk_window_move(gtree.window->window,
								x_gkrell + _GK.chart_width, y_gkrell);
			}
		gdk_flush();  /* Avoid double click race */
		}
	return FALSE;
	}


static void
create_frame_top(GtkWidget *vbox)
	{
	static GtkWidget	*ft_vbox;
	static GtkWidget	*ft_image;
	GkrellmPiximage		*im;
	gint				w, h;

	if (decorated)
		return;
	im = _GK.frame_top_piximage;

	w = _GK.chart_width + _GK.frame_left_width + _GK.frame_right_width;
	h = (_GK.frame_top_height > 0) ? _GK.frame_top_height
				: gdk_pixbuf_get_height(im->pixbuf);
//	h = h * _GK.theme_scale / 100;

	_GK.frame_top_height = h;

	_GK.total_frame_height += h;
	gkrellm_scale_piximage_to_pixmap(im,
				&gtree.frame_top_pixmap, &gtree.frame_top_mask, w, h);

	if (!ft_vbox)
		{
		ft_vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(vbox), ft_vbox);
		gtk_widget_show(ft_vbox);

		ft_image = gtk_image_new_from_pixmap(gtree.frame_top_pixmap, NULL);
		gtk_box_pack_start(GTK_BOX(ft_vbox), ft_image, FALSE, FALSE, 0);
		gtk_widget_show(ft_image);
		}
	else
		gtk_image_set_from_pixmap(GTK_IMAGE(ft_image),
				gtree.frame_top_pixmap, NULL);
	}

static void
create_frame_bottom(GtkWidget *vbox)
	{
	static GtkWidget	*fb_vbox;
	static GtkWidget	*fb_image;
	GkrellmPiximage		*im;
	gint				w, h;

	if (decorated)
		return;
	im = _GK.frame_bottom_piximage;

	w = _GK.chart_width + _GK.frame_left_width + _GK.frame_right_width;
	h = (_GK.frame_bottom_height > 0) ? _GK.frame_bottom_height
				: gdk_pixbuf_get_height(im->pixbuf);
//	h = h * _GK.theme_scale / 100;

	_GK.frame_bottom_height = h;

	_GK.total_frame_height += h;
	gkrellm_scale_piximage_to_pixmap(im,
				&gtree.frame_bottom_pixmap, &gtree.frame_bottom_mask, w, h);
	if (fb_vbox == NULL)
		{
		fb_vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(vbox), fb_vbox);
		gtk_widget_show(fb_vbox);

		fb_image = gtk_image_new_from_pixmap(gtree.frame_bottom_pixmap, NULL);
		gtk_box_pack_start(GTK_BOX(fb_vbox), fb_image, FALSE, FALSE, 0);
		gtk_widget_show(fb_image);
		}
	else
		gtk_image_set_from_pixmap(GTK_IMAGE(fb_image),
				gtree.frame_bottom_pixmap, NULL);
	}

static void
frame_transparency_warning(void)
    {
    static gint warned = -1;

	if (warned != _GK.theme_reload_count && _GK.command_line_theme)
		fprintf(stderr,
"Warning: frame overlap transparency may not work.  If any of the frame,\n"
"         chart, panel, spacer, or cap images have transparency, then all\n"
"         may need transparency.\n");
	warned = _GK.theme_reload_count;
	}

static void
draw_cap(GkrellmPiximage *piximage, gint y_mon, gint h_mon, gint h_spacer,
		gboolean top, gboolean left)
	{
	GdkPixmap	*pixmap = NULL;
	GdkBitmap	*mask = NULL;
	GdkBitmap	*frame_mask;
	gint		y, x_dst, h_pix, w_pix, h_cap;

	if (!piximage || !piximage->pixbuf)
		return;
	h_pix = gdk_pixbuf_get_height(piximage->pixbuf);
	w_pix = gdk_pixbuf_get_width(piximage->pixbuf);
//	h_pix = h_pix * _GK.theme_scale / 100;
//	w_pix = w_pix * _GK.theme_scale / 100;

	y = top ? y_mon + h_spacer : y_mon + h_mon - h_pix - h_spacer;
	if (y < y_mon)
		y = y_mon;

	h_cap = (y + h_pix <= y_mon + h_mon) ? h_pix : y_mon + h_mon - y;
	if (h_cap <= 0)
		return;

	x_dst = left ? 0 : _GK.frame_right_width - w_pix;
	gkrellm_scale_piximage_to_pixmap(piximage, &pixmap, &mask, w_pix, h_pix);
	gdk_draw_drawable(
				left ? gtree.frame_left_pixmap : gtree.frame_right_pixmap,
				_GK.draw1_GC, pixmap, 0, 0, x_dst, y, w_pix, h_cap);

	frame_mask = left ? gtree.frame_left_mask : gtree.frame_right_mask;
	if (mask && frame_mask)
		gdk_draw_drawable(frame_mask, _GK.bit1_GC, mask,
				0, 0, x_dst, y, w_pix, h_cap);
	else if (mask || frame_mask)
		frame_transparency_warning();

	gkrellm_free_pixmap(&pixmap);
	gkrellm_free_bitmap(&mask);
	}

static void
draw_frame_caps(GkrellmMonitor *mon)
	{
	GkrellmMonprivate	*mp = mon->privat;
	gint				y, h, h_left, h_right;

	if (!mp->main_vbox || mp->cap_images_off)
		return;
	y = mp->main_vbox->allocation.y;
	h = mp->main_vbox->allocation.height;
	if (mon != gkrellm_mon_host())
		y -= _GK.frame_top_height;

	h_left = h_right = 0;
	if (mp->top_spacer.piximage && _GK.frame_left_spacer_overlap)
		h_left = mp->top_spacer.height;
	if (mp->top_spacer.piximage && _GK.frame_right_spacer_overlap)
		h_right = mp->top_spacer.height;

	if (mp->top_type == GKRELLM_SPACER_CHART)
		{
		draw_cap(_GK.cap_top_left_chart_piximage, y, h, h_left, 1, 1);
		draw_cap(_GK.cap_top_right_chart_piximage, y, h, h_right, 1, 0);
		}
	else
		{
		draw_cap(_GK.cap_top_left_meter_piximage, y, h, h_left, 1, 1);
		draw_cap(_GK.cap_top_right_meter_piximage, y, h, h_right, 1, 0);
		}

	h_left = h_right = 0;
	if (mp->bottom_spacer.piximage && _GK.frame_left_spacer_overlap)
		h_left = mp->bottom_spacer.height;
	if (mp->bottom_spacer.piximage && _GK.frame_right_spacer_overlap)
		h_right = mp->bottom_spacer.height;

	if (mp->bottom_type == GKRELLM_SPACER_CHART)
		{
		draw_cap(_GK.cap_bottom_left_chart_piximage, y, h, h_left, 0, 1);
		draw_cap(_GK.cap_bottom_right_chart_piximage, y, h, h_right, 0, 0);
		}
	else
		{
		draw_cap(_GK.cap_bottom_left_meter_piximage, y, h, h_left, 0, 1);
		draw_cap(_GK.cap_bottom_right_meter_piximage, y, h, h_right, 0, 0);
		}
	}

static void
draw_left_frame_overlap(GdkPixbuf *pixbuf, GkrellmBorder *border,
			gint y_frame, gint overlap, gint height)
	{
	GkrellmPiximage		piximage;
	GdkPixmap			*pixmap = NULL;
	GdkBitmap			*mask = NULL;
	gint				h_pixbuf;

	if (overlap <= 0)
		return;
	h_pixbuf = gdk_pixbuf_get_height(pixbuf);
	piximage.pixbuf = gdk_pixbuf_new_subpixbuf(pixbuf, 0, 0,
						overlap, h_pixbuf);
	piximage.border = *border;
	gkrellm_scale_piximage_to_pixmap(&piximage, &pixmap, &mask,
						overlap, height);
	g_object_unref(G_OBJECT(piximage.pixbuf));
	gdk_draw_drawable(gtree.frame_left_pixmap, _GK.draw1_GC, pixmap,
					0, 0, _GK.frame_left_width - overlap, y_frame,
					overlap, height);
	if (mask && gtree.frame_left_mask)
		gdk_draw_drawable(gtree.frame_left_mask, _GK.bit1_GC, mask,
					0, 0, _GK.frame_left_width - overlap, y_frame,
					overlap, height);
	else if (mask || gtree.frame_left_mask)
		frame_transparency_warning();

	gkrellm_free_pixmap(&pixmap);
	gkrellm_free_bitmap(&mask);
	}

static void
draw_right_frame_overlap(GdkPixbuf *pixbuf, GkrellmBorder *border,
			gint y_frame, gint overlap, gint height)
	{
	GkrellmPiximage		piximage;
	GdkPixmap			*pixmap = NULL;
	GdkBitmap			*mask = NULL;
	gint				w_pixbuf, h_pixbuf;

	if (overlap <= 0)
		return;
	w_pixbuf = gdk_pixbuf_get_width(pixbuf);
	h_pixbuf = gdk_pixbuf_get_height(pixbuf);
	piximage.pixbuf = gdk_pixbuf_new_subpixbuf(pixbuf,
						w_pixbuf - overlap, 0, overlap, h_pixbuf);
	piximage.border = *border;
	gkrellm_scale_piximage_to_pixmap(&piximage, &pixmap, &mask,
					overlap, height);
	g_object_unref(G_OBJECT(piximage.pixbuf));
	gdk_draw_drawable(gtree.frame_right_pixmap, _GK.draw1_GC, pixmap,
					0, 0, 0, y_frame, overlap, height);
	if (mask && gtree.frame_right_mask)
		gdk_draw_drawable(gtree.frame_right_mask, _GK.bit1_GC, mask,
					0, 0, 0, y_frame, overlap, height);
	else if (mask || gtree.frame_right_mask)
		frame_transparency_warning();

	gkrellm_free_pixmap(&pixmap);
	gkrellm_free_bitmap(&mask);
	}

static void
draw_frame_overlaps(void)
	{
	GList				*list;
	GkrellmMonitor		*mon;
	GkrellmMonprivate	*mp;
	GkrellmChart		*cp;
	GkrellmPanel		*p;
	GkrellmMargin		*m;
	gint				y;
	static GkrellmBorder zero_border;

	for (list = gkrellm_get_chart_list(); list; list = list->next)
		{
		cp = (GkrellmChart *) list->data;
		mon = (GkrellmMonitor *) cp->monitor;
		if (!mon->privat->enabled || !cp->shown || cp->y_mapped < 0)
			continue;

		m = gkrellm_get_style_margins(cp->style);
		y = cp->y_mapped - _GK.frame_top_height;

		draw_left_frame_overlap(cp->bg_piximage->pixbuf,
					&_GK.frame_left_chart_border, y - m->top,
					_GK.frame_left_chart_overlap, cp->h + m->top + m->bottom);
		draw_right_frame_overlap(cp->bg_piximage->pixbuf,
					&_GK.frame_right_chart_border, y - m->top,
					_GK.frame_right_chart_overlap, cp->h + m->top + m->bottom);
		}
	for (list = gkrellm_get_panel_list(); list; list = list->next)
		{
		p = (GkrellmPanel *) list->data;
		mon = (GkrellmMonitor *) p->monitor;
		if (!mon->privat->enabled || !p->shown || p->y_mapped < 0)
			continue;
		y = p->y_mapped;

		/* Special case:  hostname is in an event box and its y_mapped is
		|  relative to that event box window, or 0.  All other panels/charts
		|  are relative to the top level window which includes the top frame.
		*/
		if (mon != gkrellm_mon_host())
			y -= _GK.frame_top_height;

		draw_left_frame_overlap(p->bg_piximage->pixbuf,
					&_GK.frame_left_panel_border, y,
					_GK.frame_left_panel_overlap, p->h);
		draw_right_frame_overlap(p->bg_piximage->pixbuf,
					&_GK.frame_right_panel_border, y,
					_GK.frame_right_panel_overlap, p->h);
		}
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		mp = mon->privat;
		if (!mp->enabled || !mp->spacers_shown)
			continue;
		if (mp->top_spacer.image && !mp->spacer_overlap_off)
			{
			y = mp->top_spacer.image->allocation.y;
			if (mon != gkrellm_mon_host())
				y -= _GK.frame_top_height;
			draw_left_frame_overlap(mp->top_spacer.piximage->pixbuf,
					&zero_border, y,
					_GK.frame_left_spacer_overlap, mp->top_spacer.height);
			draw_right_frame_overlap(mp->top_spacer.piximage->pixbuf,
					&zero_border, y,
					_GK.frame_right_spacer_overlap, mp->top_spacer.height);
			}
		if (mp->bottom_spacer.image && !mp->spacer_overlap_off)
			{
			y = mp->bottom_spacer.image->allocation.y;
			if (mon != gkrellm_mon_host())
				y -= _GK.frame_top_height;
			draw_left_frame_overlap(mp->bottom_spacer.piximage->pixbuf,
					&zero_border, y,
					_GK.frame_left_spacer_overlap, mp->bottom_spacer.height);
			draw_right_frame_overlap(mp->bottom_spacer.piximage->pixbuf,
					&zero_border, y,
					_GK.frame_right_spacer_overlap, mp->bottom_spacer.height);
			}
		draw_frame_caps(mon);
		}
	}

static gint	freeze_packing;

void
gkrellm_freeze_side_frame_packing(void)
	{
	freeze_packing += 1;
	}

void
gkrellm_thaw_side_frame_packing(void)
	{
	if (freeze_packing > 0)
		{
		freeze_packing -= 1;
		if (freeze_packing == 0)
			gkrellm_pack_side_frames();
		}
	}

void
gkrellm_pack_side_frames(void)
	{
	static GtkWidget	*lf_image, *rf_image;
	gint				x_gkrell, y_gkrell, y_bottom;
	gint				w, h;
	gboolean			was_on_bottom;

	if (   ! _GK.initialized
		|| (   monitor_previous_height == _GK.monitor_height
			&& !_GK.need_frame_packing
		   )
		|| freeze_packing
	   )
		return;
	_GK.need_frame_packing = FALSE;
	if (decorated)
		{
		_GK.frame_left_width = 0;
		_GK.frame_right_width = 0;
		return;
		}

	gdk_window_get_origin(gtree.window->window, &x_gkrell, &y_gkrell);
	gdk_drawable_get_size(gtree.window->window, &w, &h);
	was_on_bottom = (y_gkrell + h >= _GK.h_display) ? TRUE : FALSE;

	gkrellm_scale_piximage_to_pixmap(_GK.frame_left_piximage,
				&gtree.frame_left_pixmap, &gtree.frame_left_mask,
				_GK.frame_left_width, _GK.monitor_height);
	gkrellm_scale_piximage_to_pixmap(_GK.frame_right_piximage,
				&gtree.frame_right_pixmap, &gtree.frame_right_mask,
				_GK.frame_right_width, _GK.monitor_height);
	draw_frame_overlaps();

	if (!lf_image)
		{
		lf_image = gtk_image_new_from_pixmap(gtree.frame_left_pixmap, NULL);
		gtk_box_pack_start(GTK_BOX(gtree.left_vbox), lf_image,
					FALSE, FALSE, 0);
		gtk_widget_show(lf_image);
		}
	else
		gtk_image_set_from_pixmap(GTK_IMAGE(lf_image),
					gtree.frame_left_pixmap, NULL);

	if (!rf_image)
		{
		rf_image = gtk_image_new_from_pixmap(gtree.frame_right_pixmap, NULL);
		gtk_box_pack_start(GTK_BOX(gtree.right_vbox), rf_image,
					FALSE, FALSE, 0);
		gtk_widget_show(rf_image);
		}
	else
		gtk_image_set_from_pixmap(GTK_IMAGE(rf_image),
					gtree.frame_right_pixmap, NULL);

	/* If height changed so that all of GKrellM would be visible if at last
	|  user set _GK.y_position, then make sure we are really at the user set
	|  _GK.y_position.   This is a y memory effect in case a previous
	|  side frame packing caused a y move.
	*/
	h = _GK.monitor_height + _GK.total_frame_height;
	y_bottom = _GK.h_display - (_GK.y_position + h);
	if (y_bottom >= 0)
		{
		if (_GK.y_position != y_gkrell && _GK.position_valid)
			{
			gdk_window_move(gtree.window->window,
							_GK.x_position, _GK.y_position);
			gkrellm_debug(DEBUG_POSITION,
				"pack moveto %d %d=y_position (y_gkrell=%d y_bot=%d)\n",
				_GK.x_position, _GK.y_position, y_gkrell, y_bottom);
			}
		y_pack = -1;
		}
	else	/* Otherwise, do a y move to maximize visibility */
		{
		/* If GKrellM grows in height and bottom is moved off screen, move so
		|  that all of GKrellM is visible.
		*/
		y_bottom = _GK.h_display - (y_gkrell + h);
		if (y_bottom < 0)
			{
			if ((y_pack = y_gkrell + y_bottom) < 0)
				y_pack = 0;
			if (_GK.position_valid)
				{
				gdk_window_move(gtree.window->window, _GK.x_position, y_pack);
				gkrellm_debug(DEBUG_POSITION,
					"pack moveto %d %d=y_pack (y_gkrell=%d y_bot=%d)\n",
					_GK.x_position, y_pack, y_gkrell, y_bottom);
				}
			}
		/* If GKrellM bottom edge was <= screen bottom, then move to make
		|  resized GKrellM still on bottom whether window has shrunk or grown.
		*/
		else if (was_on_bottom)
			{
			if ((y_pack = _GK.h_display - h) < 0)
				y_pack = 0;
			if (_GK.position_valid)
				{
				gdk_window_move(gtree.window->window, _GK.x_position, y_pack);
				gkrellm_debug(DEBUG_POSITION,
					"pack moveto %d %d=y_pack (y_gkrell=%d on_bottom)\n",
					_GK.x_position, y_pack, y_gkrell);
				}
			}
		}
	monitor_previous_height = _GK.monitor_height;
	check_rootpixmap_transparency = 3;
	}

static gint	on_edge[4];		/* left, right, top, bottom */

static void
edge_record()
	{
	gint	x, y, w, h;

	gdk_window_get_origin(gtree.window->window, &x, &y);
	gdk_drawable_get_size(gtree.window->window, &w, &h);
	on_edge[0] = (x <= 0) ? TRUE : FALSE;
	on_edge[1] = (x + w >= _GK.w_display) ? TRUE : FALSE;
	on_edge[2] = (y <= 0) ? TRUE : FALSE;
	on_edge[3] = (y + h >= _GK.h_display) ? TRUE : FALSE;
	}

static void
fix_edges()
	{
	gint	x, y, w, h, x0, y0, y_bottom;

	if (!_GK.position_valid)
		return;
	gdk_window_get_origin(gtree.window->window, &x, &y);
	w = _GK.chart_width + _GK.frame_left_width + _GK.frame_right_width;
	h = _GK.monitor_height + _GK.total_frame_height;

	x0 = x;
	y0 = y;
	if (x < 0 || on_edge[0])
		x = 0;
	if (x > _GK.w_display - w || on_edge[1])
		x = _GK.w_display - w;
	if (y < 0 || on_edge[2])
		y = 0;
	if (y > _GK.h_display - h || on_edge[3])
		y = _GK.h_display - h;

	/* Make sure we bias to last user set _GK.y_position
	*/
	y_bottom = _GK.h_display - (_GK.y_position + h);
	if (y_bottom >= 0)
		y = _GK.y_position;
	if (x != x0 || y != y0)
		{
		/* A theme change move adjustment to keep all visible, but treat this
		|  as a packing move so can bias to the last user set _GK.y_position.
		*/
		gdk_window_move(gtree.window->window, x, y);
		if (y != _GK.y_position)
			y_pack = y;
		gkrellm_debug(DEBUG_POSITION,
			"fix_edges: %d %d (y_pos=%d)\n", x, y, _GK.y_position);
		}
	}

  /* Put an image in the spacer and chart top/bottom margin boxes accounting
  |  for regions excluded by frame overlaps.
  */
gboolean
gkrellm_render_spacer(GkrellmSpacer *spacer, gint y_src, gint h_src,
			gint l_overlap, gint r_overlap)
	{
	GkrellmPiximage	piximage;
	GdkPixbuf		*pixbuf;
	gint			w_pixbuf, h_pixbuf, w_overlap;

	pixbuf = spacer->piximage->pixbuf;
	w_pixbuf = gdk_pixbuf_get_width(pixbuf);
	h_pixbuf = gdk_pixbuf_get_height(pixbuf);
	w_overlap = l_overlap + r_overlap;

	if (   spacer->height <= 0
		|| h_src > h_pixbuf || y_src < 0 || w_overlap >= w_pixbuf
	   )
		{
		if (spacer->height > 0)
			g_warning("Bad image size for spacer or bg_chart.\n");
		return FALSE;
		}
	piximage.border = spacer->piximage->border;
	gkrellm_border_adjust(&piximage.border, -l_overlap, -r_overlap,
				-piximage.border.top, -piximage.border.bottom);

	piximage.pixbuf = gdk_pixbuf_new_subpixbuf(pixbuf, l_overlap, y_src,
				w_pixbuf - w_overlap, (h_src == 0) ? h_pixbuf : h_src);
	gkrellm_scale_piximage_to_pixmap(&piximage, &spacer->clean_pixmap,
				&spacer->mask, _GK.chart_width, spacer->height);
	g_object_unref(G_OBJECT(piximage.pixbuf));
	gkrellm_clone_pixmap(&spacer->pixmap, &spacer->clean_pixmap);
	spacer->image = gtk_image_new_from_pixmap(spacer->pixmap, spacer->mask);
	gtk_container_add(GTK_CONTAINER(spacer->vbox), spacer->image);
	gtk_widget_show_all(spacer->vbox);
	return TRUE;
	}

static void
render_monitor_spacers(GkrellmMonitor *mon)
	{
	GkrellmMonprivate	*mp = mon->privat;
	GkrellmSpacer		*top_spacer, *bottom_spacer;

	top_spacer = &mp->top_spacer;
	bottom_spacer = &mp->bottom_spacer;
	if (top_spacer->image)
		gtk_widget_destroy(top_spacer->image);
	if (bottom_spacer->image)
		gtk_widget_destroy(bottom_spacer->image);
	top_spacer->image = bottom_spacer->image = NULL;

	if (top_spacer->piximage)
		{
		if (top_spacer->height < 0)
			top_spacer->height = (mp->style_type == CHART_PANEL_TYPE) ?
					_GK.spacer_top_height_chart : _GK.spacer_top_height_meter;
		gkrellm_render_spacer(top_spacer, 0, 0,
				_GK.frame_left_spacer_overlap, _GK.frame_right_spacer_overlap);
		_GK.monitor_height += top_spacer->height;
		}
	else
		gtk_widget_hide(top_spacer->vbox);

	if (bottom_spacer->piximage)
		{
		if (bottom_spacer->height < 0)
			bottom_spacer->height = (mp->style_type == CHART_PANEL_TYPE) ?
					_GK.spacer_bottom_height_chart :
					_GK.spacer_bottom_height_meter;
		gkrellm_render_spacer(bottom_spacer, 0, 0,
				_GK.frame_left_spacer_overlap, _GK.frame_right_spacer_overlap);
		_GK.monitor_height += bottom_spacer->height;
		}
	else
		gtk_widget_hide(bottom_spacer->vbox);
	mon->privat->spacers_shown = TRUE;	/* ie, trying to show if they exist */
	}

void
gkrellm_spacers_set_types(GkrellmMonitor *mon, gint top_type, gint bot_type)
	{
	GkrellmMonprivate	*mp = (GkrellmMonprivate *) mon->privat;
	GkrellmSpacer		*ts, *bs;
	gboolean			shown;

	mp->top_type = top_type;
	mp->bottom_type = bot_type;

	ts = &mp->top_spacer;
	bs = &mp->bottom_spacer;
	shown = mp->spacers_shown;

	if (ts->piximage)
		{
		if (mp->spacers_shown)
			_GK.monitor_height -= ts->height;
		gkrellm_destroy_piximage(ts->piximage);
		}
	if (bs->piximage)
		{
		if (mp->spacers_shown)
			_GK.monitor_height -= bs->height;
		gkrellm_destroy_piximage(bs->piximage);
		}
	ts->piximage = gkrellm_clone_piximage((top_type == GKRELLM_SPACER_CHART) ?
			_GK.spacer_top_chart_piximage : _GK.spacer_top_meter_piximage);
	gkrellm_set_piximage_border(ts->piximage, &_GK.spacer_top_border);

	bs->piximage = gkrellm_clone_piximage((bot_type == GKRELLM_SPACER_CHART) ?
				_GK.spacer_bottom_chart_piximage :
				_GK.spacer_bottom_meter_piximage);
	gkrellm_set_piximage_border(bs->piximage, &_GK.spacer_bottom_border);

	ts->height = (top_type == GKRELLM_SPACER_CHART) ?
				_GK.spacer_top_height_chart : _GK.spacer_top_height_meter;
	bs->height = (bot_type == GKRELLM_SPACER_CHART) ?
				_GK.spacer_bottom_height_chart :
				_GK.spacer_bottom_height_meter;

	gkrellm_freeze_side_frame_packing();
	render_monitor_spacers(mon);
	if (!shown)
		gkrellm_spacers_hide(mon);
	gkrellm_thaw_side_frame_packing();
	}

  /* Builtin monitors need to control spacer visibility when they are
  |  enabled/disabled.  Plugins have this done for them when enabling.
  */
void
gkrellm_spacers_show(GkrellmMonitor *mon)
	{
	GkrellmMonprivate	*mp;

	if (!mon)
		return;
	mp = mon->privat;
	if (mp->spacers_shown)
		return;
	if (mp->top_spacer.piximage)
		{
		gtk_widget_show(mp->top_spacer.vbox);
		_GK.monitor_height += mp->top_spacer.height;
		}
	if (mp->bottom_spacer.piximage)
		{
		gtk_widget_show(mp->bottom_spacer.vbox);
		_GK.monitor_height += mp->bottom_spacer.height;
		}
	mp->spacers_shown = TRUE;
	gkrellm_pack_side_frames();
	if (mon == gkrellm_mon_host())
		gtk_widget_show(gtree.top1_event_box);
	}

void
gkrellm_spacers_hide(GkrellmMonitor *mon)
	{
	GkrellmMonprivate	*mp;

	if (!mon)
		return;
	mp = mon->privat;
	if (!mp->spacers_shown)
		return;
	if (mp->top_spacer.piximage)
		{
		gtk_widget_hide(mp->top_spacer.vbox);
		_GK.monitor_height -= mp->top_spacer.height;
		}
	if (mp->bottom_spacer.piximage)
		{
		gtk_widget_hide(mp->bottom_spacer.vbox);
		_GK.monitor_height -= mp->bottom_spacer.height;
		}
	mp->spacers_shown = FALSE;
	gkrellm_pack_side_frames();
	if (mon == gkrellm_mon_host())
		gtk_widget_hide(gtree.top1_event_box);
	}

static void
create_widget_tree()
	{
	gchar		*title;
#if GTK_CHECK_VERSION(2,4,0) && !defined(WIN32)
	GdkPixbuf	*icon;
#endif

	gtree.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_name(gtree.window, PACKAGE);

#if GTK_CHECK_VERSION(2,4,0) && !defined(WIN32)
	icon = gdk_pixbuf_new_from_xpm_data((const gchar **) icon_xpm);
	gtk_window_set_default_icon(icon);
#endif

	title = gkrellm_make_config_file_name(NULL, PACKAGE);
	gtk_window_set_title(GTK_WINDOW(gtree.window), title);
//	gtk_window_set_wmclass(GTK_WINDOW(gtree.window), title, "Gkrellm");
	g_free(title);

	gtree.vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gtree.window), gtree.vbox);

	gtree.top0_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(gtree.vbox), gtree.top0_event_box);
	gtree.top0_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gtree.top0_event_box), gtree.top0_vbox);

	/* The middle hbox has left frame, monitors & a right frame.
	*/
	gtree.middle_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gtree.vbox), gtree.middle_hbox);

	gtree.left_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(gtree.middle_hbox), gtree.left_event_box);
	gtree.left_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gtree.left_event_box), gtree.left_vbox);

	gtree.middle_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gtree.middle_hbox), gtree.middle_vbox);

	/* Hostname will go in an event box for moving gkrellm */
	gtree.top1_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(gtree.middle_vbox), gtree.top1_event_box);
	gtree.top1_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gtree.top1_event_box), gtree.top1_vbox);

	gtree.monitor_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gtree.middle_vbox), gtree.monitor_vbox);

	gtree.right_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(gtree.middle_hbox), gtree.right_event_box);
	gtree.right_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gtree.right_event_box), gtree.right_vbox);

	gtree.bottom_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gtree.vbox), gtree.bottom_vbox);

	gtk_widget_realize(gtree.window);

	/* gtk_window_set_resizable() */
	g_object_set(G_OBJECT(gtree.window), "allow_shrink", FALSE, NULL);
	g_object_set(G_OBJECT(gtree.window), "allow_grow", FALSE, NULL);
	if (!decorated)
		gtk_window_set_decorated((GtkWindow *) gtree.window, FALSE);

	gtk_widget_show_all(gtree.vbox);

	/* Probably don't need to realize all these here. Just a little paranoia.
	*/
	gtk_widget_realize(gtree.vbox);
	gtk_widget_realize(gtree.top0_vbox);
	gtk_widget_realize(gtree.middle_hbox);
	gtk_widget_realize(gtree.left_vbox);
	gtk_widget_realize(gtree.middle_vbox);
	gtk_widget_realize(gtree.monitor_vbox);
	gtk_widget_realize(gtree.top1_vbox);
	gtk_widget_realize(gtree.right_vbox);
	gtk_widget_realize(gtree.bottom_vbox);
	}


static gint
cb_client_event(GtkWidget  *widget, GdkEventClient *event, gpointer data)
	{
	static GdkAtom	atom_gkrellm_read_theme = GDK_NONE;

	if (!atom_gkrellm_read_theme)
		atom_gkrellm_read_theme = gdk_atom_intern("_GKRELLM_READ_THEME",FALSE);

	if (event->message_type == atom_gkrellm_read_theme)
		gkrellm_read_theme_event(NULL);
	return FALSE;
	}

  /* Callback called when configure_event is received.  This can
  |  be for window raise, lower, resize, move & maybe others
  */
static gint
cb_size_allocate(GtkWidget *w, GtkAllocation *size, gpointer data)
	{
//	g_message("x = %d, y = %d, width = %d, height = %d, data = %p\n",
//			size->x,size->y,size->width,size->height,data);
	return FALSE;
	}

static gboolean
cb_map_event(GtkWidget *widget, GdkEvent *event, gpointer data)
	{
	/* Some window managers don't recognize when _NET_WM_STATE property is set
	|  prior to window mapping.  As a temporary hack send extra _NET_WM_STATE
	|  changed messages in an attempt to get these window managers to work.
	*/
	if (_GK.sticky_state)
		gtk_window_stick(GTK_WINDOW(top_window));
	if (_GK.state_skip_pager)
		gkrellm_winop_state_skip_pager(TRUE);
	if (_GK.state_skip_taskbar)
		gkrellm_winop_state_skip_taskbar(TRUE);
	if (_GK.state_above)
		gkrellm_winop_state_above(TRUE);
	else if (_GK.state_below)
		gkrellm_winop_state_below(TRUE);

	return FALSE;
	}

static gboolean
cb_configure_notify(GtkWidget *widget, GdkEventConfigure *ev, gpointer data)
	{
	gboolean	size_change, position_change;
	gint        x, y, w, h, w_gkrellm, h_gkrellm;
	static gint	x_prev, y_prev, w_prev, h_prev;

	_GK.w_display = gdk_screen_get_width(gdk_screen_get_default());
	_GK.h_display = gdk_screen_get_height(gdk_screen_get_default());

#if !defined(WIN32)
	gdk_window_get_position(widget->window, &x, &y);
#else
	/* Windows Gtk bug? */
	x = ev->x;
	y = ev->y;
#endif

	gdk_drawable_get_size(gtree.window->window, &w, &h);

	w_gkrellm = _GK.chart_width + _GK.frame_left_width + _GK.frame_right_width;
	h_gkrellm = _GK.monitor_height + _GK.total_frame_height;

	/* If window manager decorated, I can't allow a user to resize GKrellM
	|  via the window manager.  So, try to turn it into a no-op.
	*/
	if (   decorated && monitors_visible
		&& (w_gkrellm != w || h_gkrellm != h)
	   )
		{
		gdk_window_resize(top_window->window, w_gkrellm, h_gkrellm);
		w = w_gkrellm;
		h = h_gkrellm;
		}
	size_change = (w != w_prev) | (h != h_prev);
	position_change = (x != x_prev) | (y != y_prev);

	if (size_change)
		apply_frame_transparency(FALSE);

	/* This is abit of a hose... I have to defer applying the root pixmap
	|  because windows just appearing by gtk_widget_show() are not in the
	|  right place yet even though configure notify is sent.
	*/
	if ((size_change || position_change) && !moving_gkrellm)
		check_rootpixmap_transparency = 3;
	x_prev = x;
	y_prev = y;
	w_prev = w;
	h_prev = h;

	/* At startup can be a race for configure events to be reporting correct
	|  move to position and pack_side_frames() first call.  Initial configure
	|  events will be intermediate x,y values as intial mapping happens, so
	|  using configure event position values is delayed.
	*/
	if (!configure_position_lock)
		{
		if (x >= 0 && x < _GK.w_display - 10)
			_GK.x_position = x;
		if (y >= 0 && y < _GK.h_display - 5 && y != y_pack)
			_GK.y_position = y;
		_GK.position_valid = TRUE;
		if (!moving_gkrellm)
			gkrellm_debug(DEBUG_POSITION,
				"configure-event: x_pos=%d y_pos=%d x=%d y=%d y_pack=%d\n",
				_GK.x_position, _GK.y_position, x, y, y_pack);
		}
	else
		{
		gkrellm_debug(DEBUG_POSITION,
			"locked configure-event: x=%d y=%d\n", x, y);
		}

	if (size_change || position_change)
		gkrellm_winop_update_struts();

	if (do_intro)
		{
		do_intro = FALSE;
		gkrellm_message_dialog(_("GKrellM Introduction"), _(intro_msg));
		}

	/* If GKrellM is killed (signal, shutdown, etc) it might not get
	|  a chance to save its position.  So write the position file
	|  10 seconds after window moves stop.
	*/
	if (_GK.save_position)
		save_position_countdown = 10 * _GK.update_HZ;
	return FALSE;
	}

void
gkrellm_save_all()
	{
	gkrellm_net_save_data();
	gkrellm_inet_save_data();
	if (_GK.save_position)
		set_or_save_position(1);
	if (_GK.config_modified)
		gkrellm_save_user_config();
	gkrellm_save_theme_config();
	}

static void
check_gkrellm_directories(void)
	{
	gchar	*t, *u;

	if (gkrellm_make_home_subdir(GKRELLM_DIR, NULL))
		do_intro = TRUE;	/* Defer it until main window realizes */

	gkrellm_make_home_subdir(GKRELLM_PLUGINS_DIR, NULL);
	gkrellm_make_home_subdir(GKRELLM_THEMES_DIR, NULL);
	gkrellm_make_home_subdir(GKRELLM_DATA_DIR, NULL);

	/* If no user_config specified, force a check for host-specific config
	*/
	if (!_GK.config_suffix)
		{
		_GK.found_host_config = TRUE;
		t = gkrellm_make_config_file_name(gkrellm_homedir(),
							GKRELLM_THEME_CONFIG);
		u = gkrellm_make_config_file_name(gkrellm_homedir(),
							GKRELLM_USER_CONFIG);
		if (!g_file_test(u, G_FILE_TEST_IS_REGULAR))
			{
			g_free(u);
			u = gkrellm_make_config_file_name(gkrellm_homedir(),
							GKRELLM_2_1_14_CONFIG);
			}
		if (   !g_file_test(u, G_FILE_TEST_IS_REGULAR)
			&& !g_file_test(t, G_FILE_TEST_IS_REGULAR)
		   )
			_GK.found_host_config = FALSE;
		g_free(t);
		g_free(u);
		}
	}


static gint
cb_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
	{
	/* Return FALSE to get GTK to emit the "destroy" signal.
	|  Return TRUE to not destroy the window (for verify dialog pop up).
	*/
	return(FALSE);
	}

static void
cb_destroy_event()
	{
	gtk_main_quit();
	}

static gchar	*usage_string[] =
{
N_("usage: gkrellm [options]\n"
   "options:\n"),
N_("   -t, --theme theme_dir    Select a theme directory.\n"),
N_("   -g, --geometry +x+y      Position the window on the screen.\n"),
N_("       --wm                 Allow window manager decorations.\n"),
N_("       --m2                 Left button side frame shading (for 2 btn mice).\n"),
N_("       --nt                 No transparency.\n"),
N_("   -w, --withdrawn          Draw GKrellM in withdrawn mode.\n"),
N_("   -c, --config suffix      Use alternate config files generated by\n"
   "                            appending \"suffix\" to config file names.\n"),
N_("   -f, --force-host-config  Creates config files generated by appending the\n"
   "                            hostname to config file names.  Subsequent runs\n"
   "                            automatically will use these configs unless a\n"
   "                            specific config is specified with --config.\n"
   "                            This is a convenience for allowing remote runs\n"
   "                            with independent configs in a shared home dir\n"
   "                            and for the hostname to be in the window title.\n"
   "                            This option has no effect in client mode.\n"),
N_("   -s, --server hostname    Run in client mode: connect to \"hostname\" and\n"
   "                            read monitor data from a gkrellmd server.\n"),
N_("   -P, --port server_port   Use \"server_port\" for the server connection.\n"),
N_("       --nc                 No config mode prevents configuration changes.\n"),
N_("       --config-clean       Clean out unused configs on config write.\n"),
N_("       --nolock             Allow multiple gkrellm instances.\n"),
N_("   -p, --plugin plugin.so   While developing, load your plugin under test.\n"),
N_("       --demo               Force enabling of many monitors so themers can\n"
   "                            see everything. All config saving is inhibited.\n"),
   "   -l, --logfile path       Enable error/debugging to a log file.\n",
N_("   -v, --version            Print GKrellM version number and exit.\n"),
N_("   -d, --debug-level n      Turn debugging on for selective code sections.\n"),

N_("\ndebug-level numbers are (bitwise OR to debug multiple sections):\n"
"     0x10    mail\n"
"     0x20    net\n"
"     0x40    timer button\n"
"     0x80    sensors\n"
"     0x800   inet\n"
"     0x1000  dump gkrellmd server data\n"
"     0x2000  GUI\n"
"     0x8000  battery\n"
"     0x20000 plugin \n"
"\n")
};

static void
usage()
	{
#ifdef WIN32
	GtkWidget *dialog, *content_area, *scrolled, *text_view;

	dialog = gtk_dialog_new_with_buttons(_("GKrellM"), NULL, GTK_DIALOG_MODAL,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(scrolled, 400, 300);
	gtk_container_add(GTK_CONTAINER(content_area), scrolled);

	text_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
	gtk_container_add(GTK_CONTAINER(scrolled), text_view);

	gkrellm_gtk_text_view_append_strings(text_view, usage_string,
		sizeof(usage_string) / sizeof(gchar *));

	gtk_widget_show_all(dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
#else
	gint	i;

	for (i = 0; i < (sizeof(usage_string) / sizeof(gchar *)); ++i)
		g_print("%s", _(usage_string[i]));
#endif
	}


GtkWidget *
gkrellm_monitor_vbox(void)
	{
	return gtree.monitor_vbox;
	}

void
gkrellm_build()
	{
	GkrellmMonitor		*mon;
	GList		*list;
	static gint	first_create	= TRUE;

	_GK.initialized = FALSE;
	monitor_previous_height = 0;
	_GK.total_frame_height = 0;
	_GK.any_transparency = FALSE;
	gkrellm_winop_reset();
	gkrellm_start_timer(0);

	if (!first_create)
		edge_record();
	gkrellm_alert_reset_all();
	gkrellm_panel_cleanup();

	gkrellm_theme_config();
	if (first_create)
		gkrellm_load_user_config(NULL, TRUE);
	setup_colors();
	setup_fonts();
	gkrellm_load_theme_piximages();
	gkrellm_chart_setup();
	gkrellm_freeze_side_frame_packing();

	create_frame_top(gtree.top0_vbox);

	_GK.monitor_height = 0;
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (mon->privat->main_vbox)
			{
			if (first_create)
				{
				if (mon == gkrellm_mon_host())
					gtk_box_pack_start(GTK_BOX(gtree.top1_vbox),
								mon->privat->main_vbox, FALSE, FALSE, 0);
				else
					gtk_box_pack_start(GTK_BOX(gtree.monitor_vbox),
								mon->privat->main_vbox, FALSE, FALSE, 0);
				gtk_box_pack_start(GTK_BOX(mon->privat->main_vbox),
							mon->privat->top_spacer.vbox, FALSE, FALSE, 0);
				gtk_box_pack_start(GTK_BOX(mon->privat->main_vbox),
							mon->privat->vbox, FALSE, FALSE, 0);
				gtk_box_pack_start(GTK_BOX(mon->privat->main_vbox),
							mon->privat->bottom_spacer.vbox, FALSE, FALSE, 0);
				}
			if (mon->create_monitor && mon->privat->enabled)
				{
				gkrellm_record_state(CREATE_MONITOR, mon);
				gtk_widget_show(mon->privat->vbox);
				gtk_widget_show(mon->privat->main_vbox);
				render_monitor_spacers(mon);
				(*(mon->create_monitor))(mon->privat->vbox,
							mon->privat->created ? FALSE : TRUE);
				mon->privat->created = TRUE;
				}
			gkrellm_record_state(INTERNAL, NULL);
			}
		}
	create_frame_bottom(gtree.bottom_vbox);

	_GK.initialized = TRUE;
	gkrellm_thaw_side_frame_packing();
	gkrellm_start_timer(_GK.update_HZ);
	if (!first_create)
		{
		fix_edges();
		if (!no_transparency)
			gkrellm_winop_apply_rootpixmap_transparency();
		}
	first_create = FALSE;
	}

static void
add_builtin(GkrellmMonitor *mon)
	{
	if (!mon)
		return;
	gkrellm_monitor_list = g_list_append(gkrellm_monitor_list, mon);
	if (!mon->privat)				/* Won't be null if style was added */
		mon->privat = g_new0(GkrellmMonprivate, 1);
	if (mon->create_monitor)
		{
		mon->privat->main_vbox = gtk_vbox_new(FALSE, 0);
		mon->privat->top_spacer.vbox = gtk_vbox_new(FALSE, 0);
		mon->privat->vbox = gtk_vbox_new(FALSE, 0);
		mon->privat->bottom_spacer.vbox = gtk_vbox_new(FALSE, 0);
		}
	mon->privat->enabled = TRUE;
	}

static void
load_builtin_monitors()
	{
	gkrellm_add_chart_style(NULL, "*");
	gkrellm_add_meter_style(NULL, "*");

	/* The sensors config does not have a create or update, but it does
	|  have an apply which needs to be called before the cpu or proc apply.
	|  So just put sensors config first.
	*/
	add_builtin(gkrellm_init_sensors_config_monitor());
	add_builtin(gkrellm_init_host_monitor());
	add_builtin(gkrellm_init_cal_monitor());
	add_builtin(gkrellm_init_clock_monitor());
	add_builtin(gkrellm_init_cpu_monitor());
	add_builtin(gkrellm_init_proc_monitor());
	add_builtin(gkrellm_init_sensor_monitor());
	add_builtin(gkrellm_init_disk_monitor());
	add_builtin(gkrellm_init_inet_monitor());
	add_builtin(gkrellm_init_net_monitor());
	add_builtin(gkrellm_init_timer_monitor());
	add_builtin(gkrellm_init_mem_monitor());
	add_builtin(gkrellm_init_swap_monitor());
	add_builtin(gkrellm_init_fs_monitor());
	add_builtin(gkrellm_init_mail_monitor());
	add_builtin(gkrellm_init_battery_monitor());
	add_builtin(gkrellm_init_uptime_monitor());
	}

static void
gkrellm_exit(gint exit_code)
	{
	gkrellm_sys_main_cleanup();
	gkrellm_log_cleanup();
	exit(exit_code);
	}

static void
_signal_quit(gint sig)
	{
	gkrellm_save_all();
	gkrellm_exit(1);
	}

void
gkrellm_record_state(enum GkrellmState state, GkrellmMonitor *mon)
	{
	_GK.gkrellm_state = state;
	_GK.active_monitor = mon;
	}

static void
gkrellm_abort(gint sig)
	{
	gchar			*fault,
					*state,
					buf[512];
	gboolean		do_xmessage = TRUE;
	GkrellmMonitor	*mon 	= _GK.active_monitor;

	if (sig == SIGSEGV)
		fault = _("segmentation fault");
	else if (sig == SIGFPE)
		fault = _("floating point exception");
	else
		fault = _("aborted");

	if (_GK.gkrellm_state == INITIALIZING)
		state = _("initializing");
	else if (_GK.gkrellm_state == INIT_MONITOR)
		state = "init_monitor";
	else if (_GK.gkrellm_state == CREATE_MONITOR)
		state = "create_monitor";
	else if (_GK.gkrellm_state == UPDATE_MONITOR)
		state = "update_monitor";
	else if (_GK.gkrellm_state == CREATE_CONFIG)
		state = "create_config";
	else if (_GK.gkrellm_state == LOAD_CONFIG)
		state = "load_user_config";
	else if (_GK.gkrellm_state == SAVE_CONFIG)
		state = "save_user_config";
	else if (_GK.gkrellm_state == APPLY_CONFIG)
		state = "apply_config";
	else
		{
		state = "?";
		do_xmessage = FALSE;
		}

	// FIXME: xmessage is only available on X11
	snprintf(buf, sizeof(buf), "xmessage gkrellm %s:  %s  (%s)", fault,
				(mon && mon->name) ? mon->name : "", state);
	g_warning("%s\n", buf + 9);
	if (do_xmessage)
		g_spawn_command_line_async(buf, NULL);

	signal(SIGABRT, SIG_DFL);
	abort();
	}

static void
setup_signal_handler(void)
	{
#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
	signal(SIGQUIT, _signal_quit);
#endif
	signal(SIGINT, _signal_quit);
	signal(SIGTERM, _signal_quit);
	}

void
gkrellm_sys_setup_connect(void (*setup_func)())
	{
	_GK.sys_setup_func = setup_func;
	}

gint
main(gint argc, gchar **argv)
	{
	gint						i;
	gchar						*s;
	enum GkrellmConnectResult	connect_result;
	GtkWidget					*dlg;

	gkrellm_sys_main_init();

#ifdef ENABLE_NLS
	gtk_set_locale();
#endif
	g_thread_init(NULL);
	gtk_init(&argc, &argv);		/* Will call gdk_init() */
	gkrellm_log_init();
	gtk_widget_push_colormap(gdk_rgb_get_colormap());

#ifdef ENABLE_NLS
#ifdef LOCALEDIR
#if defined(WIN32)
	gchar *install_path;
	gchar *locale_dir;
	// Prepend app install path to locale dir
	install_path = g_win32_get_package_installation_directory_of_module(NULL);
	if (install_path != NULL)
		{
	    locale_dir = g_build_filename(install_path, LOCALEDIR, NULL);
		if (locale_dir != NULL)
			{
			bindtextdomain(PACKAGE, locale_dir);
			g_free(locale_dir);
			}
	    g_free(install_path);
		}
#else
	bindtextdomain(PACKAGE, LOCALEDIR);
#endif /* !WIN32 */
#endif /* LOCALEDIR */
	textdomain(PACKAGE);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
#endif	/* ENABLE_NLS */

	_GK.start_time = time(0);
	gkrellm_current_tm = *(localtime(&_GK.start_time));

	_GK.initialized = FALSE;
	gkrellm_record_state(INITIALIZING, NULL);
	signal(SIGFPE, gkrellm_abort);
	signal(SIGSEGV, gkrellm_abort);
	signal(SIGABRT, gkrellm_abort);

	for (i = 1; i < argc; ++i)
		{
		s = argv[i];
		if (*s == '-')
			{
			++s;
			if (*s == '-')
				++s;
			}
		if ((!strcmp(s, "t") || !strcmp(s, "theme")) && i < argc - 1)
			{
			_GK.theme_path = g_strdup(argv[++i]);
			_GK.command_line_theme = g_strdup(_GK.theme_path);
			}
		else if (!strcmp(s, "sm-client-id") && i < argc - 1)
			_GK.session_id = g_strdup(argv[++i]);
		else if ((!strcmp(s, "s") || !strcmp(s, "server")) && i < argc - 1)
			{
			_GK.server = g_strdup(argv[++i]);
			_GK.client_mode = TRUE;
			}
		else if ((!strcmp(s, "P") || !strcmp(s, "port")) && i < argc-1)
			_GK.server_port = (gint) strtoul(argv[++i], NULL, 0);
		else if ((!strcmp(s, "p") || !strcmp(s, "plugin")) && i < argc - 1)
			_GK.command_line_plugin = g_strdup(argv[++i]);
		else if ((!strcmp(s, "config") || !strcmp(s, "c")) && i < argc - 1)
			_GK.config_suffix = g_strdup(argv[++i]);
		else if ((!strcmp(s, "geometry") || !strcmp(s, "g")) && i < argc - 1)
			geometry = argv[++i];
		else if (!strcmp(s, "wm"))
			_GK.command_line_decorated = TRUE;
		else if (!strcmp(s, "m2"))
			_GK.m2 = TRUE;
		else if ( (! strcmp(s, "withdrawn")) || (! strcmp(s, "w")))
			_GK.withdrawn = TRUE;
		else if (!strcmp(s, "force-host-config") || !strcmp(s, "f"))
			{
			_GK.force_host_config = TRUE;
			gkrellm_config_modified();
			}
		else if (!strcmp(s, "nt"))
			no_transparency = TRUE;
		else if (!strcmp(s, "nc"))
			_GK.no_config = TRUE;
		else if ((!strcmp(s, "debug-level") || !strcmp(s, "d")) && i < argc-1)
			_GK.debug_level = (gint) strtoul(argv[++i], NULL, 0);
		else if ((!strcmp(s, "logfile") || !strcmp(s, "l")) && i < argc-1)
			gkrellm_log_set_filename(argv[++i]);
		else if (!strncmp(s, "debug", 5))
			{
			if (s[5] != '\0')
				_GK.debug = atoi(s + 5);
			else
				_GK.debug = 1;
			}
		else if (!strcmp(s, "nolock"))
			_GK.nolock = TRUE;
		else if (!strcmp(s, "without-libsensors"))		/* temporary */
			_GK.without_libsensors = TRUE;
		else if (!strcmp(s, "use-acpi-battery"))		/* temporary */
			_GK.use_acpi_battery = TRUE;
		else if (!strcmp(s, "config-clean"))
			{
			_GK.config_clean = TRUE;
			_GK.config_modified = TRUE;
			}
		else if (!strcmp(s, "demo"))
			++_GK.demo;
		else if (!strcmp(s, "test"))
			_GK.test += 1;
		else if (!strcmp(s, "version") || !strcmp(s, "v"))
			{
			g_print("%s %d.%d.%d%s\n", PACKAGE, GKRELLM_VERSION_MAJOR,
					GKRELLM_VERSION_MINOR, GKRELLM_VERSION_REV,
					GKRELLM_EXTRAVERSION);
			exit(0);
			}
		else if (strcmp(s, "help") == 0 || strcmp(s, "h") == 0)
			{
			usage();
			exit(0);
			}
		else
			{
			g_print(_("Bad arg: %s\n"), argv[i]);
			usage();
			exit(0);
			}
		}

	if (_GK.debug_level > 0)
		g_debug("--- GKrellM %d.%d.%d ---\n", GKRELLM_VERSION_MAJOR,
			GKRELLM_VERSION_MINOR, GKRELLM_VERSION_REV);

	if (_GK.sys_setup_func)
		(*_GK.sys_setup_func)(argc, argv);

	_GK.w_display = gdk_screen_get_width(gdk_screen_get_default());
	_GK.h_display = gdk_screen_get_height(gdk_screen_get_default());

	if (_GK.server)
		{
		connect_result = gkrellm_client_mode_connect();
		while (connect_result == BAD_CONNECT)
			{
			gint	result;

			dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
					GTK_MESSAGE_ERROR, GTK_BUTTONS_YES_NO,
					"GKrellM cannot connect to server:\n"
					"\t%s:%d\n\n"
					"Do you want to retry?",
					_GK.server, _GK.server_port);
			result = gtk_dialog_run(GTK_DIALOG(dlg));
			gtk_widget_destroy(dlg);
			if (result == GTK_RESPONSE_YES)
				connect_result = gkrellm_client_mode_connect();
			else
				break;
			}
		if (connect_result == BAD_SETUP)
			{
			dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
					GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
					"GKrellM cannot get initial setup from server:\n"
					"\t\t%s:%d\n",
					_GK.server, _GK.server_port);
			gtk_dialog_run(GTK_DIALOG(dlg));
			gtk_widget_destroy(dlg);
			}
		if (connect_result != GOOD_CONNECT)
			{
			gkrellm_exit(0);
			return 0;
			}
		}

	check_gkrellm_directories();
	gkrellm_load_user_config(NULL, FALSE);
	decorated = (_GK.command_line_decorated || _GK.decorated);
	if (   _GK.command_line_plugin || _GK.command_line_theme
		|| _GK.debug_level > 0 || _GK.debug > 0 || _GK.nolock
	   )
		_GK.allow_multiple_instances_real = TRUE;
	_GK.allow_multiple_instances_real |= _GK.allow_multiple_instances;

	create_widget_tree();
	top_window = gtree.window;

	load_builtin_monitors();
	gkrellm_plugins_load();

	gkrellm_build();
	gkrellm_make_themes_list();

	if ((_GK.gtk_settings = gtk_settings_get_default()) != NULL)
		{
		g_object_get(_GK.gtk_settings,
					"gtk-theme-name", &_GK.gtk_theme_name, NULL);
		g_signal_connect(_GK.gtk_settings, "notify::gtk-theme-name",
					G_CALLBACK(gkrellm_read_theme_event), NULL);
		}

	g_signal_connect(G_OBJECT(gtree.window), "delete_event",
				G_CALLBACK(cb_delete_event), NULL);
	g_signal_connect(G_OBJECT(gtree.window), "destroy",
				G_CALLBACK(cb_destroy_event), NULL);
	g_signal_connect(G_OBJECT(gtree.window), "configure_event",
				G_CALLBACK(cb_configure_notify), NULL);
	g_signal_connect(G_OBJECT(gtree.window), "map_event",
				G_CALLBACK(cb_map_event), NULL);
	g_signal_connect(G_OBJECT(gtree.window), "size_allocate",
				G_CALLBACK(cb_size_allocate), NULL);
	g_signal_connect(G_OBJECT(gtree.window), "client_event",
				G_CALLBACK(cb_client_event), NULL);

	g_signal_connect(G_OBJECT(gtree.top0_event_box), "button_press_event",
				G_CALLBACK(top_frame_button_press), NULL );
	g_signal_connect(G_OBJECT(gtree.top1_event_box), "button_press_event",
				G_CALLBACK(top_frame_button_press), NULL );
	g_signal_connect(G_OBJECT(gtree.top0_event_box), "motion_notify_event",
				G_CALLBACK(gkrellm_motion), NULL);
	g_signal_connect(G_OBJECT(gtree.top1_event_box), "motion_notify_event",
				G_CALLBACK(gkrellm_motion), NULL);
	g_signal_connect(G_OBJECT(gtree.top0_event_box), "button_release_event",
				G_CALLBACK(top_frame_button_release), NULL );
	g_signal_connect(G_OBJECT(gtree.top1_event_box), "button_release_event",
				G_CALLBACK(top_frame_button_release), NULL );

	g_signal_connect(G_OBJECT(gtree.left_event_box), "button_press_event",
				G_CALLBACK(side_frame_button_press), NULL );
	g_signal_connect(G_OBJECT(gtree.right_event_box), "button_press_event",
				G_CALLBACK(side_frame_button_press), NULL );

	ui_manager = gkrellm_create_ui_manager_popup();

	if (_GK.sticky_state)
		gtk_window_stick(GTK_WINDOW(top_window));
	gkrellm_winop_options(argc, argv);
	gtk_widget_show(gtree.window);
	gkrellm_winop_withdrawn();

	if (geometry || _GK.save_position)
		configure_position_lock = TRUE;		/* see cb_configure_notify */

	if (geometry)		/* Command line placement overrides */
		gkrellm_winop_place_gkrellm(geometry);
	else if (_GK.save_position)
		set_or_save_position(0);

	gkrellm_start_timer(_GK.update_HZ);
	setup_signal_handler();
	gtk_main();

	gkrellm_save_all();

	/* disconnect from gkrellm-server if we're a client */
	if (_GK.server)
		{
		gkrellm_client_mode_disconnect();
		}
	gkrellm_exit(0);

	return 0;
	}
