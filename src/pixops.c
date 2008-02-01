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

void
gkrellm_free_pixmap(GdkPixmap **pixmap)
	{
	if (!pixmap)
		return;
	if (*pixmap)
		g_object_unref(G_OBJECT(*pixmap));
	*pixmap = NULL;
	}

void
gkrellm_free_bitmap(GdkBitmap **bitmap)
	{
	if (!bitmap)
		return;
	if (*bitmap)
		g_object_unref(G_OBJECT(*bitmap));
	*bitmap = NULL;
	}

gboolean
gkrellm_clone_pixmap(GdkPixmap **dest, GdkPixmap **src)
	{
	gint	w_dest = 0, h_dest = 0, w_src = 0, h_src = 0;

	if (!dest)
		return FALSE;
	if (!src || !*src)
		{
		gkrellm_free_pixmap(dest);
		return FALSE;
		}
	gdk_drawable_get_size(*src, &w_src, &h_src);
	if (*dest)
		{
		gdk_drawable_get_size(*dest, &w_dest, &h_dest);
		if (w_dest != w_src || h_dest != h_src)
			gkrellm_free_pixmap(dest);
		}
	if (!*dest)
		*dest = gdk_pixmap_new(gkrellm_get_top_window()->window,
					w_src, h_src, -1);
	gdk_draw_drawable(*dest, _GK.draw1_GC, *src, 0, 0, 0, 0, w_src, h_src);
	return TRUE;	
	}

gboolean
gkrellm_clone_bitmap(GdkBitmap **dest, GdkBitmap **src)
	{
	gint	w_dest = 0, h_dest = 0, w_src = 0, h_src = 0;

	if (!dest)
		return FALSE;
	if (!src || !*src)
		{
		gkrellm_free_bitmap(dest);
		return FALSE;
		}
	gdk_drawable_get_size(*src, &w_src, &h_src);
	if (*dest)
		{
		gdk_drawable_get_size(*dest, &w_dest, &h_dest);
		if (w_dest != w_src || h_dest != h_src)
			gkrellm_free_bitmap(dest);
		}
	if (!*dest)
		*dest = gdk_pixmap_new(gkrellm_get_top_window()->window,
					w_src, h_src, 1);
	gdk_draw_drawable(*dest, _GK.bit1_GC, *src, 0, 0, 0, 0, w_src, h_src);
	return TRUE;	
	}

static void
_render_to_pixmap(GdkPixbuf *pixbuf, GdkPixmap **pixmap, GdkBitmap **mask,
		gint x_src, gint y_src, gint x_dst, gint y_dst, gint w, gint h)
	{
	gdk_pixbuf_render_to_drawable(pixbuf, *pixmap, _GK.draw1_GC,
			x_src, y_src, x_dst, y_dst,
			w, h, GDK_RGB_DITHER_NORMAL, 0, 0);
	if (mask && *mask)
		gdk_pixbuf_render_threshold_alpha(pixbuf, *mask,
					x_src, y_src, x_dst, y_dst,
					w, h, 128 /* alpha threshold */);
	}

gboolean
gkrellm_scale_pixbuf_to_pixmap(GdkPixbuf *src_pixbuf, GdkPixmap **pixmap,
			GdkBitmap **mask, gint w_dst, gint h_dst)
	{
	GdkWindow		*window = gkrellm_get_top_window()->window;
	GdkPixbuf		*dst_pixbuf;
	gint			w_src, h_src;
	gboolean		has_alpha;
	GdkInterpType	interp_type;

	gkrellm_free_pixmap(pixmap);
	gkrellm_free_bitmap(mask);

	if (!src_pixbuf || !pixmap)
		return FALSE;

	has_alpha = gdk_pixbuf_get_has_alpha(src_pixbuf);
	w_src = gdk_pixbuf_get_width(src_pixbuf);
	h_src = gdk_pixbuf_get_height(src_pixbuf);

	if (w_dst == 0)
		w_dst = w_src;
	else if (w_dst < 0 && (w_dst = w_src * _GK.theme_scale / 100) <= 0)
		w_dst = 1;

	if (h_dst == 0)
		h_dst = h_src;
	else if (h_dst < 0 && (h_dst = h_src * _GK.theme_scale / 100) <= 0)
		h_dst = 1;

	*pixmap = gdk_pixmap_new(window, w_dst, h_dst, -1);
	if (mask && has_alpha)
		*mask = gdk_pixmap_new(window, w_dst, h_dst, 1);

	if (w_dst == w_src && h_dst == h_src)
		{
		_render_to_pixmap(src_pixbuf, pixmap, mask, 0, 0, 0, 0, w_dst, h_dst);
		return TRUE;
		}
	if (w_dst > w_src && h_dst > h_src)
		interp_type = GDK_INTERP_NEAREST;
	else
		interp_type = GDK_INTERP_BILINEAR;

	dst_pixbuf = gdk_pixbuf_scale_simple(src_pixbuf, w_dst, h_dst,interp_type);
	_render_to_pixmap(dst_pixbuf, pixmap, mask, 0, 0, 0, 0, w_dst, h_dst);
	g_object_unref(G_OBJECT(dst_pixbuf));
	return TRUE;
	}

static void
fix_border_overlap(gint *a, gint *b, gint l)
	{
	gint	A = 0, B = 0;
	gint	lb;

	lb = *a + *b;
	if (l > 1 && lb > 0)
		{
		A = *a * (l - 1) / lb;
		B = *b * (l - 1) / lb;
		}
	*a = A;
	*b = B;
	}

GdkPixbuf *
gkrellm_scale_piximage_to_pixbuf(GkrellmPiximage *piximage,
			gint w_dst, gint h_dst)
	{
	GdkPixbuf		*src_pixbuf, *dst_pixbuf;
	GkrellmBorder	b;
	gint			w_src, h_src;
	gint			src_width, src_height, dst_width, dst_height;
	gboolean		has_alpha;
	double			v_scale, h_scale;
	GdkInterpType	interp_type;

	src_pixbuf = piximage->pixbuf;
	b = piximage->border;

	has_alpha = gdk_pixbuf_get_has_alpha(src_pixbuf);
	w_src = gdk_pixbuf_get_width(src_pixbuf);
	h_src = gdk_pixbuf_get_height(src_pixbuf);

	if (w_dst == 0)
		w_dst = w_src;
	else if (w_dst < 0 && (w_dst = w_src * _GK.theme_scale / 100) <= 0)
		w_dst = 1;

	if (h_dst == 0)
		h_dst = h_src;
	else if (h_dst < 0 && (h_dst = h_src * _GK.theme_scale / 100) <= 0)
		h_dst = 1;

	dst_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, has_alpha, 8, w_dst,h_dst);

	if (b.left + b.right >= w_dst || b.left + b.right >= w_src)
		fix_border_overlap(&b.left, &b.right, (w_dst > w_src) ? w_src : w_dst);
	if (b.top + b.bottom >= h_dst || b.top + b.bottom >= h_src)
		fix_border_overlap(&b.top, &b.bottom, (h_dst > h_src) ? h_src : h_dst);

	/* Corner areas are not scaled
	*/
	if (b.left > 0 && b.top > 0)
		gdk_pixbuf_copy_area(src_pixbuf, 0, 0,
				b.left, b.top,
				dst_pixbuf, 0, 0);
	if (b.left > 0 && b.bottom > 0)
		gdk_pixbuf_copy_area(src_pixbuf, 0, h_src - b.bottom,
				b.left, b.bottom,
				dst_pixbuf, 0, h_dst - b.bottom);
	if (b.right > 0 && b.top > 0)
		gdk_pixbuf_copy_area(src_pixbuf, w_src - b.right, 0,
				b.right, b.top,
				dst_pixbuf, w_dst - b.right, 0);
	if (b.right > 0 && b.bottom > 0)
		gdk_pixbuf_copy_area(src_pixbuf, w_src - b.right, h_src - b.bottom,
				b.right, b.bottom,
				dst_pixbuf, w_dst - b.right, h_dst - b.bottom);

	dst_width = w_dst - b.left - b.right;
	dst_height = h_dst - b.top - b.bottom;
	src_width = w_src - b.left - b.right;
	src_height = h_src - b.top - b.bottom;
	if (src_width <= 0)
		src_width = 1;
	if (src_height <= 0)
		src_height = 1;
	v_scale = (double) dst_height / (double) src_height;
	h_scale = (double) dst_width / (double) src_width;
	if (v_scale > 1.0 && h_scale > 1.0)
		interp_type = GDK_INTERP_NEAREST;
	else
		interp_type = GDK_INTERP_BILINEAR;

	/* I don't want to hurt my brain again with figuring out what is going
	|  on with these pixbuf offsets, so here's one dimensional clues for the
	|  x scale > 1 case where I want to write a center stretched region into
	|  the dst_pixbuf.  The 'o' pixels are a border of width L and R, the dest
	|  center region width is dst_width.  Borders on src and dst are the same.
	|  (You'll need a fixed width font for this diagram to make sense)
	|
	|                                 src_width
	|                          L->|  |<-      ->|  |<-R
	|  src_pixbuf                 oooo..........oooo
	|  dst_pixbuf                 oooo....................oooo
	|                          L->|  |<-  dst_width     ->|  |<-R
	|  src scaled by S to get     oooooooooo....................oooooooooo
	|  center to dst_width size ->|        |<- L * S         |
	|                             |                          |
	|  Shift left by (L * x)      |                          |
	|  and then right by L        |  |                    |  |
	|  to get the x_offset  oooooooooo....................oooooooooo
	|                     ->|     |<- x_offset = -(L*S) + L  |
	|                             |                          |
	|  Now, write dst_width pixels from src_pixbuf L to dst_pixbuf L.  Look at
	|  top border case below.     |                          |
	|                             |                          |
	|  To write the right border (bottom is similar) the situation is
	|  the dst_pixbuf is larger than src_pixbuf (x scale > 1 case), but
	|  border pixels should not be scaled.  ie x_scale = 1.0 for this write.
	|                             |                          |
	|                             |           ->|  |<- R     |
	|  src_pixbuf                 oooo..........oooo         |
	|  dst_pixbuf                 oooo....................oooo
	|                             |                |    ->|  |<- R
	|  Shift src right by         |         oooo..........oooo
	|  (w_dst - w_src) and        |                |      |  |
	|  write R pixels at          ----------------------------
	|  (w_dst - R) from           |                |      |  |
	|  src_pixbuf to dst_pixbuf.  0               w_src      w_dst
	*/

	if (b.left > 0 && dst_height > 0)	/* Left border */
		gdk_pixbuf_scale(src_pixbuf, dst_pixbuf, 0, b.top,
			b.left, dst_height,
			0, (double) (b.top * -v_scale + b.top),
			1.0, v_scale, interp_type);
				
	if (b.right > 0 && dst_height > 0)	/* right border */
		gdk_pixbuf_scale(src_pixbuf, dst_pixbuf, w_dst - b.right, b.top,
			b.right, dst_height,
			(double) (w_dst - w_src), (double) (b.top * -v_scale + b.top),
			1.0, v_scale, interp_type);
				
	if (b.top > 0 && dst_width > 0)	/* top border */
		gdk_pixbuf_scale(src_pixbuf, dst_pixbuf, b.left, 0,
			dst_width, b.top,
			(double) (b.left * -h_scale + b.left), 0,
			h_scale, 1.0, interp_type);
				
	if (b.bottom > 0 && dst_width > 0)	/* bottom border */
		gdk_pixbuf_scale(src_pixbuf, dst_pixbuf, b.left, h_dst - b.bottom,
			dst_width, b.bottom,
			(double) (b.left * -h_scale + b.left), (double) (h_dst - h_src),
			h_scale, 1.0, interp_type);

	if (dst_width > 0 && dst_height > 0)	/* Center area */
		gdk_pixbuf_scale(src_pixbuf, dst_pixbuf, b.left, b.top,
			dst_width, dst_height,
			(double) (b.left * -h_scale + b.left),
			(double) (b.top * -v_scale + b.top),
			h_scale, v_scale, interp_type);

	return dst_pixbuf;
	}

gboolean
gkrellm_scale_piximage_to_pixmap(GkrellmPiximage *piximage, GdkPixmap **pixmap,
			GdkBitmap **mask, gint w_dst, gint h_dst)
	{
	GdkWindow		*window = gkrellm_get_top_window()->window;
	GdkPixbuf		*src_pixbuf, *dst_pixbuf;
	gint			w_src, h_src;
	gboolean		has_alpha;

	/* I want the pixmap freed even if there is no image to render back
	|  in.  Eg. theme switch to one with no data_in/out_piximage.
	*/
	gkrellm_free_pixmap(pixmap);
	gkrellm_free_bitmap(mask);

	if (!piximage || !piximage->pixbuf || !pixmap)
		return FALSE;

	src_pixbuf = piximage->pixbuf;
	has_alpha = gdk_pixbuf_get_has_alpha(src_pixbuf);
	w_src = gdk_pixbuf_get_width(src_pixbuf);
	h_src = gdk_pixbuf_get_height(src_pixbuf);

	if (w_dst == 0)
		w_dst = w_src;
	else if (w_dst < 0 && (w_dst = w_src * _GK.theme_scale / 100) <= 0)
		w_dst = 1;

	if (h_dst == 0)
		h_dst = h_src;
	else if (h_dst < 0 && (h_dst = h_src * _GK.theme_scale / 100) <= 0)
		h_dst = 1;

	*pixmap = gdk_pixmap_new(window, w_dst, h_dst, -1);
	if (mask && has_alpha)
		*mask = gdk_pixmap_new(window, w_dst, h_dst, 1);

	if (w_dst == w_src && h_dst == h_src)
		{
		_render_to_pixmap(src_pixbuf, pixmap, mask, 0, 0, 0, 0, w_dst, h_dst);
		return TRUE;
		}

	dst_pixbuf = gkrellm_scale_piximage_to_pixbuf(piximage, w_dst, h_dst);
	_render_to_pixmap(dst_pixbuf, pixmap, mask, 0, 0, 0, 0, w_dst, h_dst);

	g_object_unref(G_OBJECT(dst_pixbuf));
	return TRUE;
	}

void
gkrellm_paste_piximage(GkrellmPiximage *src_piximage, GdkDrawable *drawable,
		gint x_dst, gint y_dst, gint w_dst, gint h_dst)
	{
	GdkPixbuf	*dst_pixbuf;
	gint		w_src, h_src;

	if (!src_piximage || !drawable)
		return;

	w_src = gdk_pixbuf_get_width(src_piximage->pixbuf);
	h_src = gdk_pixbuf_get_height(src_piximage->pixbuf);

	if (w_dst == 0)
		w_dst = w_src;
	else if (w_dst < 0 && (w_dst = w_src * _GK.theme_scale / 100) <= 0)
		w_dst = 1;

	if (h_dst == 0)
		h_dst = h_src;
	else if (h_dst < 0 && (h_dst = h_src * _GK.theme_scale / 100) <= 0)
		h_dst = 1;

	dst_pixbuf = gkrellm_scale_piximage_to_pixbuf(src_piximage, w_dst, h_dst);
	gdk_pixbuf_render_to_drawable(dst_pixbuf, drawable, _GK.draw1_GC,
			0, 0, x_dst, y_dst,
			w_dst, h_dst, GDK_RGB_DITHER_NORMAL, 0, 0);
	g_object_unref(G_OBJECT(dst_pixbuf));
	}

void
gkrellm_paste_pixbuf(GdkPixbuf *src_pixbuf, GdkDrawable *drawable,
		gint x_dst, gint y_dst, gint w_dst, gint h_dst)
	{
	GdkPixbuf		*dst_pixbuf;
	GdkInterpType	interp_type;
	gint			w_src, h_src;

	if (!src_pixbuf || !drawable)
		return;

	w_src = gdk_pixbuf_get_width(src_pixbuf);
	h_src = gdk_pixbuf_get_height(src_pixbuf);
	if (w_dst > w_src && h_dst > h_src)
		interp_type = GDK_INTERP_NEAREST;
	else
		interp_type = GDK_INTERP_BILINEAR;

	dst_pixbuf = gdk_pixbuf_scale_simple(src_pixbuf, w_dst, h_dst,interp_type);
	gdk_pixbuf_render_to_drawable(dst_pixbuf, drawable, _GK.draw1_GC,
			0, 0, x_dst, y_dst,
			w_dst, h_dst, GDK_RGB_DITHER_NORMAL, 0, 0);
	g_object_unref(G_OBJECT(dst_pixbuf));
	}

gboolean
gkrellm_scale_theme_background(GkrellmPiximage *piximage, GdkPixmap **pixmap,
			GdkBitmap **mask, gint w_dst, gint h_dst)
	{
	GdkWindow		*window = gkrellm_get_top_window()->window;
	GdkPixbuf		*src_pixbuf, *sub_pixbuf, *pixbuf;
	GdkInterpType	interp_type;
	GkrellmBorder	bdr;
	gint			w_src, h_src, l, r, t, b;
	gint			hs, hd, ws, wd;
	gboolean		has_alpha;

	if (_GK.theme_scale == 100)
		return gkrellm_scale_piximage_to_pixmap(piximage, pixmap, mask,
					w_dst, h_dst);

	/* I want the pixmap freed even if there is no image to render back
	|  in.  Eg. theme switch to one with no data_in/out_piximage.
	*/
	gkrellm_free_pixmap(pixmap);
	gkrellm_free_bitmap(mask);

	if (!piximage || !piximage->pixbuf || !pixmap)
		return FALSE;

	src_pixbuf = piximage->pixbuf;
	has_alpha = gdk_pixbuf_get_has_alpha(src_pixbuf);
	w_src = gdk_pixbuf_get_width(src_pixbuf);
	h_src = gdk_pixbuf_get_height(src_pixbuf);

	if (w_dst == 0)
		w_dst = w_src;

	if (h_dst == 0)
		h_dst = h_src;

	*pixmap = gdk_pixmap_new(window, w_dst, h_dst, -1);
	if (mask && has_alpha)
		*mask = gdk_pixmap_new(window, w_dst, h_dst, 1);

	bdr = piximage->border;
	if (bdr.left + bdr.right >= w_src)
		fix_border_overlap(&bdr.left, &bdr.right, w_src);
	if (bdr.top + bdr.bottom >= h_src)
		fix_border_overlap(&bdr.top, &bdr.bottom, h_src);
	l = bdr.left * _GK.theme_scale / 100;
	r = bdr.right * _GK.theme_scale / 100;
	t = bdr.top * _GK.theme_scale / 100;
	b = bdr.bottom * _GK.theme_scale / 100;
	if (l + r >= w_dst)
		fix_border_overlap(&l, &r, w_dst);
	if (t + b >= h_dst)
		fix_border_overlap(&t, &b, h_dst);

	interp_type = GDK_INTERP_BILINEAR;

	if (l > 0 && t > 0)		/* top left corner */
		{
		sub_pixbuf = gdk_pixbuf_new_subpixbuf(src_pixbuf, 0, 0,
					bdr.left, bdr.top);
		pixbuf = gdk_pixbuf_scale_simple(sub_pixbuf, l, t, interp_type);
		_render_to_pixmap(pixbuf, pixmap, mask, 0, 0, 0, 0, l, t);
		g_object_unref(G_OBJECT(sub_pixbuf));
		g_object_unref(G_OBJECT(pixbuf));
		}
	if (l > 0 && b > 0)		/* bottom left corner */
		{
		sub_pixbuf = gdk_pixbuf_new_subpixbuf(src_pixbuf,
					0, h_src - bdr.bottom,
					bdr.left, bdr.bottom);
		pixbuf = gdk_pixbuf_scale_simple(sub_pixbuf, l, b, interp_type);
		_render_to_pixmap(pixbuf, pixmap, mask, 0, 0, 0, h_dst - b, l, b);
		g_object_unref(G_OBJECT(sub_pixbuf));
		g_object_unref(G_OBJECT(pixbuf));
		}
	if (r > 0 && t > 0)		/* top right corner */
		{
		sub_pixbuf = gdk_pixbuf_new_subpixbuf(src_pixbuf,
					w_src - bdr.right, 0,
					bdr.right, bdr.top);
		pixbuf = gdk_pixbuf_scale_simple(sub_pixbuf, r, t, interp_type);
		_render_to_pixmap(pixbuf, pixmap, mask, 0, 0, w_dst - r, 0, r, t);
		g_object_unref(G_OBJECT(sub_pixbuf));
		g_object_unref(G_OBJECT(pixbuf));
		}
	if (r > 0 && b > 0)		/* bottom right corner */
		{
		sub_pixbuf = gdk_pixbuf_new_subpixbuf(src_pixbuf,
					w_src - bdr.right, h_src - bdr.bottom,
					bdr.right, bdr.bottom);
		pixbuf = gdk_pixbuf_scale_simple(sub_pixbuf, r, b, interp_type);
		_render_to_pixmap(pixbuf, pixmap, mask, 0, 0,
					w_dst - r, h_dst - b, r, b);
		g_object_unref(G_OBJECT(sub_pixbuf));
		g_object_unref(G_OBJECT(pixbuf));
		}
	hs = h_src - bdr.top - bdr.bottom;
	hd = h_dst - t - b;
	if (l > 0 && hs > 0 && hd > 0)	/* left edge */
		{
		sub_pixbuf = gdk_pixbuf_new_subpixbuf(src_pixbuf, 0, bdr.top,
					bdr.left, hs);
		pixbuf = gdk_pixbuf_scale_simple(sub_pixbuf, l, hd, interp_type);
		_render_to_pixmap(pixbuf, pixmap, mask, 0, 0, 0, t, l, hd);
		g_object_unref(G_OBJECT(sub_pixbuf));
		g_object_unref(G_OBJECT(pixbuf));
		}
	if (r > 0 && hs > 0 && hd > 0)	/* right edge */
		{
		sub_pixbuf = gdk_pixbuf_new_subpixbuf(src_pixbuf,
					w_src - bdr.right, bdr.top,
					bdr.right, hs);
		pixbuf = gdk_pixbuf_scale_simple(sub_pixbuf, r, hd, interp_type);
		_render_to_pixmap(pixbuf, pixmap, mask, 0, 0, w_dst - r, t, r, hd);
		g_object_unref(G_OBJECT(sub_pixbuf));
		g_object_unref(G_OBJECT(pixbuf));
		}
	ws = w_src - bdr.left - bdr.right;
	wd = w_dst - l - r;
	if (t > 0 && ws > 0 && wd > 0)	/* top edge */
		{
		sub_pixbuf = gdk_pixbuf_new_subpixbuf(src_pixbuf,
					bdr.left, 0, ws, bdr.top);
		pixbuf = gdk_pixbuf_scale_simple(sub_pixbuf, wd, t, interp_type);
		_render_to_pixmap(pixbuf, pixmap, mask, 0, 0, l, 0, wd, t);
		g_object_unref(G_OBJECT(sub_pixbuf));
		g_object_unref(G_OBJECT(pixbuf));
		}
	if (b > 0 && ws > 0 && wd > 0)	/* bottom edge */
		{
		sub_pixbuf = gdk_pixbuf_new_subpixbuf(src_pixbuf,
					bdr.left, h_src - bdr.bottom, ws, bdr.bottom);
		pixbuf = gdk_pixbuf_scale_simple(sub_pixbuf, wd, b, interp_type);
		_render_to_pixmap(pixbuf, pixmap, mask, 0, 0, l, h_dst - b, wd, b);
		g_object_unref(G_OBJECT(sub_pixbuf));
		g_object_unref(G_OBJECT(pixbuf));
		}
	if (ws > 0 && wd > 0 && hs > 0 && hd > 0)	/* center area */
		{
		sub_pixbuf = gdk_pixbuf_new_subpixbuf(src_pixbuf,
					bdr.left, bdr.top, ws, hs);
		pixbuf = gdk_pixbuf_scale_simple(sub_pixbuf, wd, hd, interp_type);
		_render_to_pixmap(pixbuf, pixmap, mask, 0, 0, l, t, wd, hd);
		g_object_unref(G_OBJECT(sub_pixbuf));
		g_object_unref(G_OBJECT(pixbuf));
		}
	return TRUE;
	}

void
gkrellm_destroy_piximage(GkrellmPiximage *piximage)
	{
	if (!piximage || !piximage->pixbuf)
		return;
	g_object_unref(G_OBJECT(piximage->pixbuf));
	g_free(piximage);
	}

void
gkrellm_set_piximage_border(GkrellmPiximage *piximage, GkrellmBorder *border)
	{
	GkrellmBorder	*b;

	if (!piximage || !piximage->pixbuf)
		return;
	b = &piximage->border;
	*b = *border;
	if (b->left < 0)
		b->left = 0;
	if (b->right < 0)
		b->right = 0;
	if (b->top < 0)
		b->top = 0;
	if (b->bottom < 0)
		b->bottom = 0;
	}

void
gkrellm_border_adjust(GkrellmBorder *border, gint l, gint r, gint t, gint b)
	{
	border->left += l;
	border->right += r;
	border->top += t;
	border->bottom += b;
	if (border->left < 0)
		border->left = 0;
	if (border->right < 0)
		border->right = 0;
	if (border->top < 0)
		border->top = 0;
	if (border->bottom < 0)
		border->bottom = 0;
	}

GkrellmPiximage *
gkrellm_clone_piximage(GkrellmPiximage *src_piximage)
	{
	GkrellmPiximage	*piximage;

	if (!src_piximage)
		return NULL;
	piximage = g_new0(GkrellmPiximage, 1);
	piximage->border = src_piximage->border;
	piximage->pixbuf = gdk_pixbuf_copy(src_piximage->pixbuf);
	return piximage;
	}

GkrellmPiximage *
gkrellm_piximage_new_from_xpm_data(gchar **data)
	{
	GkrellmPiximage	*piximage;
	GdkPixbuf		*pixbuf;

	pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) data);
	if (!pixbuf)
		return NULL;
	piximage = g_new0(GkrellmPiximage, 1);
	piximage->pixbuf = pixbuf;
	return piximage;
	}

GkrellmPiximage *
gkrellm_piximage_new_from_file(gchar *fname)
	{
	GkrellmPiximage	*piximage;
	GdkPixbuf		*pixbuf;

	pixbuf = gdk_pixbuf_new_from_file(fname, NULL);
	if (!pixbuf)
		return NULL;
	piximage = g_new0(GkrellmPiximage, 1);
	piximage->pixbuf = pixbuf;
	return piximage;
	}

GkrellmPiximage *
gkrellm_piximage_new_from_inline(const guint8 *data, gboolean copy_pixels)
	{
	GkrellmPiximage	*piximage;
	GdkPixbuf		*pixbuf;

	pixbuf = gdk_pixbuf_new_from_inline(-1, data, copy_pixels, NULL);
 	if (!pixbuf)
		return NULL;
	piximage = g_new0(GkrellmPiximage, 1);
	piximage->pixbuf = pixbuf;
	return piximage;
	}
