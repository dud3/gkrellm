/* GKrellM
|  Copyright (C) 1999-2009 Bill Wilson
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

#include <math.h>


static void	set_grid_resolution_spin_button(GkrellmChart *, gint);


  /* For grid images of height 2 pixels, make room at bottom of chartdata
  |  window so both pixel lines will show.
  */
#define GRID_HEIGHT_Y_OFFSET_ADJUST(cp) \
		(( gdk_pixbuf_get_height((cp)->bg_grid_piximage->pixbuf) < 2) ? 0 : 1)

  /* Map internal y values with origin at lower left to X screen coordinates
  |  which have origin at upper left.
  */
#define Y_SCREEN_MAP(cp,y)	((cp)->h - (y) - 1)


#define	MIN_CHARTHEIGHT	5
#define	MAX_CHARTHEIGHT	200

static GList	*chart_list;

// #define DEBUG1
// #define DEBUG2
// #define DEBUG3

/* ----------------------------------------------------------------------- */

static gint
computed_index(GkrellmChart *cp, gint i)
	{
	gint	x;
	
	x = (cp->position + i + 1) % cp->w;
	return x;
	}

GList *
gkrellm_get_chart_list()
	{
	return chart_list;
	}

gint
gkrellm_get_chart_scalemax(GkrellmChart *cp)
	{
	if (!cp)
		return 0;
	return cp->scale_max;
	}

gint
gkrellm_get_current_chartdata(GkrellmChartdata *cd)
	{
	if (!cd)
		return 0;
	return cd->data[cd->chart->position];
	}

gint
gkrellm_get_chartdata_data(GkrellmChartdata *cd, gint index)
	{
	gint	x;

	if (!cd)
		return 0;
	x = computed_index(cd->chart, index);
	return cd->data[x];
	}

void
gkrellm_clear_chart(GkrellmChart *cp)
	{
	if (!cp)
		return;
	gdk_draw_drawable(cp->pixmap, _GK.draw1_GC, cp->bg_src_pixmap,
						0, 0,	0, 0,   cp->w, cp->h);
	gdk_draw_drawable(cp->bg_pixmap, _GK.draw1_GC, cp->bg_src_pixmap,
						0, 0,	0, 0,   cp->w, cp->h);
	if (cp->drawing_area->window)
		gdk_draw_drawable(cp->drawing_area->window, _GK.draw1_GC, cp->pixmap,
						0, 0,	0, 0,   cp->w, cp->h);
	}

void
gkrellm_clear_chart_pixmap(GkrellmChart *cp)
	{
	if (!cp)
		return;
	gdk_draw_drawable(cp->pixmap, _GK.draw1_GC, cp->bg_src_pixmap,
						0, 0,	0, 0,   cp->w, cp->h);
	gdk_draw_drawable(cp->bg_pixmap, _GK.draw1_GC, cp->bg_src_pixmap,
						0, 0,	0, 0,   cp->w, cp->h);
	}

void
gkrellm_clean_bg_src_pixmap(GkrellmChart *cp)
	{
	if (!cp)
		return;
	if (!gkrellm_winop_draw_rootpixmap_onto_transparent_chart(cp))
		gdk_draw_drawable(cp->bg_src_pixmap, _GK.draw1_GC,
				cp->bg_clean_pixmap, 0, 0, 0, 0, cp->w, cp->h);
	cp->bg_sequence_id += 1;
	}

void
gkrellm_draw_chart_grid_line(GkrellmChart *cp, GdkPixmap *pixmap, gint y)
	{
	gint	h;

	if (!cp)
		return;
	gdk_drawable_get_size(cp->bg_grid_pixmap, NULL, &h);
	gdk_draw_drawable(pixmap, _GK.draw1_GC,
				cp->bg_grid_pixmap, 0, 0, cp->x, y, cp->w, h);
	}

void
gkrellm_draw_chart_to_screen(GkrellmChart *cp)
	{
	/* Draw the expose pixmap onto the screen.
	*/
	if (cp && cp->drawing_area->window)
		gdk_draw_drawable(cp->drawing_area->window, _GK.draw1_GC, cp->pixmap,
						0, 0,	0, 0,   cp->w, cp->h);
	}

static void
default_draw_chart_function(GkrellmChart *cp)
	{
	if (!cp)
		return;
	gkrellm_draw_chartdata(cp);
	gkrellm_draw_chart_to_screen(cp);
	}

void
gkrellm_set_draw_chart_function(GkrellmChart *cp, void (*func)(), gpointer data)
	{
	if (!cp)
		return;
	cp->draw_chart = func;
	cp->draw_chart_data = data;
	}

void
gkrellm_scale_chartdata(GkrellmChartdata *cd, gint multiplier, gint divisor)
	{
	gint	i;

	if (!cd || !cd->data || divisor < 1)
		return;
	for (i = 0; i < cd->chart->w; ++i)
		cd->data[i] = cd->data[i] * multiplier / divisor;
	cd->previous = cd->previous * multiplier / divisor;
	}

void
gkrellm_offset_chartdata(GkrellmChartdata *cd, gint offset)
	{
	gint	i;

	if (!cd || !cd->data)
		return;
	for (i = 0; i < cd->chart->w; ++i)
		cd->data[i] = cd->data[i] + offset;
	cd->previous = cd->previous + offset;
	}

void
gkrellm_reset_chart(GkrellmChart *cp)
	{
	GList			*list;
	GkrellmChartdata *cd;
	gint			i;

	cp->scale_max = 0;
	cp->maxval = 0;
	cp->redraw_all = TRUE;
	cp->position = cp->w - 1;
	cp->primed = FALSE;

	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		cd->maxval = 0;
		cd->previous = 0;
		if (cd->data)
			for (i = 0; i < cp->w; ++i)
				cd->data[i] = 0;
		}
	}

void
gkrellm_reset_and_draw_chart(GkrellmChart *cp)
	{
	if (!cp)
		return;
	gkrellm_reset_chart(cp);
	if (cp->draw_chart)
		(*(cp->draw_chart))(cp->draw_chart_data);
	}

void
gkrellm_monotonic_chartdata(GkrellmChartdata *cd, gboolean value)
	{
	if (cd)
		cd->monotonic = value;
	}

void
gkrellm_set_chartdata_draw_style(GkrellmChartdata *cd, gint dstyle)
	{
	if (cd)
		cd->draw_style = dstyle;
	}

void
gkrellm_set_chartdata_draw_style_default(GkrellmChartdata *cd, gint dstyle)
	{
	if (cd && cd->chart->config && !cd->chart->config->config_loaded)
		cd->draw_style = dstyle;
	}

void
gkrellm_set_chartdata_flags(GkrellmChartdata *cd, gint flags)
	{
	if (cd)
		cd->flags = flags;
	}

static gint
chartdata_ycoord(GkrellmChart *cp, GkrellmChartdata *cd, gint yd)
	{
	glong	y;
	guint64	Y;
	
	if (cp->scale_max <= 0)
		cp->scale_max = 1;

	if (yd > 2000000000 / MAX_CHARTHEIGHT)
		{
		Y = (guint64) yd * (guint64) cd->h;
		y = Y / cp->scale_max;
		}
	else
		y = ((glong) yd * cd->h / cp->scale_max);

	if (y < 0)
		y = 0;
	if (y >= cd->h)
		y = cd->h - 1;
	if (cd->inverted)
		y = cd->h - y - 1;
	y += cd->y;
	return Y_SCREEN_MAP(cp, y);
	}

static void
draw_layer_grid_lines(GkrellmChart *cp)
	{
	GList			*list;
	GkrellmChartdata *cd;
	gint			y, y0, h, grid_res, lines;
	gint			active_split, current_split;
	gboolean		do_next_split, done_once_per_split, tmp;

	gdk_draw_drawable(cp->bg_pixmap, _GK.draw1_GC,
				cp->bg_src_pixmap, 0, 0,  0, 0,  cp->w, cp->h);
	do_next_split = TRUE;
	for (active_split = 0; do_next_split; ++active_split)
		{
		do_next_split = FALSE;
		done_once_per_split = FALSE;
		current_split = 0;
		for (list = cp->cd_list; list; list = list->next)
			{
			cd = (GkrellmChartdata *) list->data;
			if (cd->hide)
				continue;
			current_split += cd->split_chart;
			if (active_split != current_split)
				{
				if (current_split > active_split)
					do_next_split = TRUE;
				continue;
				}
			gdk_draw_drawable(cd->layer.pixmap, _GK.draw1_GC,
					*(cd->layer.src_pixmap), 0, 0,  0, 0,  cp->w, cp->h);

			grid_res = cp->config->grid_resolution;
			lines = cp->scale_max / grid_res;
			if (lines && cd->h / lines > 2)	/* No grids if h is too small */
				{
				for (y = 0; y <= cp->scale_max; y += grid_res)
					{
					if (   _GK.bg_grid_mode == GRID_MODE_RESTRAINED
						&& ((y == 0 && cp->y == 0) || y == cp->scale_max)
					   )
						continue;
					tmp = cd->inverted;	  /* Draw grid lines in one direction*/
					cd->inverted = FALSE; /* else, may not line up by 1 pixel*/
					y0 = chartdata_ycoord(cp, cd, y);
					cd->inverted = tmp;
					gdk_drawable_get_size(cd->layer.grid_pixmap, NULL, &h);
					gdk_draw_drawable(cd->layer.pixmap, _GK.draw1_GC,
						cd->layer.grid_pixmap, 0, 0, cp->x, y0, cp->w, h);

					if (!done_once_per_split)
						{
						gdk_drawable_get_size(cp->bg_grid_pixmap, NULL, &h);
						gdk_draw_drawable(cp->bg_pixmap, _GK.draw1_GC,
							cp->bg_grid_pixmap, 0, 0, cp->x, y0, cp->w, h);
						}
					}
				}
			if (current_split > 0 && !done_once_per_split)
				{
				y = cd->y - 1;		/* Get separator y value */
				y -= GRID_HEIGHT_Y_OFFSET_ADJUST(cp);
				if (y >= 0)
					{
					gdk_draw_drawable(cp->bg_pixmap, _GK.draw1_GC,
							_GK.bg_separator_pixmap,
							0, 0, cp->x, Y_SCREEN_MAP(cp, y),
							cp->w, _GK.bg_separator_height);
					}
				}
			done_once_per_split = TRUE;
			}
		}
	}

  /* Return TRUE as long as there is a next split with impulse data needing
  |  to be drawn.
  */
static gboolean
draw_chartdata_impulses(GkrellmChart *cp, GList *cd_list,
			gint i0, gint active_split)
	{
	GList			*list;
	GkrellmChartdata *cd;
	gint			n, x, y, y0, y1, yN, yI;
	gint			current_split;
	gboolean		need_next_split = FALSE;

	if (!cd_list)
		return FALSE;
	for (n = i0; n < cp->w; ++n)
		{
		x = computed_index(cp, n);
		y0 = y1 = -1;
		yN = yI = 0;
		current_split = 0;
		for (list = cp->cd_list; list; list= list->next)
			{
			cd = (GkrellmChartdata *) list->data;
			if (cd->hide)
				continue;
			current_split += cd->split_chart;
			if (   cd->draw_style != CHARTDATA_IMPULSE
				|| current_split != active_split
			   )
				{
				if (   current_split > active_split
					&& cd->draw_style == CHARTDATA_IMPULSE
				   )
					need_next_split = TRUE;
				continue;
				}
			if (cd->inverted)
				{
				if (y1 < 0)
					y1 = chartdata_ycoord(cp, cd, 0);
				yI += cd->data[x];
				y = chartdata_ycoord(cp, cd, yI);
				if (cd->data[x] > 0)
					gdk_draw_line(cd->data_bitmap, _GK.bit1_GC, n, y1, n, y);
				y1 = y;
				}
			else
				{
				if (y0 < 0)
					y0 = chartdata_ycoord(cp, cd, 0);
				yN += cd->data[x];
				y = chartdata_ycoord(cp, cd, yN);
				if (cd->data[x] > 0)
					gdk_draw_line(cd->data_bitmap, _GK.bit1_GC, n, y0, n, y);
				y0 = y;
				}
			}
		}
	/* Push the grided pixmaps through the data bitmaps onto the expose pixmap
	*/
	current_split = 0;
	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		if (cd->hide)
			continue;
		current_split += cd->split_chart;
		if (cd->draw_style == CHARTDATA_LINE || current_split != active_split)
			continue;
		if (cd->maxval > 0)
			{
			y0 = chartdata_ycoord(cp, cd, 0);
			y1 = chartdata_ycoord(cp, cd, cd->maxval);
			gdk_gc_set_clip_mask(_GK.draw1_GC, cd->data_bitmap);
			if (cd->inverted)
				gdk_draw_drawable(cp->pixmap, _GK.draw1_GC, cd->layer.pixmap,
						0, y0,  0, y0,  cp->w, y1 - y0 + 1);
			else
				gdk_draw_drawable(cp->pixmap, _GK.draw1_GC, cd->layer.pixmap,
						0, y1,  0, y1,  cp->w, y0 - y1 + 1);
			}
		}
	return need_next_split;
	}

static gint
fix_y(gint yn, gint yp, gint ypp)
	{
	gint	y;

	if (yp > ypp && yn < yp)
		{
		y = (yp + yn) / 2;
		if (y < ypp)
			y = ypp;
		}
	else if (yp < ypp && yn > yp)
		{
		y = (yp + yn) / 2;
		if (y > ypp)
			y = ypp;
		}
	else
		y = yp;
	return y;
	}

static void
draw_chartdata_lines(GkrellmChart *cp, GkrellmChartdata *cd, gint i0)
	{
	gint	n, x, xp, y, y0, y1;

	y0 = chartdata_ycoord(cp, cd, 0);
	for (n = i0; n < cp->w; ++n)
		{
		x = computed_index(cp, n);
		y1 = chartdata_ycoord(cp, cd, cd->data[x]);
		if (n == 0)
			cd->y_p = y1;
		else if (!cd->y_p_valid && i0 == cp->w - 1)
			{
			if ((xp = x - 1) < 0)
				xp = cp->w - 1;
			cd->y_p = cd->y_pp = chartdata_ycoord(cp, cd, cd->data[xp]);
			}
		y = fix_y(y1, cd->y_p, cd->y_pp);
		cd->y_pp = y;
		cd->y_p = y1;
		cd->y_p_valid = FALSE;	/* Need a store_chartdata to make it valid */
		if (cd->data[x] > 0 || (cd->inverted ? (y > y0) : (y < y0)))
			{
			if (y == y1)
				gdk_draw_point(cd->data_bitmap, _GK.bit1_GC, cp->x + n, y1);
			else
				gdk_draw_line(cd->data_bitmap, _GK.bit1_GC,
						cp->x + n, y, cp->x + n, y1);
			}
		}
	/* Push the grided pixmap through the data bitmap onto the expose pixmap
	*/
	if (cd->maxval > 0)
		{
		y0 = chartdata_ycoord(cp, cd, 0);
		y1 = chartdata_ycoord(cp, cd, cd->maxval);
		gdk_gc_set_clip_mask(_GK.draw1_GC, cd->data_bitmap);
		if (cd->inverted)
			gdk_draw_drawable(cp->pixmap, _GK.draw1_GC, cd->layer.pixmap,
					0, y0,  0, y0,  cp->w, y1 - y0 + 1);
		else
			gdk_draw_drawable(cp->pixmap, _GK.draw1_GC, cd->layer.pixmap,
					0, y1,  0, y1,  cp->w, y0 - y1 + 1);
		}
	}

  /* See the README for description of auto grid resolution behavior.
  */
static void
set_auto_grid_resolution(GkrellmChart *cp, gint maxval)
	{
	GkrellmChartconfig	*cf = cp->config;
	gint				grids, grid_res, maxval_base;

	if (maxval <= cp->maxval_auto_base)
		maxval = cp->maxval_auto_base;
	else
		{
		if (maxval > cp->maxval_peak)
			cp->maxval_peak = maxval;
		maxval_base = maxval / FULL_SCALE_GRIDS;
		if (maxval_base > cp->maxval_auto_base)
			cp->maxval_auto_base = maxval_base;
		}
	cp->maxval_auto = maxval;

	grids = cf->fixed_grids;
	if (grids == 0)		/* Auto grids mode */
		grid_res = gkrellm_125_sequence(cp->maxval_auto_base, cf->sequence_125,
					cf->low, cf->high, TRUE, FALSE);
	else
		{
		if (cf->auto_resolution_stick)
			maxval = cp->maxval_peak;
		grid_res = gkrellm_125_sequence(maxval / grids, cf->sequence_125,
					cf->low, cf->high, TRUE, TRUE);
		}
	if (grid_res != cf->grid_resolution)
		{
		cf->grid_resolution = grid_res;
		set_grid_resolution_spin_button(cp, grid_res);
		if (cf->cb_grid_resolution && !cf->cb_block)
			(*cf->cb_grid_resolution)(cf, cf->cb_grid_resolution_data);
		cp->redraw_all = TRUE;
		}
	cp->auto_recalibrate_delay = 0;
	}

static gboolean
auto_recalibrate(GkrellmChart *cp)
	{
	if (++cp->auto_recalibrate_delay < 10)
		return FALSE;
	cp->maxval_peak = 0;
	cp->maxval_auto_base = 0;
	return TRUE;
	}

static gint
setup_chart_scalemax(GkrellmChart *cp)
	{
	GkrellmChartconfig *cf = cp->config;
	glong			scalemax;
	gint			grid_res, i0;

	/* maxval may change at any gkrellm_store_chartdata(), so at each chart
	|  draw compute a scalemax and compare to last cp->scale_max.
	|  Redraw grided background if different.
	*/
	if (cf->auto_grid_resolution)
		{
		if (   cp->maxval != cp->maxval_auto
			&& (   cp->maxval > cp->maxval_auto
				|| cp->maxval_auto != cp->maxval_auto_base
			   )
		   )
			set_auto_grid_resolution(cp, cp->maxval);
		else if (   !cf->auto_resolution_stick
				 && cp->maxval < cp->maxval_auto_base / FULL_SCALE_GRIDS
				)
			{
			if (auto_recalibrate(cp))
				set_auto_grid_resolution(cp, cp->maxval);
			}
		else
			cp->auto_recalibrate_delay = 0;
		}
	grid_res = cf->grid_resolution;
	if (cf->fixed_grids)
		scalemax = grid_res * cf->fixed_grids;
	else	/* Auto scale to cp->maxval */
		{
		if (cp->maxval == 0)
			scalemax = grid_res;
		else
			scalemax = ((cp->maxval - 1) / grid_res + 1) * grid_res;
		if (cp->previous_total && scalemax > grid_res * FULL_SCALE_GRIDS)
			scalemax = grid_res * FULL_SCALE_GRIDS;
		}
	if (scalemax != cp->scale_max || cp->redraw_all)
		{
		cp->redraw_all = FALSE;
		i0 = 0;				/* Will draw all data on chart */
		cp->scale_max = scalemax;
		draw_layer_grid_lines(cp);
		}
	else
		i0 = cp->w - 1;		/* Will draw the last data point only */
	return i0;
	}

void
gkrellm_draw_chartdata(GkrellmChart *cp)
	{
	GList			*list;
	GkrellmChartdata *cd;
	gint			i0, active_split, current_split;
	gboolean		have_impulse_splits = FALSE;

	if (!cp)
		return;
	i0 = setup_chart_scalemax(cp);
	gdk_draw_drawable(cp->pixmap, _GK.draw1_GC, cp->bg_pixmap,
						0, 0,	0, 0,   cp->w, cp->h);

	current_split = active_split = 0;
	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		if (cd->hide)
			continue;
		current_split += cd->split_chart;
		if (!have_impulse_splits && cd->draw_style == CHARTDATA_IMPULSE)
			{
			have_impulse_splits = TRUE;
			active_split = current_split;
			}
		/* Clear the area of the data bitmaps that data will be drawn into
		*/
		gdk_draw_rectangle(cd->data_bitmap, _GK.bit0_GC, TRUE,
					i0, 0, cd->w - i0, cp->h);
		}

	for (  ; have_impulse_splits; ++active_split)
		have_impulse_splits = draw_chartdata_impulses(cp, cp->cd_list,
					i0, active_split);

	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		if (cd->draw_style == CHARTDATA_LINE && !cd->hide)
			draw_chartdata_lines(cp, cd, i0);
		}
	gdk_gc_set_clip_mask(_GK.draw1_GC, NULL);
	}

void
gkrellm_alloc_chartdata(GkrellmChart *cp)
	{
	GList			*list;
	GkrellmChartdata *cd;
	size_t			w;

	if (!cp)
		return;
	w = (size_t) cp->w;
	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		if (cd->w == w && cd->data)
			continue;
		cd->w = w;
		if (cd->data)
			g_free(cd->data);
		cd->data = (gint *) g_new0(gint, w);
		cd->maxval = 0;
		cp->position = cp->w - 1;
		cp->tail = cp->position;
		}
	cp->alloc_width = w;
	cp->maxval = 0;
	cp->scale_max = 0;
	cp->redraw_all = TRUE;
	}

static void
scroll_chartdata_bitmaps(GkrellmChart *cp)
	{
	GList			*list;
	GkrellmChartdata *cd;

	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		if (cd->hide)
			continue;
		gdk_draw_drawable(cd->data_bitmap, _GK.bit1_GC, cd->data_bitmap,
						1, 0, 0, 0, cp->w - 1, cp->h);
		}
	}

static gboolean
scan_for_impulse_maxval(GkrellmChart *cp, gint active_split)
	{
	GList			*list;
	GkrellmChartdata *cd;
	gint			N, I;
	gint			i, current_split;
	gboolean		need_next_split = FALSE;

	for (i = 0; i < cp->w; ++i)
		{
		/* N is normal and I inverted cumulative impulse data/split
		*/
		N = I = 0;
		current_split = 0;
		for (list = cp->cd_list; list; list = list->next)
			{
			cd = (GkrellmChartdata *) list->data;
			if (cd->hide)
				continue;
			current_split += cd->split_chart;
			if (   cd->draw_style != CHARTDATA_IMPULSE
				|| current_split != active_split
			   )
				{
				if (   current_split > active_split
					&& cd->draw_style == CHARTDATA_IMPULSE
				   )
					need_next_split = TRUE;
				continue;
				}
			if (cd->inverted)
				{
				I += cd->data[i];
				if (I > cd->maxval)
					cd->maxval = I;
				}
			else
				{
				N += cd->data[i];
				if (N > cd->maxval)
					cd->maxval = N;
				}
			if (N + I > cp->maxval)
				cp->maxval = N + I;
			}
		}
	return need_next_split;
	}

static void
scan_for_maxval(GkrellmChart *cp)
	{
	GList			*list;
	GkrellmChartdata *cd;
	gint			i, current_split, active_split;
	gboolean		have_impulse_splits = FALSE;

	cp->maxval = 0;
	current_split = 0;
	active_split = 0;
	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		if (cd->hide)
			continue;
		cd->maxval = 0;
		current_split += cd->split_chart;
		if (cd->draw_style == CHARTDATA_LINE)
			for (i = 0; i < cp->w; ++i)
				{
				if (cd->data[i] > cd->maxval)
				    cd->maxval = cd->data[i];
				if (cd->maxval > cp->maxval)
					cp->maxval = cd->maxval;
				}
		if (!have_impulse_splits && cd->draw_style == CHARTDATA_IMPULSE)
			{
			have_impulse_splits = TRUE;
			active_split = current_split;
			}
		}
	for ( ; have_impulse_splits; ++active_split)
		have_impulse_splits = scan_for_impulse_maxval(cp, active_split);
	}

void
gkrellm_store_chartdata(GkrellmChart *cp, gulong total, ...)
	{
	va_list			args;

	if (!cp)
		return;
	va_start(args, total);
	gkrellm_store_chartdatav(cp, total, args);
	va_end(args);
	}

void
gkrellm_store_chartdatav(GkrellmChart *cp, gulong total, va_list args)
	{
	GList			*list;
	GkrellmChartdata *cd;
	gulong			range, total_diff;
	gint			n, N_discard, I_discard, N, I;
	gint			active_split, current_split;
	gboolean		need_scan = FALSE;

	if (!cp)
		return;
	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		//FIXME: missing check for number of passed varargs passed in "args"
		cd->current = va_arg(args, gulong);
		if (!cd->monotonic)
			{
			cd->previous = 0;
			cp->previous_total = 0;		/* All or none if total is used */
			}
		/* Prime the pump.  Also handle data wrap around or reset to zero.
		*/
		if (cd->current < cd->previous || !cp->primed)
			cd->previous = cd->current;
		}
	if (total < cp->previous_total || !cp->primed)
		cp->previous_total = total;	  /* Wrap around, this store won't scale */
	total_diff = total - cp->previous_total;
	cp->previous_total = total;

	/* Increment position in circular buffer and remember the data
	|  value to be thrown out.
	*/
	cp->position = (cp->position + 1) % cp->w;
	cp->tail = (cp->tail + 1) % cp->w;
	n = cp->position;
	active_split = current_split = 0;
	N_discard = I_discard = 0;

	/* N is normal and I inverted cumulative impulse data/split
	*/
	N = I = 0;
	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		cd->discard = cd->data[cp->tail];
		cd->data[n] = (gint)(cd->current - cd->previous);
		cd->previous = cd->current;

		/* If using totals, scale the stored data to range between 0 and the
		|  max chart value.  Max chart value is number of grids * grid res.
		|  No. of grids is 5 in auto grid mode.  For plotting data as a %.
		*/
		if (total_diff > 0)
			{
			range = (cp->config->fixed_grids ? cp->config->fixed_grids
							: FULL_SCALE_GRIDS) * cp->config->grid_resolution;
			if (range != total_diff)
				cd->data[n] = cd->data[n] * range / total_diff;
			}
		if (cd->hide || need_scan)
			continue;

		/* Compare discarded data to new data (accounting for stacked impulse
		|  data) and decide if a new maxval must be set (new data > maxval)
		|  or if a complete rescan of the data is needed to find a new
		|  maxval (a discard > a new).
		*/
		current_split += cd->split_chart;
		if (cd->draw_style == CHARTDATA_IMPULSE)
			{
			if (current_split != active_split)
				{
				active_split = current_split;
				N_discard = I_discard = 0;
				N = I = 0;
				}
			if (cd->inverted)
				{
				I_discard += cd->discard;
				I += cd->data[n];
				if (I_discard && I_discard >= cd->maxval)
					need_scan = TRUE;
				else if (I > cd->maxval)
					cd->maxval = I;
				}
			else
				{
				N_discard += cd->discard;
				N += cd->data[n];
				if (N_discard && N_discard >= cd->maxval)
					need_scan = TRUE;
				else if (N > cd->maxval)
					cd->maxval = N;
				}
			if (N_discard + I_discard >= cd->maxval)
				need_scan = TRUE;
			else if (N + I > cp->maxval)
				cp->maxval = N + I;
			}
		else if (cd->draw_style == CHARTDATA_LINE)
			{
			cd->y_p_valid = TRUE;
			if (cd->discard && cd->discard >= cd->maxval)
				need_scan = TRUE;
			else
				{
				if (cd->data[n] > cd->maxval)
					cd->maxval = cd->data[n];
				if (cd->maxval > cp->maxval)
					cp->maxval = cd->maxval;
				}
			}
		}
	cp->primed = TRUE;
	if (need_scan || cp->redraw_all)
		scan_for_maxval(cp);
	scroll_chartdata_bitmaps(cp);
	}



/* =================================================================== */

static void
chart_destroy_text_run_list(GkrellmChart *cp)
	{
	GList			*list;

	if (!cp || !cp->text_run_list)
		return;
	for (list = cp->text_run_list; list; list = list->next)
		g_free(((GkrellmTextRun *) list->data)->text);
	gkrellm_free_glist_and_data(&cp->text_run_list);
	}

static gint
chartdata_text_y(GkrellmChart *cp, char key, gint height,
				gint y_ink, gint shadow)
	{
	GList				*list;
	GkrellmChartdata	*cd;
	gint				n, y;

	n = key - '0';
	y = -100;
	if (n >= 0)
		{
		list = g_list_nth(cp->cd_list, n / 2);
		if (list)
			{
			cd = (GkrellmChartdata *) list->data;
			if (!cd->hide)
				{
				if (n & 1)	/* Justify 2 pixels from top of ChartData view */
					{
					y = cd->y + cd->h + y_ink - 3;
					}
				else		/* Justify to bottom of ChartData view */
					{
					y = cd->y + height + y_ink + shadow - 1;
					}
				y = Y_SCREEN_MAP(cp, y);
				}
			}
		}
	return y;
	}

void
gkrellm_chart_reuse_text_format(GkrellmChart *cp)
	{
	cp->text_format_reuse = TRUE;
	}

void
gkrellm_draw_chart_text(GkrellmChart *cp, gint style_id, gchar *str)
	{
	GList			*list;
	GkrellmTextRun	*tr;
	GkrellmTextstyle *ts, *ts_default, *ts_alt;
	gchar			c, *s, *t;
	gint			h, text_length, field_width, fw;
	gint			offset;
	gint			xx, x, y, center, shadow;
	gboolean		right, set_fw, fw_right;

	if (!cp || !str)
		return;

	/* Assume text_format will be changed at each call unless caller has said
	|  we can reuse it or the whole string compares equal.
	*/
	if (   !cp->text_format_reuse
		&& !gkrellm_dup_string(&cp->text_format_string, str)
	   )
		cp->text_format_reuse = TRUE;

	if (_GK.debug_level & DEBUG_CHART_TEXT)
		{
		printf("\n");
		if (!cp->text_format_reuse)
			printf("draw_chart_text: [%s]\n", str);
		}

	if (   !cp->text_format_reuse
		|| cp->text_run_sequence_id != cp->bg_sequence_id
	   )
		chart_destroy_text_run_list(cp);
	cp->text_run_sequence_id = cp->bg_sequence_id;
	cp->text_format_reuse = FALSE;

	ts_default = gkrellm_chart_textstyle(style_id);
	ts_alt = gkrellm_chart_alt_textstyle(style_id);

	x = xx = 2;
	if (!cp->text_run_list)
		gkrellm_text_extents(ts_default->font, _("Ag8"), 3, NULL,
					&cp->h_text, &cp->baseline_ref, &cp->y_ink);

	y = 2 - cp->y_ink;
	h = fw = 0;
	tr = NULL;
	for (list = cp->text_run_list, s = str; *s; s += text_length)
		{
		if (!list)
			{
			tr = g_new0(GkrellmTextRun, 1);
			cp->text_run_list = g_list_append(cp->text_run_list, tr);
			}
		else
			{
			tr = (GkrellmTextRun *) list->data;
			list = list->next;
			tr->cache_valid = TRUE;
			}
		c = '\0';
		center = 0;
		right = FALSE;
		set_fw = FALSE;
		fw_right = FALSE;
		ts = ts_default;
		field_width = 0;
		shadow = ts_default->effect ? 1 : 0;
		while (*s == '\\')
			{
			if ((c = *(++s)) != '\0')
				++s;
			if (c == 'n')
				{
				y += cp->h_text + 1;
				x = xx;
				}
			else if (c == 'c')
				center = 1;
			else if (c == 'C')
				center = 2;
			else if (c == 'N')
				{
				x = xx;		/* A conditional newline.  If previous string */
				if (h > 2)	/* was spaces, no nl.  A space has nonzero a  */
					y += cp->h_text + 1;
				}
			else if (c == 'b')
				{
				y = cp->h - cp->h_text - cp->y_ink - 1;
				x = xx;
				}
			else if (c == 't')
				{
				y = 2 - cp->y_ink;
				x = xx;
				}
			else if (c == 'r')
				right = TRUE;
			else if (c == 'p')
				{
				y -= cp->h_text + 1;
				x = xx;
				}
			else if (c == 'w')
				set_fw = TRUE;
			else if (c == 'a')
				field_width = fw;
			else if (c == 'e')
				{
				field_width = fw;
				fw_right = TRUE;
				}
			else if (c == 'f')
				{
				ts = ts_alt;
				shadow = ts_alt->effect ? 1 : 0;
				}
			else if (c == 'x' && isdigit((unsigned char)*s))
				xx = *s++ - '0';
			else if (c == 'y' && isdigit((unsigned char)*s))
				y = *s++ - '0';
			else if (c == 'D')
				{
				y = chartdata_text_y(cp, *s++, cp->h_text, cp->y_ink, shadow);
				x = xx;
				}
			}
		t = strchr(s, (gint) '\\');
		if (t)
			text_length = t - s;
		else
			text_length = strlen(s);

		if (y == -100)	/* asked for a chartdata that is hidden */
			continue;

		if (   !tr->cache_valid || !tr->text
			|| strncmp(tr->text, s, text_length)
			|| strlen(tr->text) != text_length
		   )
			{
			gkrellm_text_extents(ts->font, s, text_length,
						&tr->w, &tr->h, &tr->baseline, &tr->y_ink);
			tr->cache_valid = FALSE;
			g_free(tr->text);
			tr->text = g_strndup(s, text_length);
			}
		h = tr->h;

		if (set_fw)
			{
			fw = tr->w + shadow;
			continue;
			}
		if (center == 1)
			x = (cp->w - tr->w) / 2;
		else if (center == 2)
			x = cp->w / 2;
		else if (fw_right)
			x = x + fw - tr->w - shadow;
		else if (right)
			x = cp->w - tr->w - 2 - shadow;
		if (text_length > 1 || (text_length == 1 && *s != ' '))
			{
			if (x != tr->x || y != tr->y)
				tr->cache_valid = FALSE;
			tr->x = x;
			tr->y = y;

			if (_GK.debug_level & DEBUG_CHART_TEXT)
				{
				gchar	buf[128];

				strncpy(buf, s, text_length);
				buf[text_length] = '\0';
				printf("draw_chart_text: [%s] ", buf);
				}
			
			offset = cp->baseline_ref - tr->baseline;	/* align baselines */
			if (_GK.chart_text_no_fill)
	    		gkrellm_draw_text(cp->pixmap, ts, x, y + offset, s,
						text_length);
			else /* Default text draw effect is fill solid and can use cache */
				{
				if (!tr->cache_valid)
					{
					if (_GK.debug_level & DEBUG_CHART_TEXT)
						printf("pango ");
					gdk_draw_drawable(cp->bg_text_pixmap, _GK.draw1_GC,
							cp->bg_pixmap,
							x - 1, y + tr->y_ink + offset - 1,
							x - 1, y + tr->y_ink + offset - 1,
							tr->w + shadow + 2, tr->h + shadow + 1);
		    		gkrellm_draw_text(cp->bg_text_pixmap, ts, x, y + offset, s,
							text_length);
					}
				if (_GK.debug_level & DEBUG_CHART_TEXT)
					printf("x=%d y=%d w=%d h=%d\n",
							x - 1, y + tr->y_ink + offset - 1,
							tr->w + shadow + 2, tr->h + shadow + 1);
				gdk_draw_drawable(cp->pixmap, _GK.draw1_GC, cp->bg_text_pixmap,
						x - 1, y + tr->y_ink + offset - 1,
						x - 1, y + tr->y_ink + offset - 1,
						tr->w + shadow + 2, tr->h + shadow + 1);
				}
			}
		if (field_width && !fw_right)
			x += (field_width > tr->w) ? field_width : tr->w;
		else
			x += tr->w + shadow;
		}
	}

gint
gkrellm_draw_chart_label(GkrellmChart *cp, GkrellmTextstyle *ts,
				gint x, gint y, gchar *s)
	{
	gint	w, h, y_ink;

	if (!cp || !ts || !s)
		return x;

	gkrellm_text_extents(ts->font, s, strlen(s), &w, &h, NULL, &y_ink);

	gdk_draw_drawable(cp->pixmap, _GK.draw1_GC, cp->bg_pixmap,
				x, y, x, y, w + ts->effect, h + ts->effect);
    gkrellm_draw_string(cp->pixmap, ts, x, y - y_ink, s);

	return x + w + ts->effect;
	}

void
gkrellm_destroy_chartdata_list(GList **cd_list_head)
	{
	GList			*list;
	GkrellmChartdata *cd;

	if (!cd_list_head)
		return;
	for (list = *cd_list_head; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		if (cd->label)
			g_free(cd->label);
		if (cd->data)
			g_free(cd->data);
		if (cd->data_bitmap)
			g_object_unref(G_OBJECT(cd->data_bitmap));
		if (cd->layer.pixmap)
			g_object_unref(G_OBJECT(cd->layer.pixmap));
		/* cd->layer.src_pixmap & cd->layer.grid_pixmap must be handled by
		|  creating monitor.
		*/
		}
	gkrellm_free_glist_and_data(cd_list_head);
	}


static void
free_chart_pixmaps(GkrellmChart *cp)
	{
	GList			*list;
	GkrellmChartdata *cd;

	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		gkrellm_free_bitmap(&cd->data_bitmap);
		gkrellm_free_pixmap(&cd->layer.pixmap);
		/* cd->layer.src_pixmap & cd->layer.grid_pixmap must be handled by
		|  creating monitor.
		*/
		}
	/* If new theme or size, the cd_list will not be destroyed so I can
	|  reuse the data arrays.   Pixmaps will be recreated.
	*/
	cp->cd_list_index = 0;

	gkrellm_free_pixmap(&cp->bg_pixmap);
	gkrellm_free_pixmap(&cp->bg_text_pixmap);
	gkrellm_free_pixmap(&cp->bg_src_pixmap);
	gkrellm_free_pixmap(&cp->bg_grid_pixmap);

	gkrellm_free_pixmap(&cp->bg_clean_pixmap);
	gkrellm_free_bitmap(&cp->bg_mask);

	gkrellm_free_pixmap(&cp->pixmap);

	gkrellm_free_pixmap(&cp->top_spacer.clean_pixmap);
	gkrellm_free_pixmap(&cp->top_spacer.pixmap);
	gkrellm_free_bitmap(&cp->top_spacer.mask);
	gkrellm_free_pixmap(&cp->bottom_spacer.clean_pixmap);
	gkrellm_free_pixmap(&cp->bottom_spacer.pixmap);
	gkrellm_free_bitmap(&cp->bottom_spacer.mask);
	}

static void
destroy_chart_data(GkrellmChart *cp)
	{
	GList			*list;
	GkrellmChartdata *cd;

	free_chart_pixmaps(cp);
	chart_destroy_text_run_list(cp);
	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		if (cd->label)
			g_free(cd->label);
		if (cd->data)
			g_free(cd->data);
		cd->label = NULL;
		cd->data = NULL;
		}
	/* Don't free the cd_list.  It is in the GkrellmChartconfig struct.
	*/
	}

  /* Destroy everything inside a chart except leave cp->config alone since
  |  the config is managed by each monitor.
  */
void
gkrellm_chart_destroy(GkrellmChart *cp)
	{
	gint	h_spacers = 0;

	if (!cp)
		return;

	if (cp->top_spacer.image)
		h_spacers = cp->top_spacer.height;
	if (cp->bottom_spacer.image)
		h_spacers += cp->bottom_spacer.height;

	gkrellm_freeze_side_frame_packing();
	if (cp->panel)
		gkrellm_panel_destroy(cp->panel);
	destroy_chart_data(cp);
	gtk_widget_destroy(cp->box);
	chart_list = g_list_remove(chart_list, cp);
	gkrellm_chartconfig_window_destroy(cp);
	if (cp->shown)
		gkrellm_monitor_height_adjust(-(cp->h + h_spacers));
	g_free(cp);
	gkrellm_thaw_side_frame_packing();
	}

void
gkrellm_chartconfig_destroy(GkrellmChartconfig **cf)
	{
	if (!cf || !*cf)
		return;
	gkrellm_destroy_chartdata_list((*cf)->chart_cd_list);
	g_free(*cf);
	*cf = NULL;
	}

void
gkrellm_chart_bg_piximage_override(GkrellmChart *cp,
			GkrellmPiximage *bg_piximage, GkrellmPiximage *bg_grid_piximage)
	{
	if (!cp || !bg_piximage || !bg_grid_piximage)
		return;
	cp->bg_piximage = bg_piximage;
	cp->bg_grid_piximage = bg_grid_piximage;
	cp->bg_piximage_override = TRUE;
	}


static void
set_chartdata_split_heights(GkrellmChart *cp)
	{
	GList			*list, *list1;
	GkrellmChartdata *cd, *cd1;
	gint			splits;
	gint			i, y0, h_avail, h_free, y_offset;

	for (i = 0, splits = 0, list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		if (cd->hide)
			continue;
		if (++i > 1 && cd->split_chart)	/* Can't split before the first one */
			++splits;
		cd->split_share = 1.0;
		for (list1 = list->next; list1; list1 = list1->next)
			{
			cd1 = (GkrellmChartdata *) list1->data;
			if (!cd1->split_chart || cd1->hide)
				continue;
			cd->split_share = cd1->split_fraction;
			break;
			}
		}
	y_offset = GRID_HEIGHT_Y_OFFSET_ADJUST(cp);
	y0 = cp->y + y_offset;
	h_avail = cp->h - cp->y - ((splits + 1) * y_offset)
				- splits * _GK.bg_separator_height;
	h_free = h_avail;
	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		if (cd->hide)
			continue;
		cd->y = y0;
		for (list1 = list->next; list1; list1 = list1->next)
			if (!((GkrellmChartdata *) list1->data)->hide)
				break;
		if (!list1)
			cd->h = h_free;
		else
			cd->h = (gint) (cd->split_share * (gfloat) h_free);
		if (cd->h < 1)
			cd->h = 1;
		if (list1 && ((GkrellmChartdata *) list1->data)->split_chart)
			{
			y0 += cd->h + _GK.bg_separator_height + y_offset;
			h_free -= cd->h;
			}
		}
	}

GkrellmChartdata *
gkrellm_add_default_chartdata(GkrellmChart *cp, gchar *label)
	{
	GdkPixmap	**src_pixmap, *grid_pixmap;

	if (!cp)
		return NULL;
	if (cp->cd_list_index & 1)
		{
		src_pixmap = &_GK.data_in_pixmap;
		grid_pixmap = _GK.data_in_grid_pixmap;
		}
	else
		{
		src_pixmap = &_GK.data_out_pixmap;
		grid_pixmap = _GK.data_out_grid_pixmap;
		}
	return gkrellm_add_chartdata(cp, src_pixmap, grid_pixmap, label);
	}

  /* Need a GdkPixmap ** because the src pixmap can change (re-rendered at
  |  size or theme changes).
  */
GkrellmChartdata *
gkrellm_add_chartdata(GkrellmChart *cp, GdkPixmap **src_pixmap,
			GdkPixmap *grid_pixmap, gchar *label)
	{
	GtkWidget		*top_win	= gkrellm_get_top_window();
	GList			*list;
	GkrellmChartdata *cd;

	if (!cp || !src_pixmap || !grid_pixmap || !label)
		return NULL;

	/* To handle theme and vert size changes without loosing data, reuse the
	|  GkrellmChartdata structs in the cd_list.
	*/
	list = g_list_nth(cp->cd_list, cp->cd_list_index++);
	if (!list)
		{
		cd = g_new0(GkrellmChartdata, 1);
		cp->cd_list = g_list_append(cp->cd_list, cd);
		cp->config->cd_list = cp->cd_list;
		cd->split_fraction = 0.5;
		}
	else
		cd = (GkrellmChartdata *) list->data;
	cd->chart = cp;
	gkrellm_dup_string(&cd->label, label);
	cd->monotonic = TRUE;
	cd->data_bitmap = gdk_pixmap_new(top_win->window, cp->w, cp->h, 1);
	cd->layer.pixmap = gdk_pixmap_new(top_win->window, cp->w, cp->h, -1);
	cd->layer.src_pixmap = src_pixmap;
	cd->layer.grid_pixmap = grid_pixmap;

	set_chartdata_split_heights(cp);
	return cd;
	}

void
gkrellm_render_data_grid_pixmap(GkrellmPiximage *im, GdkPixmap **pixmap,
			GdkColor *color)
	{
	GtkWidget   *top_win = gkrellm_get_top_window();
	gint		w, w_pixmap = 0, h = 0;

	w = gkrellm_chart_width();
	if (*pixmap)
		gdk_drawable_get_size(*pixmap, &w_pixmap, NULL);
	if (!*pixmap || w != w_pixmap)
		{
		if (im)
			{
			if ((h = gdk_pixbuf_get_height(im->pixbuf)) > 2)
				h = 2;
			gkrellm_scale_piximage_to_pixmap(im, pixmap, NULL, w, h);
			}
		else
			{
			gkrellm_free_pixmap(pixmap);
			*pixmap = gdk_pixmap_new(top_win->window, w, 1, -1);
			if (color)
				gdk_gc_set_foreground(_GK.draw1_GC, color);
			else
				gdk_gc_set_foreground(_GK.draw1_GC, &_GK.in_color_grid);
			gdk_draw_rectangle(*pixmap, _GK.draw1_GC, TRUE, 0, 0, w, 1);
			}
		}
	}

void
gkrellm_render_data_pixmap(GkrellmPiximage *im, GdkPixmap **pixmap,
			GdkColor *color, gint h)
	{
	GtkWidget   *top_win = gkrellm_get_top_window();
	gint		w, w_pixmap = 0, h_pixmap = 0;

	w = gkrellm_chart_width();
	if (*pixmap)
		gdk_drawable_get_size(*pixmap, &w_pixmap, &h_pixmap);

	if (!*pixmap || w != w_pixmap || h != h_pixmap)
		{
		if (!gkrellm_scale_piximage_to_pixmap(im, pixmap, NULL, w, h))
			{
			*pixmap = gdk_pixmap_new(top_win->window, w, h, -1);
			if (color)
				gdk_gc_set_foreground(_GK.draw1_GC, color);
			else
				gdk_gc_set_foreground(_GK.draw1_GC, &_GK.in_color_grid);
			gdk_draw_rectangle(*pixmap, _GK.draw1_GC, TRUE, 0,0,w,h);
			}
		}
	}

static void
render_default_data_pixmaps(GkrellmChart *cp)
	{
	GList		*list;
	GdkPixmap	*pixmap;
	gint		w, h, w_pixmap = 0;

	gkrellm_render_data_grid_pixmap(_GK.data_in_grid_piximage,
				&_GK.data_in_grid_pixmap, &_GK.in_color_grid);
	gkrellm_render_data_grid_pixmap(_GK.data_out_grid_piximage,
				&_GK.data_out_grid_pixmap, &_GK.out_color_grid);

	w = gkrellm_chart_width();
	pixmap = _GK.bg_separator_pixmap;
	if (pixmap)
		gdk_drawable_get_size(pixmap, &w_pixmap, NULL);
	if (!pixmap || w_pixmap != w)
		{
		if ((h = _GK.bg_separator_height) < 1 || h > 5)
			h = 2;
		if (_GK.bg_separator_piximage)
			gkrellm_scale_piximage_to_pixmap(_GK.bg_separator_piximage,
						&_GK.bg_separator_pixmap, NULL, w, h);
		else
			{
			GkrellmPiximage	*im;

			im = gkrellm_bg_panel_piximage(DEFAULT_STYLE_ID);
			gkrellm_scale_pixbuf_to_pixmap(im->pixbuf, &_GK.bg_separator_pixmap,
					NULL, w, h);
			}
		}

	h = 2;
	for (list = chart_list; list; list = list->next)
		{
		cp = (GkrellmChart *) list->data;
		if (cp->h > h)
			h = cp->h;
		}
	gkrellm_render_data_pixmap(_GK.data_in_piximage,
				&_GK.data_in_pixmap, &_GK.in_color, h);
	gkrellm_render_data_pixmap(_GK.data_out_piximage,
				&_GK.data_out_pixmap, &_GK.out_color, h);
	}

void
gkrellm_chart_setup(void)
	{
	gkrellm_free_pixmap(&_GK.data_in_pixmap);
	gkrellm_free_pixmap(&_GK.data_in_grid_pixmap);
	gkrellm_free_pixmap(&_GK.data_out_pixmap);
	gkrellm_free_pixmap(&_GK.data_out_grid_pixmap);
	gkrellm_free_pixmap(&_GK.bg_separator_pixmap);
	}

#if 0
static gint
compare_chartlist(gconstpointer a, gconstpointer b)
    {
	GkrellmChart	*cp_a	= (GkrellmChart *) a;
	GkrellmChart	*cp_b	= (GkrellmChart *) b;
	gint	result;

	if (cp_a->style_id < cp_b->style_id)
		result = -1;
	else (if cp_a->style_id > cp_b->style_id)
		result = 1;
	else
		result = 0;
	return result;
    }
#endif

static void
insert_in_chartlist(GkrellmChart *cp)
	{
	GList	*list;
	GkrellmChart	*cp_x;
	gint	position = 0;

	for (list = chart_list; list; list = list->next, ++position)
		{
		cp_x = (GkrellmChart *) list->data;
		if (cp_x->style_id > cp->style_id)
			{
			chart_list = g_list_insert(chart_list, cp, position);
			return;
			}
		}
	chart_list = g_list_append(chart_list, cp);
	}

void
gkrellm_chart_hide(GkrellmChart *cp, gboolean hide_panel)
	{
	gint	h_spacers = 0;

	if (!cp || !cp->shown)
		return;

	if (cp->top_spacer.image)
		h_spacers = cp->top_spacer.height;
	if (cp->bottom_spacer.image)
		h_spacers += cp->bottom_spacer.height;

	gtk_widget_hide(cp->box);
	gkrellm_freeze_side_frame_packing();
	if (hide_panel)
		gkrellm_panel_hide(cp->panel);
	gkrellm_monitor_height_adjust(- (cp->h + h_spacers));
	cp->shown = FALSE;
	gkrellm_thaw_side_frame_packing();
	}

void
gkrellm_chart_show(GkrellmChart *cp, gboolean show_panel)
	{
	gint	h_spacers = 0;

	if (!cp || cp->shown)
		return;

	if (cp->top_spacer.image)
		h_spacers = cp->top_spacer.height;
	if (cp->bottom_spacer.image)
		h_spacers += cp->bottom_spacer.height;

	gtk_widget_show(cp->box);
	gkrellm_freeze_side_frame_packing();
	if (show_panel)
		gkrellm_panel_show(cp->panel);
	cp->shown = TRUE;
	gkrellm_monitor_height_adjust(cp->h + h_spacers);
	gkrellm_thaw_side_frame_packing();
	}

gboolean
gkrellm_is_chart_visible(GkrellmChart *cp)
	{
	if (!cp)
		return FALSE;
	return cp->shown;
	}

gboolean
gkrellm_chart_enable_visibility(GkrellmChart *cp, gboolean new_vis,
					gboolean *current_vis)
	{
	gboolean	changed = FALSE;

	if (new_vis  && ! *current_vis)
		{
		gkrellm_chart_show(cp, TRUE);
		*current_vis  = TRUE;
		changed = TRUE;
		}
	if (!new_vis  && *current_vis)
		{
		gkrellm_chart_hide(cp, TRUE);
		*current_vis  = FALSE;
		changed = TRUE;
		}
	return changed;
	}

void
gkrellm_set_chart_height_default(GkrellmChart *cp, gint h)
	{
	if (!cp || (cp->config && cp->config->config_loaded))
		return;

	if (h < MIN_CHARTHEIGHT)
		h = MIN_CHARTHEIGHT;
	if (h > MAX_CHARTHEIGHT)
		h = MAX_CHARTHEIGHT;
	cp->h = h;
	}

static void
render_chart_margin_spacers(GkrellmChart *cp)
	{
	GkrellmMargin	*m;
	GkrellmSpacer	*ts, *bs;

	ts = &cp->top_spacer;
	bs = &cp->bottom_spacer;
	if (ts->image)
		gtk_container_remove(GTK_CONTAINER(ts->vbox), ts->image);
	if (bs->image)
		gtk_container_remove(GTK_CONTAINER(bs->vbox), bs->image);
	ts->image = bs->image = NULL;
	m = gkrellm_get_style_margins(cp->style);
	ts->piximage = cp->bg_piximage;
	ts->height   = m->top;
	bs->piximage = cp->bg_piximage;
	bs->height   = m->bottom;

	if (!gkrellm_render_spacer(ts, 0, m->top,
				_GK.frame_left_chart_overlap, _GK.frame_right_chart_overlap))
		gtk_widget_hide(ts->vbox);

	if (!gkrellm_render_spacer(bs,
				gdk_pixbuf_get_height(cp->bg_piximage->pixbuf) - m->bottom,
				m->bottom,
				_GK.frame_left_chart_overlap, _GK.frame_right_chart_overlap))
		gtk_widget_hide(bs->vbox);
	}

#if 0
static gboolean
cb_chart_map_event(GtkWidget *widget, GdkEvent *event, GkrellmChart *cp)
    {
    gdk_window_get_position(cp->drawing_area->window, NULL, &cp->y_mapped);
	if (_GK.frame_left_chart_overlap > 0 || _GK.frame_right_chart_overlap > 0)
		_GK.need_frame_packing = TRUE;
    return FALSE;
    }
#endif

static gboolean
cb_chart_size_allocate(GtkWidget *widget, GtkAllocation *size,
				GkrellmChart *cp)
    {
    gdk_window_get_position(cp->drawing_area->window, NULL, &cp->y_mapped);
	if (_GK.frame_left_chart_overlap > 0 || _GK.frame_right_chart_overlap > 0)
		_GK.need_frame_packing = TRUE;
    return FALSE;
    }

static void
render_chart_pixmaps(GkrellmChart *cp)
	{
	GkrellmPiximage	piximage;
	GkrellmMargin	*m;
	gint			h, w;

	m = gkrellm_get_style_margins(cp->style);
	w = gdk_pixbuf_get_width(cp->bg_piximage->pixbuf)
				- _GK.frame_left_chart_overlap - _GK.frame_right_chart_overlap;
	h = gdk_pixbuf_get_height(cp->bg_piximage->pixbuf) - m->top - m->bottom;

	if (   (   m->top > 0 || m->bottom > 0
			|| _GK.frame_left_chart_overlap > 0
			|| _GK.frame_right_chart_overlap > 0
		   )
		&& w > 0 && h > 0
	   )
		{
		piximage.pixbuf = gdk_pixbuf_new_subpixbuf(cp->bg_piximage->pixbuf,
				_GK.frame_left_chart_overlap, m->top, w, h);
		piximage.border = cp->bg_piximage->border;
		gkrellm_border_adjust(&piximage.border,
				-_GK.frame_left_chart_overlap, -_GK.frame_right_chart_overlap,
				-m->top, -m->bottom);
		gkrellm_scale_piximage_to_pixmap(&piximage, &cp->bg_clean_pixmap,
					&cp->bg_mask, cp->w, cp->h);
		g_object_unref(G_OBJECT(piximage.pixbuf));
		}
	else
		gkrellm_scale_piximage_to_pixmap(cp->bg_piximage,
					&cp->bg_clean_pixmap, &cp->bg_mask, cp->w, cp->h);

	gkrellm_clone_pixmap(&cp->pixmap, &cp->bg_clean_pixmap);
	gkrellm_clone_pixmap(&cp->bg_pixmap, &cp->bg_clean_pixmap);
	gkrellm_clone_pixmap(&cp->bg_src_pixmap, &cp->bg_clean_pixmap);
	gkrellm_clone_pixmap(&cp->bg_text_pixmap, &cp->bg_clean_pixmap);
	}

void
gkrellm_chart_create(GtkWidget *vbox, GkrellmMonitor *mon, GkrellmChart *cp,
			GkrellmChartconfig **config)
	{
	GkrellmMargin	*m;
	GkrellmChartconfig *cf;
	gint			h, style_id;

	if (!vbox || !mon || !cp || !config)
		return;
	if (mon->privat->style_type == CHART_PANEL_TYPE)
		style_id = mon->privat->style_id;
	else
		style_id = DEFAULT_STYLE_ID;
	cp->style = gkrellm_chart_style(style_id);
	m = gkrellm_get_style_margins(cp->style);
	cp->monitor = (gpointer) mon;
	if (!cp->bg_piximage_override)
		{
		cp->bg_piximage = gkrellm_bg_chart_piximage(style_id);
		cp->bg_grid_piximage = gkrellm_bg_grid_piximage(style_id);
		}
	cp->bg_piximage_override = FALSE;
	cp->x = 0;
/*	cp->y = 0; */
	cp->w = _GK.chart_width;
	if (!*config)
		{
		*config = gkrellm_chartconfig_new0();
		(*config)->auto_grid_resolution = TRUE;		/* the default */
		(*config)->h = cp->h;					/* In case default */
		}
	cf = cp->config = *config;
	if (cf->h < 5)
		cf->h = 40;
	cp->h = cf->h;
	if (cf->grid_resolution < 1)
		cf->grid_resolution = 1;
	cp->cd_list = cp->config->cd_list;
	cp->config->chart_cd_list = &cp->cd_list;

	if (!cp->box)
		{		
		cp->box = gtk_vbox_new(FALSE, 0);	/* not a hbox anymore !! */
		gtk_box_pack_start(GTK_BOX(vbox), cp->box, FALSE, FALSE, 0);

		cp->top_spacer.vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(cp->box), cp->top_spacer.vbox,
				FALSE, FALSE, 0);
		cp->drawing_area = gtk_drawing_area_new();
		gtk_widget_set_events(cp->drawing_area, GDK_EXPOSURE_MASK
				| GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK
				| GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK
				| GDK_POINTER_MOTION_MASK);
		gtk_box_pack_start(GTK_BOX(cp->box), cp->drawing_area,
				FALSE, FALSE, 0);
		cp->bottom_spacer.vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(cp->box), cp->bottom_spacer.vbox,
				FALSE, FALSE, 0);

		gtk_widget_show(cp->drawing_area);
		gtk_widget_show(cp->box);
		cp->shown = TRUE;
		gtk_widget_realize(cp->box);
		gtk_widget_realize(cp->drawing_area);
		cp->style_id = style_id;
		insert_in_chartlist(cp);
		cp->y_mapped = -1;
//		g_signal_connect(G_OBJECT (cp->drawing_area), "map_event",
//					G_CALLBACK(cb_chart_map_event), cp);
		g_signal_connect(G_OBJECT (cp->drawing_area), "size_allocate",
					G_CALLBACK(cb_chart_size_allocate), cp);
		}
	else
		free_chart_pixmaps(cp);

	gtk_widget_set_size_request(cp->drawing_area, cp->w, cp->h);

	cp->bg_sequence_id += 1;

	render_chart_pixmaps(cp);
	render_chart_margin_spacers(cp);

	h = gdk_pixbuf_get_height(cp->bg_grid_piximage->pixbuf);
	if (h > 2)
		h = 2;
	gkrellm_scale_piximage_to_pixmap(cp->bg_grid_piximage,
				&cp->bg_grid_pixmap, NULL, cp->w, h);

	cp->transparency = cp->style->transparency;
	_GK.any_transparency |= cp->transparency;

	gkrellm_set_draw_chart_function(cp, default_draw_chart_function, cp);
	cp->redraw_all = TRUE;
	render_default_data_pixmaps(cp);
	if (cp->shown)
		{
		gkrellm_monitor_height_adjust(cp->h + m->top + m->bottom);
		gkrellm_pack_side_frames();
		}
	}

void
gkrellm_refresh_chart(GkrellmChart *cp)
	{
	if (!cp)
		return;
	cp->redraw_all = TRUE;
	cp->maxval_auto = -1;
	if (cp->draw_chart)
		(*(cp->draw_chart))(cp->draw_chart_data);
	}

void
gkrellm_rescale_chart(GkrellmChart *cp)
	{
	if (!cp)
		return;
	scan_for_maxval(cp);
	gkrellm_refresh_chart(cp);
	}

/* =================================================================== */


static gint		map_125_table[] =
	{
	1, 2, 5,
	10, 20, 50,
	100, 200, 500,
	1000, 2000, 5000,
	10000, 20000, 50000,
	100000, 200000, 500000,
	1000000, 2000000, 5000000,
	10000000, 20000000, 50000000,
	100000000, 200000000, 500000000
	};

static gint		map_12357_table[] =
	{
	1, 2, 3, 5, 7,
	10, 15, 20, 30, 50, 70,
	100, 150, 200, 300, 500, 700,
	1000, 1500, 2000, 3000, 5000, 7000,
	10000, 15000, 20000, 30000, 50000, 70000,
	100000, 150000, 200000, 300000, 500000, 700000,
	1000000, 1500000, 2000000, 3000000, 5000000, 7000000,
	10000000, 15000000, 20000000, 30000000, 50000000, 70000000,
	100000000, 150000000, 200000000, 300000000, 500000000, 700000000
	};

gint
gkrellm_125_sequence(gint value, gboolean use125,
			gint low, gint high,
			gboolean snap_to_table, gboolean roundup)
	{
	gint    i, table_size;
	gint	*table;

	if (use125)
		{
		table = map_125_table;
		table_size = sizeof(map_125_table) / sizeof(gint);
		}
	else
		{
		table = map_12357_table;
		table_size = sizeof(map_12357_table) / sizeof(gint);
		}
	if (value < low)
		value = low;
	if (value > high)
		value = high;
	if (value < table[0])
		return table[0];
	if (value > table[table_size - 1])
		return table[table_size - 1];
	if (!snap_to_table && !roundup)
		{
		for (i = 0; i < table_size; ++i)
			{
/*printf("  mapping[%d] value=%d table=%d\n", i, value, table[i]); */
			if (value == table[i])
				return table[i];
			else if (value == table[i] - 1)
				return table[i - 1];
			else if (value == table[i] + 1)
				return table[i + 1];
			}
		}
	else if (snap_to_table && !roundup)
		{
		for (i = table_size - 1; i >= 0; --i)
			{
			if (value >= table[i])
				{
				value = table[i];
				break;
				}
			}
		}
	else if (snap_to_table && roundup)
		{
		for (i = 0; i < table_size; ++i)
			{
			if (value <= table[i])
				{
				value = table[i];
				break;
				}
			}
		}
	return value;
	}

static void
set_grid_resolution_spin_button(GkrellmChart *cp, gint res)
	{
	GtkSpinButton	*spin;

	if (!cp || !cp->config_window || !cp->config->grid_resolution_spin_button)
		return;
	spin = GTK_SPIN_BUTTON(cp->config->grid_resolution_spin_button);
	gtk_spin_button_set_value(spin, (gfloat) res);	
	}

static void
cb_chart_grid_resolution(GtkWidget *adjustment, GkrellmChart *cp)
	{
	GtkSpinButton	*spin;
	GkrellmChartconfig		*cf;
	gint			res;
	gfloat			fres;

	cf = cp->config;
	spin = GTK_SPIN_BUTTON(cf->grid_resolution_spin_button);
	if (cf->map_sequence)
		{
		res = gtk_spin_button_get_value_as_int(spin);
		if (res != cf->grid_resolution)
			{
			res = gkrellm_125_sequence(res, cf->sequence_125,
						cf->low, cf->high, FALSE, FALSE);
			cf->grid_resolution = res;
			gtk_spin_button_set_value(spin, (gfloat) res);
			if (cf->cb_grid_resolution && !cf->cb_block)
				(*cf->cb_grid_resolution)(cf, cf->cb_grid_resolution_data);
			gkrellm_refresh_chart(cp);
			}
		}
	else
		{
		fres = gtk_spin_button_get_value(spin);
		if (cf->spin_factor > 0.0)
			fres *= cf->spin_factor;
		cf->grid_resolution = (gint) fres;
		if (cf->grid_resolution < 1)
			cf->grid_resolution = 1;
		if (cf->cb_grid_resolution && !cf->cb_block)
			(*cf->cb_grid_resolution)(cf, cf->cb_grid_resolution_data);
		gkrellm_refresh_chart(cp);
		}
	}


/* ---- GkrellmChartconfig functions ---- */
void
gkrellm_chartconfig_callback_block(GkrellmChartconfig *cf, gboolean block)
	{
	if (!cf)
		return;
	cf->cb_block = block;
	}

void
gkrellm_set_chartconfig_grid_resolution(GkrellmChartconfig *cf, gint res)
	{
	if (!cf || res <= 0)
		return;
	cf->grid_resolution = res;
	}

gint
gkrellm_get_chartconfig_grid_resolution(GkrellmChartconfig *cf)
	{
	if (!cf)
		return 0;
	return cf->grid_resolution;
	}

void
gkrellm_chartconfig_grid_resolution_connect(GkrellmChartconfig *cf,
			void (*func)(gpointer), gpointer data)
	{
	if (!cf)
		return;
	cf->cb_grid_resolution = func;
	cf->cb_grid_resolution_data = data;
	}

void
gkrellm_set_chartconfig_flags(GkrellmChartconfig *cf, gint flags)
	{
	if (!cf)
		return;
	cf->flags = flags;
	}

void
gkrellm_chartconfig_grid_resolution_adjustment(GkrellmChartconfig *cf,
			gboolean map_sequence, gfloat spin_factor,
			gfloat low, gfloat high, gfloat step0, gfloat step1, gint digits,
			gint width)
	{
	if (!cf)
		return;
	cf->map_sequence = map_sequence;
	if ((cf->width = width) == 0)
		cf->width = 70 + log(high / 100000) * 5;
	if (map_sequence)
		{
		cf->low = 1;
		cf->low = (gfloat) gkrellm_125_sequence((gint) low, cf->sequence_125,
							low, high, TRUE, FALSE);
		cf->high = (gfloat) gkrellm_125_sequence((gint) high,
							cf->sequence_125, low, high, TRUE, TRUE);
		cf->step0 = 1.0;
		cf->step1 = 1.0;
		cf->digits = 0;
		}
	else
		{
		cf->low = low;
		cf->high = high;
		cf->step0 = step0;
		cf->step1 = step1;
		cf->digits = digits;
		cf->spin_factor = spin_factor;
		}
	if (cf->spin_factor < 1.0)
		cf->spin_factor = 1.0;
	cf->adjustment_is_set = TRUE;
	}

void
gkrellm_chartconfig_grid_resolution_label(GkrellmChartconfig *cf, gchar *label)
	{
	if (!cf)
		return;
	gkrellm_dup_string(&cf->grid_resolution_label, label);
	}

void
gkrellm_set_chartconfig_auto_grid_resolution(GkrellmChartconfig *cf, gboolean ato)
	{
	if (cf)
		cf->auto_grid_resolution = ato;
	}

void
gkrellm_set_chartconfig_auto_resolution_stick(GkrellmChartconfig *cf, gboolean stick)
	{
	if (cf)
		cf->auto_resolution_stick = stick;
	}

void
gkrellm_set_chartconfig_sequence_125(GkrellmChartconfig *cf, gboolean seq)
	{
	if (cf)
		cf->sequence_125 = seq;
	}

void
gkrellm_set_chartconfig_fixed_grids(GkrellmChartconfig *cf, gint fixed_grids)
	{
	if (!cf || fixed_grids < 0 || fixed_grids > 5)
		return;
	cf->fixed_grids = fixed_grids;
	}

gint
gkrellm_get_chartconfig_fixed_grids(GkrellmChartconfig *cf)
	{
	if (!cf)
		return 0;
	return cf->fixed_grids;
	}

void
gkrellm_chartconfig_fixed_grids_connect(GkrellmChartconfig *cf,
			void (*func)(gpointer), gpointer data)
	{
	if (!cf)
		return;
	cf->cb_fixed_grids = func;
	cf->cb_fixed_grids_data = data;
	}

gint
gkrellm_get_chartconfig_height(GkrellmChartconfig *cf)
	{
	if (!cf)
		return 0;
	return cf->h;
	}

void
gkrellm_chartconfig_height_connect(GkrellmChartconfig *cf,
			void (*func)(gpointer), gpointer data)
	{
	if (!cf)
		return;
	cf->cb_height = func;
	cf->cb_height_data = data;
	}

void
gkrellm_set_chart_height(GkrellmChart *cp, gint h)
	{
	GtkWidget			*top_win = gkrellm_get_top_window();
	GtkSpinButton		*spin;
	GList				*list;
	GkrellmChartdata	*cd;
	GkrellmChartconfig	*cf;
	gint				dy;

	if (!cp || cp->h == h)
		return;
	dy = h - cp->h;
	cp->h = h;
	cp->config->h = h;
	render_default_data_pixmaps(cp);
	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		g_object_unref(G_OBJECT(cd->data_bitmap));
		g_object_unref(G_OBJECT(cd->layer.pixmap));
		cd->data_bitmap = gdk_pixmap_new(top_win->window, cp->w, cp->h, 1);
		cd->layer.pixmap = gdk_pixmap_new(top_win->window, cp->w, cp->h, -1);
		}
	cp->bg_sequence_id += 1;
	render_chart_pixmaps(cp);

	cf = cp->config;
	if (cf->cb_height && !cf->cb_block)
		(*cf->cb_height)(cf, cf->cb_height_data);
	gtk_widget_set_size_request(cp->drawing_area, cp->w, cp->h);
	set_chartdata_split_heights(cp);
	if (cp->shown)
		{
		gkrellm_monitor_height_adjust(dy);
		gkrellm_pack_side_frames();
		gkrellm_refresh_chart(cp);
		}
	if (cp->config_window)
		{
		spin = GTK_SPIN_BUTTON(cf->height_spin_button);
		if (h != gtk_spin_button_get_value_as_int(spin))
			gtk_spin_button_set_value(spin, (gfloat) h);
		}
	}

gboolean
gkrellm_get_chartdata_hide(GkrellmChartdata *cd)
	{
	if (cd && cd->hide)
		return TRUE;
	return FALSE;
	}


/* =================================================================== */

static void
chart_config_window_close(GtkWidget *widget, GkrellmChart *cp)
	{
	if (cp->config_window)
		gtk_widget_destroy(cp->config_window);
	cp->config_window = NULL;
	}

void
gkrellm_chartconfig_window_destroy(GkrellmChart *cp)
	{
	chart_config_window_close(NULL, cp);
	}

static gint
chart_config_window_delete_event(GtkWidget *widget, GdkEvent *ev,
			gpointer data)
	{
	chart_config_window_close(widget, data);
	return FALSE;
	}

static void
set_resolution_menubar_items_sensitivity(GkrellmChartconfig *cf)
	{
	GtkWidget	*w;

	if (!cf->auto_resolution_item_factory)
		return;

	w = gtk_item_factory_get_widget(cf->auto_resolution_item_factory,
				_("/Control/Auto mode sticks at peak value"));
	GTK_CHECK_MENU_ITEM(w)->active = cf->auto_resolution_stick;

	w = gtk_item_factory_get_widget(cf->auto_resolution_item_factory,
				_("/Control/Auto mode recalibrate"));
	if (cf->auto_grid_resolution)
		gtk_widget_set_sensitive(w, TRUE);
	else
		gtk_widget_set_sensitive(w, FALSE);
	}

static void
cb_chart_height(GtkWidget *adjustment, GkrellmChart *cp)
	{
	GtkSpinButton	*spin;
	gint			h;

	_GK.config_modified = TRUE;
	spin = GTK_SPIN_BUTTON(cp->config->height_spin_button);
	h = gtk_spin_button_get_value_as_int(spin);
	gkrellm_set_chart_height(cp, h);
	}

static void
cb_chart_fixed_grids(GtkWidget *adjustment, GkrellmChart *cp)
	{
	GtkSpinButton		*spin;
	GkrellmChartconfig	*cf = cp->config;

	_GK.config_modified = TRUE;
	spin = GTK_SPIN_BUTTON(cf->fixed_grids_spin_button);
	cf->fixed_grids = gtk_spin_button_get_value_as_int(spin);
	if (cf->auto_grid_resolution)
		set_auto_grid_resolution(cp, cp->maxval_auto);
	if (cf->cb_fixed_grids && !cf->cb_block)
		(*cf->cb_fixed_grids)(cf, cf->cb_fixed_grids_data);

	set_resolution_menubar_items_sensitivity(cf);

	gkrellm_refresh_chart(cp);
	}

static void
cb_line_draw_style(GtkWidget *widget, GkrellmChartdata *cd)
	{
	_GK.config_modified = TRUE;
	cd->draw_style = GTK_TOGGLE_BUTTON(widget)->active;
	gkrellm_rescale_chart(cd->chart);
	}

static void
cb_auto_resolution(GtkWidget *widget, GkrellmChart *cp)
	{
	GtkWidget			*button;
	GkrellmChartconfig	*cf = cp->config;

	_GK.config_modified = TRUE;
	cf->auto_grid_resolution = GTK_TOGGLE_BUTTON(widget)->active;

	set_resolution_menubar_items_sensitivity(cf);
	button = cf->grid_resolution_spin_button;
	if (cf->auto_grid_resolution)
		gtk_widget_set_sensitive(button, FALSE);
	else
		gtk_widget_set_sensitive(button, TRUE);
	gkrellm_rescale_chart(cp);
	}

static void
cb_inverted_draw_mode(GtkWidget *widget, GkrellmChartdata *cd)
	{
	_GK.config_modified = TRUE;
	cd->inverted = GTK_TOGGLE_BUTTON(widget)->active;
	gkrellm_rescale_chart(cd->chart);
	}

static void
cb_hide(GtkWidget *widget, GkrellmChartdata *cd)
	{
	_GK.config_modified = TRUE;
	cd->hide = GTK_TOGGLE_BUTTON(widget)->active;
	set_chartdata_split_heights(cd->chart);
	gkrellm_rescale_chart(cd->chart);
	}

static void
cb_split_mode(GtkWidget *widget, GkrellmChartdata *cd)
	{
	_GK.config_modified = TRUE;
	cd->split_chart = GTK_TOGGLE_BUTTON(widget)->active;
	gtk_widget_set_sensitive(cd->split_fraction_spin_button, cd->split_chart);
	set_chartdata_split_heights(cd->chart);
	gkrellm_rescale_chart(cd->chart);
	}

static void
cb_split_fraction(GtkWidget *adjustment, GkrellmChartdata *cd)
	{
	GtkSpinButton	*spin;

	_GK.config_modified = TRUE;
	spin = GTK_SPIN_BUTTON(cd->split_fraction_spin_button);
	cd->split_fraction = gtk_spin_button_get_value(spin);
	set_chartdata_split_heights(cd->chart);
	gkrellm_rescale_chart(cd->chart);
	}


/* =================================================================== */

static void
cb_auto_res_control(GkrellmChart *cp, guint option, GtkWidget* widget)
    {
    GkrellmChartconfig *cf;
	gint			grid_res;
	gboolean		active;

	cf = cp->config;
    switch (option)
        {
        case 0:
			cp->maxval_auto_base = 0;
			cp->maxval_peak = 0;
			break;
        case 1:
            active = GTK_CHECK_MENU_ITEM(widget)->active;
			cf->auto_resolution_stick = active;
			cp->maxval_auto_base = 0;
            break;
        case 2:
            active = GTK_CHECK_MENU_ITEM(widget)->active;
			if (cf->sequence_125 && active)
				return;
			cf->sequence_125 = active;
			break;
        case 3:
            active = GTK_CHECK_MENU_ITEM(widget)->active;
			if (!cf->sequence_125 && active)
				return;
			cf->sequence_125 = !active;

			grid_res = gkrellm_125_sequence(cf->grid_resolution,
						cf->sequence_125, cf->low, cf->high, TRUE, FALSE);
			cf->grid_resolution = grid_res;
			set_grid_resolution_spin_button(cp, grid_res);
            break;
        }
	gkrellm_refresh_chart(cp);
    }


static GtkItemFactoryEntry	auto_res_control_items[] =
    {
{N_("/Control"),	NULL,	NULL,				 0,	"<LastBranch>" },
{N_("/Control/-"),	NULL,	NULL,				 0,	"<Separator>"},
{N_("/Control/Auto mode recalibrate"),
					NULL,	cb_auto_res_control, 0, "<Item>"},
{N_("/Control/Auto mode sticks at peak value"),
					NULL,	cb_auto_res_control, 1,	"<ToggleItem>"},
{N_("/Control/-"),	NULL,    NULL,				 0,	"<Separator>"},
{N_("/Control/Sequence.../1 2 5"),
					NULL,	cb_auto_res_control, 2,	"<RadioItem>"},
{N_("/Control/Sequence.../1 1.5 2 3 5 7"),
					NULL,	cb_auto_res_control, 3,
					N_("/Control/Sequence.../1 2 5")},
{N_("/Control/-"),	 NULL,	NULL,				 0,	"<Separator>"},
    };

static void
auto_resolution_control_menubar(GtkWidget **menubar, GkrellmChart *cp)
	{
	GtkItemFactory	*item_factory;
	GkrellmChartconfig		*cf = cp->config;
	gint			i, n;
	static gboolean	translated;

	n = sizeof(auto_res_control_items) / sizeof(GtkItemFactoryEntry);
	item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", NULL);
	if (!translated)
		{
		for(i = 0; i < n; i++)
			auto_res_control_items[i].path = _(auto_res_control_items[i].path);
		auto_res_control_items[6].item_type =
				_(auto_res_control_items[6].item_type);
		translated = TRUE;
		}
	gtk_item_factory_create_items(item_factory, n, auto_res_control_items, cp);
	cf->auto_resolution_item_factory = item_factory;
	set_resolution_menubar_items_sensitivity(cf);

	GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(item_factory,
		_("/Control/Sequence.../1 2 5")))->active = cf->sequence_125;
	GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(item_factory,
		_("/Control/Sequence.../1 1.5 2 3 5 7")))->active = !cf->sequence_125;

	if (menubar)
		*menubar = gtk_item_factory_get_widget(item_factory, "<main>");
	}

void
gkrellm_chartconfig_window_create(GkrellmChart *cp)
	{
	GtkWidget			*main_vbox, *vbox, *vbox1, *vbox2, *hbox;
	GtkWidget			*button;
	GList				*list;
	GkrellmChartconfig	*cf;
	GkrellmChartdata	*cd;
	GkrellmPanel		*p;
	gchar				*s;

	if (!cp || _GK.no_config)
		return;
	if (cp->config_window)
		{
		gtk_window_present(GTK_WINDOW(cp->config_window));
		return;
		}
	cp->config_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(cp->config_window), "delete_event",
			G_CALLBACK(chart_config_window_delete_event), cp);

	p = cp->panel;
	cf = cp->config;
	if (p && p->label)
		s = p->label->string;
	else
		s = NULL;
	gtk_window_set_title(GTK_WINDOW(cp->config_window),
				_("GKrellM Chart Config"));
	gtk_window_set_wmclass(GTK_WINDOW(cp->config_window),
				"Gkrellm_conf", "Gkrellm");

	main_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(cp->config_window), 4);
	gtk_container_add(GTK_CONTAINER(cp->config_window), main_vbox);
	vbox = gkrellm_gtk_framed_vbox(main_vbox, s, 4, FALSE, 4, 3);

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	for (list = cp->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		if ((cd->flags & CHARTDATA_NO_CONFIG) == CHARTDATA_NO_CONFIG)
			continue;
		vbox1 = gkrellm_gtk_framed_vbox(hbox, cd->label, 2, TRUE, 2, 2);

		if (!(cd->flags & CHARTDATA_NO_CONFIG_DRAW_STYLE))
			{
			gkrellm_gtk_check_button(vbox1, &button, cd->draw_style, FALSE, 0,
					_("Line style"));
			g_signal_connect(G_OBJECT(button), "toggled",
					G_CALLBACK(cb_line_draw_style), cd);
			}
		if (!(cd->flags & CHARTDATA_NO_CONFIG_INVERTED))
			{
			gkrellm_gtk_check_button(vbox1, &button, cd->inverted, FALSE, 0,
					_("Inverted"));
			g_signal_connect(G_OBJECT(button), "toggled",
					G_CALLBACK(cb_inverted_draw_mode), cd);
			}
		if (list != cp->cd_list && !(cd->flags & CHARTDATA_NO_CONFIG_SPLIT))
			{
			vbox2 = gkrellm_gtk_framed_vbox(vbox1, NULL, 2, FALSE, 2, 2);
			gkrellm_gtk_check_button(vbox2, &button, cd->split_chart, FALSE, 0,
					_("Split view"));
			g_signal_connect(G_OBJECT(button), "toggled",
					G_CALLBACK(cb_split_mode), cd);
			gkrellm_gtk_spin_button(vbox2, &button, cd->split_fraction,
					0.05, 0.95, 0.01, 0.05, 2, 55,
					cb_split_fraction, cd, FALSE, "");
			gtk_widget_set_sensitive(button, cd->split_chart);
			cd->split_fraction_spin_button = button;
			}
		if (cd->flags & CHARTDATA_ALLOW_HIDE)
			{
			gkrellm_gtk_check_button(vbox1, &button, cd->hide, FALSE, 0,
					_("Hide"));
			g_signal_connect(G_OBJECT(button), "toggled",
					G_CALLBACK(cb_hide), cd);
			}
		}

	cf->auto_resolution_control_menubar = NULL;
	cf->auto_resolution_item_factory = NULL;
	cf->grid_resolution_spin_button = NULL;
	cf->fixed_grids_spin_button = NULL;

	if (cf->adjustment_is_set)
		{
		gfloat	value;

		vbox1 = gkrellm_gtk_framed_vbox(vbox, _("Resolution per Grid"),
					2, FALSE, 2, 2);
		if (cf->map_sequence)
			value = (gfloat) cf->grid_resolution;
		else
			value = cf->grid_resolution / cf->spin_factor;
		gkrellm_gtk_spin_button(vbox1, &button, value,
			cf->low, cf->high, cf->step0, cf->step1, cf->digits, cf->width,
			cb_chart_grid_resolution, cp, FALSE, cf->grid_resolution_label);
		cf->grid_resolution_spin_button = button;
		if (cp->config->auto_grid_resolution)
			gtk_widget_set_sensitive(button, FALSE);

		if (!(cp->config->flags & NO_CONFIG_AUTO_GRID_RESOLUTION))
			{
			hbox = gtk_hbox_new (FALSE, 0);
			gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
			gtk_container_add(GTK_CONTAINER(vbox1), hbox);
			gkrellm_gtk_check_button(hbox, &button,
					cp->config->auto_grid_resolution, TRUE, 0,
					_("Auto"));
			g_signal_connect(G_OBJECT(button), "toggled",
					G_CALLBACK(cb_auto_resolution), cp);

			auto_resolution_control_menubar(
					&cf->auto_resolution_control_menubar, cp);
			gtk_box_pack_start(GTK_BOX(hbox),
				cf->auto_resolution_control_menubar, FALSE, TRUE, 10);
			}
		}
	if (!(cp->config->flags & NO_CONFIG_FIXED_GRIDS))
		{
		vbox1 = gkrellm_gtk_framed_vbox(vbox, _("Number of Grids"), 2, FALSE,
				2, 2);
		gkrellm_gtk_spin_button(vbox1, &button, (gfloat) cf->fixed_grids,
				0, 5, 1.0, 1.0, 0, 50,
				cb_chart_fixed_grids, cp, FALSE,
				_("0: Auto    1-5: Constant"));
		cf->fixed_grids_spin_button = button;
		}

	vbox1 = gkrellm_gtk_framed_vbox(vbox, NULL, 2, FALSE, 2, 2);
	gkrellm_gtk_spin_button(vbox1, &button, (gfloat) cp->h,
			(gfloat) _GK.chart_height_min, (gfloat) _GK.chart_height_max,
			5.0, 10.0, 0, 50,
			cb_chart_height, cp, FALSE,
			_("Chart height"));
	cf->height_spin_button = button;

	hbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbox), 5);
	gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_OK);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(chart_config_window_close), cp);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 15);
	gtk_widget_grab_default(button);

	gtk_widget_show_all(cp->config_window);
	}

void
gkrellm_save_chartconfig(FILE *f, GkrellmChartconfig *cf, gchar *mon_keyword,
			gchar *name)
	{
	GList			*list;
	GkrellmChartdata *cd;

	if (!f || !cf || !mon_keyword)
		return;
	if (name)
		fprintf(f, "%s %s %s ", mon_keyword, GKRELLM_CHARTCONFIG_KEYWORD,name);
	else
		fprintf(f, "%s %s ", mon_keyword, GKRELLM_CHARTCONFIG_KEYWORD);
	fprintf(f, "%d %d %d %d %d %d", cf->h, cf->grid_resolution,
				cf->fixed_grids, cf->auto_grid_resolution,
				cf->auto_resolution_stick, cf->sequence_125);
	for (list = cf->cd_list; list; list = list->next)
		{
		cd = (GkrellmChartdata *) list->data;
		fprintf(f, " : %d %d %d %d %.0f",
				cd->draw_style, cd->inverted, cd->hide,
				cd->split_chart, cd->split_fraction * GKRELLM_FLOAT_FACTOR);
		}
	fprintf(f, "\n");
	}

void
gkrellm_load_chartconfig(GkrellmChartconfig **config, gchar *string,
			gint max_cd)
	{
	GList				*list;
	GkrellmChartdata	*cd;
	GkrellmChartconfig	*cf;
	gchar				*s;
	gint				index = 0;

	if (!config || !string)
		return;
	if (!*config)
		{
		*config = gkrellm_chartconfig_new0();
		(*config)->auto_grid_resolution = TRUE;		/* the default */
		}
	cf = *config;
	sscanf(string, "%d %d %d %d %d %d", &cf->h, &cf->grid_resolution,
				&cf->fixed_grids, &cf->auto_grid_resolution,
				&cf->auto_resolution_stick, &cf->sequence_125);
	for (s = strchr(string, (int) ':'); s ; s = strchr(s, (int) ':'))
		{
		++s;
		list = g_list_nth(cf->cd_list, index++);
		if (!list)
			{
			cd = g_new0(GkrellmChartdata, 1);
			cd->split_fraction = 0.5;
			cf->cd_list = g_list_append(cf->cd_list, cd);
			}
		else
			cd = (GkrellmChartdata *) list->data;
		sscanf(s, "%d %d %d %d %f",
				&cd->draw_style, &cd->inverted, &cd->hide,
				&cd->split_chart, &cd->split_fraction);
		cd->split_fraction /= _GK.float_factor;
		if (cd->split_fraction <= 0.01 || cd->split_fraction >= 0.99)
			cd->split_fraction = 0.5;

		cf->config_loaded = TRUE;
		if (max_cd && index >= max_cd)
			break;
		}
	}

void
debug_dump_chart_list()
	{
	GList			*list, *cdlist;
	GkrellmChart	*cp;
	GkrellmPanel	*p;
	GkrellmChartdata *cd;

	printf("\n");
	for (list = gkrellm_get_chart_list(); list; list = list->next)
		{
		cp = (GkrellmChart *) list->data;
		p = cp->panel;
		if (p && p->label && p->label->string)
			printf("%s [%d]: ", p->label->string, cp->style_id);
		else
			printf("(null) [%d]: ", cp->style_id);
		for (cdlist = cp->cd_list; cdlist; cdlist = cdlist->next)
			{
			cd = (GkrellmChartdata *) cdlist->data;
			printf("%s %p->data ", cd->label, cd->data);
			}
		printf("\n");
		}
	}
