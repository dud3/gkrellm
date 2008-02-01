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

static GkrellmBorder	zero_border;
static GkrellmMargin	zero_margin;

  /* Smooth the krell response with an exponential moving average.
  |  Eponential MA is quicker responding than pure moving average.
  |  a   =  2 / (period + 1)
  |  ema = ema + a * (reading - ema)
  |  Don't need floating point precision here, so do the int math in an
  |  order to minimize roundoff error and scale by 256 for some precision.
  */
static gint
exp_MA(GkrellmKrell *k)
	{
	gint	ema, p, reading, round_up;

	/* First, help the krell settle to zero and full scale. This gives
	|  on/off fast response while the ema is smoothing in between.
	*/
	if (k->reading == 0 && k->last_reading == 0)
		return 0;
	if (k->reading >= k->full_scale && k->last_reading >= k->full_scale)
		return k->full_scale;
	if (k->last_reading == 0)	/* Fast liftoff as well */
		return k->reading;
	ema = k->ema << 8;
	p   = k->period + 1;		/* Don't scale this! */
	reading = (k->reading) << 8;

	ema = ema + 2 * reading / p - 2 * ema / p;
	round_up = ((ema & 0xff) > 127) ? 1 : 0;

	return (ema >> 8) + round_up;
	}

  /* If the Krell has moved, redraw its layer on its stencil.
  */
void
gkrellm_update_krell(GkrellmPanel *p, GkrellmKrell *k, gulong value)
	{
	gint	xnew, x_src, y_src, x_dst, w1, w2, w_overlap, d, frame;

	if (!p || !k || k->full_scale == 0)
		return;
	k->value = value;
	if (value < k->previous)		/* unsigned long overflow? */
		{
		k->previous = value;
		return;
		}
	if (!k->monotonic)
		k->previous = 0;
	k->reading = (gint) (value - k->previous) * k->full_scale_expand;
	if (k->reading > k->full_scale)
		k->reading = k->full_scale;

	k->ema = (k->period > 1) ? exp_MA(k) : k->reading;
	if (k->monotonic)
		k->previous = value;
	k->last_reading = k->reading;

	xnew = k->x0 + k->ema * k->w_scale / k->full_scale;

	if (xnew == k->x0 && k->reading)
		{
		xnew = k->x0 + 1;	/* Show something for all activity */
		k->ema = 1;
		}
	if (xnew == k->x_position)
		return;
	k->x_position = xnew;


	/* If the krell has depth, pick the frame to display as function of
	|  ema.  Depth == 2 means display frame 0 at xnew == 0, and frame 1
	|  everywhere else.  Depth > 2 means same thing for frame 0,
	|  diplay frame depth - 1 at xnew == full_scale, and display other
	|  frames in between at xnew proportional to ema/full_scale.
	*/
	d = k->depth;
	if (k->ema == 0 || xnew == k->x0) /* Krell can settle before ema*/
		frame = 0;
	else if (k->ema >= k->full_scale)
		frame = d - 1;
	else
		{
		if (d == 1)
			frame = 0;
		else if (d == 2)
			frame = 1;
		else
			frame = 1 + ((d - 2) * k->ema / k->full_scale);
		}
	y_src = k->h_frame * frame;
	if (k->bar_mode)
		{
		x_src = (k->bar_mode == KRELL_EXPAND_BAR_MODE_SCALED) ? 0 : k->x0;
		x_dst = k->x0;
		w_overlap = xnew - k->x0;
		}
	else
		{
		x_src = k->x_hot - (xnew - k->x0);
		if (x_src < 0)
			x_src = 0;
		x_dst = xnew - k->x_hot;
		if (x_dst < k->x0)
			x_dst = k->x0;
		w1 = k->w - x_src;
		w2 = (k->x0 + k->w_scale + 1) - x_dst;
		w_overlap = (w2 > w1) ? w1 : w2;
		}

	/* Clear the krell stencil bitmap and draw the mask.
	*/
	gdk_draw_rectangle(k->stencil, _GK.bit0_GC, TRUE,
				0,0,  k->w, k->h_frame);
	if (k->mask)
		gdk_draw_drawable(k->stencil, _GK.bit1_GC, k->mask,
				x_src, y_src, x_dst, 0, w_overlap, k->h_frame);
	else
		gdk_draw_rectangle(k->stencil, _GK.bit1_GC,
				TRUE, x_dst, 0, w_overlap, k->h_frame);

	if (!k->modified)		/* Preserve original old draw if there has been */
		k->old_draw = k->draw;	/* no intervening draw_panel_layers() */

	k->draw.x_src = x_src;
	k->draw.y_src = y_src;
	k->draw.x_dst = x_dst;
	k->draw.y_dst = k->y0;
	k->draw.w = w_overlap;
	k->draw.h = k->h_frame;

	k->modified = TRUE;
	}

void
gkrellm_decal_get_size(GkrellmDecal *d, gint *w, gint *h)
	{
	if (!d)
		return;
	if (w)
		*w = d->w;
	if (h)
		*h = d->h;
	}

void
gkrellm_draw_decal_on_chart(GkrellmChart *cp, GkrellmDecal *d,
			gint xd, gint yd)
	{
	PangoLayout			*layout;
	PangoRectangle		ink;
	GList				*list;
	GdkRectangle		rect, *r;
	GkrellmTextstyle	*ts;
	GkrellmText			*tx;
	gint				x, y, max;

	if (!cp || !d || d->state != DS_VISIBLE)
		return;
	d->x = xd;
	d->y = yd;

	if (d->text_list)
		{
		r = &d->ink;
		if (d->modified || d->chart_sequence_id != cp->bg_sequence_id)
			{
			gdk_draw_drawable(cp->bg_text_pixmap, _GK.draw1_GC, cp->bg_pixmap,
						d->x, d->y, d->x, d->y, d->w, d->h);
			layout = gtk_widget_create_pango_layout(
						gkrellm_get_top_window(), NULL);
			memset(r, 0, sizeof(GdkRectangle));
			rect.x = d->x;
			rect.y = d->y;
			rect.width = d->w;
			rect.height = d->h;
			gdk_gc_set_clip_rectangle(_GK.text_GC, &rect);
			for (list = d->text_list; list; list = list->next)
				{
				tx = (GkrellmText *) list->data;
				ts = &tx->text_style;
				pango_layout_set_font_description(layout, ts->font);
				if (d->flags & DF_TEXT_USE_MARKUP)
					pango_layout_set_markup(layout, tx->text,strlen(tx->text));
				else
					pango_layout_set_text(layout, tx->text, strlen(tx->text));
				x = d->x + tx->x_off;
				y = d->y + tx->y_off;
				if (ts->effect)
					{
					gdk_gc_set_foreground(_GK.text_GC, &ts->shadow_color);
					gdk_draw_layout_with_colors(cp->bg_text_pixmap,
							_GK.text_GC,
							x + 1, y + 1, layout,
							&ts->shadow_color, NULL);
					}
				gdk_gc_set_foreground(_GK.text_GC, &ts->color);
				gdk_draw_layout(cp->bg_text_pixmap, _GK.text_GC, x, y, layout);

				pango_layout_get_pixel_extents(layout, &ink, NULL);
				if (r->x == 0 || ink.x + x < r->x)	/* Shadow ??? */
					r->x = ink.x + x;
				if (r->y == 0 || ink.y + y < r->y)
					r->y = ink.y + y;
				max = ink.x + x + ink.width;
				if (r->x + r->width < max)
					r->width = max - r->x;
				max = ink.y + y + ink.height;
				if (r->y + r->height < max)
					r->height = max - r->y;
				}
			gdk_gc_set_clip_rectangle(_GK.text_GC, NULL);
			g_object_unref(layout);
			d->chart_sequence_id = cp->bg_sequence_id;
			d->modified = FALSE;
			}
		gdk_draw_drawable(cp->pixmap, _GK.draw1_GC, cp->bg_text_pixmap,
					r->x, r->y, r->x, r->y, r->width, r->height);
		}
	else
		{
		gdk_gc_set_clip_mask(_GK.text_GC, d->stencil);
		gdk_gc_set_clip_origin(_GK.text_GC, xd, yd);
		gdk_draw_drawable(cp->pixmap, _GK.text_GC, d->pixmap,
					0, d->y_src, xd, yd, d->w, d->h);

		gdk_gc_set_clip_mask(_GK.text_GC, NULL);
		gdk_gc_set_clip_origin(_GK.text_GC, 0, 0);
		}
	}

void
gkrellm_draw_decal_pixmap(GkrellmPanel *p, GkrellmDecal *d, gint index)
	{
	gint	y_src;

	if (!d)
		return;
	if (d->value != index)
		{
		/* Write the new decal mask onto the panel stencil
		*/
		y_src = index * d->h;
		d->y_src = y_src;
		if (d->mask)		/* Can be NULL if no transparency	*/
			gdk_draw_drawable(d->stencil, _GK.bit1_GC, d->mask,
					0, y_src, 0, 0, d->w, d->h);
		else	/* Fill decal extent with white	*/
			gdk_draw_rectangle(d->stencil, _GK.bit1_GC,
					TRUE, 0, 0, d->w, d->h);
		d->modified = TRUE;
		}
	d->value = index;
	}

static void
decal_text_list_free(GList **text_list)
	{
	GList	*list;

	if (!text_list || !*text_list)
		return;
	for (list = *text_list; list; list = list->next)
		g_free(((GkrellmText *) list->data)->text);
	gkrellm_free_glist_and_data(text_list);
	}


  /* Draw to first GkrellmText in text_list and truncate text_list to make
  |  gkrellm_draw_decal_text() compatible with plugin code written before
  |  the text_list implementation.  For backwards compatibility, d->x_off
  |  shadows the first tx->x_off.
  |  Combining gkrellm_draw_decal_text() and gkrellm_decal_text_insert()
  |  functions on the same decal may lead to unexpected results.
  */
static void
gkrellm_draw_decal_internal(GkrellmPanel *p, GkrellmDecal *d, gchar *s,
			gint value)
	{
	GkrellmText			*tx;
	GList				*list;

	if (!s || !d || d->state == DS_INVISIBLE)
		return;
	if (!d->text_list)
		d->text_list = g_list_append(d->text_list, g_new0(GkrellmText, 1));
	list = d->text_list;
	tx = (GkrellmText *) list->data;
	if (   gkrellm_dup_string(&tx->text, s)
		|| tx->x_off_prev != d->x_off
		|| tx->text_style.font != d->text_style.font
		|| tx->text_style.color.pixel != d->text_style.color.pixel
	   )
		d->modified = TRUE;
	tx->x_off = d->x_off;
	tx->x_off_prev = d->x_off;
	tx->y_off = d->y_off - d->y_ink;
	tx->text_style = d->text_style;
	if (list->next)
		decal_text_list_free(&list->next);
	d->value = value;
	}

void
gkrellm_draw_decal_text(GkrellmPanel *p, GkrellmDecal *d, gchar *s, gint value)
	{
	d->flags &= ~DF_TEXT_USE_MARKUP;

	/* In case mixing scroll_text and draw_decal_text calls
	*/
	d->flags &= ~DF_SCROLL_TEXT_DIVERTED;
	if (d->scroll_text)
		{
		g_free(d->scroll_text);
		d->scroll_text = NULL;
		}

	gkrellm_draw_decal_internal(p, d, s, value);
	}

void
gkrellm_draw_decal_markup(GkrellmPanel *p, GkrellmDecal *d, gchar *s)
	{
	d->flags |= DF_TEXT_USE_MARKUP;

	/* In case mixing scroll_text and draw_decal_text calls
	*/
	d->flags &= ~DF_SCROLL_TEXT_DIVERTED;
	if (d->scroll_text)
		{
		g_free(d->scroll_text);
		d->scroll_text = NULL;
		}

	gkrellm_draw_decal_internal(p, d, s, 0);
	}

void
gkrellm_decal_text_get_offset(GkrellmDecal *d, gint *x, gint *y)
	{
	if (!d)
		return;
	if (x)
		*x = d->x_off;
	if (y)
		*y = d->y_off;
	}

  /* Setting offset of text decal means setting offset of first GkrellmText
  |  in text_list.  For backwards compatibility with plugin code that
  |  directly accesses d->x_off, shadow d->x_off to first tx->x_off.
  */
void
gkrellm_decal_text_set_offset(GkrellmDecal *d, gint x, gint y)
	{
	GList		*list;
	GkrellmText	*tx;

	if (!d || (d->x_off == x && d->y_off == y))
		return;

	d->x_off = x;
	d->y_off = y;
	if ((list = d->text_list) != NULL)
		{
		tx = (GkrellmText *) list->data;
		tx->x_off = x;
		tx->x_off_prev = x;
		tx->y_off = y - d->y_ink;;
		}
	d->modified = TRUE;
	}

void
gkrellm_decal_text_clear(GkrellmDecal *d)
	{
	GList		*list;
	GkrellmText	*tx;

	if (!d || ((list = d->text_list) == NULL))
		return;
	tx = (GkrellmText *) list->data;

	if (list->next)
		decal_text_list_free(&list->next);

	gkrellm_dup_string(&tx->text, "");
	tx->text_style = d->text_style;

	if (d->scroll_text)
		g_free(d->scroll_text);
	d->scroll_text = NULL;

	d->modified = TRUE;
	}

  /* Insert text into a decal's text_list at a given offset.  Using text
  |  insert functions makes d->x_off and d->y_off meaningless since offsets
  |  are in each GkrellmText's tx->x_off and tx->y_off.
  */
void
gkrellm_decal_text_insert(GkrellmDecal *d, gchar *s, GkrellmTextstyle *ts,
		gint x_off, gint y_off)
	{
	GkrellmText	*tx = NULL;

	if (!s || !d || d->state == DS_INVISIBLE)
		return;
	d->flags &= ~DF_TEXT_USE_MARKUP;
	if (!ts)
		ts = &d->text_style;
	if (   d->text_list
		&& !*((GkrellmText *) d->text_list->data)->text
	   )
		tx = (GkrellmText *) d->text_list->data;

	if (!tx)
		{
		tx = g_new0(GkrellmText, 1);
		d->text_list = g_list_append(d->text_list, tx);
		}

	gkrellm_dup_string(&tx->text, s);
	tx->x_off = x_off;
	tx->x_off_prev = x_off;
	tx->y_off = y_off - d->y_ink;
	tx->text_style = *ts;

	d->modified = TRUE;
	}

void
gkrellm_decal_text_markup_insert(GkrellmDecal *d, gchar *s,
		GkrellmTextstyle *ts, gint x_off, gint y_off)
	{
	GkrellmText	*tx = NULL;

	if (!s || !d || d->state == DS_INVISIBLE)
		return;
	d->flags |= DF_TEXT_USE_MARKUP;
	if (!ts)
		ts = &d->text_style;
	if (   d->text_list
		&& !*((GkrellmText *) d->text_list->data)->text
	   )
		tx = (GkrellmText *) d->text_list->data;

	if (!tx)
		{
		tx = g_new0(GkrellmText, 1);
		d->text_list = g_list_append(d->text_list, tx);
		}

	gkrellm_dup_string(&tx->text, s);
	tx->x_off = x_off;
	tx->x_off_prev = x_off;
	tx->y_off = y_off - d->y_ink;
	tx->text_style = *ts;

	d->modified = TRUE;
	}

void
gkrellm_decal_text_nth_inserted_set_offset(GkrellmDecal *d, gint n,
			gint x, gint y)
	{
	GList		*list;
	GkrellmText	*tx;

	if (!d || (list = g_list_nth(d->text_list, n)) == NULL)
		return;

	tx = (GkrellmText *) list->data;
	if (tx->x_off != x || tx->y_off != y)
		{
		tx->x_off = x;
		tx->x_off_prev = x;
		tx->y_off = y - d->y_ink;;
		d->modified = TRUE;
		}
	}

void
gkrellm_decal_text_nth_inserted_get_offset(GkrellmDecal *d, gint n,
			gint *x, gint *y)
	{
	GList		*list;
	GkrellmText	*tx;

	if (!d || (list = g_list_nth(d->text_list, n)) == NULL)
		return;

	tx = (GkrellmText *) list->data;
	if (x)
		*x = tx->x_off;
	if (y)
		*y = tx->y_off + d->y_ink;
	}

void
gkrellm_decal_scroll_text_align_center(GkrellmDecal *d, gboolean center)
	{
	gint	prev_center;

	if (!d)
		return;
	prev_center = d->flags & DF_SCROLL_TEXT_CENTER;
	if ((center && prev_center) || (!center && !prev_center))
		return;

	if (center)
		d->flags |= DF_SCROLL_TEXT_CENTER;
	else
		d->flags &= ~DF_SCROLL_TEXT_CENTER;
	d->modified = TRUE;
	}

void
gkrellm_decal_scroll_text_horizontal_loop(GkrellmDecal *d, gboolean loop)
	{
	gint	prev_loop;

	if (!d)
		return;
	prev_loop = d->flags & DF_SCROLL_TEXT_H_LOOP;
	if ((loop && prev_loop) || (!loop && !prev_loop))
		return;

	if (loop)
		d->flags |= DF_SCROLL_TEXT_H_LOOP;
	else
		d->flags &= ~DF_SCROLL_TEXT_H_LOOP;
	d->modified = TRUE;
	}

void
gkrellm_decal_scroll_text_vertical_loop(GkrellmDecal *d, gboolean loop)
	{
	gint	prev_loop;

	if (!d)
		return;
	prev_loop = d->flags & DF_SCROLL_TEXT_V_LOOP;
	if ((loop && prev_loop) || (!loop && !prev_loop))
		return;

	if (loop)
		d->flags |= DF_SCROLL_TEXT_V_LOOP;
	else
		d->flags &= ~DF_SCROLL_TEXT_V_LOOP;
	d->modified = TRUE;
	}

void
gkrellm_decal_scroll_text_get_size(GkrellmDecal *d, gint *w, gint *h)
	{
	if (!d)
		return;
	if (w)
		*w = d->scroll_width;
	if (h)
		*h = d->scroll_height
					+ ((d->flags & DF_SCROLL_TEXT_V_LOOP) ? d->y_ink : 0);
	}

static PangoLayout *
decal_scroll_text_layout(GkrellmDecal *d, gchar *text, gint *yink)
	{
	PangoLayout			*layout;
	PangoRectangle		ink, logical;
	GkrellmTextstyle	*ts;
	gint				y_ink;

	ts = &d->text_style;
	layout = gtk_widget_create_pango_layout(gkrellm_get_top_window(), NULL);
	pango_layout_set_spacing(layout, 0);
	if (d->flags & DF_SCROLL_TEXT_CENTER)
		pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	pango_layout_set_font_description(layout, ts->font);
	if (d->flags & DF_TEXT_USE_MARKUP)
		pango_layout_set_markup(layout, text, strlen(text));
	else
		pango_layout_set_text(layout, text, strlen(text));

	pango_layout_get_pixel_extents(layout, &ink, &logical);
	d->scroll_width = logical.width + ts->effect;	/* incl trailing spaces */
	d->scroll_height = ink.height + ts->effect;
	y_ink = ink.y - logical.y;

	/* y_ink change can happen if markup up text sizes differently from sizing
	|  at decal creation.  Adjust the decal y_ink so text doesn't get bumped
	|  upward which clips top of text string.  This means the decal height
	|  should probably also be changed because now descenders might get
	|  clipped when switching back out of scrolling.
	|  But this is much less noticeable... later.
	*/
	if (d->y_ink > y_ink)
		d->y_ink = y_ink;

	if (yink)
		*yink = y_ink;
	return layout;
	}

  /* If mixing draw_decal_text and scroll_text calls, must reset decal regions
  |  in bg_text_layer_pixmap to bg_pixmap at first switch to scroll_text calls.
  */
static void
reset_text_layer_pixmap_decal_region(GkrellmPanel *p, GkrellmDecal *d)
	{
	if (d->flags & DF_MOVED)
		{
		gdk_draw_drawable(p->bg_text_layer_pixmap, _GK.draw1_GC,
					p->bg_pixmap,
					d->x_old, d->y_old, d->x_old, d->y_old, d->w, d->h);
		d->flags &= ~DF_MOVED;
		}
	else
		gdk_draw_drawable(p->bg_text_layer_pixmap, _GK.draw1_GC,
					p->bg_pixmap,
					d->x, d->y,  d->x, d->y,   d->w, d->h);
	}

  /* Draw Pango rendered text onto a pixmap which can be scrolled across
  |  a decal's extents.  So when scrolling, a slow Pango redraw at each scroll
  |  step can be avoided if the text string has not changed.
  |  There are a couple of limitations because it's impossible to generate a
  |  stencil bitmask of Pango rendered text (Pango doesn't allow writting to
  |  depth 1 and it looks at the background pixmap colors as it renders):
  |    1) A scroll text decal is not transparent and will hide any objects
  |       underneath it, so when this condition is detected, scroll drawing
  |       is diverted to Pango at each step gkrellm_draw_decal_text().
  |    2) The scroll text must be rendered onto a background which for many
  |       themes won't exactly blend with the panel background as the scroll
  |       pixmap is scrolled.  It should be at worst only slightly noticeable
  |       for most themes, but might be a problem for themes with high gradient
  |       panel backgrounds.
  */
static void
gkrellm_decal_scroll_text_set_internal(GkrellmPanel *p, GkrellmDecal *d,
			gchar *text)
	{
	GtkWidget			*top_win = gkrellm_get_top_window();
	PangoLayout			*layout;
	GdkPixbuf			*pixbuf;
	GList				*list;
	GkrellmDecal		*dcheck;
	GkrellmBorder		*b;
	GkrellmTextstyle	*ts;
	gint				dx, y_ink;
	gboolean			new_text, no_scroll_caching, first_scroll;

	if (!p || !text || !d || d->state == DS_INVISIBLE)
		return;

	first_scroll = (d->scroll_text == NULL);

	new_text = gkrellm_dup_string(&d->scroll_text, text);
	no_scroll_caching = (p->transparency || p->scroll_text_cache_off);

	for (list = p->decal_list; list; list = list->next)
		{
		dcheck = (GkrellmDecal *) list->data;

		/* Scroll text underneath is OK */
		if (dcheck == d && !no_scroll_caching)
			break;

		/* But scroll text overlapping and on top of any other decal requires
		|  scroll text to be pushed.
		*/
		if (   no_scroll_caching
			|| (   d->x + d->w > dcheck->x
				&& d->x < dcheck->x + dcheck->w
				&& d->y + d->h > dcheck->y
				&& d->y < dcheck->y + dcheck->h
			   )
		   )
			{
			if (first_scroll && d->text_list)
				reset_text_layer_pixmap_decal_region(p, d);

			d->flags |= (DF_TEXT_OVERLAPS | DF_SCROLL_TEXT_DIVERTED);
			if (new_text)
				{
				layout = decal_scroll_text_layout(d, text, NULL);
				g_object_unref(G_OBJECT(layout));
				}
			gkrellm_draw_decal_internal(p, d, text, -1);
			return;
			}
		}
	if (d->text_list)	/* Erase any previous gkrellm_draw_decal_text()	*/
		{
		reset_text_layer_pixmap_decal_region(p, d);
		decal_text_list_free(&d->text_list);
		d->value = -1;
		}
	d->modified = TRUE;

	if (!new_text && !(d->flags & DF_SCROLL_TEXT_DIVERTED))
		return;

	layout = decal_scroll_text_layout(d, text, &y_ink);
	gkrellm_free_pixmap(&d->pixmap);
	d->pixmap = gdk_pixmap_new(top_win->window,
					d->scroll_width, d->scroll_height, -1);

	if (!d->pixmap)		/* too wide maybe? */
		{
		d->flags |= (DF_TEXT_OVERLAPS | DF_SCROLL_TEXT_DIVERTED);
		g_object_unref(G_OBJECT(layout));
		gkrellm_draw_decal_text(p, d, text, -1);
		return;
		}
	d->flags &= ~(DF_TEXT_OVERLAPS | DF_SCROLL_TEXT_DIVERTED);

	dx = d->w - d->scroll_width;

	if (dx >= 0)
		pixbuf = gdk_pixbuf_get_from_drawable(NULL, p->bg_pixmap, NULL,
				d->x + ((d->flags & DF_SCROLL_TEXT_CENTER) ? dx / 2 : 0), d->y,
				0, 0, d->scroll_width, d->h);
	else
		{
		gint	xfudge, yfudge;	/* Many themes have sloppy margins XXX */

		xfudge = d->w > 20 ? d->w / 6 : 0;	/* and gradient compensate a bit */
		yfudge = d->h > 5 ? d->h / 5 : 0;
		pixbuf = gdk_pixbuf_get_from_drawable(NULL, p->bg_pixmap, NULL,
				d->x + xfudge, d->y + yfudge, 0, 0,
				d->w - 2 * xfudge, d->h - 2 * yfudge);
		}

	if (pixbuf)
		{
		gkrellm_paste_pixbuf(pixbuf, d->pixmap, 0, 0,
					d->scroll_width, d->scroll_height);
		g_object_unref(G_OBJECT(pixbuf));
		}
	else
		{
		b = &p->bg_piximage->border;
		gkrellm_paste_pixbuf(p->bg_piximage->pixbuf, d->pixmap,
					b->left, b->top,
					d->scroll_width + b->left + b->right,
					d->scroll_height + b->top + b->bottom); /* XXX */
		}

	ts = &d->text_style;
	if (ts->effect)
		{
		gdk_gc_set_foreground(_GK.text_GC, &ts->shadow_color);
		gdk_draw_layout_with_colors(d->pixmap, _GK.text_GC,
					1, -y_ink + 1, layout,
					&ts->shadow_color, NULL);
		}
	gdk_gc_set_foreground(_GK.text_GC, &ts->color);
	gdk_draw_layout(d->pixmap, _GK.text_GC, 0, -y_ink, layout);

	g_object_unref(G_OBJECT(layout));
	}

void
gkrellm_decal_scroll_text_set_markup(GkrellmPanel *p, GkrellmDecal *d,
			gchar *text)
	{
	if (!d)
		return;
	d->flags |= DF_TEXT_USE_MARKUP;
	gkrellm_decal_scroll_text_set_internal(p, d, text);
	}

void
gkrellm_decal_scroll_text_set_text(GkrellmPanel *p, GkrellmDecal *d,
			gchar *text)
	{
	if (!d)
		return;
	d->flags &= ~DF_TEXT_USE_MARKUP;
	gkrellm_decal_scroll_text_set_internal(p, d, text);
	}


void
gkrellm_move_krell_yoff(GkrellmPanel *p, GkrellmKrell *k, gint y)
	{
	if (!k)
		return;
	if (!k->modified)
		k->old_draw = k->draw;
	k->y0 = k->draw.y_dst = y;
	k->modified = TRUE;
	}

static void
_destroy_krell(GkrellmKrell *k)
	{
	if (!k)
		return;
	gkrellm_free_bitmap(&k->stencil);
	gkrellm_free_pixmap(&k->pixmap);
	gkrellm_free_bitmap(&k->mask);
	g_free(k);
	}

void
gkrellm_destroy_krell(GkrellmKrell *k)
	{
	if (!k)
		return;
	gkrellm_remove_krell((GkrellmPanel *) k->panel, k);
	_destroy_krell(k);
	}

void
gkrellm_remove_krell(GkrellmPanel *p, GkrellmKrell *k)
	{
	GkrellmDrawrec	*dr;

	if (!k || !k->panel || !p || !g_list_find(p->krell_list, k))
		return;
	p->krell_list = g_list_remove(p->krell_list, k);
	k->panel = NULL;
	dr = &k->draw;
	if (p->pixmap && p->bg_pixmap)
		{
		gdk_draw_drawable(p->pixmap, _GK.draw1_GC, p->bg_pixmap,
			dr->x_dst, dr->y_dst, dr->x_dst, dr->y_dst, dr->w, dr->h);
		if (p->drawing_area && p->drawing_area->window)
			gdk_draw_drawable(p->drawing_area->window, _GK.draw1_GC,
				p->bg_pixmap,
				dr->x_dst, dr->y_dst, dr->x_dst, dr->y_dst, dr->w, dr->h);
		gkrellm_draw_panel_layers_force(p);
		}
	}

void
gkrellm_insert_krell(GkrellmPanel *p, GkrellmKrell *k, gboolean append)
	{
	if (!k || k->panel || !p || g_list_find(p->krell_list, k))
		return;
	if (append)
		p->krell_list = g_list_append(p->krell_list, k);
	else
		p->krell_list = g_list_prepend(p->krell_list, k);
	k->panel = (GkrellmPanel *) p;
	gkrellm_draw_panel_layers_force(p);
	}

void
gkrellm_insert_krell_nth(GkrellmPanel *p, GkrellmKrell *k, gint n)
	{
	if (!p || !k || g_list_find(p->krell_list, k))
		return;
	p->krell_list = g_list_insert(p->krell_list, k, n);
	k->panel = (GkrellmPanel *) p;
	gkrellm_draw_panel_layers_force(p);
	}

void
gkrellm_destroy_krell_list(GkrellmPanel *p)
	{
	GList	*list;

	if (!p)
		return;
	for (list = p->krell_list; list; list = list->next)
		_destroy_krell((GkrellmKrell *) list->data);
	if (p->krell_list)
		g_list_free(p->krell_list);
	p->krell_list = NULL;
	}

void
gkrellm_set_krell_margins(GkrellmPanel *p, GkrellmKrell *k,
				gint left, gint right)
	{
	if (!k)
		return;
	if (left > _GK.chart_width - 3)
		left = _GK.chart_width - 3;
	k->x0 = left;
	k->w_scale  = _GK.chart_width - k->x0 - right - 1;
	if (k->w_scale < 2)
		k->w_scale = 2;
	if (p && p->pixmap && p->bg_pixmap)
		{
		/* Move the krell proportionally between the new limits
		*/
		k->x_position = -1;
		if (k->monotonic)
			k->previous -= k->value;
		gkrellm_update_krell(p, k, k->value);
		gkrellm_draw_panel_layers_force(p);
		}
	}

void
gkrellm_set_krell_full_scale(GkrellmKrell *k, gint full_scale, gint expand)
	{
	if (!k)
		return;
	k->full_scale_expand = (expand <= 0 ) ? 1 : expand;
	k->full_scale = full_scale * k->full_scale_expand;
	}
	
GkrellmKrell *
gkrellm_create_krell(GkrellmPanel *p, GkrellmPiximage *im, GkrellmStyle *style)
	{
	GtkWidget		*top_win = gkrellm_get_top_window();
	GkrellmKrell	*k;
	gint			w, h, h_render, w_render;

	k = (GkrellmKrell *) g_new0(GkrellmKrell, 1);
	if (p)
		p->krell_list = g_list_append(p->krell_list, k);
	k->panel = (gpointer) p;
	k->bar_mode = k->flags = 0;

	if (im == NULL || style == NULL)
		{
		printf(_("create_krell: NULL image or style\n"));
		exit(0);
		}

	/* Set left krell margin */
	k->x0 = style->krell_left_margin;
	if (k->x0 > _GK.chart_width - 3)
		k->x0 = _GK.chart_width - 3;

	/* Set right krell margin via w_scale (-1 adjust to keep x_hot visible */
	k->w_scale  = _GK.chart_width - k->x0 - style->krell_right_margin - 1;
	if (k->w_scale < 2)
		k->w_scale = 2;

	/* krell_yoff values: >= 0, put krell at krell_yoff in the panel.
	|                     == -1 put krell inside of margins at top margin.
    |                     == -2 put krell at bottom of panel
    |                     == -3 put krell inside of margins at bottom margin.
	|  For anything else, caller must gkrellm_krell_move_yoff() after create.
	|  Inside of top margin handled here, but panel configure must handle
	|  bottom of panel adjustments.
	*/
	k->y0 = style->krell_yoff;
	if (k->y0 > 0 && !style->krell_yoff_not_scalable )
		k->y0 = k->y0 * _GK.theme_scale / 100;
	if (k->y0 == -1 || k->y0 == -3)
		k->flags |= KRELL_FLAG_BOTTOM_MARGIN;
	if (k->y0 == -1)
		gkrellm_get_top_bottom_margins(style, &k->y0, NULL);
	w = gdk_pixbuf_get_width(im->pixbuf);
	h = gdk_pixbuf_get_height(im->pixbuf);

	if (style->krell_x_hot < 0)
		style->krell_x_hot = w / 2;
	w_render = w * _GK.theme_scale / 100;
	k->x_hot = style->krell_x_hot * _GK.theme_scale / 100;

	switch (style->krell_expand)
		{
		case KRELL_EXPAND_NONE:
			break;
		case KRELL_EXPAND_BAR_MODE:
			w_render = _GK.chart_width;
			k->bar_mode = KRELL_EXPAND_BAR_MODE;
			break;
		case KRELL_EXPAND_BAR_MODE_SCALED:
			w_render = k->w_scale + 1;		/* undo -1 from w_scale calc */
			k->bar_mode = KRELL_EXPAND_BAR_MODE_SCALED;
			break;
		case KRELL_EXPAND_LEFT:
			if (style->krell_x_hot > 0 && style->krell_x_hot < w)
				{
				w_render = _GK.chart_width * w / style->krell_x_hot;
				k->x_hot = _GK.chart_width - 1;
				}
			break;
		case KRELL_EXPAND_LEFT_SCALED:
			if (style->krell_x_hot > 0 && style->krell_x_hot < w)
				{
				w_render = (k->w_scale + 1) * w / style->krell_x_hot;
				k->x_hot = k->w_scale;
				}
			break;
		case KRELL_EXPAND_RIGHT:
			if (w > style->krell_x_hot)
				{
				w_render = _GK.chart_width * w / (w - style->krell_x_hot);
				k->x_hot = style->krell_x_hot * w_render / w;
				}
			break;
		case KRELL_EXPAND_RIGHT_SCALED:
			if (w > style->krell_x_hot)
				{
				w_render = (k->w_scale + 1) * w / (w - style->krell_x_hot);
				k->x_hot = style->krell_x_hot * w_render / w;
				}
			break;
		}
	k->depth = style->krell_depth;
	if (k->depth < 1)
		k->depth = 1;
	k->h_frame = h / k->depth * _GK.theme_scale / 100;
	if (k->h_frame < 1)
		k->h_frame = 1;
	h_render = k->h_frame * k->depth;

	gkrellm_scale_piximage_to_pixmap(im, &k->pixmap, &k->mask,
				w_render, h_render);

	k->h = h_render;
	k->w = w_render;

	k->stencil = gdk_pixmap_new(top_win->window, _GK.chart_width,
					k->h_frame, 1);
	k->period = style->krell_ema_period;
	if (k->period >= 4 * _GK.update_HZ)
		k->period = 4 * _GK.update_HZ;

	k->x_position = -1;      /* Force initial draw   */
	k->full_scale_expand = 1;
	k->monotonic = TRUE;
	return k;
	}

void
gkrellm_monotonic_krell_values(GkrellmKrell *k, gboolean mono)
	{
	if (k)
		k->monotonic = mono;
	}

/* -------------------------------------------------------------- */

static void
remove_from_button_list(GkrellmPanel *p, GkrellmDecalbutton *b)
	{
	if (!p || !b)
		return;
	p->button_list = g_list_remove(p->button_list, b);
	g_free(b);

	if (!p->button_list && p->drawing_area)
		{
		g_signal_handler_disconnect(G_OBJECT(p->drawing_area), p->id_press);
		g_signal_handler_disconnect(G_OBJECT(p->drawing_area), p->id_release);
		g_signal_handler_disconnect(G_OBJECT(p->drawing_area), p->id_enter);
		g_signal_handler_disconnect(G_OBJECT(p->drawing_area), p->id_leave);
		p->button_signals_connected = FALSE;
		}
	}

void
gkrellm_move_decal(GkrellmPanel *p, GkrellmDecal *d, gint x, gint y)
	{
	if (!d || (d->x == x && d->y == y))
		return;
	if (!(d->flags & DF_MOVED))	/* If no intervening draw_panel_layers() */
		{						/* then preserve original old position   */
		d->x_old = d->x;
		d->y_old = d->y;
		}
	d->x = x;
	d->y = y;
	d->flags |= DF_MOVED;
	d->modified = TRUE;

	if (p)
		p->need_decal_overlap_check = TRUE;
	}

void
gkrellm_decal_on_top_layer(GkrellmDecal *d, gboolean top)
	{
	GkrellmPanel	*p = (GkrellmPanel *) d->panel;
	if (!d)
		return;
	if (top)
		d->flags |= DF_TOP_LAYER;
	else
		d->flags &= ~DF_TOP_LAYER;
	if (p)
		p->need_decal_overlap_check = TRUE;

	d->modified = TRUE;
	}

static void
_destroy_decal(GkrellmDecal *d)
	{
	if (!d)
		return;
	if ((d->flags & DF_LOCAL_PIXMAPS) && d->pixmap)
		g_object_unref(G_OBJECT(d->pixmap));
	if (d->stencil)
		g_object_unref(G_OBJECT(d->stencil));
	if (d->text_list)
		decal_text_list_free(&d->text_list);
	if (d->scroll_text)
		g_free(d->scroll_text);
	if (d->panel)
		((GkrellmPanel *)d->panel)->need_decal_overlap_check = TRUE;

	g_free(d);
	}

static void
_remove_decal(GkrellmPanel *p, GkrellmDecal *d)
	{
	if (!g_list_find(p->decal_list, d))
		return;
	p->decal_list = g_list_remove(p->decal_list, d);
	p->need_decal_overlap_check = TRUE;
	d->panel = NULL;
	if (p->pixmap && p->bg_pixmap)
		{
		gdk_draw_drawable(p->pixmap, _GK.draw1_GC, p->bg_pixmap,
				d->x, d->y,  d->x, d->y,   d->w, d->h);
		if (p->drawing_area && p->drawing_area->window)
			gdk_draw_drawable(p->drawing_area->window, _GK.draw1_GC,
					p->bg_pixmap, d->x, d->y,  d->x, d->y,   d->w, d->h);
		gkrellm_draw_panel_layers_force(p);
		}
	}

void
gkrellm_destroy_decal(GkrellmDecal *d)
	{
	GkrellmDecalbutton	*b;

	if (d && d->panel)
		{
		if ((b = gkrellm_decal_is_button(d)) != NULL)
			gkrellm_destroy_button(b);
		else
			{
			_remove_decal((GkrellmPanel *) d->panel, d);
			_destroy_decal(d);
			}
		}
	else
		_destroy_decal(d);
	}

void
gkrellm_remove_decal(GkrellmPanel *p, GkrellmDecal *d)
	{
	if (!p || !d || gkrellm_decal_is_button(d))
		return;
	_remove_decal(p, d);
	}

void
gkrellm_insert_decal(GkrellmPanel *p, GkrellmDecal *d, gboolean append)
	{
	if (!p || !d || g_list_find(p->decal_list, d))
		return;
	if (append)
		p->decal_list = g_list_append(p->decal_list, d);
	else
		p->decal_list = g_list_prepend(p->decal_list, d);
	d->panel = (gpointer) p;
	p->need_decal_overlap_check = TRUE;
	gkrellm_draw_panel_layers_force(p);
	}

void
gkrellm_insert_decal_nth(GkrellmPanel *p, GkrellmDecal *d, gint n)
	{
	if (!p || !d || g_list_find(p->decal_list, d))
		return;
	p->decal_list = g_list_insert(p->decal_list, d, n);
	d->panel = (gpointer) p;
	p->need_decal_overlap_check = TRUE;
	gkrellm_draw_panel_layers_force(p);
	}

void
gkrellm_destroy_decal_list(GkrellmPanel *p)
	{
	GkrellmDecalbutton	*b;
	GkrellmDecal		*d;
	GList				*list;

	if (!p)
		return;
	for (list = p->decal_list; list; list = list->next)
		{
		d = (GkrellmDecal *) list->data;
		if ((b = gkrellm_decal_is_button(d)) != NULL)
			remove_from_button_list(p, b);
		_destroy_decal(d);
		}
	if (p->decal_list)
		g_list_free(p->decal_list);
	p->decal_list = NULL;
	}

GkrellmDecal *
gkrellm_create_decal_pixmap(GkrellmPanel *p,
				GdkPixmap *pixmap, GdkBitmap *mask,
				gint depth, GkrellmStyle *style, gint x, gint y)
	{
	GtkWidget		*top_win;
	GkrellmMargin	*m;
	GkrellmDecal	*d;

	if (!pixmap)
		return NULL;
	top_win = gkrellm_get_top_window();
	d = (GkrellmDecal *) g_new0(GkrellmDecal, 1);
	if (p)
		{
		p->decal_list = g_list_append(p->decal_list, d);
		p->need_decal_overlap_check = TRUE;
		}
	d->panel = (gpointer) p;

	gdk_drawable_get_size(pixmap, &d->w, &d->h);
	if (depth > 0)
		d->h /= depth;
	if (d->h == 0)
		d->h = 1;

	d->x = x;
	m = gkrellm_get_style_margins(style);
	if (d->x < 0 && style)
		{
		if (   style->label_position < 50
			&& style->label_position != GKRELLM_LABEL_NONE
		   )
			d->x = _GK.chart_width - d->w - m->right;
		else
			d->x = m->left;
		}
	d->y = y;
	if (d->y < 0)
		d->y = m->top;

	d->pixmap = pixmap;
	d->mask   = mask;
	d->stencil = gdk_pixmap_new(top_win->window, d->w, d->h, 1);

	d->value = -1;		/* Force initial draw */
	d->flags = 0;
	d->state = DS_VISIBLE;
	return d;
	}

static GkrellmDecal *
_create_decal_text(GkrellmPanel *p, gchar *string,
				GkrellmTextstyle *ts, GkrellmStyle *style,
				gint x, gint y, gint w, gint h, gint y_ink, gboolean markup)
	{
	GtkWidget		*top_win;
	GkrellmMargin	*m;
	GkrellmDecal	*d;
	GkrellmText		*tx;
	gint			width, height, baseline;

	top_win = gkrellm_get_top_window();
	d = (GkrellmDecal *) g_new0(GkrellmDecal, 1);
	if (p)
		{
		p->decal_list = g_list_append(p->decal_list, d);
		p->need_decal_overlap_check = TRUE;
		}
	d->panel = (gpointer) p;

	d->text_style = ts ? *ts : *gkrellm_meter_textstyle(0);
	tx = g_new0(GkrellmText, 1);
	d->text_list = g_list_append(d->text_list, tx);
	gkrellm_dup_string(&tx->text, "");
	tx->text_style = d->text_style;

	if (string && *string)
		{
		if (markup)
			gkrellm_text_markup_extents(d->text_style.font,
					string, strlen(string),
					&width, &height, &baseline, &d->y_ink);
		else
			gkrellm_text_extents(d->text_style.font,
					string, strlen(string),
					&width, &height, &baseline, &d->y_ink);
		d->h = height + d->text_style.effect;
		}
	else
		{
		width = 2;
		d->h = h;
		d->y_ink = y_ink;
		}
	if (d->h <= 0)
		d->h = 2;

	/* Width defaults to full chart width
	|  minus borders unless w > 0, or w == 0 to use string width.
	*/
	if (style)
		{
		m = gkrellm_get_style_margins(style);
		if (w < 0)
			d->w = _GK.chart_width - m->left - m->right;
		else if (w == 0)
			d->w = width + d->text_style.effect;
		else
			d->w = w;

		d->x = (x >= 0) ? x : m->left;

		d->y = y;
		if (d->y < 0)
			d->y = m->top;
		}
	else
		{
		if (w < 0)
			d->w = _GK.chart_width;
		else if (w == 0)
			d->w = width;
		else
			d->w = w;
		d->x = x;
		d->y = y;
		}
	if (d->w == 0)
		d->w = 1;
	d->pixmap = gdk_pixmap_new(top_win->window, d->w, d->h, -1);;
	d->mask   = NULL;
	d->stencil = gdk_pixmap_new(top_win->window, d->w, d->h, 1);

	d->value = -1;		/* Force initial draw */
	d->flags = DF_LOCAL_PIXMAPS;
	d->state = DS_VISIBLE;

	return d;
	}

GkrellmDecal *
gkrellm_create_decal_text(GkrellmPanel *p, gchar *string,
				GkrellmTextstyle *ts, GkrellmStyle *style,
				gint x, gint y, gint w)
	{
	return _create_decal_text(p, string, ts, style, x, y, w, 0, 0, FALSE);
	}

GkrellmDecal *
gkrellm_create_decal_text_markup(GkrellmPanel *p, gchar *string,
				GkrellmTextstyle *ts, GkrellmStyle *style,
				gint x, gint y, gint w)
	{
	return _create_decal_text(p, string, ts, style, x, y, w, 0, 0, TRUE);
	}

GkrellmDecal *
gkrellm_create_decal_text_with_height(GkrellmPanel *p,
				GkrellmTextstyle *ts, GkrellmStyle *style,
				gint x, gint y, gint w, gint h, gint y_ink)
	{
	return _create_decal_text(p, NULL, ts, style, x, y, w, h, y_ink, FALSE);
	}

void
gkrellm_make_decal_invisible(GkrellmPanel *p, GkrellmDecal *d)
	{
	if (!p || !d || d->state == DS_INVISIBLE)
		return;
	d->state = DS_INVISIBLE;
	d->modified = TRUE;
	p->need_decal_overlap_check = TRUE;
	gkrellm_draw_panel_layers(p);
	}

void
gkrellm_make_decal_visible(GkrellmPanel *p, GkrellmDecal *d)
	{
	if (!p || !d || d->state == DS_VISIBLE)
		return;
	d->state = DS_VISIBLE;
	d->modified = TRUE;
	p->need_decal_overlap_check = TRUE;
	gkrellm_draw_panel_layers(p);
	}

gint
gkrellm_is_decal_visible(GkrellmDecal *d)
	{
	if (!d)
		return FALSE;
	return (d->state == DS_VISIBLE) ? TRUE : FALSE;
	}

/* ===================================================================== */
#define	AUTO_HIDE_BUTTON 	1


gboolean
gkrellm_in_decal(GkrellmDecal *d, GdkEventButton *ev)
	{
	if (!d || !ev)
		return FALSE;
	if (   ev->x >= d->x && ev->x < d->x + d->w
		&& ev->y >= d->y && ev->y < d->y + d->h
	   )
		return TRUE;
	return FALSE;
	}

static void
set_button_index(GkrellmDecalbutton *b, gint index, gint do_draw)
	{
	if (!b)
		return;
	gkrellm_draw_decal_pixmap(b->panel, b->decal, index);
	b->cur_index = index;
	if (do_draw)
		gkrellm_draw_panel_layers(b->panel);
	}

void
gkrellm_set_button_sensitive(GkrellmDecalbutton *b, gboolean sensitive)
	{
	if (b)
		b->sensitive = sensitive;
	}

void
gkrellm_hide_button(GkrellmDecalbutton *b)
	{
	if (!b)
		return;
	b->sensitive = FALSE;
	gkrellm_make_decal_invisible(b->panel, b->decal);
	}

void
gkrellm_show_button(GkrellmDecalbutton *b)
	{
	if (!b)
		return;
	b->sensitive = TRUE;
	gkrellm_make_decal_visible(b->panel, b->decal);
	}

gboolean
gkrellm_in_button(GkrellmDecalbutton *b, GdkEventButton *ev)
	{
	return gkrellm_in_decal(b->decal, ev);
	}

static gint
cb_decal_button_press(GtkWidget *widget, GdkEventButton *ev, GkrellmPanel *p)
	{
	GList				*list;
	GkrellmDecalbutton	*b;
	gboolean			stop_sig = FALSE;

	if (ev->type != GDK_BUTTON_PRESS)	/* I'm connected to "event" */
		return FALSE;
	for (list = p->button_list; list; list = list->next)
		{
		b = (GkrellmDecalbutton *) list->data;
		if (   b->sensitive && (*(b->cb_in_button))(b, ev, b->in_button_data)
			&& (   (b->cb_button_click && ev->button == 1)
				|| (b->cb_button_right_click && ev->button == 3)
			   )
		   )
			{
			if (b->cur_index != b->pressed_index)
				{
				b->saved_index = b->cur_index;
				set_button_index(b, b->pressed_index, 1);
				}
			stop_sig = TRUE;
			break;
			}
		}
	return stop_sig;
	}

static gint
cb_decal_button_release(GtkWidget *widget, GdkEventButton *ev, GkrellmPanel *p)
	{
	GList				*list;
	GkrellmDecalbutton	*b;
	gboolean			stop_sig = FALSE;

	if (ev->type != GDK_BUTTON_RELEASE)	/* I'm connected to "event" */
		return FALSE;
	for (list = p->button_list; list; list = list->next)
		{
		b = (GkrellmDecalbutton *) list->data;
		if (b->cur_index == b->pressed_index)
			{
			set_button_index(b, b->saved_index, 1);
			if ( (*(b->cb_in_button))(b, ev, b->in_button_data) )
				{
				if (b->cb_button_click && ev->button == 1)
					(*(b->cb_button_click))(b, b->data);
				else if (b->cb_button_right_click && ev->button == 3)
					(*(b->cb_button_right_click))(b, b->right_data);
				}
			stop_sig = TRUE;
		 	}
		}
	return stop_sig;
	}

static gint
cb_decal_button_leave(GtkWidget *widget, GdkEventButton *ev, GkrellmPanel *p)
	{
	GList				*list;
	GkrellmDecalbutton	*b;

	if (ev->type != GDK_LEAVE_NOTIFY)	/* I'm connected to "event" */
		return FALSE;
	for (list = p->button_list; list; list = list->next)
		{
		b = (GkrellmDecalbutton *) list->data;
		if (b->type == AUTO_HIDE_BUTTON)
			{
			if (b->sensitive)
				{
				set_button_index(b, 0, 0);
				b->decal->state = DS_INVISIBLE;
				b->decal->modified = TRUE;
				p->need_decal_overlap_check = TRUE;
				gkrellm_draw_panel_layers(p);
				}
			}
		else
			if (b->cur_index == b->pressed_index)
				set_button_index(b, b->saved_index, 1);
		}
	return FALSE;
	}

static gint
cb_decal_button_enter(GtkWidget *widget, GdkEventButton *ev, GkrellmPanel *p)
	{
	GList				*list;
	GkrellmDecalbutton	*b;

	if (ev->type != GDK_ENTER_NOTIFY)	/* I'm connected to "event" */
		return FALSE;
	for (list = p->button_list; list; list = list->next)
		{
		b = (GkrellmDecalbutton *) list->data;
		if (b->type != AUTO_HIDE_BUTTON || ! b->sensitive)
			continue;
		b->decal->state = DS_VISIBLE;
		set_button_index(b, 0, 0);
		b->decal->modified = TRUE;
		p->need_decal_overlap_check = TRUE;
		gkrellm_draw_panel_layers(b->panel);
		}
	return FALSE;
	}

void
gkrellm_set_decal_button_index(GkrellmDecalbutton *b, gint index)
	{
	if (!b)
		return;
	if (b->cur_index == b->pressed_index)
		b->saved_index = index;	/* Throw away old save */
	else
		set_button_index(b, index, 0);
	}

GkrellmDecalbutton *
gkrellm_decal_is_button(GkrellmDecal *d)
	{
	GkrellmPanel		*p;
	GkrellmDecalbutton	*b;
	GList				*list;

	if ((p = (GkrellmPanel *) d->panel) == NULL)
		return NULL;
	for (list = p->button_list; list; list = list->next)
		{
		b = (GkrellmDecalbutton *) list->data;
		if (b->decal == d)
			return b;
		}
	return NULL;
	}

void
gkrellm_destroy_button(GkrellmDecalbutton *b)
	{
	GkrellmPanel	*p;

	if (!b)
		return;
	p = b->panel;
	if (p && b->decal)
		_remove_decal(p, b->decal);
	if (b->decal)
		_destroy_decal(b->decal);
	remove_from_button_list(p, b);
	}


void
gkrellm_set_in_button_callback(GkrellmDecalbutton *b,
			gint (*func)(), gpointer data)
	{
	if (!b)
		return;
	b->cb_in_button = func;
	b->in_button_data = data;
	}

void
gkrellm_decal_button_connect(GkrellmDecalbutton *b, void (*func)(), void *data)
	{
	if (!b)
		return;
	b->data = data;
	b->cb_button_click = func;
	}

void
gkrellm_decal_button_right_connect(GkrellmDecalbutton *b,
				void (*func)(), void *data)
	{
	if (!b)
		return;
	b->right_data = data;
	b->cb_button_right_click = func;
	}

void
gkrellm_panel_button_signals_connect(GkrellmPanel *p)
	{
	/* I want DecalButton event handlers to be called before any monitor
	|  handlers, but DecalButtons will be recreated at theme changes and
	|  thus its handlers will be after panel handlers. So, connect to
	|  "event" which is called first, and check for the right event in
	|  the callback.
	|  There is only one event handler per signal per panel.  Buttons do
	|  not have their own event box so they may be efficiently animated.
	*/
	if (   !p || p->button_signals_connected
		|| !p->button_list || !p->drawing_area
	   )
		return;
	p->id_press = g_signal_connect(G_OBJECT(p->drawing_area),
				"event", G_CALLBACK(cb_decal_button_press), p);
	p->id_release = g_signal_connect(G_OBJECT(p->drawing_area),
				"event", G_CALLBACK(cb_decal_button_release), p);
	p->id_enter = g_signal_connect(G_OBJECT(p->drawing_area),
				"event", G_CALLBACK(cb_decal_button_enter), p);
	p->id_leave = g_signal_connect(G_OBJECT(p->drawing_area),
				"event", G_CALLBACK(cb_decal_button_leave), p);
	p->button_signals_connected = TRUE;
	}

  /* Make an existing decal into a decal button.  The decal should already
  |  be created and be in the panel's decal list.
  */
GkrellmDecalbutton *
gkrellm_make_decal_button(GkrellmPanel *p, GkrellmDecal *d, void (*func)(),
			void *data, gint cur_index, gint pressed_index)
	{
	GkrellmDecalbutton	*b;

	if (!p || !d)
		return NULL;
	b = g_new0(GkrellmDecalbutton, 1);
	b->panel = p;
	b->decal = d;
	b->pressed_index = pressed_index;
	b->data = data;
	b->cb_button_click = func;
	b->cb_in_button = gkrellm_in_button;
	b->in_button_data = NULL;
	b->sensitive = TRUE;
	set_button_index(b, cur_index, 0);

	p->button_list = g_list_append(p->button_list, b);
	gkrellm_panel_button_signals_connect(p);
	p->need_decal_overlap_check = TRUE;

	return b;
	}


GkrellmDecalbutton *
gkrellm_make_overlay_button(GkrellmPanel *p, void (*func)(), void *data,
				gint x, gint y, gint w, gint h,
				GkrellmPiximage *normal_piximage,
				GkrellmPiximage *pressed_piximage)
	{
	GtkWidget			*top_win;
	GkrellmDecalbutton	*b;
	GkrellmDecal		*d;
	GdkPixmap			*pm = NULL, *pixmap;
	GdkBitmap			*m = NULL, *mask;

	if (!p)
		return NULL;
	top_win = gkrellm_get_top_window();
	if (x < 0)
		{
		w -= x;
		x = 0;
		}
	if (y < 0)
		{
		h -= y;
		y = 0;
		}
	if (x + w > _GK.chart_width)
		w = _GK.chart_width - x;
	if (p->h > 0 && y + h > p->h)
		h = p->h - y;
	if (h < 2)
		h = 2;
	if (w < 4)
		w = 4;

	pixmap = gdk_pixmap_new(top_win->window, w, 2 * h, -1);
	mask = gdk_pixmap_new(top_win->window, w, 2 * h, 1);

	if (normal_piximage && pressed_piximage)
		{
		gkrellm_scale_piximage_to_pixmap(normal_piximage, &pm, &m, w, h);
		gdk_draw_drawable(pixmap, _GK.draw1_GC, pm, 0, 0, 0, 0, w, h);
		if (m)
			gdk_draw_drawable(mask, _GK.bit1_GC, m, 0, 0, 0, 0, w, h);
		else
			gdk_draw_rectangle(mask, _GK.bit1_GC, TRUE, 0, 0, w, h);
		gkrellm_free_pixmap(&pm);
		gkrellm_free_bitmap(&m);

		gkrellm_scale_piximage_to_pixmap(pressed_piximage, &pm, &m, w, h);
		gdk_draw_drawable(pixmap, _GK.draw1_GC, pm, 0, 0, 0, h, w, h);
		if (m)
			gdk_draw_drawable(mask, _GK.bit1_GC, m, 0, 0, 0, h, w, h);
		else
			gdk_draw_rectangle(mask, _GK.bit1_GC, TRUE, 0, h, w, h);
		gkrellm_free_pixmap(&pm);
		gkrellm_free_bitmap(&m);
		}
	else	/* Make a default frame. */
		{
		GdkColor	gray0, gray1;
		GdkColormap	*cmap;

		cmap = gdk_colormap_get_system();
		gdk_color_parse("gray65", &gray0);
		gdk_color_parse("gray100", &gray1);
		gdk_colormap_alloc_color(cmap, &gray0, FALSE, TRUE);
		gdk_colormap_alloc_color(cmap, &gray1, FALSE, TRUE);

		gdk_gc_set_foreground(_GK.draw1_GC, &gray1);
		gdk_draw_line(pixmap, _GK.draw1_GC, 0, 0, w - 1, 0);		
		gdk_draw_line(pixmap, _GK.draw1_GC, 0, 0, 0, h - 1);		
		gdk_draw_line(pixmap, _GK.draw1_GC, 0, 2 * h - 1, w - 1, 2 * h - 1);
		gdk_draw_line(pixmap, _GK.draw1_GC, w - 1, 2 * h - 1, w - 1, h);

		gdk_gc_set_foreground(_GK.draw1_GC, &gray0);
		gdk_draw_line(pixmap, _GK.draw1_GC, 0, h - 1, w - 1, h - 1);		
		gdk_draw_line(pixmap, _GK.draw1_GC, w - 1, h - 1, w - 1, 0);		
		gdk_draw_line(pixmap, _GK.draw1_GC, 0, h, w - 1, h);		
		gdk_draw_line(pixmap, _GK.draw1_GC, 0, h, 0, 2 * h - 1);		

		gdk_draw_rectangle(mask, _GK.bit1_GC, TRUE, 0, 0, w, 2 * h);		
		gdk_draw_rectangle(mask, _GK.bit0_GC, TRUE, 1, 1, w - 2, h - 2);
		gdk_draw_rectangle(mask, _GK.bit0_GC, TRUE, 1, h + 1, w - 2, h - 2);

		gdk_colormap_free_colors(cmap, &gray0, 1);
		gdk_colormap_free_colors(cmap, &gray1, 1);
		}

	d = gkrellm_create_decal_pixmap(p, pixmap, mask, 2, NULL, x, y);
	d->flags |= (DF_LOCAL_PIXMAPS | DF_OVERLAY_PIXMAPS);
	d->state = DS_INVISIBLE;

	b = gkrellm_make_decal_button(p, d, func, data, 0, 1);
	b->type = AUTO_HIDE_BUTTON;

	return b;
	}

GkrellmDecalbutton *
gkrellm_put_decal_in_panel_button(GkrellmPanel *p, GkrellmDecal *d,
			void (*func)(), void *data, GkrellmMargin *margin_pad)
	{
	GkrellmDecalbutton	*b;
	GkrellmBorder		*border;
	GkrellmMargin		*margin;
	gint				x, y, w, h, a;

	if (!p || !d)
		return NULL;
	border = &_GK.button_panel_border;
	margin = margin_pad ? margin_pad : &zero_margin;

	x = d->x - border->left - margin->left;
	a = d->x + d->w + border->right + margin->right;
	w = a - x;

	y = d->y - border->top - margin->top;
	a = d->y + d->h + border->bottom + margin->bottom;
	h = a - y;

	b = gkrellm_make_overlay_button(p, func, data, x, y, w, h,
				_GK.button_panel_out_piximage, _GK.button_panel_in_piximage);
	return b;
	}

GkrellmDecalbutton *
gkrellm_put_decal_in_meter_button(GkrellmPanel *p, GkrellmDecal *d,
			void (*func)(), void *data, GkrellmMargin *margin_pad)
	{
	GkrellmDecalbutton	*b;
	GkrellmBorder		*border;
	GkrellmMargin		*margin;
	gint				x, y, w, h, a;

	if (!p || !d)
		return NULL;
	border = &_GK.button_meter_border;
	margin = margin_pad ? margin_pad : &zero_margin;

	x = d->x - border->left - margin->left;
	a = d->x + d->w + border->right + margin->right;
	w = a - x;

	y = d->y - border->top - margin->top;
	a = d->y + d->h + border->bottom + margin->bottom;
	h = a - y;

	b = gkrellm_make_overlay_button(p, func, data, x, y, w, h,
				_GK.button_meter_out_piximage, _GK.button_meter_in_piximage);
	return b;
	}


static GkrellmDecalbutton *
put_label_in_button(GkrellmPanel *p, void (*func)(), void *data, gint type,
			gint pad)
	{
	GkrellmDecalbutton	*b;
	GkrellmLabel		*lbl;
	GkrellmMargin		*m;
	GkrellmPiximage		*normal_piximage, *pressed_piximage;
	GkrellmBorder		*style_border, *fr_border;
	gint				x, y, w, h;

	if (!p || p->h == 0)
		return NULL;

	fr_border = (type == METER_PANEL_TYPE) ? &_GK.button_meter_border
										: &_GK.button_panel_border;
	normal_piximage = (type == METER_PANEL_TYPE)
			? _GK.button_meter_out_piximage : _GK.button_panel_out_piximage;
	pressed_piximage = (type == METER_PANEL_TYPE)
			? _GK.button_meter_in_piximage : _GK.button_panel_in_piximage;

	/* If no panel label, put the whole panel in the button.
	*/
	if ((lbl = p->label) == NULL || lbl->string == NULL || lbl->position < 0)
		{
		m = gkrellm_get_style_margins(p->style);
		style_border = p->style ? &p->style->border : &zero_border;
		x = 0 + m->left;
		y = 0 + style_border->top - fr_border->top;
		w = p->w - x - m->right;
		h = p->h - y - style_border->bottom + fr_border->bottom;
		}
	else
		{
		x = lbl->x_panel - fr_border->left - pad;
		y = lbl->y_panel - fr_border->top;
		w = lbl->width + fr_border->left + fr_border->right + 2 * pad;
		h = lbl->height + (p->textstyle->effect ? 1 : 0)
				+ fr_border->top + fr_border->bottom;
		}
	b = gkrellm_make_overlay_button(p, func, data, x, y, w, h,
					normal_piximage, pressed_piximage);
	return b;
	}


GkrellmDecalbutton *
gkrellm_put_label_in_panel_button(GkrellmPanel *p, void (*func)(), void *data,
				gint pad)
	{
	GkrellmDecalbutton	*b;

	b = put_label_in_button(p, func, data, CHART_PANEL_TYPE, pad);
	return b;
	}


GkrellmDecalbutton *
gkrellm_put_label_in_meter_button(GkrellmPanel *p, void (*func)(), void *data,
				gint pad)
	{
	GkrellmDecalbutton	*b;

	b = put_label_in_button(p, func, data, METER_PANEL_TYPE, pad);
	return b;
	}

GkrellmDecalbutton *
gkrellm_make_scaled_button(GkrellmPanel *p, GkrellmPiximage *im,
			void (*func)(), void *data,
			gboolean auto_hide, gboolean set_default_border,
			gint depth, gint cur_index, gint pressed_index,
			gint x, gint y, gint w, gint h)
	{
	GtkWidget			*top_win;
	GkrellmDecalbutton	*b;
	GkrellmDecal		*d;
	GkrellmBorder		*bdr;
	GdkPixmap			*pixmap;
	GdkBitmap			*mask;

	if (!p)
		return NULL;
	if (!im)
		{
		im = _GK.decal_button_piximage;
		depth = 2;
		cur_index = 0;
		pressed_index = 1;
		}
	if (depth < 2)
		depth = 2;
	if (w < 0)
		w = gkrellm_chart_width();
	else
		{
		if (w == 0)
			w = gdk_pixbuf_get_width(im->pixbuf);
		if ((w = w * _GK.theme_scale / 100) < 2)
			w = 2;
		}

	if (h < 0)
		h = p->h;
	else
		{
		if (h == 0)
			h = gdk_pixbuf_get_height(im->pixbuf) / depth;
		if ((h = h * _GK.theme_scale / 100) < 2)
			h = 2;
		}

	top_win = gkrellm_get_top_window();
	pixmap = gdk_pixmap_new(top_win->window, w, depth * h, -1);
	mask = gdk_pixmap_new(top_win->window, w, depth * h, 1);
	if (set_default_border)
		{
		bdr = &im->border;
		bdr->left = bdr->right = bdr->top = bdr->bottom = 1;
		}
	gkrellm_scale_piximage_to_pixmap(im, &pixmap, &mask, w, depth * h);
	d = gkrellm_create_decal_pixmap(p, pixmap, mask, depth, NULL, x, y);
	d->flags |= DF_LOCAL_PIXMAPS;
	b = gkrellm_make_decal_button(p, d, func, data, cur_index, pressed_index);
	if (auto_hide)
		{
		d->state = DS_INVISIBLE;
		b->type = AUTO_HIDE_BUTTON;
		}
	return b;
	}


GkrellmDecal *
gkrellm_make_scaled_decal_pixmap(GkrellmPanel *p, GkrellmPiximage *im,
			GkrellmStyle *style, gint depth,
			gint x, gint y, gint w, gint h)
	{
	GtkWidget		*top_win;
	GdkPixmap		*pixmap;
	GdkBitmap		*mask;
	GkrellmMargin	*m;
	GkrellmDecal	*d;

	if (!im)
		return NULL;
	top_win = gkrellm_get_top_window();
	d = (GkrellmDecal *) g_new0(GkrellmDecal, 1);
	if (p)
		{
		p->decal_list = g_list_append(p->decal_list, d);
		p->need_decal_overlap_check = TRUE;
		}
	d->panel = (gpointer) p;

	if (depth < 1)
		depth = 1;

	if (   w < 1
		&& (w = gdk_pixbuf_get_width(im->pixbuf) * _GK.theme_scale / 100) < 1
	   )
		w = 1;
	d->w = w;

	if (h < 1)
		h = gdk_pixbuf_get_height(im->pixbuf) / depth * _GK.theme_scale / 100;

	if (h < 1)
		h = 1;
	d->h = h;

	pixmap = gdk_pixmap_new(top_win->window, d->w, d->h * depth, -1);
	mask = gdk_pixmap_new(top_win->window, d->w, d->h * depth, 1);
	gkrellm_scale_piximage_to_pixmap(im, &pixmap, &mask, d->w, d->h * depth);

	d->x = x;
	m = gkrellm_get_style_margins(style);
	if (d->x < 0 && style)
		{
		if (   style->label_position < 50
			&& style->label_position != GKRELLM_LABEL_NONE
		   )
			d->x = _GK.chart_width - d->w - m->right;
		else
			d->x = m->left;
		}
	d->y = y;
	if (d->y < 0)
		d->y = m->top;

	d->pixmap = pixmap;
	d->mask   = mask;
	d->stencil = gdk_pixmap_new(top_win->window, d->w, d->h, 1);

	d->value = -1;		/* Force initial draw */
	d->flags = DF_LOCAL_PIXMAPS;
	d->state = DS_VISIBLE;
	return d;
	}
