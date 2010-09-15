/* GKrellM
|  Copyright (C) 1999-2010 Bill Wilson
|
|  Author:  Stefan Gehn    stefan+gkrellm@srcbox.net
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

/*
Wanted logic:

- g_print for user-visible messages, --version and --help fall into this category.
  g_print usage should be kept at a minimum because
  gkrellm is a gui-app, while gkrellmd is a daemon. Neither of them is suited
  for terminal-interaction.

- gkrellm_debug(DEBUG_FOO, "msg"); for all debug messages.

- g_warning("msg") for all failed function calls etc.

Output should go to:
  - g_print        : gui-window or stdout where applicable
  - gkrellm_debug  : gui-window or logfile if set
  - gkrellm_warning: gui-window or logfile if set
  */


#include "log.h"
#include "log-private.h"

#include <stdio.h>

// Include gkrellm headers to access _GK struct inside gkrellm_debug()
#if defined(GKRELLM_SERVER)
	#include "../server/gkrellmd.h"
	#include "../server/gkrellmd-private.h"
#else
	#include "../src/gkrellm.h"
	#include "../src/gkrellm-private.h"
#endif

typedef struct _GkrellmLogFacility
{
	GkrellmLogFunc log;
	GkrellmLogCleanupFunc cleanup;
} GkrellmLogFacility;

static GPtrArray *s_log_facility_ptr_array = NULL;


// ----------------------------------------------------------------------------
// Logging into a logfile

/**
 * Handle to a logfile.
 * Set by gkrellm_log_set_filename() and used by gkrellm_log_to_file()
 **/
static FILE *s_gkrellm_logfile = NULL;

static gboolean
gkrellm_log_file_cleanup()
	{
	if (s_gkrellm_logfile)
		fclose(s_gkrellm_logfile);
	s_gkrellm_logfile = NULL;
	return TRUE;
	}

static void
gkrellm_log_file_log(GLogLevelFlags log_level, const gchar *message)
	{
	time_t raw_time;
	char *local_time_str;

	if (!s_gkrellm_logfile)
		return;

	// Prepend log message with current date/time
	time(&raw_time);
	local_time_str = ctime(&raw_time);
	local_time_str[24] = ' '; // overwrite newline with space
	fputs(local_time_str, s_gkrellm_logfile);

	fputs(message, s_gkrellm_logfile);
	fflush(s_gkrellm_logfile);
	}

void
gkrellm_log_set_filename(const gchar* filename)
	{
	// Remove from logging chain if we already had been registered before
	// This also cleans up an open logfile.
	gkrellm_log_unregister(gkrellm_log_file_log);

	if (filename && filename[0] != '\0')
		{
		// Open the file to log into
		s_gkrellm_logfile = g_fopen(filename, "at");
		// Add our callbacks to logging chain
		if (s_gkrellm_logfile)
			{
			gkrellm_log_register(gkrellm_log_file_log, NULL,
				gkrellm_log_file_cleanup);
			}
		}
	}


// ----------------------------------------------------------------------------

//! Logs onto stdout/stderr
static void
gkrellm_log_to_terminal(GLogLevelFlags log_level, const gchar *message)
	{
	// warning, error or critical go to stderr
	if (log_level & G_LOG_LEVEL_WARNING
		|| log_level & G_LOG_LEVEL_CRITICAL
		|| log_level & G_LOG_LEVEL_ERROR)
		{
		fputs(message, stderr);
		return;
		}
#if defined(WIN32)
	// debug on windows gets special treatment
	if (log_level & G_LOG_LEVEL_DEBUG)
		OutputDebugStringA(message);
#endif
	// Everything also ends up on stdout
	// (may be invisible on most desktop-systems, especially on windows!)
	fputs(message, stdout);
	}


// ----------------------------------------------------------------------------

//! Handler that receives all the log-messages first
static void
gkrellm_log_handler(const gchar *log_domain, GLogLevelFlags log_level,
	const gchar *message, gpointer user_data)
	{
	gchar *localized_message;
	gint i;
	GkrellmLogFacility *f;

	localized_message = g_locale_from_utf8(message, -1, NULL, NULL, NULL);
	if (localized_message == NULL)
		{
		for (i = 0; i < s_log_facility_ptr_array->len; i++)
			{
			f = (g_ptr_array_index(s_log_facility_ptr_array, i));
			f->log(log_level, message);
			}
		}
	else
		{
		for (i = 0; i < s_log_facility_ptr_array->len; i++)
			{
			f = (g_ptr_array_index(s_log_facility_ptr_array, i));
			f->log(log_level, localized_message);
			}

		g_free(localized_message);
		}
	}


// ----------------------------------------------------------------------------
// Non-Static functions that can be used in gkrellm

void
gkrellm_log_init()
	{
	if (s_log_facility_ptr_array)
		return; // already initialized
	s_log_facility_ptr_array = g_ptr_array_new();
	g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
		| G_LOG_FLAG_RECURSION, gkrellm_log_handler, NULL);
	gkrellm_log_register(gkrellm_log_to_terminal, NULL, NULL);
	}

void
gkrellm_log_cleanup()
	{
	gint i;
	GkrellmLogFacility *f;

	if (!s_log_facility_ptr_array)
		return; // gkrellm_log_init() not called yet

	// Call cleanup on all log-facilities and free our internal struct
	for (i = 0; i < s_log_facility_ptr_array->len; i++)
		{
		f = (g_ptr_array_index(s_log_facility_ptr_array, i));
		if (f->cleanup != NULL)
			f->cleanup();
		g_free(f);
		}
	g_ptr_array_free(s_log_facility_ptr_array, TRUE);
	s_log_facility_ptr_array = NULL;
	}

gboolean
gkrellm_log_register(
	GkrellmLogFunc log,
	GkrellmLogInitFunc init,
	GkrellmLogCleanupFunc cleanup)
	{
	GkrellmLogFacility *f;
	gint i;

	if (!s_log_facility_ptr_array)
		return FALSE; // gkrellm_log_init() not called yet

	// Check if log callback is already regisrered
	for (i = 0; i < s_log_facility_ptr_array->len; i++)
		{
		f = (g_ptr_array_index(s_log_facility_ptr_array, i));
		if (f->log == log)
			return TRUE;
		}

	if (init != NULL && init() == FALSE)
		return FALSE;

	// remember logging function and cleanup function in a struct
	f = g_new0(GkrellmLogFacility, 1);
	f->log = log;
	f->cleanup = cleanup;

	// add struct to list of log facilities
	g_ptr_array_add(s_log_facility_ptr_array, (gpointer)f);
	return TRUE;
	}

gboolean
gkrellm_log_unregister(GkrellmLogFunc log)
	{
	gint i;
	GkrellmLogFacility *f;

	if (!s_log_facility_ptr_array)
		return FALSE; // gkrellm_log_init() not called yet

	for (i = 0; i < s_log_facility_ptr_array->len; i++)
		{
		f = (g_ptr_array_index(s_log_facility_ptr_array, i));
		if (f->log == log)
			{
			if (f->cleanup != NULL)
				f->cleanup();
			g_ptr_array_remove_index(s_log_facility_ptr_array, i);
			g_free(f);
			return TRUE;
			}
		}
	return FALSE;
	}


// ----------------------------------------------------------------------------
// Public functions that can be used in gkrellm and plugins

void
gkrellm_debug(guint debug_level, const gchar *format, ...)
	{
	if (_GK.debug_level & debug_level)
		{
		va_list varargs;
		va_start(varargs, format);

		g_logv(NULL, G_LOG_LEVEL_DEBUG, format, varargs);

		va_end(varargs);
		}
	}

void
gkrellm_debugv(guint debug_level, const gchar *format, va_list arg)
	{
	if (_GK.debug_level & debug_level)
		{
		g_logv(NULL, G_LOG_LEVEL_DEBUG, format, arg);
		}
	}
