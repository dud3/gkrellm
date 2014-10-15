/* GKrellM
|  Copyright (C) 1999-2014 Bill Wilson
|                2007-2014 Stefan Gehn
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
*/

#ifndef WIN32_PLUGIN_H
#define WIN32_PLUGIN_H

#include "gkrellmd.h"

typedef struct
{
    /* gkrellmd serve data functions used by builtins and plugins.
    */
    void		(*gkrellmd_plugin_serve_setup)(GkrellmdMonitor *mon, gchar *name, gchar *line);
    void		(*gkrellmd_need_serve)(GkrellmdMonitor *mon);
    void		(*gkrellmd_set_serve_name)(GkrellmdMonitor *mon, const gchar *name);
    void		(*gkrellmd_serve_data)(GkrellmdMonitor *mon, gchar *line);
    void		(*gkrellmd_add_serveflag_done)(gboolean *);
    gboolean	(*gkrellmd_check_client_version)(GkrellmdMonitor *mon, gint major, gint minor, gint rev);
    
    const gchar	*(*gkrellmd_config_getline)(GkrellmdMonitor *mon);
    
    void		(*gkrellmd_client_input_connect)(GkrellmdMonitor *mon, void (*func)(GkrellmdClient *, gchar *));
    
    
    /* Small set of useful functions duplicated from src/utils.c.
    |  These really should just be in the gkrellm_ namespace for sysdep code
    |  common to gkrellm and gkrellmd, but for convenience, offer them in
    |  both gkrellm_ and gkrellmd_ namespaces.
    */
    void        (*gkrellmd_free_glist_and_data)(GList **list_head);
    gboolean    (*gkrellmd_getline_from_gstring)(GString **, gchar *, gint);
    gchar *     (*gkrellmd_dup_token)(gchar **string, gchar *delimeters);
    gboolean    (*gkrellmd_dup_string)(gchar **dst, gchar *src);
    
    void		(*gkrellm_free_glist_and_data)(GList **list_head);
    gboolean	(*gkrellm_getline_from_gstring)(GString **, gchar *, gint);
    gchar*      (*gkrellm_dup_token)(gchar **string, gchar *delimeters);
    gboolean    (*gkrellm_dup_string)(gchar **dst, gchar *src);
    
    
    /* Plugins should use above data serve functions instead of this.
    */
    gint        (*gkrellmd_send_to_client)(GkrellmdClient *client, gchar *buf);
    
    
    /* Misc
    */
    void        (*gkrellmd_add_mailbox)(gchar *);
    GkrellmdTicks * (*gkrellmd_ticks)(void);
    gint           (*gkrellmd_get_timer_ticks)(void);

    //---------------------------------------------------------------------------
    // new since 2.3.2

    // gkrellm_debug is not called from libgkrellm, only gkrellm_debugv
    void (*gkrellm_debugv)(guint debug_level, const gchar *format, va_list arg);

} win32_plugin_callbacks;


/// part of win32-plugin.c
/// 
extern win32_plugin_callbacks gkrellmd_callbacks;


/// \brief initializes \p gkrellmd_callbacks
/// Has to be called at gkrellmd startup before loading plugins.
/// \note only needed on win32
void win32_init_callbacks(void);

#endif // WIN32_PLUGIN_H
