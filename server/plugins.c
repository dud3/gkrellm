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

#include "gkrellmd.h"
#include "gkrellmd-private.h"
#if defined(WIN32)
    #include "win32-plugin.h"
#endif

#include <gmodule.h>


static GList	*plugin_list;
GList			*gkrellmd_plugin_enable_list;
gchar			*plugin_install_log;


/* ======================================================================= */
/* Plugin interface to gkrellmd.
*/


static void
gkrellmd_plugin_log(gchar *string1, ...)
	{
	va_list		args;
	gchar		*s, *old_log;

	if (!string1)
		return;
	va_start(args, string1);
	s = string1;
	while (s)
		{
		old_log = plugin_install_log;
		if (plugin_install_log)
			plugin_install_log = g_strconcat(plugin_install_log, s, NULL);
		else
			plugin_install_log = g_strconcat(s, NULL);
		g_free(old_log);
		s = va_arg(args, gchar *);
		}
	va_end(args);
	}

GkrellmdMonitor *
gkrellmd_plugin_install(gchar *plugin_name)
	{
	GModule					*module;
	GkrellmdMonitor			*m;
	GkrellmdMonitor			*(*init_plugin)();
	gchar					buf[256];

	if (!g_module_supported())
		return NULL;
	module = g_module_open(plugin_name, 0);
	gkrellmd_plugin_log(plugin_name, "\n", NULL);

	if (! module)
		{
		snprintf(buf, sizeof(buf), _("\tError: %s\n"), g_module_error());
		gkrellmd_plugin_log(buf, NULL);
		return NULL;
		}
	if (!g_module_symbol(module, "gkrellmd_init_plugin",
				(gpointer) &init_plugin))
		{
		snprintf(buf, sizeof(buf), _("\tError: %s\n"), g_module_error());
		gkrellmd_plugin_log(buf, NULL);
		g_module_close(module);
		return NULL;
		}

//	mon_tmp.name = g_strdup(plugin_name);
//	gkrellm_record_state(INIT_MONITOR, &mon_tmp);

#if defined(WIN32)
	{
		win32_plugin_callbacks ** plugin_cb = NULL;

		if (!g_module_symbol(module, "cb", (gpointer) &plugin_cb))
		{
			snprintf(buf, sizeof(buf), _("\tError: %s\n"), g_module_error());
			gkrellmd_plugin_log(buf, NULL);
			g_module_close(module);
			return NULL;
		}
		*plugin_cb = &gkrellmd_callbacks;
	}
#endif

	m = (*init_plugin)();

//	g_free(mon_tmp.name);
//	mon_tmp.name = NULL;
//	gkrellm_record_state(INTERNAL, NULL);

	if (m == NULL)
		{
		gkrellmd_plugin_log(
				_("\tOoops! plugin returned NULL, not installng\n"), NULL);
		g_module_close(module);
		return NULL;
		}

	gkrellmd_plugin_log(_("\tInstalled OK\n"), NULL);

	if (!m->privat)		/* may get set in gkrellm_config_getline() */
		{
		m->privat = g_new0(GkrellmdMonitorPrivate, 1);
		m->privat->config_list = gkrellmd_plugin_config_list;
		}

	m->privat->handle = module;
	m->privat->path = plugin_name;
	if (!m->name)
		m->name = g_path_get_basename(m->privat->path);

	return m;
	}


static gchar *
gkrellmd_string_suffix(gchar *string, gchar *suffix)
	{
	gchar	*dot;

	if (string == NULL || suffix == NULL)
		return NULL;
	dot = strrchr(string, '.');
	if (dot && !strcmp(dot + 1, suffix))
		return dot + 1;
	return NULL;
	}

static gboolean
gkrellmd_plugin_enabled(gchar *name)
	{
	GList	*list;
	gchar	*check, *s;
	gint	len;

	for (list = gkrellmd_plugin_enable_list; list; list = list->next)
		{
		check = (gchar *) list->data;
		s = strstr(name, check);
		len = strlen(check);
		if (   s
			&& s == name
			&& (*(name + len) == '\0' || !strcmp(name + len, ".so"))
		   )
			return TRUE;
		}
	return FALSE;
	}

static void
gkrellmd_plugin_scan(gchar *path)
	{
    GDir			*dir;
    gchar			*name, *filename;
	GList			*list;
	GkrellmdMonitor	*m = NULL;
	gchar			*s;
	gboolean		exists;
	
	/*if (path != NULL)
		g_print("Searching for plugins in '%s'\n", path);*/
	
	if (!path || !*path || (dir = g_dir_open(path, 0, NULL)) == NULL)
		return;
	while ((name = (gchar *) g_dir_read_name(dir)) != NULL)
		{
		if (!gkrellmd_string_suffix(name, G_MODULE_SUFFIX))
			continue;

		/* If there's a libtool .la archive, won't want to load this .so
		*/
		if (   !gkrellmd_string_suffix(name, "la")
			&& (s = strrchr(name, '.')) != NULL
		   )
			{
			s = g_strndup(name, s - name);
			filename = g_strconcat(path, G_DIR_SEPARATOR_S, s, ".la", NULL);
			exists = g_file_test(filename, G_FILE_TEST_EXISTS);
			g_free(s);
			g_free(filename);
			if (exists)
				continue;
			}
		if (_GK.list_plugins)
			{
			printf("%s    (%s)\n", name, path);
			continue;
			}
		for (list = plugin_list; list; list = list->next)
			{
			m = (GkrellmdMonitor *) list->data;
			s = g_path_get_basename(m->privat->path);
			exists = !strcmp(s, name);
			g_free(s);
			if (exists)
				break;
			m = NULL;
			}
		s = g_strconcat(path, G_DIR_SEPARATOR_S, name, NULL);
		if (m)
			{
			gkrellmd_plugin_log(_("Ignoring duplicate plugin "), s, "\n", NULL);
			g_free(s);
			continue;
			}
		if (!gkrellmd_plugin_enabled(name))
			{
			gkrellmd_plugin_log(s, "\n",
					"\tNot enabled in gkrellmd.conf - skipping\n", NULL);
			continue;
			}
		m = gkrellmd_plugin_install(s);
		if (m)	/* s is saved for use */
			plugin_list = g_list_append(plugin_list, m);
		else
			g_free(s);
		}
	g_dir_close(dir);
	}

GList *
gkrellmd_plugins_load(void)
	{
	GkrellmdMonitor	*m;
	gchar			*path;

	if (_GK.command_line_plugin)
		{
		if (   *_GK.command_line_plugin != '.'
			&& !strchr(_GK.command_line_plugin, G_DIR_SEPARATOR)
		   )
			path = g_strconcat(".", G_DIR_SEPARATOR_S,
						_GK.command_line_plugin, NULL);
		else
			path = g_strdup(_GK.command_line_plugin);
		gkrellmd_plugin_log(_("*** Command line plugin:\n"), NULL);
		if ((m = gkrellmd_plugin_install(path)) == NULL)
			g_free(path);
		else
			plugin_list = g_list_append(plugin_list, m);
		gkrellmd_plugin_log("\n", NULL);
		}

	path = g_build_filename(_GK.homedir, GKRELLMD_PLUGINS_DIR, NULL);
	gkrellmd_plugin_scan(path);
	g_free(path);

#if defined(WIN32)
	path = NULL;
#if GLIB_CHECK_VERSION(2,16,0)
	gchar *install_path;
	install_path = g_win32_get_package_installation_directory_of_module(NULL);
	if (install_path != NULL)
		{
		path = g_build_filename(install_path, "lib", "gkrellm2", "plugins-gkrellmd", NULL);
		g_free(install_path);
		}
#else
	// deprecated since glib 2.16.0
	path = g_win32_get_package_installation_subdirectory(NULL, NULL, "lib/gkrellm2/plugins-gkrellmd");
#endif
	if (path)
		{
		gkrellmd_plugin_scan(path);
		g_free(path);
		}
#endif

#if defined(GKRELLMD_LOCAL_PLUGINS_DIR)
	gkrellmd_plugin_scan(GKRELLMD_LOCAL_PLUGINS_DIR);
#endif

#if defined(GKRELLMD_SYSTEM_PLUGINS_DIR)
	gkrellmd_plugin_scan(GKRELLMD_SYSTEM_PLUGINS_DIR);
#endif

	return plugin_list;
	}
