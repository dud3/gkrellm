 /* GKrellM Windows Portion
|  Copyright (C) 2002 Bill Nalen
|                2007-2009 Stefan Gehn
|
|  Authors:  Bill Nalen     bill@nalens.com
|            Stefan Gehn    stefan+gkrellm@srcbox.net
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

#include "win32-plugin.h"

__declspec(dllexport) win32_plugin_callbacks* cb = NULL;


/* gkrellmd serve data functions used by builtins and plugins.
*/
void		gkrellmd_plugin_serve_setup(GkrellmdMonitor *mon, gchar *name, gchar *line)
    {cb->gkrellmd_plugin_serve_setup(mon,name,line);}
void		gkrellmd_need_serve(GkrellmdMonitor *mon)
    {cb->gkrellmd_need_serve(mon);}
void		gkrellmd_set_serve_name(GkrellmdMonitor *mon, const gchar *name)
    {cb->gkrellmd_set_serve_name(mon,name);}
void		gkrellmd_serve_data(GkrellmdMonitor *mon, gchar *line)
    {cb->gkrellmd_serve_data(mon,line);}
void		gkrellmd_add_serveflag_done(gboolean *done)
    {cb->gkrellmd_add_serveflag_done(done);}
gboolean	gkrellmd_check_client_version(GkrellmdMonitor *mon, gint major, gint minor, gint rev)
    {return cb->gkrellmd_check_client_version(mon,major,minor,rev);}

const gchar	*gkrellmd_config_getline(GkrellmdMonitor *mon)
    {return gkrellmd_config_getline(mon);}

void		gkrellmd_client_input_connect(GkrellmdMonitor *mon,
						void (*func)(GkrellmdClient *, gchar *))
    {cb->gkrellmd_client_input_connect(mon, func);}


  /* Small set of useful functions duplicated from src/utils.c.
  |  These really should just be in the gkrellm_ namespace for sysdep code
  |  common to gkrellm and gkrellmd, but for convenience, offer them in
  |  both gkrellm_ and gkrellmd_ namespaces.
  */
void		gkrellmd_free_glist_and_data(GList **list_head)
    {cb->gkrellmd_free_glist_and_data(list_head);}
gboolean	gkrellmd_getline_from_gstring(GString **str, gchar *ch, gint l)
    {return cb->gkrellmd_getline_from_gstring(str, ch, l);}
gchar		*gkrellmd_dup_token(gchar **str, gchar *delim)
    {return cb->gkrellmd_dup_token(str, delim);}
gboolean	gkrellmd_dup_string(gchar **dst, gchar *src)
    {return cb->gkrellmd_dup_string(dst, src);}

void		gkrellm_free_glist_and_data(GList **list_head)
    {cb->gkrellm_free_glist_and_data(list_head);}
gboolean	gkrellm_getline_from_gstring(GString **str, gchar *ch, gint l)
    {return cb->gkrellm_getline_from_gstring(str, ch, l);}
gchar		*gkrellm_dup_token(gchar **str, gchar *delim)
    {return cb->gkrellm_dup_token(str, delim);}
gboolean	gkrellm_dup_string(gchar **dst, gchar *src)
    {return cb->gkrellm_dup_string(dst, src);}


  /* Plugins should use above data serve functions instead of this.
  */
gint		gkrellmd_send_to_client(GkrellmdClient *client, gchar *buf)
    {return cb->gkrellmd_send_to_client(client, buf);}


  /* Misc
  */
void		gkrellmd_add_mailbox(gchar *g)
    {cb->gkrellmd_add_mailbox(g);}
GkrellmdTicks * gkrellmd_ticks(void)
    {return cb->gkrellmd_ticks();}
gint           gkrellmd_get_timer_ticks(void)
    {return cb->gkrellmd_get_timer_ticks();}

//---------------------------------------------------------------------------
// new since 2.3.2

void          gkrellm_debug(guint debug_level, const gchar *format, ...)
	{
  va_list arg;
  va_start(arg, format);
  cb->gkrellm_debugv(debug_level, format, arg);
  va_end(arg);
	}

void          gkrellm_debugv(guint debug_level, const gchar *format, va_list arg)
	{
  cb->gkrellm_debugv(debug_level, format, arg);
	}
