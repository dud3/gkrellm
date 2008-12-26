 /* GKrellM Windows Portion
|  Copyright (C) 2006-2007 Stefan Gehn
|
|  Author:  Stefan Gehn   metz AT gehn DOT net
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

win32_plugin_callbacks gkrellmd_callbacks;

void win32_init_callbacks(void)
{
    /* gkrellmd serve data functions used by builtins and plugins.
    */
    gkrellmd_callbacks.gkrellmd_plugin_serve_setup = gkrellmd_plugin_serve_setup;
    gkrellmd_callbacks.gkrellmd_need_serve = gkrellmd_need_serve;
    gkrellmd_callbacks.gkrellmd_set_serve_name = gkrellmd_set_serve_name;
    gkrellmd_callbacks.gkrellmd_serve_data = gkrellmd_serve_data;
    gkrellmd_callbacks.gkrellmd_add_serveflag_done = gkrellmd_add_serveflag_done;
    gkrellmd_callbacks.gkrellmd_check_client_version = gkrellmd_check_client_version;

    gkrellmd_callbacks.gkrellmd_config_getline = gkrellmd_config_getline;

    gkrellmd_callbacks.gkrellmd_client_input_connect = gkrellmd_client_input_connect;


    /* Small set of useful functions duplicated from src/utils.c.
    |  These really should just be in the gkrellm_ namespace for sysdep code
    |  common to gkrellm and gkrellmd, but for convenience, offer them in
    |  both gkrellm_ and gkrellmd_ namespaces.
    */
    gkrellmd_callbacks.gkrellmd_free_glist_and_data = gkrellmd_free_glist_and_data;
    gkrellmd_callbacks.gkrellmd_getline_from_gstring = gkrellmd_getline_from_gstring;
    gkrellmd_callbacks.gkrellmd_dup_token = gkrellmd_dup_token;
    gkrellmd_callbacks.gkrellmd_dup_string = gkrellmd_dup_string;

    gkrellmd_callbacks.gkrellm_free_glist_and_data = gkrellm_free_glist_and_data;
    gkrellmd_callbacks.gkrellm_getline_from_gstring = gkrellm_getline_from_gstring;
    gkrellmd_callbacks.gkrellm_dup_token = gkrellm_dup_token;
    gkrellmd_callbacks.gkrellm_dup_string = gkrellm_dup_string;


    /* Plugins should use above data serve functions instead of this.
    */
    gkrellmd_callbacks.gkrellmd_send_to_client = gkrellmd_send_to_client;


    /* Misc
    */
    gkrellmd_callbacks.gkrellmd_add_mailbox = gkrellmd_add_mailbox;
    gkrellmd_callbacks.gkrellmd_ticks = gkrellmd_ticks;
    gkrellmd_callbacks.gkrellmd_get_timer_ticks = gkrellmd_get_timer_ticks;

    //---------------------------------------------------------------------------
    // new since 2.3.2

    gkrellmd_callbacks.gkrellm_debugv = gkrellm_debugv;
}

