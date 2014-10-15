/* GKrellM
|  Copyright (C) 1999-2014 Bill Wilson
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

/* ===============  string utility functions ================= */


gboolean
gkrellm_dup_string(gchar **dst, gchar *src)
	{
	if (!dst || (!*dst && !src))
		return FALSE;
	if (*dst)
		{
		if (src && !strcmp(*dst, src))
			return FALSE;
		g_free(*dst);
		}
	*dst = g_strdup(src);
	return TRUE;
	}

static gboolean
any(gchar c, gchar *s)
	{
	while (*s)
		if (c == *s++)
			return TRUE;
	return FALSE;
	}

  /* Return a duplicated token from a string.  "*string" points to the source
  |  string and is updated to point to the string remaining after the
  |  found token.  If there is no next token, return an empty dupped string
  |  (not a NULL pointer) and leave *string unchanged.
  |  Unlike strtok(): args are not modified, gkrellm_token() can be used on
  |  constant strings, delimeter identity is not lost, and it's thread safe.
  |  Only the caller's initial string pointer is modified.
  */
gchar *
gkrellm_dup_token(gchar **string, gchar *delimeters)
	{
	gchar			*str, *s, *delims;
	gboolean		quoted = FALSE;

	if (!string || !*string)
		return g_strdup("");

	str = *string;
	delims = delimeters ? delimeters : " \t\n";
	while (any(*str, delims))
		++str;

	if (*str == '"')
		{
		quoted = TRUE;
		++str;
		for (s = str; *s && *s != '"'; ++s)
			;
		}
	else
		for (s = str; *s && !any(*s, delims); ++s)
			;

	*string = (quoted && *s) ? s + 1 : s;
	return g_strndup(str, s - str);
	}

  /* Cut out an optionally quoted string.  This is destructive to the src.
  */
gchar *
gkrellm_cut_quoted_string(gchar *src, gchar **endptr)
	{
	gchar	*s;

	while (*src == ' ' || *src == '\t')
		++src;
	if (*src == '"')
		{
		s = strchr(++src, '"');
		if (s == NULL)
			{
			if (endptr)
				*endptr = src;
			g_warning(_("Unterminated quote\n"));
			return NULL;
			}
		*s = '\0';
		if (endptr)
			*endptr = s + 1;
		}
	else
		{
		for (s = src; *s != '\0' && *s != ' ' && *s != '\t'; ++s)
			;
		if (endptr)
			*endptr = *s ? s + 1 : s;
		*s = '\0';
		}
	return src;
	}

  /* If there is a line in the gstring ('\n' delimited) copy it to the
  |  line buffer including the newline and erase it from the gstring.
  */
gboolean
gkrellm_getline_from_gstring(GString **gstring, gchar *line, gint size)
	{
	GString	*gstr	= *gstring;
	gchar   *s;
	gint    len, n;

	if (gstr && gstr->str && (s = strchr(gstr->str, '\n')) != NULL)
		{
		n = len = s - gstr->str + 1;
		if (n >= size)
			n = size - 1;				/* Truncate the line to fit */
		strncpy(line, gstr->str, n);
		line[n] = '\0';
		*gstring = g_string_erase(gstr, 0, len);
		return TRUE;
		}
	return FALSE;
	}

/* ===============  list utility functions ================= */

void
gkrellm_free_glist_and_data(GList **list_head)
	{
	GList	*list;

	if (*list_head == NULL)
		return;

	/* could use g_list_foreach(*list_head, (G_FUNC)g_free, NULL);
	*/
	for (list = *list_head; list; list = list->next)
		if (list->data)
			g_free(list->data);
	g_list_free(*list_head);
	*list_head = NULL;
	}


GList *
gkrellm_string_in_list(GList *list, gchar *s)
	{
	if (!s)
		return NULL;
	for ( ; list; list = list->next)
		{
		if (!strcmp((gchar *) list->data, s))
			return list;
		}
	return NULL;
	}

gint
gkrellm_string_position_in_list(GList *list, gchar *s)
	{
	gint	i, n = -1;

	if (!s)
		return -1;
	for (i = 0 ; list; list = list->next, ++i)
		{
		if (!strcmp((gchar *) list->data, s))
			{
			n = i;
			break;
			}
		}
	return n;
	}



/* ===============  file utility functions ================= */
gchar *
gkrellm_homedir(void)
	{
	gchar	*homedir;

	homedir = (gchar *) g_get_home_dir();
	if (!homedir)
		homedir = ".";
	return homedir;
	}

gboolean
gkrellm_make_home_subdir(gchar *subdir, gchar **path)
	{
	gchar	*dir;
	gint	result	= FALSE;

	dir = g_build_path(G_DIR_SEPARATOR_S, gkrellm_homedir(), subdir, NULL);
	if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
		{
		if (g_mkdir(dir, 0755) < 0)
			g_warning(_("Cannot create directory: %s\n"), dir);
		else
			result = TRUE;
		}
	if (path)
		*path = dir;
	else
		g_free(dir);
	return result;
	}


/* ===============  GtkWidget utility functions ================= */

gchar *
gkrellm_gtk_entry_get_text(GtkWidget **entry)
	{
	static /*const*/ gchar *def_s = "";
	gchar *s = def_s;

	if (*entry)
		{
		s = (gchar *)gtk_entry_get_text(GTK_ENTRY(*entry));
		while (*s == ' ' || *s == '\t')
			++s;
		}
	return s;
	}


/* ===============  Miscellaneous utility functions ================= */

  /* Print a size, abbreviating it to kilo, mega, or giga units depending
  |  on its magnitude.
  |  An aside:  Memory capacities are traditionally reported in binary
  |  units (Kib, Mib, etc) while just about everything else should be
  |  reported in decimal units (KB, MB, etc).  This includes transfer
  |  rates, and disk capacities, contrary to what many people think.
  |  Take a look at http://www.pcguide.com/intro/fun/bindec.htm
  */
gint
gkrellm_format_size_abbrev(gchar *buf, size_t buflen, gfloat size,
		GkrellmSizeAbbrev *tbl, size_t tbl_size)
	{
	gfloat	abs_size;
	gint	i;

	abs_size = (size < 0.0) ? -size : size;

	for (i = 0; i < tbl_size - 1; ++i)
		if (abs_size < tbl[i].limit)
			break;
	return snprintf(buf, buflen, tbl[i].format, size / tbl[i].divisor);
	}


  /* Next three calls return string extent info.  Width extents are logical
  |  so that spaces will be counted while height extent is ink so that gkrellm
  |  can optimize vertical space utilization.
  */
gint
gkrellm_gdk_text_width(PangoFontDescription *font_desc,
				const gchar *string, gint len)
	{
	PangoLayout				*layout;
	gint					w, h;

	layout = gtk_widget_create_pango_layout(gkrellm_get_top_window(), NULL);
	pango_layout_set_font_description(layout, font_desc);
	pango_layout_set_text(layout, string, len);
	pango_layout_get_pixel_size(layout, &w, &h);
	g_object_unref(layout);
	return w;
	}

gint
gkrellm_gdk_text_markup_width(PangoFontDescription *font_desc,
				const gchar *string, gint len)
	{
	PangoLayout				*layout;
	gint					w, h;

	layout = gtk_widget_create_pango_layout(gkrellm_get_top_window(), NULL);
	pango_layout_set_font_description(layout, font_desc);
	pango_layout_set_markup(layout, string, len);
	pango_layout_get_pixel_size(layout, &w, &h);
	g_object_unref(layout);
	return w;
	}

gint
gkrellm_gdk_string_width(PangoFontDescription *font_desc, gchar *string)
	{
	PangoLayout	*layout;
	gint		w, h;

	layout = gtk_widget_create_pango_layout(gkrellm_get_top_window(), NULL);
	pango_layout_set_font_description(layout, font_desc);
	pango_layout_set_text(layout, string, strlen(string));
	pango_layout_get_pixel_size(layout, &w, &h);
	g_object_unref(layout);
	return w;
	}

gint
gkrellm_gdk_string_markup_width(PangoFontDescription *font_desc, gchar *string)
	{
	PangoLayout	*layout;
	gint		w, h;

	layout = gtk_widget_create_pango_layout(gkrellm_get_top_window(), NULL);
	pango_layout_set_font_description(layout, font_desc);
	pango_layout_set_markup(layout, string, strlen(string));
	pango_layout_get_pixel_size(layout, &w, &h);
	g_object_unref(layout);
	return w;
	}


void
gkrellm_text_extents(PangoFontDescription *font_desc, gchar *text,
		gint len, gint *width, gint *height, gint *baseline, gint *y_ink)
	{
	PangoLayout		*layout;
	PangoLayoutIter	*iter;
	PangoRectangle	ink, logical;
	gchar			*utf8;
	gint			base;

	layout = gtk_widget_create_pango_layout(gkrellm_get_top_window(), NULL);
	pango_layout_set_font_description(layout, font_desc);
	if (g_utf8_validate(text, -1, NULL))
		pango_layout_set_text(layout, text, len);
	else
		{
		utf8 = g_locale_to_utf8(text, -1, NULL, NULL, NULL);
		pango_layout_set_text(layout, utf8, len);
		g_free(utf8);
		}
	iter = pango_layout_get_iter(layout);
	base = pango_layout_iter_get_baseline(iter) / PANGO_SCALE;
	pango_layout_get_pixel_extents(layout, &ink, &logical);
	pango_layout_iter_free(iter);
	g_object_unref(layout);

	if (width)
		*width = logical.width;
	if (height)
		*height = ink.height;
	if (baseline)
		*baseline = base;
	if (y_ink)
		*y_ink = ink.y - logical.y;
	}

void
gkrellm_text_markup_extents(PangoFontDescription *font_desc, gchar *text,
		gint len, gint *width, gint *height, gint *baseline, gint *y_ink)
	{
	PangoLayout		*layout;
	PangoLayoutIter	*iter;
	PangoRectangle	ink, logical;
	gchar			*utf8;
	gint			base;

	layout = gtk_widget_create_pango_layout(gkrellm_get_top_window(), NULL);
	pango_layout_set_font_description(layout, font_desc);
	if (g_utf8_validate(text, -1, NULL))
		pango_layout_set_markup(layout, text, len);
	else
		{
		utf8 = g_locale_to_utf8(text, -1, NULL, NULL, NULL);
		pango_layout_set_markup(layout, utf8, len);
		g_free(utf8);
		}
	iter = pango_layout_get_iter(layout);
	base = pango_layout_iter_get_baseline(iter) / PANGO_SCALE;
	pango_layout_get_pixel_extents(layout, &ink, &logical);
	pango_layout_iter_free(iter);
	g_object_unref(layout);

	if (width)
		*width = logical.width;
	if (height)
		*height = ink.height;
	if (baseline)
		*baseline = base;
	if (y_ink)
		*y_ink = ink.y - logical.y;
	}

void
gkrellm_gdk_draw_string(GdkDrawable *drawable, PangoFontDescription *font_desc,
			GdkGC *gc, gint x, gint y, gchar *string)
	{
	PangoLayout	*layout;

	layout = gtk_widget_create_pango_layout(gkrellm_get_top_window(), NULL);
	pango_layout_set_font_description(layout, font_desc);
	pango_layout_set_text(layout, string, strlen(string));
	gdk_draw_layout(drawable, gc, x, y, layout);
	g_object_unref(layout);
	}

void
gkrellm_gdk_draw_string_markup(GdkDrawable *drawable,
			PangoFontDescription *font_desc,
			GdkGC *gc, gint x, gint y, gchar *string)
	{
	PangoLayout	*layout;

	layout = gtk_widget_create_pango_layout(gkrellm_get_top_window(), NULL);
	pango_layout_set_font_description(layout, font_desc);
	pango_layout_set_markup(layout, string, strlen(string));
	gdk_draw_layout(drawable, gc, x, y, layout);
	g_object_unref(layout);
	}

void
gkrellm_gdk_draw_text(GdkDrawable *drawable, PangoFontDescription *font_desc,
			GdkGC *gc, gint x, gint y, gchar *string, gint len)
	{
	PangoLayout	*layout;

	layout = gtk_widget_create_pango_layout(gkrellm_get_top_window(), NULL);
	pango_layout_set_font_description(layout, font_desc);
	pango_layout_set_text(layout, string, len);
	gdk_draw_layout(drawable, gc, x, y, layout);
	g_object_unref(layout);
	}

void
gkrellm_gdk_draw_text_markup(GdkDrawable *drawable,
			PangoFontDescription *font_desc,
			GdkGC *gc, gint x, gint y, gchar *string, gint len)
	{
	PangoLayout	*layout;

	layout = gtk_widget_create_pango_layout(gkrellm_get_top_window(), NULL);
	pango_layout_set_font_description(layout, font_desc);
	pango_layout_set_markup(layout, string, len);
	gdk_draw_layout(drawable, gc, x, y, layout);
	g_object_unref(layout);
	}

  /* Gtk config widgets work with utf8, so as long as I'm using gdk_draw
  |  functions, both utf8 and current locale versions of strings drawn on
  |  GKrellM must be maintained.  If src is not utf8, *dst is converted
  |  to utf8 and this should fix 1.2 -> 2.0 user_config conversions
  |  (This function will usually be called from config loading).
  |  dst_locale is piggy backing so when gdk_draw is replaced by Pango
  |  equivalents, usage of this function can be replaced with a simple
  |  gkrellm_dup_string().
  |  2.2.0 converts to using Pango.  Before replacing with gkrellm_dup_string,
  |  temporarily just treat dst_locale as a direct copy of dst_utf8.
  */
gboolean
gkrellm_locale_dup_string(gchar **dst_utf8, gchar *src, gchar **dst_locale)
	{
	if (!dst_utf8 || (!*dst_utf8 && !src))
		return FALSE;
	if (*dst_utf8)
		{
		if (src && !strcmp(*dst_utf8, src))
			return FALSE;
		g_free(*dst_utf8);
		g_free(*dst_locale);
		}
	if (src)
		{
		if (g_utf8_validate(src, -1, NULL))
			{
			*dst_utf8 = g_strdup(src);

			*dst_locale = g_strdup(src);
//			*dst_locale = g_locale_from_utf8(src, -1, NULL, NULL, NULL);
//			if (!*dst_locale)
//				*dst_locale = g_strdup(src);
			}
		else
			{
			*dst_utf8 = g_locale_to_utf8(src, -1, NULL, NULL, NULL);
			if (!*dst_utf8)
				*dst_utf8 = g_strdup(src);

			*dst_locale = g_strdup(*dst_utf8);
//			*dst_locale = g_strdup(src);
			}
		}
	else
		{
		*dst_utf8 = NULL;
		*dst_locale = NULL;
		}
	return TRUE;
	}

