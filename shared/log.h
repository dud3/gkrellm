/* GKrellM
*  Copyright (C) 1999-2008 Bill Wilson
*
*  @author Bill Wilson  <billw@gkrellm.net>
*
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

#endif //GK_LOG_H
