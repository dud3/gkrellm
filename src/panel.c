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

static GList	*panel_list;


GList *
gkrellm_get_panel_list(void)
	{
	return panel_list;
	}

void
gkrellm_panel_configure_add_height(GkrellmPanel *p, gint h)
	{
	if (!p)
		return;
	p->h_configure += h;
	}

void
gkrellm_panel_configure_set_height(GkrellmPanel *p, gint h)
	{
	if (!p)
		return;
	p->h_configure = h;
	}

gint
gkrellm_panel_configure_get_height(GkrellmPanel *p)
	{
	if (!p)
		return 0;
	return p->h_configure;
	}

static void
setup_panel_style(GkrellmPanel *p, GkrellmStyle *style)
	{
	GkrellmLabel	*lbl	= p->label;

	p->style = style;
	if (!style)
		return;
	lbl->position = style->label_position;
	p->transparency = style->transparency;
	p->scroll_text_cache_off = style->scroll_text_cache_off;
	_GK.any_transparency |= p->transparency;
	if (!p->textstyle)
		{
		p->textstyle = gkrellm_textstyle_new0();
		p->textstyle->internal = TRUE;
		}
	if (p->textstyle->internal)
		{
		*(p->textstyle) = style->label_tsA;
		p->textstyle->internal = TRUE;
		p->textstyle->font = *(style->label_tsA.font_seed);
		}
	}

void
gkrellm_panel_label_get_position(GkrellmStyle *style, gint *x_position,
				gint *y_off)
	{
	if (!style)
		return;
	if (x_position)
		*x_position = style->label_position;
	if (y_off)
		*y_off = style->label_yoff;
	}


  /* Calculate panel height required for given border, label height,
  |  krell heights, and decal heights.  Also calculate label extents
  |  and load all this info into a GkrellmLabel structure.
  |  After panel height is calculated, calling routine may want to adjust
  |  krell or decal y offsets for centering.  
  */
void
gkrellm_panel_configure(GkrellmPanel *p, gchar *string, GkrellmStyle *style)
	{
	GList			*list;
	GkrellmKrell	*k;
	GkrellmDecal	*d;
	GkrellmLabel	*lbl;
	GkrellmTextstyle *ts = NULL;
	gint			y, h_panel, h, baseline;
	gint			top_margin = 0, bottom_margin = 0;

	if (!p)
		return;
	p->style = style;
	lbl = p->label;
	lbl->position = GKRELLM_LABEL_CENTER;
	setup_panel_style(p, style);
	gkrellm_get_top_bottom_margins(style, &top_margin, &bottom_margin);

	ts = p->textstyle;

	if (string)
		{
		if (g_utf8_validate(string, -1, NULL))
			gkrellm_dup_string(&lbl->string, string);
		else
			lbl->string = g_locale_to_utf8(string, -1, NULL, NULL, NULL);
		}
	else
		gkrellm_dup_string(&lbl->string, string);

	if (lbl->string && ts && lbl->position >= 0)
		gkrellm_text_extents(ts->font, string, strlen(string),
				&lbl->width, &lbl->height, &baseline, NULL);
	else
		{
		lbl->width = 0;
		lbl->height = 0;
		baseline = 0;
		}

	if (style && style->label_yoff > 0)
		h_panel = style->label_yoff + bottom_margin;
	else
		h_panel = top_margin + bottom_margin;

	h_panel += lbl->height + (ts ? ts->effect : 0);

	/* If krell_yoff is -1, then in gkrellm_create_krell() k->y0 was put
	|  at the top margin.  If krell_yoff is < -1, then here I will bottom
	|  justify the krell (so must go through krell list twice).
	|  GkrellmDecals are assumed to fit inside of margins (unless they are
	|  overlays).  Caller must subtract off bottom_margin if this is not so.
	*/
	for (list = p->decal_list; list; list = list->next)
		{
		d = (GkrellmDecal *) list->data;
		if (d->flags & DF_OVERLAY_PIXMAPS)
			continue;
		h = d->y + d->h + bottom_margin;
		if (h > h_panel)
			h_panel = h;
		}
	for (list = p->krell_list; list; list = list->next)
		{
		k = (GkrellmKrell *) list->data;
		if (k->y0 == -2)		/* Will bottom justify */
			y = 0;
		else if (k->y0 == -3)	/* Will bottom margin justify */
			y = top_margin;
		else
			y = k->y0;
		h = y + k->h_frame;
		if (k->flags & KRELL_FLAG_BOTTOM_MARGIN)
			h += bottom_margin;
		if (h > h_panel)
			h_panel = h;
		}
	for (list = p->krell_list; list; list = list->next)
		{
		k = (GkrellmKrell *) list->data;
		if (k->y0 < 0)
			{
			k->y0 = h_panel - k->h_frame;		/* Bottom justify */
			if (k->flags & KRELL_FLAG_BOTTOM_MARGIN)	/* or y0 == -3 */
				k->y0 -= bottom_margin;
			}
		}

	if (h_panel <= 0)
		h_panel = 1;

	if (style && style->label_yoff > 0)
		lbl->y_panel = style->label_yoff - (baseline - lbl->height);
	else
		lbl->y_panel = top_margin - (baseline - lbl->height);
	p->h_configure = h_panel;
	}

static void
draw_panel_label(GkrellmPanel *p, gboolean to_bg)
	{
	GkrellmLabel		*lbl	= p->label;
	GkrellmTextstyle	*ts		= p->textstyle;
	GkrellmMargin		*m;
	gchar				*s;
	gint				xdst;

	if (   lbl && ts
		&& ((s = lbl->string) != NULL)
		&& lbl->position >= 0
	   )
		{
		m = gkrellm_get_style_margins(p->style);
		lbl->width = gkrellm_gdk_string_width(ts->font, s) + ts->effect;
		xdst = gkrellm_label_x_position(lbl->position, p->w,
					lbl->width, m->left);
		lbl->x_panel = xdst;
		gkrellm_draw_string(p->pixmap, ts, xdst, lbl->y_panel, s);
		if (to_bg)
			gkrellm_draw_string(p->bg_pixmap, ts, xdst, lbl->y_panel, s);
		}
	}

static void
draw_panel(GkrellmPanel *p, gint to_screen)
	{
	if (!gkrellm_winop_draw_rootpixmap_onto_transparent_panel(p))
		gdk_draw_drawable(p->bg_pixmap, _GK.draw1_GC, p->bg_clean_pixmap,
					0, 0, 0, 0, p->w, p->h);

	gdk_draw_drawable(p->pixmap, _GK.draw1_GC, p->bg_pixmap,
					0, 0, 0, 0, p->w, p->h);
	draw_panel_label(p, TRUE);

	gdk_draw_drawable(p->bg_text_layer_pixmap, _GK.draw1_GC, p->bg_pixmap,
					0, 0, 0, 0, p->w, p->h);

	if (p->drawing_area->window && to_screen)
		{
		gdk_draw_drawable(p->drawing_area->window, _GK.draw1_GC, p->pixmap,
					0, 0, 0, 0, p->w, p->h);
		gkrellm_draw_panel_layers_force(p);
		}
	}

void
gkrellm_panel_label_on_top_of_decals(GkrellmPanel *p, gboolean mode)
	{
	if (p)
		p->label_on_top_of_decals = mode;
	}

void
gkrellm_draw_panel_label(GkrellmPanel *p)
	{
	if (p)
		draw_panel(p, TRUE);
	}

void
gkrellm_panel_destroy(GkrellmPanel *p)
	{
	if (!p)
		return;
	gkrellm_reset_panel_alerts(p);
	gkrellm_destroy_krell_list(p);
	gkrellm_destroy_decal_list(p);	/* Also destroys buttons */
	if (p->button_list || p->button_signals_connected)
		fprintf(stderr, "gkrellm_destroy_panel: button_list=%p connected=%d\n",
				p->button_list, p->button_signals_connected);
	if (p->label)
		{
		if (p->label->string)
			g_free(p->label->string);
		g_free(p->label);
		}
	if (p->layout)
		g_object_unref(G_OBJECT(p->layout));
	if (p->textstyle && p->textstyle->internal)
		g_free(p->textstyle);
	if (p->pixmap)
		g_object_unref(G_OBJECT(p->pixmap));
	if (p->bg_pixmap)
		g_object_unref(G_OBJECT(p->bg_pixmap));
	if (p->bg_text_layer_pixmap)
		g_object_unref(G_OBJECT(p->bg_text_layer_pixmap));
	if (p->bg_clean_pixmap)
		g_object_unref(G_OBJECT(p->bg_clean_pixmap));
	if (p->bg_mask)
		g_object_unref(G_OBJECT(p->bg_mask));

	if (p->hbox)
		gtk_widget_destroy(p->hbox);
	panel_list = g_list_remove(panel_list, p);
	if (p->shown)
		gkrellm_monitor_height_adjust(- p->h);
	g_free(p);
	gkrellm_pack_side_frames();
	}

void
gkrellm_panel_bg_piximage_override(GkrellmPanel *p,
		GkrellmPiximage *bg_piximage)
	{
	if (!p || !bg_piximage)
		return;
	p->bg_piximage = bg_piximage;
	p->bg_piximage_override = TRUE;
	}

void
gkrellm_panel_keep_lists(GkrellmPanel *p, gboolean keep)
	{
	if (p)
		p->keep_lists = keep;
	}

#if 0
static gboolean
cb_panel_map_event(GtkWidget *widget, GdkEvent *event, GkrellmPanel *p)
	{
	gdk_window_get_position(p->drawing_area->window, NULL, &p->y_mapped);
	if (_GK.frame_left_panel_overlap > 0 || _GK.frame_right_panel_overlap > 0)
		_GK.need_frame_packing = TRUE;
	return FALSE;
	}
#endif

static gboolean
cb_panel_size_allocate(GtkWidget *widget, GtkAllocation *size, GkrellmPanel *p)
	{
	gdk_window_get_position(p->drawing_area->window, NULL, &p->y_mapped);
	if (_GK.frame_left_panel_overlap > 0 || _GK.frame_right_panel_overlap > 0)
		_GK.need_frame_packing = TRUE;
	return FALSE;
	}

void
gkrellm_panel_create(GtkWidget *vbox, GkrellmMonitor *mon, GkrellmPanel *p)
	{
	GtkWidget		*hbox;
	GkrellmPiximage	piximage;
	GtkWidget   	*top_win = gkrellm_get_top_window();

	if (!vbox || !mon || !p)
		return;
	p->monitor = (gpointer) mon;
	if (!p->style)	/* gkrellm_panel_configure() may not have been called. */
		setup_panel_style(p, mon->privat->panel_style);

	if (!p->bg_piximage_override)
		{
		if (mon->privat->style_type == CHART_PANEL_TYPE)
			p->bg_piximage = gkrellm_bg_panel_piximage(mon->privat->style_id);
		else
			p->bg_piximage = gkrellm_bg_meter_piximage(mon->privat->style_id);
		}
	p->bg_piximage_override = FALSE;

	/* If not being called from rebuild or after a panel destroy, then panel
	|  still has a height that must be accounted for.
	*/
	if (p->h && p->shown)
		gkrellm_monitor_height_adjust(- p->h);
	p->h = p->h_configure;
	p->w = _GK.chart_width;

	if (p->hbox == NULL)
		{
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_container_add (GTK_CONTAINER(vbox), hbox);
		p->hbox = hbox;
		p->drawing_area = gtk_drawing_area_new();
		p->layout = gtk_widget_create_pango_layout(top_win, NULL);
		gtk_widget_set_events (p->drawing_area, GDK_EXPOSURE_MASK
				| GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK
				| GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK
				| GDK_POINTER_MOTION_MASK);
		gtk_box_pack_start(GTK_BOX(p->hbox), p->drawing_area,
				FALSE, FALSE, 0);
		gtk_widget_show(p->drawing_area);
		gtk_widget_show(hbox);
		p->shown = TRUE;
		gtk_widget_realize(hbox);
		gtk_widget_realize(p->drawing_area);
		panel_list = g_list_append(panel_list, p);
		p->y_mapped = -1;
		g_signal_connect(G_OBJECT (p->drawing_area), "size_allocate",
					G_CALLBACK(cb_panel_size_allocate), p);
		}
	gtk_widget_set_size_request(p->drawing_area, p->w, p->h);

	if (_GK.frame_left_panel_overlap > 0 || _GK.frame_right_panel_overlap > 0)
		{
		piximage.pixbuf = gdk_pixbuf_new_subpixbuf(p->bg_piximage->pixbuf,
				_GK.frame_left_panel_overlap, 0,
				gdk_pixbuf_get_width(p->bg_piximage->pixbuf)
							- _GK.frame_left_panel_overlap
							- _GK.frame_right_panel_overlap,
				gdk_pixbuf_get_height(p->bg_piximage->pixbuf));
		piximage.border = p->bg_piximage->border;
		gkrellm_border_adjust(&piximage.border,
				-_GK.frame_left_panel_overlap, -_GK.frame_right_panel_overlap,
				0, 0);
//		gkrellm_scale_piximage_to_pixmap(&piximage, &p->bg_clean_pixmap,
//				&p->bg_mask, p->w, p->h);
		gkrellm_scale_theme_background(&piximage, &p->bg_clean_pixmap,
				&p->bg_mask, p->w, p->h);
		g_object_unref(G_OBJECT(piximage.pixbuf));
		}
	else
		gkrellm_scale_theme_background(p->bg_piximage, &p->bg_clean_pixmap,
			&p->bg_mask, p->w, p->h);
//		gkrellm_scale_piximage_to_pixmap(p->bg_piximage, &p->bg_clean_pixmap,
//			&p->bg_mask, p->w, p->h);

	if (p->bg_text_layer_pixmap)
		g_object_unref(G_OBJECT(p->bg_text_layer_pixmap));
	p->bg_text_layer_pixmap = gdk_pixmap_new(top_win->window, p->w, p->h, -1);
	if (p->bg_pixmap)
		g_object_unref(G_OBJECT(p->bg_pixmap));
	p->bg_pixmap = gdk_pixmap_new(top_win->window, p->w, p->h, -1);
	if (p->pixmap)
		g_object_unref(G_OBJECT(p->pixmap));
	p->pixmap = gdk_pixmap_new(top_win->window, p->w, p->h, -1);

	if (p->shown)
		{
		gkrellm_monitor_height_adjust(p->h);
		gkrellm_pack_side_frames();
		}
	p->need_decal_overlap_check = TRUE;
	draw_panel(p, FALSE);
	gkrellm_panel_button_signals_connect(p);
	}

void
gkrellm_panel_hide(GkrellmPanel *p)
	{
	if (!p || !p->shown)
		return;
	gtk_widget_hide(p->hbox);
	p->shown = FALSE;
	gkrellm_monitor_height_adjust(- p->h);
	gkrellm_pack_side_frames();
	}

void
gkrellm_panel_show(GkrellmPanel *p)
	{
	if (!p || p->shown)
		return;
	gtk_widget_show(p->hbox);
	p->shown = TRUE;
	gkrellm_monitor_height_adjust(p->h);
	gkrellm_pack_side_frames();
	}

gboolean
gkrellm_is_panel_visible(GkrellmPanel *p)
    {
    if (!p)
        return FALSE;
    return p->shown;
    }

gboolean
gkrellm_panel_enable_visibility(GkrellmPanel *p, gboolean new_vis,
					gboolean *current_vis)
	{
	gboolean	changed = FALSE;

	if (new_vis  && ! *current_vis)
		{
		gkrellm_panel_show(p);
		*current_vis  = TRUE;
		changed = TRUE;
		}
	if (!new_vis  && *current_vis)
		{
		gkrellm_panel_hide(p);
		*current_vis  = FALSE;
		changed = TRUE;
		}
	return changed;
	}

  /* Called from rebuild.  All panels must be cleaned out of things
  |  that will be recreated in the create() routines.  GKrellM <= 1.0.x
  |  left it up to plugins to destroy decal/krell lists.  Now this is
  |  enforced, but grandfather out the plugins have not upgraded to using
  |  the new gkrellm_panel_create() functions.
  */
void
gkrellm_panel_cleanup(void)
	{
	GList	*list, *list1;
	GkrellmPanel	*p;

	for (list = panel_list; list; list = list->next)
		{
		p = (GkrellmPanel *) list->data;
		if (!p->keep_lists)
			{
			gkrellm_destroy_krell_list(p);
			gkrellm_destroy_decal_list(p);	/* Also destroys buttons */
			if (p->krell_list || p->decal_list || p->button_list)
				fprintf(stderr,
					"GKrellM: gkrellm_panel_cleanup krell=%p decal=%p button=%p\n",
						p->krell_list, p->decal_list, p->button_list);
			}
		else
			{
			for (list1 = p->decal_list; list1; list1 = list1->next)
				((GkrellmDecal *) list1->data)->value = -1;
			for (list1 = p->krell_list; list1; list1 = list1->next)
				((GkrellmKrell *) list1->data)->x_position = -1;
			}
		p->h = 0;
		p->h_configure = 0;
		p->style = NULL;
		}
	}


  /* Check text_list decals for overlap of other decals and set flag to force
  |  drawing in the push function instead of the normal text pixmap layer
  */
static void
panel_decal_check_text_overlap(GkrellmDecal *d, GList *decal_list)
	{
	GList			*list;
	GkrellmDecal	*dcheck;

	if (!d || (d->state == DS_INVISIBLE))
		return;
	for (list = decal_list; list; list = list->next)
		{
		dcheck = (GkrellmDecal *) list->data;
		if (dcheck->state == DS_INVISIBLE)
			continue;

		if (   d->x + d->w > dcheck->x
			&& d->x < dcheck->x + dcheck->w
			&& d->y + d->h > dcheck->y
			&& d->y < dcheck->y + dcheck->h
		   )
			{
			/* text_list decals need push if overlapping any other decal.
			*/
			if (d->text_list)
				d->flags |= DF_TEXT_OVERLAPS;
			if (dcheck->text_list)
				dcheck->flags |= DF_TEXT_OVERLAPS;
			}
		}
	}


  /* Do the Pango text rendering.  If called when pixmap is bg_text_layer,
  |  it is a background draw.  But if called from push_decal_pixmaps() it is
  |  considered a push operation because it is being done whenever any panel
  |  decals or krells are modified.  In this case, Pango does the pixel pushing
  |  instead of gkrellm as was done in pre 2.2.0.
  */
static void
panel_draw_decal_text_list(GdkPixmap *pixmap, GkrellmDecal *d)
	{
	PangoLayout			*layout;
	GdkRectangle		rect;
	GList				*list;
	GkrellmText			*tx;
	GkrellmTextstyle	*ts;
	gchar				*s;
	gint				x, y;

	layout = gtk_widget_create_pango_layout(gkrellm_get_top_window(), NULL);
	rect.x = d->x;
	rect.y = d->y;
	rect.width = d->w;
	rect.height = d->h;
	gdk_gc_set_clip_rectangle(_GK.text_GC, &rect);
	for (list = d->text_list; list; list = list->next)
		{
		tx = (GkrellmText *) list->data;
		if (!*tx->text)
			continue;
		ts = &tx->text_style;

		pango_layout_set_font_description(layout, ts->font);
		x = tx->x_off;
		y = tx->y_off;
		if (d->flags & DF_SCROLL_TEXT_DIVERTED)
			{
			if (d->flags & DF_SCROLL_TEXT_CENTER)
				pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
			if (d->flags & DF_SCROLL_TEXT_H_LOOP)
				{
				x %= d->scroll_width;
				if (x > 0)
					x -= d->scroll_width;
				s = g_strconcat(tx->text, tx->text, NULL);
				if (d->flags & DF_TEXT_USE_MARKUP)
					pango_layout_set_markup(layout, s, strlen(s));
				else
					pango_layout_set_text(layout, s, strlen(s));
				g_free(s);
				}
			else if (d->flags & DF_SCROLL_TEXT_V_LOOP)
				{
				y %= d->scroll_height + d->y_ink;
				if (y > 0)
					y -= d->scroll_height + d->y_ink;
				s = g_strconcat(tx->text, "\n", tx->text, NULL);
				if (d->flags & DF_TEXT_USE_MARKUP)
					pango_layout_set_markup(layout, s, strlen(s));
				else
					pango_layout_set_text(layout, s, strlen(s));
				g_free(s);
				}
			else
				{
				if (d->flags & DF_TEXT_USE_MARKUP)
					pango_layout_set_markup(layout, tx->text,strlen(tx->text));
				else
					pango_layout_set_text(layout, tx->text, strlen(tx->text));
				}
			}
		else
			{
			if (d->flags & DF_TEXT_USE_MARKUP)
				pango_layout_set_markup(layout, tx->text, strlen(tx->text));
			else
				pango_layout_set_text(layout, tx->text, strlen(tx->text));
			}
		x += d->x;
		y += d->y;

		if (ts->effect)
			{
			gdk_gc_set_foreground(_GK.text_GC, &ts->shadow_color);
			gdk_draw_layout_with_colors(pixmap, _GK.text_GC,
						x + 1, y + 1, layout,
						&ts->shadow_color, NULL);
			}
		gdk_gc_set_foreground(_GK.text_GC, &ts->color);
		gdk_draw_layout(pixmap, _GK.text_GC, x, y, layout);
		}
	gdk_gc_set_clip_rectangle(_GK.text_GC, NULL);
	g_object_unref(layout);
	}

  /* Draw text decals on a cache text layer to avoid high overhead Pango
  |  drawing of them each time gkrellm_draw_panel_layers() is called.
  |  May draw here if a text decal was found to not overlap any other decal
  |  and if it's not flagged to be drawn on the top layer.
  |  The text layer is then used as the background for push decal and krell
  |  draws.
  */
static void
panel_draw_decal_text_layer(GkrellmPanel *p)
	{
	GList			*list;
	GkrellmDecal	*d;

	for (list = p->decal_list; list; list = list->next)
		{
		d = (GkrellmDecal *) list->data;

		if (   !d->modified
			|| !d->text_list
			|| (d->flags & DF_TOP_LAYER)
			|| (d->flags & DF_TEXT_OVERLAPS)
		   )
			continue;
		if (d->state != DS_INVISIBLE)
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

			panel_draw_decal_text_list(p->bg_text_layer_pixmap, d);
			}
		p->modified = TRUE;
		}
	}

  /* Push decal pixmaps through their stencils onto a Panel expose pixmap
  */
static gboolean
push_decal_pixmaps(GkrellmPanel *p, gboolean top_layer)
	{
	GList			*list;
	GkrellmDecal	*d;
	gint			x, y, w, h;
	gboolean		on_top, is_non_overlapping_text_decal;
	gboolean		restore_gc = FALSE, do_top_layer = FALSE;

	if (!p)
		return FALSE;
	for (list = p->decal_list; list; list = list->next)
		{
		d = (GkrellmDecal *) list->data;
		on_top = (d->flags & DF_TOP_LAYER);
		is_non_overlapping_text_decal
				= (d->text_list && !(d->flags & DF_TEXT_OVERLAPS));

		if (on_top && !top_layer)
			do_top_layer = TRUE;

		if (   d->state != DS_VISIBLE
			|| (top_layer && !on_top)
			|| (!top_layer && (on_top || is_non_overlapping_text_decal))
		   )
			continue;
		if (d->text_list)
			panel_draw_decal_text_list(p->pixmap, d);
		else if (d->scroll_text)
			{
			x = d->x_off;
			y = d->y_off;

			gdk_draw_drawable(p->pixmap, _GK.draw1_GC, d->pixmap,
						-x, -y, d->x, d->y, d->w, d->h);
			if (d->flags & DF_SCROLL_TEXT_H_LOOP)
				{
				x %= d->scroll_width;
				if (x > 0)
					{
					gdk_draw_drawable(p->pixmap, _GK.draw1_GC, d->pixmap,
							d->scroll_width - x, -y,
							d->x, d->y, x, d->h);
					}
				else if (   x <= 0
						 && (w = d->scroll_width + x) < d->w
						)
					{
					gdk_draw_drawable(p->pixmap, _GK.draw1_GC, d->pixmap,
							0, -y,
							d->x + w, d->y, d->w - w, d->h);
					}
				}
			if (d->flags & DF_SCROLL_TEXT_V_LOOP)
				{
				y %= d->scroll_height + d->y_ink;
				if (y > 0)
					gdk_draw_drawable(p->pixmap, _GK.draw1_GC, d->pixmap,
							-x, d->scroll_height + d->y_ink - y,
							d->x, d->y, d->w, y);
				else if (   y <= 0
						 && (h = d->scroll_height + d->y_ink + y) < d->h
						)
					gdk_draw_drawable(p->pixmap, _GK.draw1_GC, d->pixmap,
							-x, 0,
							d->x, d->y + h, d->w, d->h - h);
				}
			}
		else
			{
			gdk_gc_set_clip_mask(_GK.draw3_GC, d->stencil);
			gdk_gc_set_clip_origin(_GK.draw3_GC, d->x, d->y);
			gdk_draw_drawable(p->pixmap, _GK.draw3_GC, d->pixmap,
						0, d->y_src, d->x, d->y, d->w, d->h);
			restore_gc = TRUE;
			}
		}
	if (restore_gc)
		{
		gdk_gc_set_clip_mask(_GK.draw3_GC, NULL);
		gdk_gc_set_clip_origin(_GK.draw3_GC, 0, 0);
		}

	return do_top_layer;
	}

  /* Push krell pixmaps through their stencils onto a Panel expose pixmap
  */
static void
push_krell_pixmaps(GkrellmPanel *p)
	{
	GList			*list;
	GkrellmKrell	*k;
	GkrellmDrawrec	*dr;
	gboolean		restore_clip_mask   = FALSE,
					restore_clip_origin = FALSE;

	if (!p)
		return;
	for (list = p->krell_list; list; list = list->next)
		{
		k = (GkrellmKrell *) list->data;
		gdk_gc_set_clip_mask(_GK.text_GC, k->stencil);
		if (k->y0 != 0 || restore_clip_origin)
			{
			gdk_gc_set_clip_origin(_GK.text_GC, 0, k->y0);
			restore_clip_origin = TRUE;
			}
		dr = &k->draw;
		gdk_draw_drawable(p->pixmap, _GK.text_GC, k->pixmap,
					dr->x_src, dr->y_src, dr->x_dst, dr->y_dst, dr->w, dr->h);
		restore_clip_mask = TRUE;
		}
	if (restore_clip_mask)
		gdk_gc_set_clip_mask(_GK.text_GC, NULL);
	if (restore_clip_origin)
		gdk_gc_set_clip_origin(_GK.text_GC, 0, 0);
	}

void
gkrellm_draw_panel_layers(GkrellmPanel *p)
	{
	GList			*list;
	GkrellmKrell	*k;
	GkrellmDecal	*d;
	gboolean		do_top_layer_decals;

	if (!p || !p->drawing_area)
		return;

	if (p->need_decal_overlap_check)
		{
		gdk_draw_drawable(p->bg_text_layer_pixmap, _GK.draw1_GC,
					p->bg_pixmap,
					0, 0,  0, 0,  p->w, p->h);
		for (list = p->decal_list; list; list = list->next)
			{
			d = (GkrellmDecal *) list->data;
			if (!(d->flags & DF_SCROLL_TEXT_DIVERTED))
				d->flags &= ~DF_TEXT_OVERLAPS;
			d->modified = TRUE;
			}
		for (list = p->decal_list; list; list = list->next)
			{
			d = (GkrellmDecal *) list->data;
			panel_decal_check_text_overlap(d, list->next);
			}
		}
	p->need_decal_overlap_check = FALSE;

	panel_draw_decal_text_layer(p);

	for (list = p->decal_list; list; list = list->next)
		{
		d = (GkrellmDecal *) list->data;
		if (d->modified)
			{
			d->modified = FALSE;
			p->modified = TRUE;
			}
		}
	for (list = p->krell_list; list; list = list->next)
		{
		k = (GkrellmKrell *) list->data;
		if (k->modified)
			{
			k->modified = FALSE;
			p->modified = TRUE;
			}
		}

	/* For each layer, push new layer image onto the expose pixmap.
	*/
	if (p->modified)
		{
		gdk_draw_drawable(p->pixmap, _GK.draw1_GC, p->bg_text_layer_pixmap,
					0, 0,  0, 0,  p->w, p->h);

		do_top_layer_decals = push_decal_pixmaps(p, FALSE);
		if (p->label_on_top_of_decals)
			draw_panel_label(p, FALSE);
		push_krell_pixmaps(p);
		if (do_top_layer_decals)
			push_decal_pixmaps(p, TRUE);

		if (p->drawing_area->window)
			gdk_draw_drawable(p->drawing_area->window, _GK.draw1_GC, p->pixmap,
						0, 0,   0, 0,   p->w, p->h);
		}
	p->modified = FALSE;
	}


void
gkrellm_draw_panel_layers_force(GkrellmPanel *p)
	{
	GList	*list;

	if (!p)
		return;
	for (list = p->decal_list; list; list = list->next)
		((GkrellmDecal *) list->data)->modified = TRUE;
	for (list = p->krell_list; list; list = list->next)
		((GkrellmKrell *) list->data)->modified = TRUE;
	gkrellm_draw_panel_layers(p);
	}
