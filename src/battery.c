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

#include "gkrellm.h"
#include "gkrellm-private.h"
#include "gkrellm-sysdeps.h"

#include <math.h>


#define	BAT_CONFIG_KEYWORD	"battery"

typedef enum
	{
	BATTERYDISPLAY_PERCENT,
	BATTERYDISPLAY_TIME,
	BATTERYDISPLAY_RATE,
	BATTERYDISPLAY_EOM	/* end of modes */
	}
	BatteryDisplayMode; 


typedef struct
	{
	gint				id;
	GkrellmPanel		*panel;
	GkrellmKrell		*krell;
	GkrellmDecal		*power_decal;
	GkrellmDecal		*time_decal;

	GkrellmAlert		*alert;

	gboolean			enabled;

	BatteryDisplayMode	display_mode;
	gfloat				charge_rate;	/* % / min */

	gboolean			present,
						on_line,
						charging;
	gint				percent;
	gint				time_left;		/* In minutes, -1 if minutes unavail */
	}
	Battery;

static GList			*battery_list;
static GkrellmMonitor	*mon_battery;
static GtkWidget		*battery_vbox;

static Battery			*composite_battery,
						*launch_battery;

static gboolean			enable_composite,
						enable_each,
						enable_estimate;

static gint				poll_interval = 5,
						full_cap_fallback = 5000;

static GkrellmLauncher	launch;

static GkrellmAlert		*bat_alert;		/* One alert dupped for each battery */

static gint				style_id;

static gboolean			alert_units_percent,
						alert_units_need_estimate_mode;

static void		(*read_battery_data)();
static void		create_battery_panel(Battery *bat, gboolean first_create);

static gint		n_batteries;


static Battery *
battery_nth(gint n, gboolean create)
	{
	Battery			*bat;

	if (n > 10)
		return NULL;
	if (n < 0)
		{
		if (!composite_battery && create)
			{
			bat = g_new0(Battery, 1);
			battery_list = g_list_prepend(battery_list, bat);
			bat->id = GKRELLM_BATTERY_COMPOSITE_ID;		/* -1 */
			composite_battery = bat;
			gkrellm_alert_dup(&bat->alert, bat_alert);
			}
		return composite_battery;
		}

	if (composite_battery)
		++n;

	while (   (bat = (Battery *) g_list_nth_data(battery_list, n)) == NULL
		   && create
		  )
		{
		bat = g_new0(Battery, 1);
		battery_list = g_list_append(battery_list, bat);
		bat->id = n_batteries++;
		gkrellm_alert_dup(&bat->alert, bat_alert);
		}
	return bat;
	}


  /* Themers need to be able to see the battery monitor.
  */
static void
read_battery_demo(void)
	{
	gboolean	on_line, charging;
	gint		percent, time_left;
	static gint	bump = 60;

	if (bump <= 5)
		bump = 60;
	bump -= 5;
	on_line = bump > 45;
	if (on_line)
		{
		charging = TRUE;
		time_left = 200 + (60 - bump) * 20;
		percent = time_left / 5;
		}
	else
		{
		charging = FALSE;
		time_left = bump;
		percent = 1 + bump;
		}
	gkrellm_battery_assign_data(0, TRUE, on_line, charging,
				percent, time_left);
	}


static gboolean
setup_battery_interface(void)
	{
    if (!read_battery_data && !_GK.client_mode && gkrellm_sys_battery_init())
        read_battery_data = gkrellm_sys_battery_read_data;
	if (_GK.demo)
		read_battery_data = read_battery_demo;
    return read_battery_data ? TRUE : FALSE;
	}

void 
gkrellm_battery_client_divert(void (*read_func)())
	{
	read_battery_data = read_func;
	}

void
gkrellm_battery_assign_data(gint n, gboolean present, gboolean on_line,
			gboolean charging, gint percent, gint time_left)
	{
	Battery	*bat;

	bat = battery_nth(n, TRUE);
	if (!bat)
		return;
	bat->present = present;
	bat->on_line = on_line;
	bat->charging = charging;
	bat->percent = percent;
	bat->time_left = time_left;
	}

  /* Help out some laptops with Linux ACPI bugs */
gint
gkrellm_battery_full_cap_fallback(void)
	{
	return full_cap_fallback;
	}


/* -------------------------------------------------------------- */


/* estimate (guess-timate?) battery time remaining, based on the rate of 
   discharge (and conversely the time to charge based on the rate of charge).
  - some BIOS' only provide battery levels, not any estimate of the time
    remaining
    
  Battery charge/discharge characteristics (or, why dc/dt doesn't really work)
  - the charge/discharge curves of most battery types tend to be very non-
    linear (http://www.google.com/search?q=battery+charge+discharge+curve)
  - on discharge, most battery types will initially fall somewhat rapidly
    from 100 percent, then flatten out and stay somewhat linear until
    suddenly "dropping out" when nearly depleted (approx. 10-20% capacity).
    For practical purposes we can consider this point to be the end of the
    discharge curve. This is simple enough to model via a fixed capacity
    offset to cut out just at the knee of this curve, and allows us to
    reasonably approximate the rest of the curve by a linear function
    and simple dc/dt calculation.
  - with regard to charging, however, it's not quite so easy. With a
    constant voltage charger, the battery capacity rises exponentially
    (charging current decreases as battery terminal voltage rises). The
    final stages of charging are very gradual, with a relatively long
    period at "almost but not quite 100%".

    Unfortunately a linear extrapolation at the beginning of an 
    exponential curve will be a poor approximation to the true expected
    time to charge, tending to be significantly undervalued. Using an 
    exponential model to estimate time to approx. 90-95% (2.5 * exp. time
    constant) seems to give a more reasonable fit. That said, the poor
    relative resolution at higher charge values makes estimating the
    exponential time constant difficult towards the end of the charge 
    cycle (the curve's very nearly flat). So, I've settled on a mixed 
    model - for c < ~70 I use an exponential model, and switch to linear
    above that (or if the charge rate seems to have otherwise "flatlined").

    Empirically, this method seems to give reasonable results [1] - 
    certainly  much better than seeing "0:50:00 to full" for a good half an
    hour (i.e. as happens with apmd, which uses a linear model for both
    charging + discharging). Note that a constant-current charger should
    be pretty well linear all the way along the charge curve, which means
    the linear rate extrapolation should work well in this case. The user
    can choose which model they wish to use via estimate_model.

    [1] I logged my Compaq Armada M300's capacity (via /proc/apm) over one
    complete discharge/charge cycle (machine was idle the whole time). The
    discharge curve was linear to approx. 14% when the BIOS alerted of 
    impending doom; upon plugging in the external power supply the capacity
    rose exponentially to 100%, with a time constant of approx. 0.8 hr (i.e. 
    approx. 2+ hrs to full charge).

  Linear rate of change calculation:
  - in an ideal, continuous world, estimated time to 0(100) would simply 
    be the remaining capacity divided by the charge rate
       ttl = c / dc(t)/dt
  - alas, the reported battery capacity is bound to integer values thus 
    c(t) is a discontinuous function. i.e. has fairly large steps. And of
    course then dc/dt is undefined at the discontinuities.
  - to avoid this issue the rate of charge is determined by the deltas from
    the start of the last state change (charge/discharge cycle) (time T)
       ttl(t) = c(t) / ((C - c(t)) / (T - t))    C = charge at time T
    Furthermore, the rate changes are windowed and filtered to mitigate 
    c(t) transients (e.g. at the start of discharge) and smooth over 
    discontinuities (and fudge for battery characteristics, ref. above).
*/

#define BAT_SLEEP_DETECT 300		/* interval of >300s => must have slept */
#define BAT_DISCHARGE_TRANSIENT 10	/* ignore first 10% of discharge cycle */
#define BAT_EMPTY_CAPACITY 12		/* when is the battery "flat"? */
#define BAT_RATECHANGE_WINDOW 90	/* allow rate changes after 90s */
#define BAT_CHARGE_MODEL_LIMIT 60	/* linear charge model cutover point */
#define BAT_RATE_SMOOTHING 0.3		/* rate smoothing weight */

/* #define BAT_ESTIMATE_DEBUG */

  /* user-provided nominal battery runtimes, hrs (used to determine initial 
  |  discharge, stable, charge rate (%/min))
  */
static gfloat	estimate_runtime[2] = {0};
static gint		estimate_model[2] = {0};
static gboolean	reset_estimate;

static void
estimate_battery_time_left(Battery *bat)
	{
	/* ?0 = at time 0 (state change); ?1 = at last "sample" (rate change) */
	static time_t	t0 = -1, t1;
	static gint		p0, p1;
	static gint		c0;
	static gfloat	rate = 0;
	static time_t	dt;
	static gint		dp;
	time_t			t = time(NULL);

	/* 1 charging; 0 power on and stable; -1 discharging */
	gint			charging = bat->charging ? 1 : (bat->on_line ? 0 : -1);

#ifdef BAT_ESTIMATE_DEBUG
		fprintf(stderr, "%ld bc?=%d ac?=%d (%+d) bp=%d\t", t, 
			bat->charging, bat->on_line, charging, 
			bat->percent);
#endif

	if (   reset_estimate || t0 < 0 || c0 != charging
		|| (t - t1) > BAT_SLEEP_DETECT
	   )
		{
		/* init, state change, or sleep/hibernation
		*/
		reset_estimate = FALSE;
		c0 = charging;

		t0 = t1 = t;
		if (charging < 0 && (bat->percent > 100 - BAT_DISCHARGE_TRANSIENT))
			p0 = p1 = 100 - BAT_DISCHARGE_TRANSIENT;
		else
			p0 = p1 = bat->percent;
		dp = dt = 0;
		rate = 0.0;

		/* convert runtime (hrs) to signed rate (%/min)
		*/
		if (charging < 0)
			rate = -100 / (estimate_runtime[0] * 60);
		else if (charging > 0)
			rate =  100 / (estimate_runtime[1] * 60);

#ifdef BAT_ESTIMATE_DEBUG
		fprintf(stderr, "[r = %.3f]\t", rate);
#endif
		}
	else
		{
		time_t	dt1 = t - t1;		/* delta since last rate change */
		gint	dp1 = bat->percent - p1;

		/* time for a rate change?
		*/
		if (   dt1 > BAT_RATECHANGE_WINDOW
			&& ((charging > 0 && dp1 >= 0) || (charging < 0 && dp1 <= 0))
		   )
			{
			dt = t - t0;					/* since state change */
			dp = bat->percent - p0;

			if (dp1 == 0)	/* flatlining (dp/dt = 0) */
				rate = (1 - BAT_RATE_SMOOTHING/4) * rate;
			else
				rate = BAT_RATE_SMOOTHING *
						((gdouble) dp / (gdouble) (dt/60)) + 
						(1 - BAT_RATE_SMOOTHING) * rate;

#ifdef BAT_ESTIMATE_DEBUG
			fprintf(stderr, "%d [dp = %+d dt = %.2f rate = %.3f]\t",
					(gint) dp1, dp, (gdouble) dt / 60, rate);
#endif

			t1 = t;
			p1 = bat->percent;
			}
		}

	if (charging && rate != 0.0)	/* (dis)charging */
		{
		gfloat	eta;
		gint 	p = charging > 0 ? 100 - bat->percent : 
						bat->percent - BAT_EMPTY_CAPACITY;

		if (   charging > 0 && estimate_model[1]
			&& bat->percent < BAT_CHARGE_MODEL_LIMIT && dp > 0
		   )
			/* charging, use exponential: eta =~ 2.5 * time-constant (~=92%) */
			eta = -2.5 * dt/60 / (log(1 - (gdouble)dp/(gdouble)(p+dp)));
		else
			eta = abs((gdouble)p / rate);	/* use linear */

#ifdef BAT_ESTIMATE_DEBUG
		fprintf(stderr, "eta = %.2f\t", eta);
#endif

		/* round off to nearest 5 mins */
		bat->time_left = (gint)((eta > 0 ? eta + 2.5: 0) / 5) * 5;
		bat->charge_rate = rate;
		}
	else
		{
		bat->time_left = INT_MAX;	/* inf */
		bat->charge_rate = 0.0;
		}

#ifdef BAT_ESTIMATE_DEBUG
		fprintf(stderr, "\n");
#endif
	}

static void
draw_time_left_decal(Battery *bat, gboolean force)
	{
	GkrellmDecal	*d;
	gchar			buf[16];
	gint			x, w, t;
	int				battery_display_mode = bat->display_mode;
	static BatteryDisplayMode	last_mode = BATTERYDISPLAY_EOM;

	if (!bat->panel)
		return;
	if (bat->time_left == -1)
		battery_display_mode = BATTERYDISPLAY_PERCENT;
	if (last_mode != battery_display_mode)
		force = TRUE;
	last_mode = bat->display_mode;

	switch (battery_display_mode)
		{
		case BATTERYDISPLAY_TIME:
			t = bat->time_left;
			if (t == INT_MAX || t == INT_MIN)
				snprintf(buf, sizeof(buf), "--");
			else
				snprintf(buf, sizeof(buf), "%2d:%02d", t / 60, t % 60);
			break;

		case BATTERYDISPLAY_RATE:
			/* t is used by draw_decal_text() to see if a refresh is reqd */
			t = (gint) (bat->charge_rate * 100.0);
			snprintf(buf, sizeof(buf), "%0.1f%%/m",
						bat->charge_rate);
			break;

		case BATTERYDISPLAY_PERCENT:
		default:
			t = bat->percent;
			if (t == -1)	/* APM battery flags should cause hide... but */
				snprintf(buf, sizeof(buf), "no bat");
			else
				snprintf(buf, sizeof(buf), "%d%%", t);
			break;
		}

	d = bat->time_decal;
	w = gkrellm_gdk_string_width(d->text_style.font, buf);
	x = (d->w - w) / 2;
	if (x < 0)
		x = 0;
	d->x_off = x;
	gkrellm_draw_decal_text(bat->panel, d, buf, force ? -1 : t);
	}

static void
update_battery_panel(Battery *bat)
	{
	GkrellmPanel	*p  = bat->panel;

	if (!p)
		return;
	if (!bat->present)
		{	/* Battery can be removed while running */
		gkrellm_panel_hide(p);
		return;
		}
	gkrellm_panel_show(p);

	if (bat->time_left > 0 && bat->charging)
		bat->charge_rate = (gfloat) (100 - bat->percent) / (gfloat) bat->time_left;
	else
		bat->charge_rate = 0.0;

	if (enable_estimate)
		estimate_battery_time_left(bat);

	if (bat->on_line)
		{
		gkrellm_reset_alert(bat->alert);
		gkrellm_freeze_alert(bat->alert);
		gkrellm_draw_decal_pixmap(p, bat->power_decal, D_MISC_AC);
		}
	else
		{
		if (   (bat == composite_battery && enable_composite)
			|| (bat != composite_battery && enable_each)
			)
			{
			gkrellm_thaw_alert(bat->alert);
			gkrellm_check_alert(bat->alert, alert_units_percent
						? bat->percent : bat->time_left);
			}
		gkrellm_draw_decal_pixmap(p, bat->power_decal, D_MISC_BATTERY);
		}
	draw_time_left_decal(bat, FALSE);
	gkrellm_update_krell(p, bat->krell, bat->percent);
	gkrellm_draw_panel_layers(p);
	}

static void
update_battery(void)
	{
	GList		*list;
	Battery		*bat;
	static gint	seconds = 0;

	if (!enable_each && !enable_composite)
		return;
	if (GK.second_tick)
		{
		if (seconds == 0)
			{
			for (list = battery_list; list; list = list->next)
				((Battery *) list->data)->present = FALSE;

			(*read_battery_data)();
			for (list = battery_list; list; list = list->next)
				{
				bat = (Battery *) list->data;
				if (!bat->panel)
					create_battery_panel(bat, TRUE);
				if (bat->enabled)
					update_battery_panel(bat);
				}
			}
		seconds = (seconds + 1) % poll_interval;
		}
	}

static gboolean
cb_expose_event(GtkWidget *widget, GdkEventExpose *ev, GkrellmPanel *p)
	{
	gdk_draw_drawable(widget->window,
			widget->style->fg_gc[GTK_WIDGET_STATE(widget)], p->pixmap,
			ev->area.x, ev->area.y, ev->area.x, ev->area.y,
			ev->area.width, ev->area.height);
	return FALSE;
	}

static gboolean
cb_panel_enter(GtkWidget *w, GdkEventButton *ev, Battery *bat)
	{
	gkrellm_decal_on_top_layer(bat->time_decal, TRUE);
	gkrellm_draw_panel_layers(bat->panel);
	return FALSE;
	}

static gboolean
cb_panel_leave(GtkWidget *w, GdkEventButton *ev, Battery *bat)
	{
	gkrellm_decal_on_top_layer(bat->time_decal, FALSE);
	gkrellm_draw_panel_layers(bat->panel);
	return FALSE;
	}


static gboolean
cb_panel_press(GtkWidget *widget, GdkEventButton *ev, Battery *bat)
	{
	GkrellmDecal			*d;
	static gboolean	time_unavailable_warned;

	d = launch.decal;
	if (ev->button == 3)
		gkrellm_open_config_window(mon_battery);
	else if (   ev->button == 2
			 || (ev->button == 1 && !d)
			 || (ev->button == 1 && d && ev->x < d->x)
			)
		{
		if (bat->time_left == -1 && bat->present)
			{
			if (!time_unavailable_warned)
				gkrellm_message_dialog(_("GKrellM Battery"),
					_("Battery times are unavailable.  You\n"
					  "could select the Estimated Time option."));
			time_unavailable_warned = TRUE;
			bat->display_mode = BATTERYDISPLAY_PERCENT;
			}
		else
			{
			bat->display_mode++;
			if (bat->display_mode == BATTERYDISPLAY_EOM)
				bat->display_mode = 0;

			draw_time_left_decal(bat, TRUE);
			gkrellm_draw_panel_layers(bat->panel);
			gkrellm_config_modified();
			}
		}
	return FALSE;
	}

static void
create_battery_panel(Battery *bat, gboolean first_create)
	{
	GkrellmPanel		*p;
	GkrellmStyle		*style;
	GkrellmMargin		*m;
	gint				x, w;

	if (!bat->panel)
		bat->panel = gkrellm_panel_new0();
	p = bat->panel;
	style = gkrellm_meter_style(style_id);
	m = gkrellm_get_style_margins(style);
	bat->power_decal = gkrellm_create_decal_pixmap(p,
			gkrellm_decal_misc_pixmap(), gkrellm_decal_misc_mask(),
			N_MISC_DECALS, style, m->left, -1);

	x = bat->power_decal->x + bat->power_decal->w + 2;
	w = gkrellm_chart_width() - x - m->right;
	bat->time_decal = gkrellm_create_decal_text(p, "8/%",
						gkrellm_meter_textstyle(style_id),
						style, x, -1, w);

	bat->krell = gkrellm_create_krell(p,
						gkrellm_krell_meter_piximage(style_id), style);
	gkrellm_monotonic_krell_values(bat->krell, FALSE);
	gkrellm_set_krell_full_scale(bat->krell, 100, 1);

	gkrellm_panel_configure(p, NULL, style);
	gkrellm_panel_create(battery_vbox, mon_battery, p);

	/* Center the decals with respect to each other.
	*/
	if (bat->power_decal->h > bat->time_decal->h)
		bat->time_decal->y += (bat->power_decal->h - bat->time_decal->h) / 2;
	else
		bat->power_decal->y += (bat->time_decal->h - bat->power_decal->h) / 2;

	if (first_create)
		{
		g_signal_connect(G_OBJECT(p->drawing_area), "expose_event",
				G_CALLBACK(cb_expose_event), p);
		g_signal_connect(G_OBJECT(p->drawing_area), "button_press_event",
				G_CALLBACK(cb_panel_press), bat);
		g_signal_connect(G_OBJECT(p->drawing_area), "enter_notify_event",
                G_CALLBACK(cb_panel_enter), bat);
		g_signal_connect(G_OBJECT(p->drawing_area), "leave_notify_event",
                G_CALLBACK(cb_panel_leave), bat);
		 }

	gkrellm_setup_decal_launcher(p, &launch, bat->time_decal);
	if (   (bat == composite_battery && enable_composite)
		|| (bat->id == 0 && composite_battery && !enable_composite)
		|| (bat->id == 0 && !composite_battery)
		)
		launch_battery = bat;

	if (bat == composite_battery)
		bat->enabled = enable_composite;
	else
		bat->enabled = enable_each;

	if (bat->enabled)
		update_battery_panel(bat);
	else
		gkrellm_panel_hide(p);
	}

static void
spacer_visibility(void)
	{
	GList		*list;
	Battery		*bat;
	gboolean	enabled = FALSE;

	for (list = battery_list; list; list = list->next)
		{
		bat = (Battery *) list->data;
		enabled |= bat->enabled;
		}
	if (enabled)
		gkrellm_spacers_show(mon_battery);
	else
		gkrellm_spacers_hide(mon_battery);
	}

static void
create_battery(GtkWidget *vbox, gint first_create)
	{
	GList	*list;
	Battery	*bat;

	battery_vbox = vbox;
	if (_GK.demo)
		enable_each = TRUE;
	for (list = battery_list; list; list = list->next)
		{
		bat = (Battery *) list->data;
		create_battery_panel(bat, first_create);
		}
	spacer_visibility();
	}


  /* Expand alert command substitution variables:
  |  $H - hostname           $n - battery id
  |  $t - time left          $p - percent
  |  $o - online (boolean)   $c - charging (boolean)
  */
static void
cb_command_process(GkrellmAlert *alert, gchar *src, gchar *buf, gint size,
			Battery *bat)
	{
	gchar		c, *s;
	gint		len, value;

	if (!buf || size < 1)
		return;
	--size;
	*buf = '\0';
	if (!src)
		return;
	for (s = src; *s != '\0' && size > 0; ++s)
		{
		len = 1;
		if (*s == '$' && *(s + 1) != '\0')
			{
			value = -1;
			if ((c = *(s + 1)) == 'H')
				len = snprintf(buf, size, "%s", gkrellm_sys_get_host_name());
			else if (c == 'n' && bat != composite_battery)
				value = bat->id;
			else if (c == 't')
				value = bat->time_left;
			else if (c == 'p')
				value = bat->percent;
			else if (c == 'o')
				value = bat->on_line;
			else if (c == 'c')
				value = bat->charging;
			else
				len = 0;

			if (value >= 0)
				len = snprintf(buf, size, "%d", value);
			++s;
			}
		else
			*buf = *s;
		size -= len;
		buf += len;
		}
	*buf = '\0';
	}

static void
cb_battery_alert_trigger(GkrellmAlert *alert, Battery *bat)
	{
	GkrellmAlertdecal	*ad;
	GkrellmDecal		*d;

	alert->panel = bat->panel;
	ad = &alert->ad;
	d = bat->time_decal;
	ad->x = d->x + 1;
	ad->y = d->y - 2;
	ad->w = d->w - 1;
	ad->h = d->h + 4;
	gkrellm_render_default_alert_decal(alert);
	}

static void
dup_battery_alert(void)
	{
	GList	*list;
	Battery	*bat;

	for (list = battery_list; list; list = list->next)
		{
		bat = (Battery *) list->data;
		gkrellm_alert_dup(&bat->alert, bat_alert);
		gkrellm_alert_trigger_connect(bat->alert,
					cb_battery_alert_trigger, bat);
		gkrellm_alert_command_process_connect(bat->alert,
					cb_command_process, bat);
		}
	}


  /* If the OS reports battery times, alerts will always have minutes units.
  |  If the OS does not report battery times the initial alert create will
  |  have minutes units if the estimate time option is enabled and it will
  |  have battery percent level units if estimate time option is off.  Alert
  |  creates from load config will have units in effect at last save config.
  */
static void
create_alert(void)
	{
	Battery	*bat;

	if (!battery_list)
		return;
	bat = (Battery *) battery_list->data;

	if (!bat_alert)
		{
		alert_units_need_estimate_mode = FALSE;

		if (   alert_units_percent
			|| (bat->time_left == -1 && !enable_estimate)
		   )
			{
			if (bat->time_left == -1)
				alert_units_percent = TRUE;
			bat_alert = gkrellm_alert_create(NULL, _("Battery"),
					_("Battery Percent Limits"),
					FALSE, TRUE, TRUE, 99, 0, 1, 10, 0);
			}
		else
			{
			bat_alert = gkrellm_alert_create(NULL, _("Battery"),
					_("Battery Minutes Remaining Limits"),
					FALSE, TRUE, TRUE, 500, 0, 1, 10, 0);
			if (bat->time_left == -1)
				alert_units_need_estimate_mode = TRUE;
			}
		}
	gkrellm_alert_config_connect(bat_alert, dup_battery_alert, NULL);

	/* This alert is a master to be dupped and is itself never checked */
	}


static void
save_battery_config(FILE *f)
	{
	GList	*list;
	Battery	*bat;

	fprintf(f, "%s enable %d\n", BAT_CONFIG_KEYWORD, enable_each);
	fprintf(f, "%s enable_composite %d\n", BAT_CONFIG_KEYWORD,
					enable_composite);
	fprintf(f, "%s estimate_time %d\n", BAT_CONFIG_KEYWORD, enable_estimate);

	/* 2.1.15: scale saved float values to avoid decimal points in the config.
	*/
	fprintf(f, "%s estimate_time_discharge %.0f\n", BAT_CONFIG_KEYWORD,
				estimate_runtime[0] * GKRELLM_FLOAT_FACTOR);
	fprintf(f, "%s estimate_time_charge %.0f\n", BAT_CONFIG_KEYWORD,
				estimate_runtime[1] * GKRELLM_FLOAT_FACTOR);
	fprintf(f, "%s estimate_time_charge_model %d\n", BAT_CONFIG_KEYWORD,
				estimate_model[1]);
	fprintf(f, "%s full_cap_fallback %d\n", BAT_CONFIG_KEYWORD,
						full_cap_fallback);
	if (!_GK.client_mode)
		fprintf(f, "%s poll_interval %d\n", BAT_CONFIG_KEYWORD, poll_interval);
	if (launch.command)
		fprintf(f, "%s launch1 %s\n", BAT_CONFIG_KEYWORD, launch.command);
	if (launch.tooltip_comment)
		fprintf(f, "%s tooltip_comment %s\n",
					BAT_CONFIG_KEYWORD, launch.tooltip_comment);
	fprintf(f, "%s alert_units_percent %d\n", BAT_CONFIG_KEYWORD,
				alert_units_percent);
	for (list = battery_list; list; list = list->next)
		{
		bat = (Battery *) list->data;
		if (bat == composite_battery)	/* Don't 2.1.16 backwards break */
			fprintf(f, "%s display_mode_composite %d %d\n", BAT_CONFIG_KEYWORD,
						bat->display_mode, bat->id);
		else
			fprintf(f, "%s display_mode %d %d\n", BAT_CONFIG_KEYWORD,
						bat->display_mode, bat->id);
		}
	gkrellm_save_alertconfig(f, bat_alert, BAT_CONFIG_KEYWORD, NULL);
	}

static void
load_battery_config(gchar *arg)
	{
	Battery			*bat;
	gint			display_mode, n = 0;
	gchar			config[32], item[CFG_BUFSIZE],
					name[CFG_BUFSIZE], item1[CFG_BUFSIZE];

	if (sscanf(arg, "%31s %[^\n]", config, item) == 2)
		{
		if (!strcmp(config, "enable"))
			sscanf(item, "%d", &enable_each);
		if (!strcmp(config, "enable_composite"))
			sscanf(item, "%d", &enable_composite);
		else if (!strcmp(config, "estimate_time"))
			sscanf(item, "%d", &enable_estimate);
		else if (!strcmp(config, "estimate_time_discharge"))
			{
			sscanf(item, "%f", &estimate_runtime[0]);
			estimate_runtime[0] /= _GK.float_factor;
			}
		else if (!strcmp(config, "estimate_time_charge"))
			{
			sscanf(item, "%f", &estimate_runtime[1]);
			estimate_runtime[1] /= _GK.float_factor;
			}
		else if (!strcmp(config, "estimate_time_charge_model"))
			sscanf(item, "%d", &estimate_model[1]);
		else if (!strcmp(config, "full_cap_fallback"))
			sscanf(item, "%d", &full_cap_fallback);
		else if (!strcmp(config, "poll_interval"))
			sscanf(item, "%d", &poll_interval);
		else if (!strcmp(config, "launch1"))
			launch.command = g_strdup(item);
		else if (!strcmp(config, "tooltip_comment"))
			launch.tooltip_comment = g_strdup(item);
		else if (!strncmp(config, "display_mode", 12))
			{
			sscanf(item, "%d %d", &display_mode, &n);
			if ((bat = battery_nth(n, FALSE)) != NULL)
				bat->display_mode = display_mode;
			}
		else if (!strcmp(config, "alert_units_percent"))
			sscanf(item, "%d", &alert_units_percent);
		else if (!strcmp(config, GKRELLM_ALERTCONFIG_KEYWORD))
			{
			if (!strncmp(item, "BAT", 3))	/* Config compat musical chairs */
				sscanf(item, "%32s %[^\n]", name, item1);
			else
				strcpy(item1, item);
			create_alert();
			gkrellm_load_alertconfig(&bat_alert, item1);
			dup_battery_alert();
			}
		}
	}


static GtkWidget	*launch_entry,
					*tooltip_entry;

static GtkWidget	*estimate_runtime_spin_button[2],
					*estimate_model_button[2];

static void
update_battery_panels(void)
	{
	GList		*list;
	Battery		*bat;

	for (list = battery_list; list; list = list->next)
		{
		bat = (Battery *) list->data;
		if (bat->enabled)
			update_battery_panel(bat);
		}
	}

static void
cb_set_alert(GtkWidget *button, Battery *bat)
	{
	create_alert();
	gkrellm_alert_config_window(&bat_alert);
	}

static void
alert_units_percent_cb(GtkToggleButton *button, gpointer data)
	{
	GList	*list;
	Battery	*bat;

	alert_units_percent = button->active;

	if (bat_alert)
		{
		for (list = battery_list; list; list = list->next)
			{
			bat = (Battery *) list->data;
			gkrellm_reset_alert(bat->alert);
			gkrellm_alert_destroy(&bat->alert);
			}
		gkrellm_alert_destroy(&bat_alert);
		gkrellm_config_message_dialog(_("GKrellM Battery"),
				_("The Battery alert units are changed\n"
				  "and the alert must be reconfigured."));
		}
	}

static void
cb_enable_estimate(GtkToggleButton *button, GtkWidget *box)
	{
	GList		*list;
	Battery		*bat;
	gboolean	enable;

	enable = button->active;
	gtk_widget_set_sensitive(box, enable);

	if (enable_estimate != enable)
		{
		/* If alert units need estimated time mode and estimation switches off,
		|  destroy the alert because the alert units can now only be percent.
		*/
		for (list = battery_list; list; list = list->next)
			{
			bat = (Battery *) list->data;
			if (bat->alert && (!enable && alert_units_need_estimate_mode))
				gkrellm_alert_destroy(&bat->alert);
			}
		if (   bat_alert
		    && (!enable && alert_units_need_estimate_mode)
		    && !alert_units_percent
		   )
			{
			gkrellm_alert_destroy(&bat_alert);
			gkrellm_config_message_dialog(_("GKrellM Battery"),
					_("The Battery alert units are changed\n"
					  "and the alert must be reconfigured."));
			}
		}
	enable_estimate = enable;
	update_battery_panels();
	}

static void
cb_runtime(GtkWidget *entry, gpointer data)
	{
	gint	i = GPOINTER_TO_INT(data) - 1;

	estimate_runtime[i] = gtk_spin_button_get_value(
			GTK_SPIN_BUTTON(estimate_runtime_spin_button[i]));
	reset_estimate = TRUE;
	update_battery_panels();
	}


static void
cb_enable(GtkToggleButton *button, gpointer data)
	{
	GList	*list;
	Battery	*bat;
	gint	which  = GPOINTER_TO_INT(data);

	if (which == 0)
		enable_composite = enable_each = button->active;
	else if (which == 1)
		enable_each = button->active;
	else if (which == 2)
		enable_composite = button->active;

	for (list = battery_list; list; list = list->next)
		{
		bat = (Battery *) list->data;
		if (bat == composite_battery)
			bat->enabled = enable_composite;
		else
			bat->enabled = enable_each;

		if (bat->enabled)
			{
			gkrellm_panel_show(bat->panel);
			update_battery_panel(bat);
			}
		else
			{
			gkrellm_reset_alert(bat->alert);
			gkrellm_panel_hide(bat->panel);
			}
		}
	if (composite_battery)
		{
		gkrellm_remove_launcher(&launch);

		if (composite_battery->enabled)
			{
			gkrellm_setup_decal_launcher(composite_battery->panel,
						&launch, composite_battery->time_decal);
			launch_battery = composite_battery;
			}
		else
			{
			bat = battery_nth(0, FALSE);
			if (bat && bat->enabled)
				{
				gkrellm_setup_decal_launcher(bat->panel,
						&launch, bat->time_decal);
				launch_battery = bat;
				}
			}

		}
	spacer_visibility();
	}

static void
cb_estimate_model(GtkWidget *entry, gpointer data)
	{
	gint	i = GPOINTER_TO_INT(data);

	estimate_model[i] = 
			GTK_TOGGLE_BUTTON(estimate_model_button[i])->active;
	reset_estimate = TRUE;
	update_battery_panels();
	}

static void
cb_poll_interval(GtkWidget *entry, GtkSpinButton *spin)
	{
	poll_interval = gtk_spin_button_get_value_as_int(spin);
	}

static void 
cb_launch_entry(GtkWidget *widget, gpointer data)
	{
	if (!launch_battery)
		return;
	gkrellm_apply_launcher(&launch_entry, &tooltip_entry,
				launch_battery->panel, &launch, gkrellm_launch_button_cb);
	}


static gchar	*battery_info_text[] =
{
N_("<h>Setup\n"),
N_("<b>Display Estimated Time\n"),
N_("If battery times are not reported by the BIOS or if the reported times\n"
"are inaccurate, select this option to display a battery time remaining or\n"
"time to charge which is calculated based on the current battery percentage\n"
"level, user supplied total battery times, and a linear extrapolation model.\n"),
"\n",
N_("<b>Total Battery Times\n"),
N_("Enter the typical total run time and total charge time in hours for your\n"
"battery.\n"),
"\n",
N_("<b>Exponential Charge Model\n"),		/* xgettext:no-c-format */
N_("For some charging systems battery capacity rises exponentially, which\n"
"means the simple linear model will grossly underestimate the time to 100%.\n"
"Select the exponential model for more accuracy in this case.\n"),
"\n",
"<b>",
N_("Alerts"),
"\n",
N_("Substitution variables may be used in alert commands.\n"),
N_("\t$p    battery percent level.\n"),
N_("\t$t    battery time left.\n"),
N_("\t$n    battery number.\n"),
N_("\t$o    online state (boolean).\n"),
N_("\t$c    charging state (boolean).\n"),
"\n",
N_("<h>Mouse Button Actions:\n"),
N_("<b>\tLeft "),
N_(" click on the charging state decal to toggle the display mode\n"
"\t\tbetween a minutes, percentage, or charging rate display.\n"),
N_("<b>\tMiddle "),
N_(" clicking anywhere on the Battery panel also toggles the display mode.\n")
};

static void
create_battery_tab(GtkWidget *tab_vbox)
	{
	GtkWidget	*tabs, *table, *vbox, *vbox1, *vbox2,
				*hbox, *hbox2, *text;
	Battery		*bat;
	gint		i;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

/* -- Setup tab */
	vbox = gkrellm_gtk_notebook_page(tabs, _("Options"));
	vbox = gkrellm_gtk_framed_vbox(vbox, NULL, 2, TRUE, 10, 6);

	if (composite_battery && n_batteries > 0)
		{
		vbox1 = gkrellm_gtk_category_vbox(vbox, _("Enable"), 2, 2, TRUE);

		gkrellm_gtk_check_button_connected(vbox1, NULL,
				enable_composite, FALSE, FALSE, 0,
				cb_enable, GINT_TO_POINTER(2),
				_("Composite Battery"));
		gkrellm_gtk_check_button_connected(vbox1, NULL,
				enable_each, FALSE, FALSE, 0,
				cb_enable, GINT_TO_POINTER(1),
				_("Real Batteries"));
		}
	else
		gkrellm_gtk_check_button_connected(vbox, NULL,
				enable_each, FALSE, FALSE, 10,
				cb_enable, GINT_TO_POINTER(0),
				_("Enable Battery"));

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, 0);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gkrellm_gtk_check_button_connected(vbox2, NULL, 
			enable_estimate, FALSE, FALSE, 2,
			cb_enable_estimate, vbox1, 
			_("Display estimated time remaining and time to charge"));
	gtk_widget_set_sensitive(vbox1, enable_estimate ? TRUE : FALSE);
	gtk_box_pack_start(GTK_BOX(vbox2), vbox1, FALSE, FALSE, 0);

	vbox1 = gkrellm_gtk_category_vbox(vbox1, NULL, 0, 0, TRUE);
	gkrellm_gtk_spin_button(vbox1, &estimate_runtime_spin_button[0], 
			estimate_runtime[0], 0.1, 24, 0.1, 1.0, 1, 55,
			cb_runtime, GINT_TO_POINTER(1), FALSE,
			_("Total battery run time in hours"));
	gkrellm_gtk_spin_button(vbox1, &estimate_runtime_spin_button[1], 
			estimate_runtime[1], 0.1, 24, 0.1, 1.0, 1, 55,
			cb_runtime, GINT_TO_POINTER(2), FALSE,
			_("Total battery charge time in hours"));

	gkrellm_gtk_check_button_connected(vbox1, &estimate_model_button[1], 
            estimate_model[1], FALSE, FALSE, 0,
			cb_estimate_model, GINT_TO_POINTER(1),
			_("Exponential charge model"));

	if (!_GK.client_mode)
		{
		hbox2 = gtk_hbox_new(FALSE, 0);
		gkrellm_gtk_spin_button(hbox2, NULL,
				(gfloat) poll_interval, 1, 3600, 1, 10, 0, 55,
				cb_poll_interval, NULL, FALSE,
				_("Seconds between updates"));
		gtk_box_pack_end(GTK_BOX(vbox), hbox2, FALSE, FALSE, 6);
		}

	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Launch Commands"),
				4, 0, TRUE);
	table = gkrellm_gtk_launcher_table_new(vbox1, 1);
	gkrellm_gtk_config_launcher(table, 0, &launch_entry, &tooltip_entry, 
					_("Battery"), &launch);
	g_signal_connect(G_OBJECT(launch_entry), "changed",
			G_CALLBACK(cb_launch_entry), NULL);
	g_signal_connect(G_OBJECT(tooltip_entry), "changed",
			G_CALLBACK(cb_launch_entry), NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 8);
	gkrellm_gtk_alert_button(hbox, NULL, FALSE, FALSE, 4, TRUE,
				cb_set_alert, NULL);
	if (battery_list)
		{
		bat = (Battery *) battery_list->data;
		if (bat && bat->time_left >= 0)		/* No choice if no battery times */
			gkrellm_gtk_check_button_connected(hbox, NULL,
					alert_units_percent, FALSE, FALSE, 16,
					alert_units_percent_cb, NULL,
					_("Alerts check for percent capacity remaining."));
		}


/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	for (i = 0; i < sizeof(battery_info_text)/sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(battery_info_text[i]));
	}

static GkrellmMonitor	monitor_battery =
	{
	N_("Battery"),			/* Name, for config tab.	*/
	MON_BATTERY,			/* Id, 0 if a plugin		*/
	create_battery,			/* The create function		*/
	update_battery,			/* The update function		*/
	create_battery_tab,		/* The config tab create function	*/
	NULL,				/* Apply the config function		*/

	save_battery_config,	/* Save user conifg			*/
	load_battery_config,	/* Load user config			*/
	BAT_CONFIG_KEYWORD,	/* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_battery_monitor(void)
	{
	estimate_runtime[0] =  1.5;	/* 1.5 hour battery */
	estimate_runtime[1] =	3.0;	/* 3 hour recharge */
	if (_GK.client_mode)
		poll_interval = 1;

	monitor_battery.name=_(monitor_battery.name);

	style_id = gkrellm_add_meter_style(&monitor_battery, BATTERY_STYLE_NAME);
	mon_battery = &monitor_battery;
	if (setup_battery_interface())
		{
		(*read_battery_data)();
		return &monitor_battery;
		}
	return NULL;
	}
