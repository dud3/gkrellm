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
#include "gkrellm-sysdeps.h"

#include "pixmaps/mem/krell_buffers.xpm"
#include "pixmaps/mem/krell_cache.xpm"

  /* The mem monitor has two extra krells which can be themed.
  |  So, the theme "mem" subdir can have:
  |      bg_panel.png
  |      krell.png
  |      krell_buffers.png		# an extension image, defaults to included xpm
  |      krell_cache.png		# an extension image, defaults to included xpm
  |
  |  The gkrellmrc can have theme extension variables for these extra krells:
  |      set_integer mem_krell_buffers_yoff n
  |      set_integer mem_krell_buffers_depth n
  |      set_integer mem_krell_buffers_x_hot n
  |      set_integer mem_krell_cache_yoff n
  |      set_integer mem_krell_cache_depth n
  |      set_integer mem_krell_cache_x_hot n
  */

#define	MIN_GRID_RES		20
#define	MAX_GRID_RES		100000
#define DEFAULT_GRID_RES	1000

#define DEFAULT_SWAP_CHART_HEIGHT 20

#define	DEFAULT_FORMAT	_("$t - $f free")

#define MEG(x)	((gulong)(((x) + (1 << 19)) >> 20))

typedef struct
	{
	GkrellmPanel *panel;
	GkrellmKrell *krell_used,	/* Meter styled, shows fraction used	*/
				*krell_delta,	/* Chart styled, for page in/out deltas */
				*krell_buffers,
				*krell_cache;
	gchar		*label,
				*data_format,	/* Format string for scrolling text		*/
				*data_format_shadow; /* shadow format for gdk_draw compat */
	gint		style_id;
	gint		x_label;
	GkrellmDecal *decal_label;
	gboolean	label_is_data,
				restore_label,	/* Helper to know when to toggle data fmt off*/
				mouse_entered,
				all_krells;
	gint		enabled;
	GkrellmLauncher	launch;

	GkrellmAlert *alert;

	guint64		total,		/* Total memory or swap in system */
				used,		/* Amount of memory (calulated) or swap used  */
				free,		/* Not used by swap	monitor */
				shared,		/* Not used by swap	monitor */
				buffers,	/* Not used by swap	monitor */
				cached;		/* Not used by swap	monitor */
	}
	MeminfoMeter;

typedef struct
	{
	GtkWidget			*vbox;
	GkrellmChart		*chart;
	GkrellmChartdata	*in_cd,
						*out_cd;
	GkrellmChartconfig	*chart_config;
	gboolean			enabled;
	gboolean			extra_info;

	gulong				page_in,
						page_out;
	}
	MeminfoChart;


static GkrellmMonitor	*mon_mem,
						*mon_swap;

static MeminfoMeter	mem,
					swap;
static MeminfoChart	swap_chart;

static gint			x_scroll,
					x_mon_motion,
					x_moved,
					ascent;

static MeminfoMeter	*mon_in_motion;


static void	(*read_mem_data)();
static void	(*read_swap_data)();


static gint
setup_meminfo_interface(void)
	{
	if (!read_mem_data && !_GK.client_mode && gkrellm_sys_mem_init())
		{
		read_mem_data = gkrellm_sys_mem_read_data;
		read_swap_data = gkrellm_sys_swap_read_data;
		}
	return read_mem_data ? TRUE : FALSE;
	}

void
gkrellm_mem_client_divert(void (*read_mem_func)(), void (*read_swap_func)())
	{
	read_mem_data = read_mem_func;
	read_swap_data = read_swap_func;
	}

void
gkrellm_mem_assign_data(guint64 total, guint64 used, guint64 free,
                guint64 shared, guint64 buffers, guint64 cached)
	{
	mem.total = total;
	mem.used = used;
	mem.free = free;
	mem.shared = shared;
	mem.buffers = buffers;
	mem.cached = cached;
	}

void
gkrellm_swap_assign_data(guint64 total, guint64 used,
                gulong swap_in, gulong swap_out)
	{
	swap.total = total;
	swap.used = used;
	swap_chart.page_in = swap_in;
	swap_chart.page_out = swap_out;
	}


  /* Reading system memory data can be expensive,
  |  so I do some dynamic adjustments on how often I do the updates.
  |  I increase the update rate if system activity is detected.
  |  This is an effort to get good meter response and to
  |  not contribute to cpu chart activity during quiet times, ie maintain
  |  a good S/N where I'm the noise.
  */
#define PIPE_SIZE	3
static gint		mem_pipe[PIPE_SIZE],
				swap_pipe[PIPE_SIZE];
static gint		force_update	= TRUE;

gboolean
force_meminfo_update(void)
	{
	gint	i, force;

	force = force_update ? TRUE : FALSE;
	force_update = FALSE;
	if (GK.second_tick)
		{
		for (i = 1; i < PIPE_SIZE; ++i)
			if (mem_pipe[i] || swap_pipe[i])
				force = TRUE;
		if (_GK.cpu_sys_activity > 3)
			force = TRUE;
		}
	return force;
	}

static gint
format_meminfo_data(MeminfoMeter *mm, gchar *src_string, gchar *buf, gint size)
	{
	gulong		t, u, f, ur, fr;
	gchar		*s, *label;
	gint		len;
	gboolean	raw;

	if (!buf || size < 1)
		return -1;
	--size;
	*buf = '\0';
	if (!src_string)
		return -1;
	label = mm->label;
	t = MEG(mm->total);
	u = MEG(mm->used);
	f = t - u;

	fr = MEG(mem.free);
	ur = u + MEG(mem.shared + mem.buffers + mem.cached);

	for (s = src_string; *s != '\0' && size > 0; ++s)
		{
		len = 1;
		raw = FALSE;
		if (*s == '$' && *(s + 1) != '\0')
			{
			if (*(s + 2) == 'r')  /* print raw free/used */
				raw = TRUE;
			switch(*(s + 1))
				{
				case 'l':
					len = snprintf(buf, size, "%s", label);
					break;
				case 't':
					len = snprintf(buf, size, "%ldM", t);
					break;
				case 'u':
					len = snprintf(buf, size, "%ldM", raw ? ur : u);
					break;
				case 'U':
					if (t > 0)
						len = snprintf(buf, size, "%ld%%",
							100 * (raw ? ur : u) / t);
					break;
				case 'f':
					len = snprintf(buf, size, "%ldM", raw ? fr : f);
					break;
				case 'F':
					if (t > 0)
						len = snprintf(buf, size, "%ld%%",
							100 * (raw ? fr : f) / t);
					break;
				case 's':
					if (mm == &mem)
						len = snprintf(buf, size, "%ldM", MEG(mem.shared));
					break;
				case 'b':
					if (mm == &mem)
						len = snprintf(buf, size, "%ldM",MEG(mem.buffers));
					break;
				case 'c':
					if (mm == &mem)
						len = snprintf(buf, size, "%ldM", MEG(mem.cached));
					break;
				case 'H':
						len = snprintf(buf, size, "%s",
									gkrellm_sys_get_host_name());
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
			if (raw)
				++s;
			}
		else
			*buf = *s;
		size -= len;
		buf += len;
		}
	*buf = '\0';
	return t + u + 1;		/* A way to know if decal text changed. */
	}

static gint
draw_decal_label(MeminfoMeter *mm, gint draw_to_screen)
	{
	GkrellmDecal		*d;
	GkrellmTextstyle	ts_save;
	gchar				buf[128];
	gint				x_off, w;

	d = mm->decal_label;
	if (! mm->label_is_data)
		{
		gkrellm_decal_text_set_offset(d, mm->x_label, 0);
		gkrellm_draw_decal_text(mm->panel, d, mm->label, 0);
		}
    else
        {
		ts_save = d->text_style;
		d->text_style = *gkrellm_meter_alt_textstyle(mm->style_id);

		format_meminfo_data(mm, mm->data_format_shadow, buf, sizeof(buf));
		gkrellm_decal_scroll_text_set_markup(mm->panel, d, buf);
		gkrellm_decal_scroll_text_get_size(d, &w, NULL);
		if (w  > d->w)
			x_off = d->w / 3 - x_scroll;
		else
			x_off = 0;
		gkrellm_decal_text_set_offset(d, x_off, 0);

		d->text_style = ts_save;
		}
	if (draw_to_screen)
		gkrellm_draw_panel_layers(mm->panel);
	return w;
	}

static void
cb_command_process(GkrellmAlert *alert, gchar *src, gchar *dst, gint len,
			MeminfoMeter *m)
	{
	format_meminfo_data(m, src, dst, len);
	}

static void
record_activity(gint *pipe, gint modified)
	{
	gint	i;

	for (i = PIPE_SIZE - 1; i > 0; --i)
		pipe[i] = pipe[i-1];
	pipe[0] = modified;
	}


static GkrellmSizeAbbrev	swap_blocks_abbrev[] =
	{
	{ KB_SIZE(1),		1,				"%.0f" },
	{ KB_SIZE(20),		KB_SIZE(1),		"%.1fK" },
	{ MB_SIZE(1),		KB_SIZE(1),		"%.0fK" },
	{ MB_SIZE(20),		MB_SIZE(1),		"%.1fM" }
	};


static gchar    *text_format,
				*text_format_locale;

static void
format_chart_text(MeminfoChart *mc, gchar *buf, gint size)
	{
	GkrellmChart	*cp;
	gchar	c, *s;
	size_t	tbl_size;
	gint	len, in_blocks, out_blocks, blocks;

	--size;
	*buf = '\0';
	cp = mc->chart;
	in_blocks = gkrellm_get_current_chartdata(mc->in_cd);
	out_blocks = gkrellm_get_current_chartdata(mc->out_cd);
	tbl_size = sizeof(swap_blocks_abbrev) / sizeof(GkrellmSizeAbbrev);
	for (s = text_format_locale; *s != '\0' && size > 0; ++s)
		{
		len = 1;
		if (*s == '$' && *(s + 1) != '\0')
			{
			blocks = -1;
			if ((c = *(s + 1)) == 'T')
				blocks = in_blocks + out_blocks;
			else if (c == 'M')
				blocks = gkrellm_get_chart_scalemax(cp);
			else if (c == 'i')
				blocks = in_blocks;
			else if (c == 'o')
				blocks = out_blocks;
			else
				{
				*buf = *s;
				if (size > 1)
					{
					*(buf + 1) = *(s + 1);
					++len;
					}
				}
			if (blocks >= 0)
				len = gkrellm_format_size_abbrev(buf, size, (gfloat) blocks,
						&swap_blocks_abbrev[0], tbl_size);
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
draw_extra(MeminfoChart *mc)
    {
    gchar       buf[128];

	if (!mc->extra_info)
		return;
	format_chart_text(mc, buf, sizeof(buf));
	gkrellm_draw_chart_text(mc->chart, DEFAULT_STYLE_ID, buf);	/* XXX */
	}

static void
refresh_chart(MeminfoChart *mc)
	{
	if (mc->chart)
		{
		gkrellm_draw_chartdata(mc->chart);
		draw_extra(mc);
		gkrellm_draw_chart_to_screen(mc->chart);
		}
	}

static gint
cb_extra(GtkWidget *widget, GdkEventButton *ev, gpointer data)
	{
	MeminfoChart	*mc	= (MeminfoChart *) data;

	if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS)
		{
		mc->extra_info = !mc->extra_info;
		gkrellm_config_modified();
		refresh_chart(mc);
		}
	else if (   ev->button == 3
			 || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS)
			)
		gkrellm_chartconfig_window_create(mc->chart);
	return FALSE;
	}

static void
update_meminfo(void)
	{
	GkrellmChart	*cp;
	gulong			u, b, c, used;
	gint			w_scroll, w, full_scale;

	if (! (mem.enabled || swap.enabled || swap_chart.enabled))
		return;
	if (GK.five_second_tick || force_meminfo_update())
		(*read_mem_data)();
	(*read_swap_data)();

	if (GK.second_tick)
		{
		MeminfoChart	*mc	= &swap_chart;

		if ((cp = mc->chart) != NULL && GK.second_tick)
			{
			gkrellm_store_chartdata(cp, 0, mc->page_out, mc->page_in);
			refresh_chart(mc);
			}
		}
	if (mem.enabled)
		{
		full_scale = (gint) (mem.total >> 12);
		gkrellm_set_krell_full_scale(mem.krell_used, full_scale, 1);
		gkrellm_set_krell_full_scale(mem.krell_buffers, full_scale, 1);
		gkrellm_set_krell_full_scale(mem.krell_cache, full_scale, 1);

		used = u = (gulong) (mem.used >> 12);
		b = u + (gulong)(mem.buffers >> 12);
		c = b + (gulong)(mem.cached >> 12);
		if (   (mem.label_is_data && mon_in_motion && x_moved)
			|| mem.mouse_entered
		   )
			u = b = c = 0;
		gkrellm_update_krell(mem.panel, mem.krell_used, u);
		gkrellm_update_krell(mem.panel, mem.krell_buffers, b);
		gkrellm_update_krell(mem.panel, mem.krell_cache, c);
		record_activity(mem_pipe, mem.krell_used->modified);
		if (mem.alert && GK.second_tick)
			gkrellm_check_alert(mem.alert,
						100.0 * (gfloat) used / (gfloat) full_scale);
		}
	if (swap.enabled)
		{
		full_scale = (gint) (swap.total >> 12);
		gkrellm_set_krell_full_scale(swap.krell_used, full_scale, 1);
		used = u = (gulong)(swap.used >> 12);
		if (   (swap.label_is_data && mon_in_motion && x_moved)
			|| swap.mouse_entered
		   )
			u = 0;
		gkrellm_update_krell(swap.panel, swap.krell_used, u);
		record_activity(swap_pipe, swap.krell_used->modified);
		if (swap.alert && GK.second_tick)
			gkrellm_check_alert(swap.alert,
						100.0 * (gfloat) used / (gfloat) full_scale);
		}
	if (swap.krell_delta && swap.enabled)
		gkrellm_update_krell(swap.panel, swap.krell_delta,
				swap_chart.page_in + swap_chart.page_out);

	w = w_scroll = 0;
	if (mem.label_is_data && mon_in_motion != &mem && mem.enabled)
		w_scroll = draw_decal_label(&mem, 1);
	if (swap.label_is_data && mon_in_motion != &swap && swap.enabled)
		{
		if ((w = draw_decal_label(&swap, 1)) > w_scroll)
			w_scroll = w;
		}
	if (!mon_in_motion)
		{
		if (w_scroll > mem.decal_label->w)
			x_scroll = (x_scroll + ((gkrellm_update_HZ() < 7) ? 2 : 1))
						% (w_scroll - mem.decal_label->w / 3);
		else
			x_scroll = 0;
		}
	gkrellm_draw_panel_layers(mem.panel);
	gkrellm_draw_panel_layers(swap.panel);
	}

static gint
meminfo_expose_event(GtkWidget *widget, GdkEventExpose *ev)
	{
	GdkPixmap	*pixmap	= NULL;

	if (widget == mem.panel->drawing_area)
		pixmap = mem.panel->pixmap;
	else if (widget == swap.panel->drawing_area)
		pixmap = swap.panel->pixmap;
	else if (swap_chart.chart && widget == swap_chart.chart->drawing_area)
		pixmap = swap_chart.chart->pixmap;
	if (pixmap)
		gdk_draw_drawable(widget->window, gkrellm_draw_GC(1), pixmap,
				ev->area.x, ev->area.y, ev->area.x, ev->area.y,
				ev->area.width, ev->area.height);
	return FALSE;
	}

static gint
cb_panel_enter(GtkWidget *w, GdkEventButton *ev, MeminfoMeter *mm)
	{
	if (mm->label_is_data)
		mm->mouse_entered = TRUE;
	return FALSE;
	}

static gint
cb_panel_leave(GtkWidget *w, GdkEventButton *ev, MeminfoMeter *mm)
	{
	mm->mouse_entered = FALSE;
	return FALSE;
	}

static gint
cb_panel_release(GtkWidget *widget, GdkEventButton *ev)
	{
    if (ev->button == 3)
		return FALSE;
	if (mon_in_motion)
		{
		if (mon_in_motion->restore_label)
			{
			if (mon_in_motion->label_is_data)
				gkrellm_config_modified();
			mon_in_motion->label_is_data = FALSE;
			draw_decal_label(mon_in_motion, 1);
			}
		mon_in_motion->restore_label = TRUE;
		}
	mon_in_motion = NULL;
	x_moved = FALSE;
	return FALSE;
	}

static gint
cb_panel_press(GtkWidget *widget, GdkEventButton *ev, MeminfoMeter *mm)
    {
	if (ev->button == 3)
		{
		gkrellm_open_config_window(mon_mem);
		return FALSE;
		}
	if (   ev->button == 1 && mm->launch.button
		&& gkrellm_in_decal(mm->launch.button->decal, ev)
	   )
		return FALSE;
	if (widget == mem.panel->drawing_area)
		mon_in_motion = &mem;
	else if (widget == swap.panel->drawing_area)
		mon_in_motion = &swap;
	else
		return FALSE;
	if (! mon_in_motion->label_is_data)
		{
		mon_in_motion->label_is_data = TRUE;
		mon_in_motion->restore_label = FALSE;
		mon_in_motion->mouse_entered = TRUE;
		gkrellm_config_modified();
		}
	x_mon_motion = ev->x;
	draw_decal_label(mon_in_motion, 1);
	x_moved = FALSE;
	return FALSE;
	}

static gint
cb_panel_motion(GtkWidget *widget, GdkEventButton *ev)
	{
	GdkModifierType	state;
	GkrellmDecal	*d;
	PangoFontDescription *font_desc;
	gchar			buf[128];
	gint			w, x_delta;

	state = ev->state;
	if (   ! mon_in_motion
		|| ! (state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK))
		|| ! mon_in_motion->label_is_data
	   )
		{
		mon_in_motion = NULL;
		return FALSE;
		}
	d = mon_in_motion->decal_label;
	font_desc = gkrellm_meter_alt_textstyle(mon_in_motion->style_id)->font;

	format_meminfo_data(mon_in_motion, mon_in_motion->data_format_shadow,
				buf, sizeof(buf));
	w = gkrellm_gdk_string_width(font_desc, buf);
	if (w > d->w)
		{
		x_delta = ev->x - x_mon_motion;
		x_mon_motion = ev->x;
		d->x_off += x_delta;
		if (d->x_off < -w)
			d->x_off = -w;
		if (d->x_off > d->w)
			d->x_off = d->w;
		x_scroll = d->w / 3 - d->x_off;
		if (mem.label_is_data)
			draw_decal_label(&mem, 1);
		if (swap.label_is_data)
			draw_decal_label(&swap, 1);
		mon_in_motion->restore_label = FALSE;
		}
	x_moved = TRUE;
	return FALSE;
	}

static void
setup_scaling(GkrellmChartconfig *cf, MeminfoChart *mc)
	{
	GkrellmChart	*cp	  = mc->chart;
	gint	res   = DEFAULT_GRID_RES,
			grids = FULL_SCALE_GRIDS;

	if (cp)
		{
		grids = gkrellm_get_chartconfig_fixed_grids(cp->config);
		res = gkrellm_get_chartconfig_grid_resolution(cp->config);
		}
	if (grids == 0)
		grids = FULL_SCALE_GRIDS;

	if (swap.krell_delta)
		swap.krell_delta->full_scale = res * grids / gkrellm_update_HZ();
	}

static void
destroy_chart(MeminfoChart *mc)
	{
	if (! mc->chart)
		return;
	gkrellm_chart_destroy(mc->chart);
	mc->chart = NULL;
	mc->enabled = FALSE;
	}

static void
create_chart(MeminfoChart *mc, gint first_create)
	{
	GkrellmChart				*cp;
	static GkrellmPiximage		*piximage;

	if (first_create)
		mc->chart = gkrellm_chart_new0();
	cp = mc->chart;

	if (gkrellm_load_piximage("bg_chart", NULL, &piximage, SWAP_STYLE_NAME))
		gkrellm_chart_bg_piximage_override(cp, piximage,
					gkrellm_bg_grid_piximage(swap.style_id));
	gkrellm_set_chart_height_default(cp, DEFAULT_SWAP_CHART_HEIGHT);
	gkrellm_chart_create(mc->vbox, mon_swap, cp, &mc->chart_config);
	mc->out_cd = gkrellm_add_default_chartdata(cp, _("Swap Out"));
	mc->in_cd = gkrellm_add_default_chartdata(cp, _("Swap In"));
	gkrellm_set_draw_chart_function(cp, refresh_chart, mc);
    gkrellm_chartconfig_fixed_grids_connect(cp->config,
                setup_scaling, mc);
    gkrellm_chartconfig_grid_resolution_connect(cp->config,
                setup_scaling, mc);
    gkrellm_chartconfig_grid_resolution_adjustment(cp->config, TRUE,
                0, (gfloat) MIN_GRID_RES, (gfloat) MAX_GRID_RES, 0, 0, 0, 70);
    gkrellm_chartconfig_grid_resolution_label(cp->config,
                _("Swap in/out pages per sec"));
    if (gkrellm_get_chartconfig_grid_resolution(cp->config) < MIN_GRID_RES)
        gkrellm_set_chartconfig_grid_resolution(cp->config, DEFAULT_GRID_RES);

	gkrellm_alloc_chartdata(cp);

	if (first_create)
		{
		g_signal_connect(G_OBJECT(cp->drawing_area), "expose_event",
					G_CALLBACK(meminfo_expose_event), NULL);
		g_signal_connect(G_OBJECT(cp->drawing_area), "button_press_event",
					G_CALLBACK(cb_extra), mc);
		gtk_widget_show(mc->vbox);
		}
	else
		refresh_chart(mc);	/* Avoid second lag at theme/size switches */
	mc->enabled = TRUE;
	ascent = 0;
	}

static void
connect_panel_signals(GkrellmPanel *p, MeminfoMeter *mm)
		{
		g_signal_connect(G_OBJECT (p->drawing_area), "expose_event",
					G_CALLBACK(meminfo_expose_event), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area), "button_press_event",
					G_CALLBACK(cb_panel_press), mm);
		g_signal_connect(G_OBJECT(p->drawing_area), "button_release_event",
					G_CALLBACK(cb_panel_release), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area), "motion_notify_event",
					G_CALLBACK(cb_panel_motion), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area), "enter_notify_event",
					G_CALLBACK(cb_panel_enter), mm);
		g_signal_connect(G_OBJECT(p->drawing_area), "leave_notify_event",
					G_CALLBACK(cb_panel_leave), mm);
		}

static void
create_mem_panel(GtkWidget *vbox, gint first_create)
	{
	GkrellmPiximage	*im = NULL;
	MeminfoMeter	*mm;
	GkrellmPanel	*p;
	GkrellmTextstyle *ts;
	GkrellmStyle	*s;
	gchar			*expand, buf[64], *u;
	gint			w_label, label_x_position, label_y_off;

	mm = &mem;
	if (first_create)
		mm->panel = gkrellm_panel_new0();
	if (!mem.all_krells)	/* Krells are not in panel krell list where */
		{					/* they would be automatically destroyed. */
		gkrellm_destroy_krell(mem.krell_buffers);
		gkrellm_destroy_krell(mem.krell_cache);
		}
	p = mm->panel;

	/* I've got two extra krells for buffers and cache.  Use the #included
	|  images unless themer has customized.
	*/
	s = gkrellm_copy_style(gkrellm_meter_style(mm->style_id));

	s->krell_yoff = 0;
	s->krell_depth = 1;
	s->krell_x_hot = -1;
	gkrellm_get_gkrellmrc_integer("mem_krell_cache_depth", &s->krell_depth);
	gkrellm_get_gkrellmrc_integer("mem_krell_cache_x_hot", &s->krell_x_hot);
	gkrellm_get_gkrellmrc_integer("mem_krell_cache_yoff", &s->krell_yoff);
	expand = gkrellm_get_gkrellmrc_string("mem_krell_cache_expand");
	gkrellm_set_krell_expand(s, expand);
	g_free(expand);
	gkrellm_load_piximage("krell_cache", krell_cache_xpm, &im, MEM_STYLE_NAME);
	mm->krell_cache = gkrellm_create_krell(p, im, s);
	gkrellm_monotonic_krell_values(mm->krell_cache, FALSE);

	s->krell_yoff = 0;
	s->krell_depth = 1;
	s->krell_x_hot = -1;
	gkrellm_get_gkrellmrc_integer("mem_krell_buffers_depth", &s->krell_depth);
	gkrellm_get_gkrellmrc_integer("mem_krell_buffers_x_hot", &s->krell_x_hot);
	gkrellm_get_gkrellmrc_integer("mem_krell_buffers_yoff", &s->krell_yoff);
	expand = gkrellm_get_gkrellmrc_string("mem_krell_buffers_expand");
	gkrellm_set_krell_expand(s, expand);
	g_free(expand);
	gkrellm_load_piximage("krell_buffers", krell_buffers_xpm,
						 &im,MEM_STYLE_NAME);
	mm->krell_buffers = gkrellm_create_krell(p, im, s);
	gkrellm_monotonic_krell_values(mm->krell_buffers, FALSE);

	/* Unlike the style pointer passed to gkrellm_panel_configure(), the krells
	|  don't need the style to persist.
	*/
	g_free(s);
	if (im)
		gkrellm_destroy_piximage(im);

	s = gkrellm_meter_style(mm->style_id);
	gkrellm_panel_label_get_position(s, &label_x_position, &label_y_off);

	mm->krell_used = gkrellm_create_krell(p,
						gkrellm_krell_meter_piximage(mm->style_id), s);
	gkrellm_monotonic_krell_values(mm->krell_used, FALSE);

	if (mem.label)
		g_free(mem.label);
	if (label_x_position == GKRELLM_LABEL_NONE)
		mem.label = g_strdup("");
	else
		mem.label = g_strdup(_("Mem"));
	if (!g_utf8_validate(mem.label, -1, NULL))
		{
		u = g_locale_to_utf8(mem.label, -1, NULL, NULL, NULL);
		g_free(mem.label);
		mem.label = u;
		}

	snprintf(buf, sizeof(buf), "%sMemfj8", mem.label);
	mm->decal_label = gkrellm_create_decal_text(p, buf,
				gkrellm_meter_textstyle(mm->style_id), s, -1,
				(label_y_off > 0) ? label_y_off : -1,
				-1);
	gkrellm_panel_configure(p, NULL, s);
	gkrellm_panel_create(vbox, mon_mem, p);

	ts = &mm->decal_label->text_style;
	w_label = gkrellm_gdk_string_width(ts->font, mm->label) + ts->effect;
	mm->x_label = gkrellm_label_x_position(label_x_position,
				mm->decal_label->w,
				w_label, 0);
	draw_decal_label(mm, 0);

	if (first_create)
		connect_panel_signals(p, mm);

	mm->launch.margin.left = - mm->x_label + 2;
	mm->launch.margin.right = -(mm->decal_label->w - mm->x_label - w_label) +2;
	gkrellm_setup_decal_launcher(p, &mm->launch, mm->decal_label);

	if (!mm->enabled)
		gkrellm_panel_hide(p);
	if (!mem.all_krells)
		{
		gkrellm_remove_krell(mem.panel, mem.krell_buffers);
		gkrellm_remove_krell(mem.panel, mem.krell_cache);
		}
	}

static void
create_swap_panel(GtkWidget *vbox, gint first_create)
	{
	MeminfoMeter	*mm;
	GkrellmPanel	*p;
	GkrellmStyle	*style, *panel_style;
	GkrellmTextstyle *ts;
	gchar			buf[64], *u;
	gint			w_label, label_x_position, label_y_off;

	mm = &swap;
	if (first_create)
		mm->panel = gkrellm_panel_new0();
	p = mm->panel;

	style = gkrellm_meter_style(mm->style_id);
	gkrellm_panel_label_get_position(style, &label_x_position, &label_y_off);

	/* Need a chart styled krell on the swap meter panel, but want it to track
	|  the meter krell margins.
	*/
	panel_style = gkrellm_copy_style(gkrellm_panel_style(DEFAULT_STYLE_ID));
	panel_style->krell_left_margin = style->krell_left_margin;
	panel_style->krell_right_margin = style->krell_right_margin;
	mm->krell_delta = gkrellm_create_krell(p,
			gkrellm_krell_panel_piximage(DEFAULT_STYLE_ID), panel_style);
	g_free(panel_style);	/* unlike panels, krell styles need not persist */

	mm->krell_used = gkrellm_create_krell(p,
				gkrellm_krell_meter_piximage(mm->style_id), style);
	gkrellm_monotonic_krell_values(mm->krell_used, FALSE);

	if (swap.label)
		g_free(swap.label);
	if (label_x_position == GKRELLM_LABEL_NONE)
		swap.label = g_strdup("");
	else
		swap.label = g_strdup(_("Swap"));
	if (!g_utf8_validate(swap.label, -1, NULL))
		{
		u = g_locale_to_utf8(swap.label, -1, NULL, NULL, NULL);
		g_free(swap.label);
		swap.label = u;
		}

	snprintf(buf, sizeof(buf), "%sMemfj8", swap.label);
	mm->decal_label = gkrellm_create_decal_text(p, buf,
				gkrellm_meter_textstyle(mm->style_id), style, -1,
				(label_y_off > 0) ? label_y_off : -1,
				-1);

	gkrellm_panel_configure(p, NULL, style);
	gkrellm_panel_create(vbox, mon_swap, p);

	ts = &mm->decal_label->text_style;
	w_label = gkrellm_gdk_string_width(ts->font, mm->label) + ts->effect;
	mm->x_label = gkrellm_label_x_position(label_x_position,
				mm->decal_label->w,
				w_label, 0);
	draw_decal_label(mm, 0);

	if (first_create)
		connect_panel_signals(p, mm);

	mm->launch.margin.left = - mm->x_label + 1;
	mm->launch.margin.right = -(mm->decal_label->w - mm->x_label - w_label) +1;
	gkrellm_setup_decal_launcher(p, &mm->launch, mm->decal_label);

	if (!mm->enabled)
		gkrellm_panel_hide(p);
	}

static void
spacer_visibility(void)
	{
	gint	top, bot;

	top = swap_chart.enabled ? GKRELLM_SPACER_CHART : GKRELLM_SPACER_METER;
	bot = (swap_chart.enabled && !(mem.enabled || swap.enabled)) ?
				GKRELLM_SPACER_CHART : GKRELLM_SPACER_METER;
	gkrellm_spacers_set_types(mon_mem, top, bot);

	if (mem.enabled || swap.enabled || swap_chart.enabled)
		gkrellm_spacers_show(mon_mem);
	else
		gkrellm_spacers_hide(mon_mem);
	}

  /* No separate swap monitor create function.  Use create_mem() to create
  |  swap chart, mem meter, and swap meter so they will all be a unit in
  |  the same vbox.
  */
static void
create_mem(GtkWidget *vbox, gint first_create)
	{
	if (first_create)
		{
		swap_chart.vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), swap_chart.vbox, FALSE, FALSE, 0);

		(*read_mem_data)();
		(*read_swap_data)();
		}
	if (swap_chart.enabled)
		create_chart(&swap_chart, first_create);
	create_mem_panel(vbox, first_create);
	create_swap_panel(vbox, first_create);
	setup_scaling(NULL, &swap_chart);
	spacer_visibility();
	}


#define	MEM_CONFIG_KEYWORD	"meminfo"

static void
cb_alert_trigger(GkrellmAlert *alert, MeminfoMeter *m)
	{
	/* Full panel alert, default decal.
	*/
	alert->panel = m->panel;
	}

static void
create_alert(MeminfoMeter *m)
	{
	gchar	*label;

	if (m == &mem)
		label = _("Memory");
	else
		label = _("Swap");
	m->alert = gkrellm_alert_create(NULL, label,
			_("Percent Usage"),
			TRUE, FALSE, TRUE,
			100, 10, 1, 10, 0);
	gkrellm_alert_trigger_connect(m->alert, cb_alert_trigger, m);
	gkrellm_alert_command_process_connect(m->alert, cb_command_process, m);
	}

static void
save_meminfo_config(FILE *f)
	{
	fprintf(f, "%s mem_meter %d %d %d\n", MEM_CONFIG_KEYWORD,
				mem.enabled, mem.label_is_data, mem.all_krells);
	fprintf(f, "%s swap_meter %d %d\n", MEM_CONFIG_KEYWORD,
				swap.enabled, swap.label_is_data);
	fprintf(f, "%s swap_chart %d %d\n", MEM_CONFIG_KEYWORD,
				swap_chart.enabled, swap_chart.extra_info);
	gkrellm_save_chartconfig(f, swap_chart.chart_config,
				MEM_CONFIG_KEYWORD, NULL);

	fprintf(f, "%s mem_launch %s\n", MEM_CONFIG_KEYWORD,
				mem.launch.command);
	fprintf(f, "%s mem_tooltip %s\n", MEM_CONFIG_KEYWORD,
				mem.launch.tooltip_comment);
	fprintf(f, "%s mem_data_format %s\n", MEM_CONFIG_KEYWORD,mem.data_format);

	fprintf(f, "%s swap_launch %s\n", MEM_CONFIG_KEYWORD,
				swap.launch.command);
	fprintf(f, "%s swap_tooltip %s\n", MEM_CONFIG_KEYWORD,
				swap.launch.tooltip_comment);
	fprintf(f, "%s swap_data_format %s\n", MEM_CONFIG_KEYWORD,
				swap.data_format);

	fprintf(f, "%s text_format %s\n", MEM_CONFIG_KEYWORD, text_format);

	if (mem.alert)
		gkrellm_save_alertconfig(f, mem.alert, MEM_CONFIG_KEYWORD, "mem");
	if (swap.alert)
		gkrellm_save_alertconfig(f, swap.alert, MEM_CONFIG_KEYWORD, "swap");
	}

static void
load_meminfo_config(gchar *arg)
	{
	MeminfoMeter *m = NULL;
	gchar		config[32], name[16], item[CFG_BUFSIZE], item1[CFG_BUFSIZE];
	gint		n;

	n = sscanf(arg, "%31s %[^\n]", config, item);
	if (n == 2)
		{
		if (strcmp(config, "mem_meter") == 0)
			{
			sscanf(item, "%d %d %d", &mem.enabled,
					&mem.label_is_data, &mem.all_krells);
			if (mem.label_is_data)
				mem.restore_label = TRUE;
			}
		else if (strcmp(config, "swap_meter") == 0)
			{
			sscanf(item, "%d %d", &swap.enabled, &swap.label_is_data);
			if (swap.label_is_data)
				swap.restore_label = TRUE;
			}
		else if (strcmp(config, "swap_chart") == 0)
			sscanf(item, "%d %d", &swap_chart.enabled, &swap_chart.extra_info);
		else if (!strcmp(config, GKRELLM_CHARTCONFIG_KEYWORD))
			gkrellm_load_chartconfig(&swap_chart.chart_config, item, 2);
		else if (!strcmp(config, "mem_launch"))
			mem.launch.command = g_strdup(item);
		else if (!strcmp(config, "mem_tooltip"))
			mem.launch.tooltip_comment = g_strdup(item);
		else if (!strcmp(config, "mem_data_format"))
			gkrellm_locale_dup_string(&mem.data_format, item,
						&mem.data_format_shadow);
		else if (!strcmp(config, "swap_launch"))
			swap.launch.command = g_strdup(item);
		else if (!strcmp(config, "swap_tooltip"))
			swap.launch.tooltip_comment = g_strdup(item);
		else if (!strcmp(config, "swap_data_format"))
			gkrellm_locale_dup_string(&swap.data_format, item,
						&swap.data_format_shadow);
		else if (!strcmp(config, "text_format"))
			gkrellm_locale_dup_string(&text_format, item, &text_format_locale);
		else if (!strcmp(config, GKRELLM_ALERTCONFIG_KEYWORD))
			{
			if (sscanf(item, "%15s %[^\n]", name, item1) == 2)
				{
				if (!strcmp(name, "mem"))
					m = &mem;
				else if (!strcmp(name, "swap"))
					m = & swap;
				if (m)
					{
					if (!m->alert)
						create_alert(m);
					gkrellm_load_alertconfig(&m->alert, item1);
					}
				}
			}
		}
	}

/* --------------------------------------------------------------------- */

static GtkWidget	*mem_launch_entry,
					*mem_tooltip_entry,
					*swap_launch_entry,
					*swap_tooltip_entry;

static GtkWidget	*mem_format_combo,
					*swap_format_combo;

static GtkWidget	*text_format_combo;

static GtkWidget	*mem_alert_button,
					*swap_alert_button;


static void
cb_text_format(GtkWidget *widget, gpointer data)
	{
	gchar	*s;

	s = gkrellm_gtk_entry_get_text(&(GTK_COMBO(text_format_combo)->entry));
	gkrellm_locale_dup_string(&text_format, s, &text_format_locale);
	refresh_chart(&swap_chart);
	}

static void
cb_mem_enable(GtkWidget *button, gpointer data)
    {
	gboolean enabled;

	enabled = GTK_TOGGLE_BUTTON(button)->active;
	gkrellm_panel_enable_visibility(mem.panel, enabled, &mem.enabled);
	spacer_visibility();
	gtk_widget_set_sensitive(mem_alert_button, enabled);
	}

static void
cb_swap_enable(GtkWidget *button, gpointer data)
    {
	gboolean enabled;

	enabled = GTK_TOGGLE_BUTTON(button)->active;
	gkrellm_panel_enable_visibility(swap.panel, enabled, &swap.enabled);
	spacer_visibility();
	gtk_widget_set_sensitive(swap_alert_button, enabled);
	}

static void
cb_swap_chart_enable(GtkWidget *button, gpointer data)
    {
	gboolean enabled;

	enabled = GTK_TOGGLE_BUTTON(button)->active;
	if (enabled && !swap_chart.enabled)
		create_chart(&swap_chart, TRUE);
	else if (!enabled && swap_chart.enabled)
		destroy_chart(&swap_chart);
	setup_scaling(NULL, &swap_chart);
	spacer_visibility();
	}

static void
cb_launch_entry(GtkWidget *widget, gpointer data)
	{
	if (GPOINTER_TO_INT(data) == 0)
		gkrellm_apply_launcher(&mem_launch_entry, &mem_tooltip_entry,
					mem.panel, &mem.launch, gkrellm_launch_button_cb);
	else
		gkrellm_apply_launcher(&swap_launch_entry, &swap_tooltip_entry,
					swap.panel, &swap.launch, gkrellm_launch_button_cb);
	}

static void
cb_all_krells(GtkWidget *button, gpointer data)
    {
	gboolean enabled;

	enabled = GTK_TOGGLE_BUTTON(button)->active;
	if (enabled && !mem.all_krells)
		{		/* krell list order needs to be: cache, buffer, used */
		gkrellm_insert_krell(mem.panel, mem.krell_buffers, FALSE);
		gkrellm_insert_krell(mem.panel, mem.krell_cache, FALSE);
		}
	else if (!enabled && mem.all_krells)
		{
		gkrellm_remove_krell(mem.panel, mem.krell_buffers);
		gkrellm_remove_krell(mem.panel, mem.krell_cache);
		}
	mem.all_krells = enabled;
	}

static void
cb_mem_format(GtkWidget *widget, gpointer data)
	{
	gchar	*s;

	s = gkrellm_gtk_entry_get_text(&(GTK_COMBO(mem_format_combo)->entry));

	/* In case Pango markup tags, don't accept line unless valid markup.
	|  Ie, markup like <span ...> xxx </span> or <b> xxx </b>
	*/
	if (   strchr(s, '<') != NULL
		&& !pango_parse_markup(s, -1, 0, NULL, NULL, NULL, NULL)
	   )
		return;

	if (gkrellm_locale_dup_string(&mem.data_format, s,
				&mem.data_format_shadow))
		mem.decal_label->value = -1;	/* Force redraw */
	}

static void
cb_swap_format(GtkWidget *widget, gpointer data)
	{
	gchar	*s;

	s = gkrellm_gtk_entry_get_text(&(GTK_COMBO(swap_format_combo)->entry));

	if (   strchr(s, '<') != NULL
		&& !pango_parse_markup(s, -1, 0, NULL, NULL, NULL, NULL)
	   )
		return;

	if (gkrellm_locale_dup_string(&swap.data_format, s,
				&swap.data_format_shadow))
		swap.decal_label->value = -1;
	}

static void
cb_set_alert(GtkWidget *button, MeminfoMeter *m)
	{
	if (!m->alert)
		create_alert(m);
	gkrellm_alert_config_window(&m->alert);
	}

#define	DEFAULT_TEXT_FORMAT	"$T"

static gchar	*mem_info_text[] =
{
N_("<h>Used and Free\n"),
N_("The used and free memory here are calculated from the kernel reported\n"
"used and free by subtracting or adding the buffers and cache memory.  See\n"
"the README and compare to the \"-/+ buffers/cache:\" line from the free\n"
"command.  If you show three memory krells, the kernel \"raw free\" is\n"
"the space after the rightmost krell.\n"),
"\n",
N_("<h>Chart Labels\n"),
N_("Substitution variables for the format string for chart labels:\n"),
N_("\t$M    maximum chart value\n"),
N_("\t$T    total swap in blocks + swap out blocks\n"),
N_("\t$i    swap in blocks\n"),
N_("\t$o    swap out blocks\n"),
"\n",
N_("<h>Panel Labels\n"),
N_("Substitution variables for the format string for the Mem and Swap\n"
"panels (a MiB is a binary megabyte - 2^20):\n"),

N_("For memory and swap:\n"),
N_("\t$t    total MiB\n"),
N_("\t$u    used MiB\n"),
N_("\t$f    free MiB\n"),
N_("\t$U    used %\n"),
N_("\t$F    free %\n"),
N_("\t$l    the panel label"),
"\n",
N_("For memory only:\n"),
N_("\t$s    shared MiB\n"),
N_("\t$b    buffered MiB\n"),
N_("\t$c    cached MiB\n"),
"\n",
N_("The free and used variables may have a 'r' qualifier for printing\n"
   "raw free and raw used values.  For example: $fr for raw free.\n"),
"\n",
N_("Substitution variables may be used in alert commands.\n"),

"\n",
N_("<h>Mouse Button Actions:\n"),
N_("<b>\tLeft "),
N_("click on a panel to scroll a programmable display of\n"
"\t\tof memory or swap usage.\n")


};

static void
create_meminfo_tab(GtkWidget *tab_vbox)
	{
	GtkWidget		*tabs;
	GtkWidget		*vbox, *vbox1;
	GtkWidget		*table;
	GtkWidget		*hbox;
	GtkWidget		*text, *label;
	GList			*list;
	gint			i;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

/* --Options Tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Options"));

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Swap"),
				4, 0, TRUE);
    gkrellm_gtk_check_button_connected(vbox1, NULL,
				swap_chart.enabled, FALSE, FALSE, 0,
				cb_swap_chart_enable, NULL,
				_("Enable swap pages in/out chart"));
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, TRUE, 5);
    gkrellm_gtk_check_button_connected(hbox, NULL,
				swap.enabled, FALSE, FALSE, 0,
				cb_swap_enable, GINT_TO_POINTER(1),
				_("Enable swap meter"));
	gkrellm_gtk_alert_button(hbox, &swap_alert_button, FALSE, FALSE, 4, FALSE,
				cb_set_alert, &swap);
	if (!swap.enabled)
		gtk_widget_set_sensitive(swap_alert_button, FALSE);

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Memory"),
				4, 0, TRUE);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, TRUE, 5);
    gkrellm_gtk_check_button_connected(hbox, NULL,
				mem.enabled, FALSE, FALSE, 0,
				cb_mem_enable, NULL,
				_("Enable memory meter"));
	gkrellm_gtk_alert_button(hbox, &mem_alert_button, FALSE, FALSE, 4, FALSE,
				cb_set_alert, &mem);
	if (!mem.enabled)
		gtk_widget_set_sensitive(mem_alert_button, FALSE);
    gkrellm_gtk_check_button_connected(vbox1, NULL,
				mem.all_krells, FALSE, FALSE, 0,
				cb_all_krells, NULL,
		_("Show three memory krells:   [used | buffers | cache | raw free]"));

/* -- Setup tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Format String for Chart Labels"),
				4, 0, TRUE);
	text_format_combo = gtk_combo_new();
	gtk_box_pack_start(GTK_BOX(vbox1), text_format_combo, FALSE, FALSE, 2);
	list = NULL;
	list = g_list_append(list, text_format);
	list = g_list_append(list, DEFAULT_TEXT_FORMAT);
	list = g_list_append(list, "$T\\C\\f$M");
	list = g_list_append(list, "\\c\\f$M\\b$T");
	list = g_list_append(list,
				"\\ww\\C\\f$M\\D2\\f\\ai\\.$i\\D1\\f\\ao\\.$o");
	list = g_list_append(list,
				"\\ww\\C\\f$M\\D3\\f\\ai\\.$i\\D0\\f\\ao\\.$o");
	gtk_combo_set_popdown_strings(GTK_COMBO(text_format_combo), list);
	gtk_combo_set_case_sensitive(GTK_COMBO(text_format_combo), TRUE);
	g_list_free(list);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(text_format_combo)->entry),
			text_format);
	g_signal_connect(G_OBJECT(GTK_COMBO(text_format_combo)->entry), "changed",
			G_CALLBACK(cb_text_format), NULL);

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Format String for Panel Labels"),
				4, 6, TRUE);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
	mem_format_combo = gtk_combo_new();
	gtk_box_pack_start(GTK_BOX(hbox), mem_format_combo, TRUE, TRUE, 0);
	list = NULL;
	list = g_list_append(list, mem.data_format);
	list = g_list_append(list, DEFAULT_FORMAT);
	list = g_list_append(list, _("$t - $u used"));
	list = g_list_append(list, _("$t - $U"));
	list = g_list_append(list, _("$t - $u used  $s sh  $b bf  $c ca"));
	gtk_combo_set_popdown_strings(GTK_COMBO(mem_format_combo), list);
	gtk_combo_set_case_sensitive(GTK_COMBO(mem_format_combo), TRUE);
	g_list_free(list);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(mem_format_combo)->entry),
			mem.data_format);
	g_signal_connect(G_OBJECT(GTK_COMBO(mem_format_combo)->entry), "changed",
			G_CALLBACK(cb_mem_format), NULL);
	label = gtk_label_new(_("Mem"));
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
	swap_format_combo = gtk_combo_new();
	gtk_box_pack_start(GTK_BOX(hbox), swap_format_combo, TRUE, TRUE, 0);
	list = NULL;
	list = g_list_append(list, swap.data_format);
	list = g_list_append(list, DEFAULT_FORMAT);
	list = g_list_append(list, _("$t - $u used"));
	list = g_list_append(list, _("$t - $U"));
	gtk_combo_set_popdown_strings(GTK_COMBO(swap_format_combo), list);
	gtk_combo_set_case_sensitive(GTK_COMBO(swap_format_combo), TRUE);
	g_list_free(list);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(swap_format_combo)->entry),
			swap.data_format);
	g_signal_connect(G_OBJECT(GTK_COMBO(swap_format_combo)->entry), "changed",
			G_CALLBACK(cb_swap_format), NULL);
	label = gtk_label_new(_("Swap"));
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Launch Commands"),
				4, 0, TRUE);
	table = gkrellm_gtk_launcher_table_new(vbox1, 2);
	gkrellm_gtk_config_launcher(table, 0,  &mem_launch_entry,
				&mem_tooltip_entry, _("Mem"), &(mem.launch));
	g_signal_connect(G_OBJECT(mem_launch_entry), "changed",
				G_CALLBACK(cb_launch_entry), GINT_TO_POINTER(0));
	g_signal_connect(G_OBJECT(mem_tooltip_entry), "changed",
				G_CALLBACK(cb_launch_entry), GINT_TO_POINTER(0));

	gkrellm_gtk_config_launcher(table, 1,  &swap_launch_entry,
				&swap_tooltip_entry, _("Swap"), &(swap.launch));
	g_signal_connect(G_OBJECT(swap_launch_entry), "changed",
				G_CALLBACK(cb_launch_entry), GINT_TO_POINTER(1));
	g_signal_connect(G_OBJECT(swap_tooltip_entry), "changed",
				G_CALLBACK(cb_launch_entry), GINT_TO_POINTER(1));

/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	for (i = 0; i < sizeof(mem_info_text)/sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(mem_info_text[i]));
	}


  /* The meminfo monitor is a bit of a hybrid.  To provide for easy theming,
  |  the mem, swap, and swap_chart monitors are created as separate monitors,
  |  but they all have several common routines (update, config, ...).  Where
  |  a common routine is used, it is entered in only one of the GkrellmMonitor
  |  structures, and NULL is entered in the others.
  */
static GkrellmMonitor	monitor_mem =
	{
	N_("Memory"),		/* Name, for config tab.	*/
	MON_MEM,			/* Id,  0 if a plugin		*/
	create_mem,			/* The create function		*/
	update_meminfo,		/* The update function		*/
	create_meminfo_tab, /* The config tab create function	*/
	NULL, 				/* Instant apply */

	save_meminfo_config, /* Save user conifg			*/
	load_meminfo_config, /* Load user config			*/
	MEM_CONFIG_KEYWORD, /* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_mem_monitor(void)
	{
	monitor_mem.name = _(monitor_mem.name);
	mon_mem = &monitor_mem;
	mem.style_id = gkrellm_add_meter_style(mon_mem, MEM_STYLE_NAME);

	gkrellm_locale_dup_string(&mem.data_format, DEFAULT_FORMAT,
			&mem.data_format_shadow);
	mem.enabled = TRUE;

	if (setup_meminfo_interface())
		return &monitor_mem;
	return NULL;
	}


static GkrellmMonitor	monitor_swap =
	{
	NULL,			/* Name, for config tab. Done in mon_mem*/
	MON_SWAP,		/* Id,  0 if a plugin		*/
	NULL,			/* The create function		*/
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
gkrellm_init_swap_monitor(void)
	{
	mon_swap = &monitor_swap;
	swap.style_id = gkrellm_add_meter_style(mon_swap, SWAP_STYLE_NAME);

	gkrellm_locale_dup_string(&swap.data_format, DEFAULT_FORMAT,
			&swap.data_format_shadow);
	swap.enabled = TRUE;

	swap_chart.enabled = FALSE;
	swap_chart.extra_info = TRUE;
	gkrellm_locale_dup_string(&text_format, DEFAULT_TEXT_FORMAT,
					&text_format_locale);

	if (setup_meminfo_interface())	/* XXX */
		return &monitor_swap;
	return NULL;
	}
