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

#include "gkrellmd.h"
#include "gkrellmd-private.h"

gboolean
gkrellmd_dup_string(gchar **dst, gchar *src)
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

gboolean
gkrellm_dup_string(gchar **dst, gchar *src)
	{
	return gkrellmd_dup_string(dst, src);
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
gkrellmd_dup_token(gchar **string, gchar *delimeters)
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

gchar *
gkrellm_dup_token(gchar **string, gchar *delimeters)
	{
	return gkrellmd_dup_token(string, delimeters);
	}

void
gkrellmd_free_glist_and_data(GList **list_head)
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

void
gkrellm_free_glist_and_data(GList **list_head)
	{
	gkrellmd_free_glist_and_data(list_head);
	}

  /* If there is a line in the gstring ('\n' delimited) copy it to the
  |  line buffer including the newline and erase it from the gstring.
  */
gboolean
gkrellmd_getline_from_gstring(GString **gstring, gchar *line, gint size)
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

gboolean
gkrellm_getline_from_gstring(GString **gstring, gchar *line, gint size)
	{
	return gkrellmd_getline_from_gstring(gstring, line, size);
	}
