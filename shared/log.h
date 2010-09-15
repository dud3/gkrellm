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

#ifndef GK_LOG_H
#define GK_LOG_H

#include <glib.h>

/**
 * @brief Prints our and/or logs a debug message.
 *  
 * If a logfile was set @see gkrellm_log_set_filename() the message will
 * be logged into the logfile as well.
 **/    
void gkrellm_debug(guint debug_level, const gchar *format, ...);
void gkrellm_debugv(guint debug_level, const gchar *format, va_list arg);

#endif //GK_LOG_H
