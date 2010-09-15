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

#include "gkrellm.h"
#include "gkrellm-private.h"

#include <math.h>

static GList	*gkrellm_alert_list,
				*alert_plugin_list;


static void
run_command(GkrellmAlert *alert, gchar *command)
	{
	gchar	cmd[1024];

	if (!command || !*command)
		return;
	if (alert->cb_command_process)
		(*alert->cb_command_process)(alert, command, cmd, sizeof(cmd),
				alert->cb_command_process_data);
	else
		snprintf(cmd, sizeof(cmd), "%s", command);
	g_spawn_command_line_async(cmd, NULL);
	}

void
gkrellm_render_default_alert_decal(GkrellmAlert *alert)
	{
	GkrellmPiximage	*im;
	GkrellmAlertdecal		*ad;

	if (!alert)
		return;
	ad = &alert->ad;
	if (ad->w <= 0 || ad->h <= 0)
		return;
	if (alert->high.alarm_on || alert->low.alarm_on)
		gkrellm_get_decal_alarm_piximage(&im, &ad->nframes);
	else if (alert->high.warn_on || alert->low.warn_on)
		gkrellm_get_decal_warn_piximage(&im, &ad->nframes);
	else
		return;
	gkrellm_scale_piximage_to_pixmap(im, &ad->pixmap, &ad->mask, ad->w,
				ad->h * ad->nframes);
	}

gboolean
gkrellm_alert_decal_visible(GkrellmAlert *alert)
	{
	if (alert && alert->ad.decal)
		return TRUE;
	return FALSE;
	}

static gboolean
create_alert_objects(GkrellmAlert *alert)
	{
	GkrellmAlertdecal		*ad;
//	GkrellmKrell			*k;

	/* Whichever monitor created the alert that is being triggered:
	|  1) has set a panel pointer in the create and is done, so I will make
	|     here a default panel sized alert decal for him, or
	|  2) has not set a panel ptr, but will do so in the callback and then
	|     let me make the default alert, or
	|  3) has done one of the 2 above panel possibilities, and will also
	|     render a custom size/position for the alert decal and set the pixmap.
	|     For this case, the alert decal may be a render of the default decal
	|     image or may be a custom image from the monitor.  The work needed is
	|     setting ad x,y,w,h and nframes and rendering to ad->pixmap.
	*/
	if (alert->cb_trigger)
		(*alert->cb_trigger)(alert, alert->cb_trigger_data);
	if (!alert->panel)
		return FALSE;		/* Monitor may need trigger deferred */
	ad = &alert->ad;
	if (!alert->ad.pixmap /* && style is decal alert */)
		{
		ad->x = 0;
		ad->y = 0;
		ad->w = alert->panel->w;
		ad->h = alert->panel->h;
		gkrellm_render_default_alert_decal(alert);
		}
	/* Don't let create_decal append the decal, I want to insert it first so
	|  it will appear under all other panel decals.
	*/
	ad->decal = gkrellm_create_decal_pixmap(NULL, ad->pixmap, ad->mask,
				ad->nframes, NULL, ad->x, ad->y);
	gkrellm_insert_decal(alert->panel, ad->decal, FALSE /* prepend */);

#if 0
	k = gkrellm_create_krell(NULL,
			gkrellm_krell_mini_piximage(), gkrellm_krell_mini_style());
	k->y0 = 5;
	k->full_scale = 100;

	alert->ak.krell = k;
	gkrellm_insert_krell(alert->panel, k, TRUE);
#endif
	return TRUE;
	}

static void
destroy_alert_objects(GkrellmAlert *alert)
	{
	gkrellm_destroy_decal(alert->ad.decal);
	if (alert->ad.pixmap)
		g_object_unref(G_OBJECT(alert->ad.pixmap));
	alert->ad.pixmap = NULL;
	alert->ad.mask = NULL;
	alert->ad.decal = NULL;
	gkrellm_destroy_krell(alert->ak.krell);
	alert->ak.krell = NULL;
	}

static void
plugin_warn(GkrellmAlert *alert, gboolean state)
	{
	GList					*list;
	GkrellmAlertPlugin		*gap;
	GkrellmAlertPluginLink	*apl;

	for (list = alert->plugin_list; list; list = list->next)
		{
		apl = (GkrellmAlertPluginLink *) list->data;
		gap = apl->alert_plugin;
		if (MONITOR_ENABLED(gap->mon) && gap->warn_func)
			(*gap->warn_func)(alert, apl->data, state);
		}
	}

static void
plugin_alarm(GkrellmAlert *alert, gboolean state)
	{
	GList					*list;
	GkrellmAlertPlugin		*gap;
	GkrellmAlertPluginLink	*apl;

	for (list = alert->plugin_list; list; list = list->next)
		{
		apl = (GkrellmAlertPluginLink *) list->data;
		gap = apl->alert_plugin;
		if (MONITOR_ENABLED(gap->mon) && gap->alarm_func)
			(*gap->alarm_func)(alert, apl->data, state);
		}
	}

static void
trigger_warn(GkrellmAlert *alert, GkrellmTrigger *trigger)
	{
	if (!trigger->warn_on)
		{
		trigger->warn_on = TRUE;
		create_alert_objects(alert);
		gkrellm_alert_list = g_list_append(gkrellm_alert_list, alert);
		if (!alert->suppress_command)
			{
			run_command(alert, alert->warn_command);
			plugin_warn(alert, ON);
			}
		alert->suppress_command = FALSE;
		alert->warn_repeat = alert->warn_repeat_set;
		}
	}

static void
stop_warn(GkrellmAlert *alert, GkrellmTrigger *trigger)
	{
	if (trigger->warn_on)
		{
		plugin_warn(alert, OFF);
		trigger->warn_on = FALSE;
		destroy_alert_objects(alert);
		gkrellm_alert_list = g_list_remove(gkrellm_alert_list, alert);
		alert->warn_repeat = 0;
		alert->suppress_command = FALSE;
		}
	}

static void
trigger_alarm(GkrellmAlert *alert, GkrellmTrigger *trigger)
	{
	if (!trigger->alarm_on)
		{
		trigger->alarm_on = TRUE;
		trigger->warn_delay = 0;
		create_alert_objects(alert);
		gkrellm_alert_list = g_list_append(gkrellm_alert_list, alert);
		if (!alert->suppress_command)
			{
			run_command(alert, alert->alarm_command);
			plugin_alarm(alert, ON);
			}
		alert->suppress_command = FALSE;
		alert->alarm_repeat = alert->alarm_repeat_set;
		}
	}

static void
stop_alarm(GkrellmAlert *alert, GkrellmTrigger *trigger)
	{
	if (trigger->alarm_on)
		{
		plugin_alarm(alert, OFF);
		trigger->alarm_on = FALSE;
		destroy_alert_objects(alert);
		gkrellm_alert_list = g_list_remove(gkrellm_alert_list, alert);
		alert->alarm_repeat = 0;;
		alert->suppress_command = FALSE;
		}
	}

void
gkrellm_freeze_alert(GkrellmAlert *alert)
	{
	if (alert)
		alert->freeze = TRUE;
	}

void
gkrellm_thaw_alert(GkrellmAlert *alert)
	{
	if (alert)
		alert->freeze = FALSE;
	}

static gboolean
check_high_alarm_limit(GkrellmAlert *alert, gfloat value)
	{
	GkrellmTrigger	*trigger = &alert->high;

	if (value < trigger->alarm_limit)
		{
		trigger->alarm_delay = alert->delay;
		return FALSE;
		}
	if (trigger->alarm_delay == 0)
		{
		if (trigger->warn_delay > 0)
			trigger->warn_delay -= 1;
		return TRUE;
		}
	trigger->alarm_delay -= 1;
	return FALSE;
	}

static gboolean
check_high_warn_limit(GkrellmAlert *alert, gfloat value)
	{
	GkrellmTrigger	*trigger = &alert->high;

	if (value < trigger->warn_limit)
		{
		trigger->warn_delay = alert->delay;
		return FALSE;
		}
	if (trigger->warn_delay == 0)
		return TRUE;
	trigger->warn_delay -= 1;
	return FALSE;
	}

static gboolean
check_low_warn_limit(GkrellmAlert *alert, gfloat value)
	{
	GkrellmTrigger	*trigger = &alert->low;

	if (value > trigger->warn_limit)
		{
		trigger->warn_delay = alert->delay;
		return FALSE;
		}
	if (trigger->warn_delay == 0)
		return TRUE;
	trigger->warn_delay -= 1;
	return FALSE;
	}

static gboolean
check_low_alarm_limit(GkrellmAlert *alert, gfloat value)
	{
	GkrellmTrigger	*trigger = &alert->low;

	if (value > trigger->alarm_limit)
		{
		trigger->alarm_delay = alert->delay;
		return FALSE;
		}
	if (trigger->alarm_delay == 0)
		{
		if (trigger->warn_delay > 0)
			trigger->warn_delay -= 1;
		return TRUE;
		}
	trigger->alarm_delay -= 1;
	return FALSE;
	}

void
gkrellm_check_alert(GkrellmAlert *alert, gfloat value)
	{
	GList					*plist;
	GkrellmAlertPlugin		*gap;
	GkrellmAlertPluginLink	*apl;

	if (!alert || !alert->activated || alert->freeze || !_GK.initialized)
		return;

	if (alert->delay == 0 || GK.second_tick)
		{
		if (alert->check_low)
			{
			if (check_low_alarm_limit(alert, value))
				{				
				stop_alarm(alert, &alert->high);
				stop_warn(alert, &alert->high);
				stop_warn(alert, &alert->low);
				trigger_alarm(alert, &alert->low);
				}
			else
				{
				stop_alarm(alert, &alert->low);
				if (check_low_warn_limit(alert, value))
					{
					stop_alarm(alert, &alert->high);
					stop_warn(alert, &alert->high);
					trigger_warn(alert, &alert->low);
					}
				else
					stop_warn(alert, &alert->low);
				}
			}
		if (alert->check_high)
			{
			if (check_high_alarm_limit(alert, value))
				{
				stop_warn(alert, &alert->high);
				stop_warn(alert, &alert->low);
				stop_alarm(alert, &alert->low);
				trigger_alarm(alert, &alert->high);
				}
			else
				{
				stop_alarm(alert, &alert->high);
				if (check_high_warn_limit(alert, value))
					{
					stop_warn(alert, &alert->low);
					stop_alarm(alert, &alert->low);
					trigger_warn(alert, &alert->high);
					}
				else
					stop_warn(alert, &alert->high);
				}
			}
		}

	for (plist = alert->plugin_list; plist; plist = plist->next)
		{
		apl = (GkrellmAlertPluginLink *) plist->data;
		gap = apl->alert_plugin;
		if (MONITOR_ENABLED(gap->mon) && gap->check_func)
			(*gap->check_func)(alert, apl->data, value);
		}
	}

void
gkrellm_alert_trigger_connect(GkrellmAlert *alert, void (*func)(),
			gpointer data)
	{
	if (!alert)
		return;
	alert->cb_trigger = func;
	alert->cb_trigger_data = data;
	}

void
gkrellm_alert_stop_connect(GkrellmAlert *alert, void (*func)(), gpointer data)
	{
	if (!alert)
		return;
	alert->cb_stop = func;
	alert->cb_stop_data = data;
	}

static void
destroy_alert(GkrellmAlert *alert)
	{
	GList					*plist, *list;
	GkrellmAlertPlugin		*gap;
	GkrellmAlertPluginLink	*apl;
	gpointer				data;

	if (!alert)
		return;
	if (g_list_find(gkrellm_alert_list, alert))
		{
		gkrellm_alert_list = g_list_remove(gkrellm_alert_list, alert);
		destroy_alert_objects(alert);
		}

	for (plist = alert_plugin_list; plist; plist = plist->next)
		{
		data = NULL;
		gap = (GkrellmAlertPlugin *) plist->data;
		for (list = alert->plugin_list; list; list = list->next)
			{
			apl = (GkrellmAlertPluginLink *) list->data;
			if (apl->alert_plugin == gap)
				{
				data = apl->data;
				break;
				}
			}
		if (MONITOR_ENABLED(gap->mon) && gap->destroy_func)
			(*gap->destroy_func)(alert, data);
		}
	gkrellm_free_glist_and_data(&alert->plugin_list);	/* should all be detached */
	g_free(alert->name);
	g_free(alert->unit_string);
	g_free(alert->alarm_command);
	g_free(alert->warn_command);
	g_free(alert->id_string);
	g_free(alert);
	}

void
gkrellm_alert_destroy(GkrellmAlert **ap)
	{
	if (!ap || !*ap)
		return;
	gkrellm_alert_window_destroy(ap);
	destroy_alert(*ap);
	*ap = NULL;
	}

GkrellmAlert *
gkrellm_alert_create(GkrellmPanel *p, gchar *name, gchar *unit_string,
		gboolean check_high, gboolean check_low, gboolean do_updates,
		gfloat max_high, gfloat min_low,
		gfloat step0, gfloat step1, gint digits)
	{
	GkrellmAlert	*alert;

	alert = g_new0(GkrellmAlert, 1);
	alert->panel = p;
	alert->name = g_strdup(name);
	alert->unit_string = g_strdup(unit_string);
	alert->check_high = check_high;
	alert->check_low = check_low;
	alert->do_panel_updates = do_updates;
	alert->max_high = max_high;
	alert->min_low = min_low;
	alert->step0 = step0;
	alert->step1 = step1;
	alert->digits = digits;

	alert->alarm_command = g_strdup("");
	alert->warn_command = g_strdup("");
	alert->do_alarm_command = alert->do_warn_command = TRUE;

	gkrellm_alert_set_triggers(alert, min_low, min_low, min_low, min_low);
	alert->activated = FALSE;
	alert->check_hardwired = FALSE;

	return alert;
	}

  /* Set alarm trigger values for hardwired alarms which have no config
  */
void
gkrellm_alert_set_triggers(GkrellmAlert *alert,
				gfloat high_alarm, gfloat high_warn,
				gfloat low_warn, gfloat low_alarm)
	{
	if (alert->check_high)
		{
		alert->high.alarm_limit = high_alarm;
		alert->high.warn_limit = high_warn;
		}
	if (alert->check_low)
		{
		alert->low.warn_limit = low_warn;
		alert->low.alarm_limit = low_alarm;
		}
	alert->activated = TRUE;
	alert->check_hardwired = TRUE;
	}


void
gkrellm_alert_commands_config(GkrellmAlert *alert,
						gboolean alarm, gboolean warn)
	{
	alert->do_alarm_command = alarm;
	alert->do_warn_command = warn;
	}

void
gkrellm_alert_set_delay(GkrellmAlert *alert, gint delay)
	{
	if (!alert)
		return;
	alert->delay = delay;
	alert->low.warn_delay = alert->low.alarm_delay = delay;
	alert->high.warn_delay = alert->high.alarm_delay = delay;
	}

void
gkrellm_alert_delay_config(GkrellmAlert *alert, gint step,
			gint high, gint low)
	{
	if (!alert || step < 1 || high < step)
		return;
	low = (low / step) * step;
	high = (high / step) * step;

	alert->delay_high = (gfloat) high;
	alert->delay_low = (gfloat) low;
	alert->delay_step = (gfloat) step;
	gkrellm_alert_set_delay(alert, low);
	}

void
gkrellm_alert_dup(GkrellmAlert **a_dst, GkrellmAlert *a_src)
	{
	GkrellmAlert	*alert;

	if (!a_dst || !a_src)
		return;
	if (*a_dst)
		(*a_dst)->plugin_list = NULL;	/* XXX fixme */
	gkrellm_alert_destroy(a_dst);
	alert = gkrellm_alert_create(a_src->panel, a_src->name,
				a_src->unit_string, a_src->check_high, a_src->check_low,
				a_src->do_panel_updates, a_src->max_high, a_src->min_low,
				a_src->step0, a_src->step1, a_src->digits);
	gkrellm_dup_string(&alert->alarm_command, a_src->alarm_command);
	gkrellm_dup_string(&alert->warn_command, a_src->warn_command);
	alert->warn_repeat_set = a_src->warn_repeat_set;
	alert->alarm_repeat_set = a_src->alarm_repeat_set;
	alert->low = a_src->low;
	alert->high = a_src->high;
	gkrellm_alert_set_delay(alert, a_src->delay);
	alert->plugin_list = a_src->plugin_list;	/* XXX fixme */

	alert->activated = a_src->activated;
	*a_dst = alert;
	}

static void
reset_alert(GkrellmAlert *alert, gboolean suppress)
	{
	GkrellmTrigger	*th = &alert->high,
					*tl = &alert->low;

	if (   suppress
		&& (   ((th->alarm_on || tl->alarm_on) && alert->alarm_repeat_set == 0)
			|| ((th->warn_on || tl->warn_on) && alert->warn_repeat_set == 0)
		   )
	   )
		alert->suppress_command = TRUE;
	th->alarm_on = FALSE;
	th->warn_on = FALSE;
	tl->alarm_on = FALSE;
	tl->warn_on = FALSE;
	alert->alarm_repeat = 0;
	alert->warn_repeat = 0;
	destroy_alert_objects(alert);
	}

void
gkrellm_reset_alert(GkrellmAlert *alert)
	{
	if (!alert)
		return;
	if (g_list_find(gkrellm_alert_list, alert))
		{
		reset_alert(alert, FALSE);
		gkrellm_alert_list = g_list_remove(gkrellm_alert_list, alert);
		}
	}

void
gkrellm_reset_alert_soft(GkrellmAlert *alert)
	{
	if (!alert)
		return;
	if (g_list_find(gkrellm_alert_list, alert))
		{
		reset_alert(alert, TRUE);
		gkrellm_alert_list = g_list_remove(gkrellm_alert_list, alert);
		}
	}

void
gkrellm_reset_panel_alerts(GkrellmPanel *p)
	{
	GList			*list;
	GkrellmAlert	*alert;
	gboolean		done = FALSE;

	if (!p)
		return;
	while (!done)
		{
		done = TRUE;	/* Assume won't find any */
		for (list = gkrellm_alert_list; list; list = list->next)
			{
			alert = (GkrellmAlert *) list->data;
			if (alert->panel != p)
				continue;
			done = FALSE;
			reset_alert(alert, FALSE);
			gkrellm_alert_list = g_list_remove(gkrellm_alert_list, alert);
			alert->panel = NULL;
			alert->cb_trigger = NULL;
			alert->cb_trigger_data = NULL;
			alert->cb_stop = NULL;
			alert->cb_stop_data = NULL;
			break;
			}
		}
	}

  /* At theme changes, turn all alerts off so there won't be any alert decals
  |  in monitor lists when they are destroyed.  The alerts should just get
  |  retriggered.  Surely an alert going off is a good time to change themes.
  */
void
gkrellm_alert_reset_all(void)
	{
	GList	*list;

	for (list = gkrellm_alert_list; list; list = list->next)
		reset_alert((GkrellmAlert *) list->data, TRUE);
	g_list_free(gkrellm_alert_list);
	gkrellm_alert_list = NULL;
	}

void
gkrellm_alert_update(void)
	{
	GList					*list, *plist;
	GkrellmAlert			*alert;
	GkrellmAlertdecal		*ad;
	GkrellmAlertkrell		*ak;
	GkrellmAlertPlugin		*gap;
	GkrellmAlertPluginLink	*apl;

	for (list = gkrellm_alert_list; list; list = list->next)
		{
		alert = (GkrellmAlert *) list->data;
		ad = &alert->ad;
		ak = &alert->ak;
		if (ak->krell)
			{
			ak->krell_position = (ak->krell_position + 2) % 100;
			gkrellm_update_krell(alert->panel, ak->krell, ak->krell_position);
			}
		if (ad->decal)
			{
			if (ad->frame <= 0)
				ad->dir = 1;
			else if (ad->frame >= ad->nframes - 1)
				ad->dir = 0;
			ad->frame += (ad->dir) ? 1 : -1;
			gkrellm_draw_decal_pixmap(alert->panel, ad->decal, ad->frame);
			}
		if (alert->do_panel_updates)
			gkrellm_draw_panel_layers(alert->panel);
		if (GK.second_tick)
			{
			if (alert->alarm_repeat > 0 && --alert->alarm_repeat == 0)
				{
				run_command(alert, alert->alarm_command);
				alert->alarm_repeat = alert->alarm_repeat_set;
				}
			if (alert->warn_repeat > 0 && --alert->warn_repeat == 0)
				{
				run_command(alert, alert->warn_command);
				alert->warn_repeat = alert->warn_repeat_set;
				}
			for (plist = alert->plugin_list; plist; plist = plist->next)
				{
				apl = (GkrellmAlertPluginLink *) plist->data;
				gap = apl->alert_plugin;
				if (MONITOR_ENABLED(gap->mon) && gap->update_func)
					(*gap->update_func)(alert, apl->data);
				}
			}
		}
	}

gboolean
gkrellm_alert_is_activated(GkrellmAlert *alert)
	{
	if (alert)
		return alert->activated;
	return FALSE;
	}

void
gkrellm_alert_get_alert_state(GkrellmAlert *alert, gboolean *alarm_state,
				gboolean *warn_state)
	{
	if (!alert)
		return;
	if (warn_state)
		*warn_state = (alert->high.warn_on || alert->low.warn_on);
	if (alarm_state)
		*alarm_state = (alert->high.alarm_on || alert->low.alarm_on);
	}


/* ------------------------------------------------------------ */
/* Plugin interface to alerts. */

GkrellmAlertPlugin *
gkrellm_alert_plugin_add(GkrellmMonitor *mon, gchar *name)
	{
	GkrellmAlertPlugin	*gap;

	if (!mon || !name || !*name)
		return NULL;
	gap = g_new0(GkrellmAlertPlugin, 1);
	gap->mon = mon;
	gap->name = g_strdup(name);
	alert_plugin_list = g_list_append(alert_plugin_list, gap);
	return gap;
	}

void
gkrellm_alert_plugin_alert_connect(GkrellmAlertPlugin *gap,
			void (*alarm_func)(), void (*warn_func)(),
			void (*update_func)(), void (*check_func)(),
			void (*destroy_func)())
	{
	gap->alarm_func = alarm_func;
	gap->warn_func = warn_func;
	gap->update_func = update_func;
	gap->check_func = check_func;
	gap->destroy_func = destroy_func;
	}

void
gkrellm_alert_plugin_config_connect(GkrellmAlertPlugin *gap, gchar *tab_name,
			void (*config_create_func)(), void (*config_apply_func),
			void (*config_save_func)(), void (*config_load_func)())
	{
	if (!gap)
		return;
	g_free(gap->tab_name);
	gap->tab_name = g_strdup(tab_name);
	gap->config_create_func = config_create_func;
	gap->config_apply_func = config_apply_func;
	gap->config_save_func = config_save_func;
	gap->config_load_func = config_load_func;
	}

  /* The id_string is so a plugin can get a unique config name for an alert.
  */
gchar *
gkrellm_alert_plugin_config_get_id_string(GkrellmAlert *alert)
	{
	if (alert)
		return alert->id_string;
	return NULL;
	}

void
gkrellm_alert_plugin_alert_attach(GkrellmAlertPlugin *gap,
			GkrellmAlert *alert, gpointer data)
	{
	GkrellmAlertPluginLink	*apl;

	if (!gap || !alert)
		return;
	apl = g_new0(GkrellmAlertPluginLink, 1);
	apl->alert_plugin = gap;
	apl->data = data;
	alert->plugin_list = g_list_append(alert->plugin_list, apl);
	}

void
gkrellm_alert_plugin_alert_detach(GkrellmAlertPlugin *gap, GkrellmAlert *alert)
	{
	GList					*list;
	GkrellmAlertPluginLink	*apl;

	if (!gap || !alert)
		return;
	for (list = alert->plugin_list; list; list = list->next)
		{
		apl = (GkrellmAlertPluginLink *) list->data;
		if (apl->alert_plugin == gap)
			{
			alert->plugin_list = g_list_remove(alert->plugin_list, apl);
			g_free(apl);
			break;
			}
		}
	}

gpointer
gkrellm_alert_plugin_get_data(GkrellmAlertPlugin *gap,GkrellmAlert *alert)
	{
	GList					*list;
	GkrellmAlertPluginLink	*apl;

	if (!gap || !alert)
		return NULL;
	for (list = alert->plugin_list; list; list = list->next)
		{
		apl = (GkrellmAlertPluginLink *) list->data;
		if (apl->alert_plugin == gap)
			return apl->data;
		}
	return NULL;
	}

void
gkrellm_alert_plugin_command_process(GkrellmAlert *alert, gchar *src,
			gchar *dst, gint dst_size)
	{
	if (alert && alert->cb_command_process)
		(*alert->cb_command_process)(alert, src, dst, dst_size,
				alert->cb_command_process_data);
	else
		snprintf(dst, dst_size, "%s", src);
	}


/* ------------------------------------------------------------ */
void
gkrellm_alert_command_process_connect(GkrellmAlert *alert, void (*func)(),
						gpointer data)
	{
	if (!alert)
		return;
	alert->cb_command_process = func;
	alert->cb_command_process_data = data;
	}

void
gkrellm_alert_config_connect(GkrellmAlert *alert, void (*func)(),gpointer data)
	{
	if (!alert)
		return;
	alert->cb_config = func;
	alert->cb_config_data = data;
	}

void
gkrellm_alert_config_create_connect(GkrellmAlert *alert,
						void (*func)(), gpointer data)
	{
	if (!alert)
		return;
	alert->cb_config_create = func;
	alert->cb_config_create_data = data;
	}

static void
alert_delete(GtkWidget *widget, GkrellmAlert **ap)
	{
	GkrellmAlert			*alert;

	if (!ap)
		return;
	alert = *ap;
	alert->activated = FALSE;
	alert->config_closing = TRUE;
	if (alert->cb_config)
		(*alert->cb_config)(alert, alert->cb_config_data);
	if (alert->config_window)
		gtk_widget_destroy(alert->config_window);
	destroy_alert(alert);
	*ap = NULL;
	}

static void
alert_close(GtkWidget *widget, GkrellmAlert **ap)
	{
	GkrellmAlert	*alert;

	if (!ap)
		return;
	alert = *ap;
	if (!alert->activated)
		alert_delete(NULL, ap);
	else if (alert->config_window)
		{
		gtk_widget_destroy(alert->config_window);
		alert->config_window = NULL;
		alert->delete_button = NULL;
		}
	}

void
gkrellm_alert_window_destroy(GkrellmAlert **ap)
	{
	alert_close(NULL, ap);
	}

static gint
alert_config_window_delete_event(GtkWidget *widget, GdkEvent *ev,
		GkrellmAlert **ap)
	{
	alert_close(widget, ap);
	return FALSE;
	}

static void
alert_apply(GtkWidget *widget, GkrellmAlert **ap)
	{
	GList					*plist, *list;
	GkrellmAlert			*alert;
	GkrellmAlertPlugin		*gap;
	GkrellmAlertPluginLink	*apl;
	gpointer				data;
	gchar					*s;
	GtkSpinButton			*spin;
	gint					n;

	alert = *ap;
	if (!alert->activated && !alert->config_modified)
		return;
	if (alert->high.alarm_limit_spin_button)
		{
		spin = GTK_SPIN_BUTTON(alert->high.alarm_limit_spin_button);
		alert->high.alarm_limit = gtk_spin_button_get_value(spin);
		spin = GTK_SPIN_BUTTON(alert->high.warn_limit_spin_button);
		alert->high.warn_limit = gtk_spin_button_get_value(spin);
		}

	if (alert->low.alarm_limit_spin_button)
		{
		spin = GTK_SPIN_BUTTON(alert->low.alarm_limit_spin_button);
		alert->low.alarm_limit = gtk_spin_button_get_value(spin);
		spin = GTK_SPIN_BUTTON(alert->low.warn_limit_spin_button);
		alert->low.warn_limit = gtk_spin_button_get_value(spin);
		}

	if (alert->high.alarm_limit == 0.0 && alert->low.alarm_limit == 0.0)
		return;
	if (alert->alarm_command_entry)
		{
		spin = GTK_SPIN_BUTTON(alert->alarm_repeat_spin_button);
		alert->alarm_repeat_set = gtk_spin_button_get_value_as_int(spin);

		s = gkrellm_gtk_entry_get_text(&alert->alarm_command_entry);
		gkrellm_dup_string(&alert->alarm_command, s);
		if (!*s)
			alert->alarm_repeat_set = 0;
		if (alert->high.alarm_on || alert->low.alarm_on)
			alert->alarm_repeat = alert->alarm_repeat_set;
		}
	if (alert->warn_command_entry)
		{
		spin = GTK_SPIN_BUTTON(alert->warn_repeat_spin_button);
		alert->warn_repeat_set = gtk_spin_button_get_value_as_int(spin);

		s = gkrellm_gtk_entry_get_text(&alert->warn_command_entry);
		gkrellm_dup_string(&alert->warn_command, s);
		if (!*s)
			alert->warn_repeat_set = 0;
		if (alert->high.warn_on || alert->low.warn_on)
			alert->warn_repeat = alert->warn_repeat_set;
		}
	if (alert->delay_spin_button)
		{
		spin = GTK_SPIN_BUTTON(alert->delay_spin_button);
		n = gtk_spin_button_get_value_as_int(spin);
		gkrellm_alert_set_delay(alert, n / alert->delay_step);
		}
	alert->activated = TRUE;

	for (plist = alert_plugin_list; plist; plist = plist->next)
		{
		data = NULL;
		gap = (GkrellmAlertPlugin *) plist->data;
		for (list = alert->plugin_list; list; list = list->next)
			{
			apl = (GkrellmAlertPluginLink *) list->data;
			if (apl->alert_plugin == gap)
				{
				data = apl->data;
				break;
				}
			}
		if (MONITOR_ENABLED(gap->mon) && gap->config_apply_func)
			(*gap->config_apply_func)(alert, data, alert->config_closing);
		}

	gtk_widget_set_sensitive(alert->delete_button, alert->activated);
	gtk_widget_set_sensitive(alert->icon_box, alert->activated);
	if (alert->cb_config)
		(*alert->cb_config)(alert, alert->cb_config_data);
	gkrellm_config_modified();
	}

static void
alert_ok(GtkWidget *widget, GkrellmAlert **ap)
	{
	(*ap)->config_closing = TRUE;
	alert_apply(NULL, ap);
	alert_close(NULL, ap);
	}

static void
cb_delay_spin_changed(GtkAdjustment *adjustment, GkrellmAlert *alert)
	{
	GtkSpinButton	*spin;
	gint			delay;

	spin = GTK_SPIN_BUTTON(alert->delay_spin_button);
	delay = gtk_spin_button_get_value_as_int(spin);
	if ((delay % alert->delay_step) != 0)
		{
		delay = delay / alert->delay_step * alert->delay_step;
		gtk_spin_button_set_value(spin, delay);
		}
	alert->config_modified = TRUE;
	gtk_widget_set_sensitive(alert->icon_box, TRUE);
	}

static void
cb_high_alarm_spin_changed(GtkAdjustment *adjustment, GkrellmAlert *alert)
	{
	GtkSpinButton	*spin;
	gfloat			alarm, warn;

	spin = GTK_SPIN_BUTTON(alert->high.alarm_limit_spin_button);
	alarm = gtk_spin_button_get_value(spin);
	spin = GTK_SPIN_BUTTON(alert->high.warn_limit_spin_button);
	warn = gtk_spin_button_get_value(spin);
	if (alarm < warn)
		gtk_spin_button_set_value(spin, alarm);
	alert->config_modified = TRUE;
	gtk_widget_set_sensitive(alert->icon_box, TRUE);
	}

static void
cb_high_warn_spin_changed(GtkWidget *adjustment, GkrellmAlert *alert)
	{
	GtkSpinButton	*spin;
	gfloat			alarm, warn, low_warn;

	spin = GTK_SPIN_BUTTON(alert->high.warn_limit_spin_button);
	warn = gtk_spin_button_get_value(spin);
	spin = GTK_SPIN_BUTTON(alert->high.alarm_limit_spin_button);
	alarm = gtk_spin_button_get_value(spin);
	if (alarm < warn)
		gtk_spin_button_set_value(spin, warn);
	if (alert->check_low)
		{
		spin = GTK_SPIN_BUTTON(alert->low.warn_limit_spin_button);
		low_warn = gtk_spin_button_get_value(spin);
		if (low_warn > warn)
			gtk_spin_button_set_value(spin, warn);
		}
	alert->config_modified = TRUE;
	gtk_widget_set_sensitive(alert->icon_box, TRUE);
	}

static void
cb_low_warn_spin_changed(GtkWidget *adjustment, GkrellmAlert *alert)
	{
	GtkSpinButton	*spin;
	gfloat			alarm, warn, high_warn;

	spin = GTK_SPIN_BUTTON(alert->low.warn_limit_spin_button);
	warn = gtk_spin_button_get_value(spin);
	spin = GTK_SPIN_BUTTON(alert->low.alarm_limit_spin_button);
	alarm = gtk_spin_button_get_value(spin);
	if (alarm > warn)
		gtk_spin_button_set_value(spin, warn);
	if (alert->check_high)
		{
		spin = GTK_SPIN_BUTTON(alert->high.warn_limit_spin_button);
		high_warn = gtk_spin_button_get_value(spin);
		if (high_warn < warn)
			gtk_spin_button_set_value(spin, warn);
		}
	alert->config_modified = TRUE;
	gtk_widget_set_sensitive(alert->icon_box, TRUE);
	}

static void
cb_low_alarm_spin_changed(GtkWidget *adjustment, GkrellmAlert *alert)
	{
	GtkSpinButton	*spin;
	gfloat			alarm, warn;

	spin = GTK_SPIN_BUTTON(alert->low.alarm_limit_spin_button);
	alarm = gtk_spin_button_get_value(spin);
	spin = GTK_SPIN_BUTTON(alert->low.warn_limit_spin_button);
	warn = gtk_spin_button_get_value(spin);
	if (alarm > warn)
		gtk_spin_button_set_value(spin, alarm);
	alert->config_modified = TRUE;
	gtk_widget_set_sensitive(alert->icon_box, TRUE);
	}

void
gkrellm_alert_config_window(GkrellmAlert **ap)
	{
	GtkWidget				*tabs;
	GtkWidget				*main_vbox, *tab_vbox, *vbox, *vbox1 = NULL;
	GtkWidget				*hbox, *hbox1, *image;
	GtkWidget				*table = NULL;
	GtkWidget				*button;
	GtkWidget				*label;
	GtkWidget				*separator;
	GList					*plist, *list;
	GkrellmAlert			*alert;
	GkrellmAlertPlugin		*gap;
	GkrellmAlertPluginLink	*apl;
	gpointer				data;
	gchar					*title;
	gint					w, n_tabs = 1;

	if (!ap || !*ap)
		return;
	alert = *ap;
	if (!alert->config_window)
		{
		alert->config_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		g_signal_connect(G_OBJECT(alert->config_window), "delete_event",
				G_CALLBACK(alert_config_window_delete_event), ap);
		gtk_window_set_title(GTK_WINDOW(alert->config_window),
				_("GKrellM Set Alerts"));
		gtk_window_set_wmclass(GTK_WINDOW(alert->config_window),
				"Gkrellm_conf", "Gkrellm");

		gtk_container_set_border_width(GTK_CONTAINER(alert->config_window), 4);
		main_vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(alert->config_window), main_vbox);

		tabs = gtk_notebook_new();
		gtk_box_pack_start(GTK_BOX(main_vbox), tabs, TRUE, TRUE, 0);
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);

		tab_vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Alerts"));

		if (alert->name && alert->unit_string)
			title = g_strdup_printf("%s - %s",
						alert->name, alert->unit_string);
		else if (alert->name)
			title = g_strdup_printf("%s", alert->name);
		else
			title = g_strdup_printf("%s", alert->unit_string);

		vbox = gkrellm_gtk_framed_vbox(tab_vbox, title, 4, FALSE, 4, 3);
		g_free(title);

		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		w = 70;
		if (alert->max_high > 100000)
			w += log(alert->max_high / 100000) * 5;

		alert->high.alarm_limit_spin_button = NULL;
		alert->high.warn_limit_spin_button = NULL;
		if (alert->check_high  && !alert->check_hardwired)
			{
			vbox1 = gkrellm_gtk_framed_vbox(hbox, _("High Limits"),
						2, FALSE, 2,2);
			gkrellm_gtk_spin_button(vbox1,
					&alert->high.alarm_limit_spin_button,
					alert->high.alarm_limit, alert->min_low, alert->max_high,
					alert->step0, alert->step1,
					alert->digits, w, cb_high_alarm_spin_changed, alert, FALSE,
					_("High alarm limit"));
		
			gkrellm_gtk_spin_button(vbox1, &alert->high.warn_limit_spin_button,
					alert->high.warn_limit, alert->min_low, alert->max_high,
					alert->step0, alert->step1,
					alert->digits, w, cb_high_warn_spin_changed, alert, FALSE,
					_("High warn limit"));
			}

		alert->low.alarm_limit_spin_button = NULL;
		alert->low.warn_limit_spin_button = NULL;
		if (alert->check_low && !alert->check_hardwired)
			{
			vbox1 = gkrellm_gtk_framed_vbox_end(hbox, _("Low Limits"),
					2, FALSE, 2, 2);
			gkrellm_gtk_spin_button(vbox1, &alert->low.warn_limit_spin_button,
					alert->low.warn_limit, alert->min_low, alert->max_high,
					alert->step0, alert->step1,
					alert->digits, w, cb_low_warn_spin_changed, alert, FALSE,
					_("Low warn limit"));
			gkrellm_gtk_spin_button(vbox1, &alert->low.alarm_limit_spin_button,
					alert->low.alarm_limit, alert->min_low, alert->max_high,
					alert->step0, alert->step1,
					alert->digits, w, cb_low_alarm_spin_changed, alert, FALSE,
					_("Low alarm limit"));
			}
		if (alert->delay_step > 0)
			{
			vbox1 = gkrellm_gtk_framed_vbox(vbox, NULL, 2, FALSE, 2, 2);
			gkrellm_gtk_spin_button(vbox1, &alert->delay_spin_button,
					alert->delay * alert->delay_step,
					alert->delay_low, alert->delay_high,
					alert->delay_step, alert->delay_step,
					0, 70, cb_delay_spin_changed, alert, FALSE,
					_("Seconds limit conditions must exist to have an alert"));
			}
		if (alert->cb_config_create)
			{
			vbox1 = gkrellm_gtk_framed_vbox(vbox, NULL, 2, FALSE, 2, 2);
			(*alert->cb_config_create)(alert, vbox1,
						alert->cb_config_create_data);
			}

		if (alert->do_alarm_command || alert->do_warn_command)
			{
			vbox1 = gkrellm_gtk_framed_vbox(vbox,
					_("Commands - with repeat intervals in seconds"),
					2, FALSE, 2, 2);
			table = gtk_table_new(3 /* across */, 3 /* down */, FALSE);
			gtk_table_set_col_spacings(GTK_TABLE(table), 4);
			gtk_box_pack_start(GTK_BOX(vbox1), table, FALSE, FALSE, 0);
			}

		alert->alarm_command_entry = alert->alarm_repeat_spin_button = NULL;
		if (alert->do_alarm_command)
			{
			label = gtk_label_new(_("Alarm command:"));
			gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
			gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
			alert->alarm_command_entry = gtk_entry_new();
			gtk_entry_set_max_length(GTK_ENTRY(alert->alarm_command_entry),
					255);
			gtk_table_attach_defaults(GTK_TABLE(table),
					alert->alarm_command_entry, 1, 2, 0, 1);
			gtk_entry_set_text(GTK_ENTRY(alert->alarm_command_entry),
					alert->alarm_command);
			gkrellm_gtk_spin_button(NULL, &alert->alarm_repeat_spin_button,
					alert->alarm_repeat_set, 0, 1000,
					1, 10, 0, 60, NULL, NULL, FALSE, NULL);
			gtk_table_attach_defaults(GTK_TABLE(table),
					alert->alarm_repeat_spin_button, 2, 3, 0, 1);
			}

		alert->warn_command_entry = alert->warn_repeat_spin_button = NULL;
		if (alert->do_warn_command)
			{
			label = gtk_label_new(_("Warn command:"));
			gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
			gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
			alert->warn_command_entry = gtk_entry_new();
			gtk_entry_set_max_length(GTK_ENTRY(alert->warn_command_entry),
					255);
			gtk_table_attach_defaults(GTK_TABLE(table),
					alert->warn_command_entry, 1, 2, 1, 2);
			gtk_entry_set_text(GTK_ENTRY(alert->warn_command_entry),
					alert->warn_command);
			gtk_widget_set_size_request(alert->warn_command_entry, 300, -1);
			gkrellm_gtk_spin_button(NULL, &alert->warn_repeat_spin_button,
					alert->warn_repeat_set, 0, 1000,
					1, 10, 0, 60, NULL, NULL, FALSE, NULL);
			gtk_table_attach_defaults(GTK_TABLE(table),
					alert->warn_repeat_spin_button, 2, 3, 1, 2);
			}

		if (alert->do_alarm_command || alert->do_warn_command)
			{
			separator = gtk_hseparator_new();
			gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 4);

			label = gtk_label_new(
		   _("A repeat of zero seconds executes the command once per alert."));
			gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
			gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
			gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, FALSE, 0);
			}

		for (plist = alert_plugin_list; plist; plist = plist->next)
			{
			data = NULL;
			gap = (GkrellmAlertPlugin *) plist->data;
			for (list = alert->plugin_list; list; list = list->next)
				{
				apl = (GkrellmAlertPluginLink *) list->data;
				if (apl->alert_plugin == gap)
					{
					data = apl->data;
					break;
					}
				}
			if (MONITOR_ENABLED(gap->mon) && gap->config_create_func)
				{
				tab_vbox = gkrellm_gtk_framed_notebook_page(tabs,
							gap->tab_name);
				(*gap->config_create_func)(tab_vbox, alert, data);
				++n_tabs;
				}
			}

		if (n_tabs == 1)
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(tabs), FALSE);

		alert->icon_box = gtk_event_box_new();
		hbox1 = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(main_vbox), hbox1, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox1), alert->icon_box, TRUE, FALSE, 0);
		image = gtk_image_new_from_pixbuf(gkrellm_alert_pixbuf());
		gtk_container_add(GTK_CONTAINER(alert->icon_box), image);
		gtk_widget_set_sensitive(alert->icon_box, alert->activated);

		hbox = gtk_hbutton_box_new();
		gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
		gtk_box_set_spacing(GTK_BOX(hbox), 5);
		gtk_box_pack_end(GTK_BOX(hbox1), hbox, FALSE, FALSE, 0);

		alert->delete_button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
		GTK_WIDGET_SET_FLAGS(alert->delete_button, GTK_CAN_DEFAULT);
		g_signal_connect(G_OBJECT(alert->delete_button), "clicked",
				G_CALLBACK(alert_delete), ap);
		gtk_box_pack_start(GTK_BOX(hbox), alert->delete_button, TRUE, TRUE, 0);
		gtk_widget_set_sensitive(alert->delete_button, alert->activated);

		button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
		GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
		g_signal_connect(G_OBJECT(button), "clicked",
				G_CALLBACK(alert_apply), ap);
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

		button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
		GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
		g_signal_connect(G_OBJECT(button), "clicked",
				G_CALLBACK(alert_close), ap);
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

		button = gtk_button_new_from_stock(GTK_STOCK_OK);
		GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
		g_signal_connect(G_OBJECT(button), "clicked",
				G_CALLBACK(alert_ok), ap);
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_widget_grab_default(button);

		gtk_widget_show_all(alert->config_window);
		}
	else
		gtk_window_present(GTK_WINDOW(alert->config_window));
	alert->config_closing = FALSE;
	}

void
gkrellm_save_alertconfig(FILE *f, GkrellmAlert *alert,
			gchar *mon_keyword, gchar *name)
	{
	GList					*list;
	GkrellmAlertPlugin		*gap;
	GkrellmAlertPluginLink	*apl;
	gchar					*s, *p;

	if (!f || !alert || !mon_keyword)
		return;
	if (name)
		s = g_strdup_printf("%s %s %s ", mon_keyword,
				GKRELLM_ALERTCONFIG_KEYWORD, name);
	else
		s = g_strdup_printf("%s %s ", mon_keyword,GKRELLM_ALERTCONFIG_KEYWORD);

	if (alert->alarm_command && *alert->alarm_command)
		fprintf(f, "%s alarm_command %s\n", s, alert->alarm_command);
	if (alert->warn_command && *alert->warn_command)
		fprintf(f, "%s warn_command %s\n", s, alert->warn_command);
	fprintf(f, "%s values %d %d %d %d\n", s, alert->do_panel_updates,
			alert->check_high, alert->check_low, alert->check_hardwired);
	fprintf(f, "%s repeat %d %d\n", s, alert->alarm_repeat_set,
			alert->warn_repeat_set);

	/* 2.1.15: scale saved float values to avoid decimal points in the config
	|  because of locale breakage if decimal point changes '.' <-> ',"
	*/
	fprintf(f, "%s limits %.0f %.0f %.0f %.0f\n", s,
				alert->high.alarm_limit * GKRELLM_FLOAT_FACTOR,
				alert->high.warn_limit  * GKRELLM_FLOAT_FACTOR,
				alert->low.warn_limit   * GKRELLM_FLOAT_FACTOR,
				alert->low.alarm_limit  * GKRELLM_FLOAT_FACTOR);
	if (alert->delay_step > 0)
		fprintf(f, "%s delay %d %d %d %d\n", s, alert->delay,
					alert->delay_high, alert->delay_low, alert->delay_step);

	/* name can be quoted, but the id_string should not have embedded quotes.
	|  id_string is so a plugin can get a unique config name for an alert
	*/
	g_free(alert->id_string);
	p = name;
	if (p && *p == '"')
		++p;
	alert->id_string = g_strconcat(mon_keyword, p ? "-" : NULL, p, NULL);
	if ((p = strrchr(alert->id_string, '"')) != NULL)
		*p = '\0';
	for (p = alert->id_string; *p; ++p)
		if (*p == '/' || *p == ' ')
			*p = '-';
	fprintf(f, "%s id_string %s\n", s, alert->id_string);

	for (list = alert->plugin_list; list; list = list->next)
		{
		apl = (GkrellmAlertPluginLink *) list->data;
		gap = apl->alert_plugin;
		if (MONITOR_ENABLED(gap->mon) && gap->config_save_func)
			{
			p = g_strconcat(s, "plugin ", gap->name, NULL);
			(*gap->config_save_func)(alert, apl->data, f, p, alert->id_string);
			g_free(p);
			}
		}
	g_free(s);
	}

void
gkrellm_load_alertconfig(GkrellmAlert **ap, gchar *config_line)
	{
	GList					*list;
	GkrellmAlert 			*alert;
	GkrellmAlertPlugin		*gap;
	gchar					config[32], item[CFG_BUFSIZE];
	gchar					name[64], item1[CFG_BUFSIZE];
	gint					n;

	if (!ap || !config_line)
		return;
	if (!*ap)
		*ap = g_new0(GkrellmAlert, 1);
	alert = *ap;

	n = sscanf(config_line, "%31s %[^\n]", config, item);
	if (n != 2)
		return;

	if (!strcmp(config, "alarm_command"))
		gkrellm_dup_string(&alert->alarm_command, item);
	else if (!strcmp(config, "warn_command"))
		gkrellm_dup_string(&alert->warn_command, item);
	else if (!strcmp(config, "values"))
		sscanf(item, "%d %d %d %d", &alert->do_panel_updates,
			&alert->check_high, &alert->check_low, &alert->check_hardwired);
	else if (!strcmp(config, "delay"))
		{
		sscanf(item, "%d %d %d %d", &n,
				&alert->delay_high, &alert->delay_low, &alert->delay_step);
		gkrellm_alert_set_delay(alert, n);
		}
	else if (!strcmp(config, "repeat"))
		sscanf(item, "%d %d", &alert->alarm_repeat_set,
				&alert->warn_repeat_set);
	else if (!strcmp(config, "limits"))
		{
		sscanf(item, "%f %f %f %f",
				&alert->high.alarm_limit, &alert->high.warn_limit,
				&alert->low.warn_limit, &alert->low.alarm_limit);
		
		alert->high.alarm_limit /= _GK.float_factor;
		alert->high.warn_limit  /= _GK.float_factor;
		alert->low.warn_limit   /= _GK.float_factor;
		alert->low.alarm_limit  /= _GK.float_factor;
		}
	else if (!strcmp(config, "id_string"))
		{
		gkrellm_dup_string(&alert->id_string, item);
		for (list = alert_plugin_list; list; list = list->next)
			{
			gap = (GkrellmAlertPlugin *) list->data;
			if (MONITOR_ENABLED(gap->mon) && gap->config_load_func)
				(*gap->config_load_func)(alert, "id_string", alert->id_string);
			}
		}
	else if (!strcmp(config, "plugin"))
		{
		if (sscanf(item, "%63s %[^\n]", name, item1) == 2)
			{
			for (list = alert_plugin_list; list; list = list->next)
				{
				gap = (GkrellmAlertPlugin *) list->data;
				if (!strcmp(name, gap->name))
					{
					if (MONITOR_ENABLED(gap->mon) && gap->config_load_func)
						(*gap->config_load_func)(alert,
									item1, alert->id_string);
					break;
					}
				}
			}
		}
	alert->activated = TRUE;
	}


/* ------------------------------------------------------------------- */
/* gdk-pixbuf-csource --static alert_inline.png */

/* GdkPixbuf RGBA C-Source image dump 1-byte-run-length-encoded */

static const guint8 alert_inline[] = 
{ ""
  /* Pixbuf magic (0x47646b50) */
  "GdkP"
  /* length: header (24) + pixel_data (925) */
  "\0\0\3\265"
  /* pixdata_type (0x2010002) */
  "\2\1\0\2"
  /* rowstride (72) */
  "\0\0\0H"
  /* width (18) */
  "\0\0\0\22"
  /* height (18) */
  "\0\0\0\22"
  /* pixel_data: */
  "\227\0\0\0\0\10\204}{\377B89\377\20\24\20\377\10\2\10\377\0\2\0\377\30"
  "\24\20\3779<9\377\214\212\204\377\211\0\0\0\0\12e`T\37732*\377\202]@"
  "\377\251eF\377\267g?\377\265cF\377\243eG\377\202VG\37773$\377io`\377"
  "\207\0\0\0\0\14PVG\377a<*\377\257^?\377\310\211q\377\333\262\241\377"
  "\222\202t\377\311\254\234\377\333\262\233\377\310\211q\377\257^?\377"
  "Z=+\377\\bM\377\205\0\0\0\0\16cfS\377Z=+\377\257^?\377\320\234\203\377"
  "\333\270\241\377\252\217}\377\31\25\23\3772*%\377\342\274\251\377\333"
  "\270\241\377\320\234\203\377\257^?\377Z=+\377|~k\377\203\0\0\0\0\17\204"
  "\206{\37773$\377\251_?\377\301\202b\377\325\253\222\377\341\255\233\377"
  "aNE\377\222tb\377\0\0\0\377\310\232\211\377\333\257\233\377\325\253\222"
  "\377\302{h\377\251Z?\37773$\377\203\0\0\0\0\12JAB\377xQ1\377\257[?\377"
  "\320\234\203\377\325\234\203\377\333\236\205\3770#\35\377\333\236\205"
  "\377\30\21\17\377aF;\377\202\325\234\203\377\4\310\222u\377\257[?\377"
  "tJ1\377kik\377\202\0\0\0\0\5\20\24\20\377\235[G\377\257[?\377\317\212"
  "p\377\325\215u\377\202vOA\377\11\325\215u\377vM>\377\0\0\0\377\325\213"
  "q\377\317\212p\377\317\213v\377\260T8\377\235[G\377)$)\377\202\0\0\0"
  "\0\20\0\2\0\377\251Z?\377\267X?\377\310q\\\377\310{c\377.\32\25\377\270"
  "lW\377\316z\\\377sA6\377\0\0\0\377sA6\377\317vb\377\301mV\377\257Q?\377"
  "\245R8\377\10\2\10\377\202\0\0\0\0\6\0\2\0\377\251S?\377\260T8\377\277"
  "N9\377\226M9\377,\25\21\377\202\205@4\377\10\200B4\377,\25\21\377+\26"
  "\20\377\277N9\377\270K9\377\252M8\377\237N8\377\10\2\10\377\202\0\0\0"
  "\0\20\30\24\30\377\230Y@\377\252S8\377\267R?\377T)\34\377\200>.\377\300"
  "]F\377\307[F\377\300]F\377\200<.\377\0\0\0\377\221J<\377\260L8\377\245"
  "J8\377\230VG\377!(!\377\202\0\0\0\0\7""9<1\377xO@\377\245J8\377\227D"
  "1\377\0\0\0\377\266S8\377\302Q?\377\203\274S?\377\6\0\0\0\377N\37\31"
  "\377\252F8\377\236F2\377~N@\377cic\377\202\0\0\0\0\12\234\216\224\377"
  "73$\377\237G8\377\22\10\6\377\0\0\0\377N\40\26\377\242?1\377\257I2\377"
  "\266G8\377N\40\26\377\202\0\0\0\377\3F\37\26\377\231D9\37773$\377\204"
  "\0\0\0\0\5ii`\377T3#\377\231E2\377\237G8\377\252F8\377\203\251B2\377"
  "\6\252F8\377\236@2\377\231@2\377\231E2\377N6#\377{\204q\377\205\0\0\0"
  "\0\4baY\377T3#\377\223D2\377\231E2\377\202\236@2\377\6\237<6\377\231"
  "E2\377\231@2\377\223@2\377T3#\377ii`\377\207\0\0\0\0\4t}j\377-4#\377"
  "~N@\377\216K9\377\202\223@2\377\4\222K@\377xO@\37773$\377{\204q\377\212"
  "\0\0\0\0\6kik\377!(!\377\10\2\10\377\0\2\0\377)$)\377kik\377\230\0\0"
  "\0\0"};

static GdkPixbuf	*alert_pixbuf;

GdkPixbuf *
gkrellm_alert_pixbuf(void)
	{
	if (!alert_pixbuf)
		alert_pixbuf = gdk_pixbuf_new_from_inline(-1, alert_inline,
						FALSE, NULL);
	return alert_pixbuf;
	}
