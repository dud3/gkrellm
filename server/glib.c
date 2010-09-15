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

#include "gkrellmd.h"
#include "gkrellmd-private.h"

#if !GLIB_CHECK_VERSION(2,0,0)
/* glib2 compatibility functions for compiling gkrellmd under glib1.2
*/

GDir *
g_dir_open(gchar *path, guint flags, gpointer error)
	{
	GDir	*gdir;
	DIR		*dir;

	dir = opendir(path);
	if (!dir)
		return NULL;
	gdir = g_new0(GDir, 1);
	gdir->dir = dir;
	return gdir;
	}

gchar*
g_dir_read_name(GDir *dir)
	{
	struct dirent *entry;

	while ((entry = readdir(dir->dir)) != NULL)
		{
		if (   !strcmp(entry->d_name, ".")
			|| !strcmp(entry->d_name, "..")
		   )
			continue;
		return entry->d_name;
		}
	return NULL;
	}

void
g_dir_close(GDir *dir)
	{
	closedir(dir->dir);
	g_free(dir);
	}

gboolean
g_file_test(gchar *filename, gint test)
	{
	struct stat		s;

	if ((test & G_FILE_TEST_EXISTS) && (access(filename, F_OK) == 0))
		return TRUE;
	if (   (test & G_FILE_TEST_IS_DIR)
		&& stat(filename, &s) == 0
		&& S_ISDIR(s.st_mode)
	   )
		return TRUE;
	if (   (test & G_FILE_TEST_IS_REGULAR)
		&& stat(filename, &s) == 0
		&& S_ISREG(s.st_mode)
	   )
		return TRUE;

	return FALSE;
	}


gchar *
g_build_filename(gchar *first, ...)
	{
	gchar		*str;
	va_list		args;
	gchar		*s, *element, *next_element;
	gboolean	is_first = TRUE;

	va_start(args, first);
	next_element = first;
	str = g_strdup("");

	while (1)
		{
		if (next_element)
			{
			element = next_element;
			next_element = va_arg(args, gchar *);
			}
		else
			break;
		if (is_first)
			{
			is_first = FALSE;
			g_free(str);
			str = g_strdup(element);
			}
		else
			{
			s = str;
			str = g_strconcat(str, G_DIR_SEPARATOR_S, element, NULL);
			g_free(s);
			}
		}
	va_end (args);

	return str;
	}

gchar *
g_path_get_basename(gchar *fname)
	{
	gchar	*s;

	if (!*fname)
		return g_strdup(".");

	s = strrchr(fname, G_DIR_SEPARATOR);
	if (!s)
		return g_strdup(fname);
	return g_strdup(s + 1);		/* don't handle paths ending in slash */
	}

#endif
