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
#include "gkrellm-sysdeps.h"
#include "inet.h"

typedef struct
	{
	GtkWidget	*vbox;
	gchar		*name;

	GkrellmChart		*chart;
	GkrellmChart		*chart_minute;
	GkrellmChart		*chart_hour;
	GkrellmChartconfig	*chart_config_minute;
	GkrellmChartconfig	*chart_config_hour;
	GkrellmPanel		*panel;
	gboolean			hour_mode;
	gint				cd_length;

	gboolean			extra_info;
	GkrellmLauncher		launch;
	GtkWidget			*launch_entry,
						*tooltip_entry;
	GtkWidget			*launch_table;

	gshort				*mark_data;		/* Draw marks if hits for any second */
	gint				mark_position,
						mark_prev_hits;

	GkrellmDecal		*list_decal;
	GkrellmDecalbutton	*list_button;
	GString				*connection_string;
	GList				*tcp_save_list;
	gboolean			busy;
	gboolean			connection_string_event;
	gboolean			config_modified;

	gchar				*label0;
	gint				active0;
	gint				prev_active0;
	gulong				hits0_minute,
						hits0_hour;
	gboolean			data0_is_range;
	gulong				port0_0,
						port0_1;

	gchar				*label1;
	gint				active1;
	gint				prev_active1;
	gulong				hits1_minute,
						hits1_hour;
	gboolean			data1_is_range;
	gulong				port1_0,
						port1_1;

	gulong				krell_hits;
	}
	InetMon;

static GkrellmMonitor *mon_inet;

static GList		*inet_mon_list;
static GList		*active_tcp_list,
					*free_tcp_list;

void 				(*read_tcp_data)();

static GtkWidget	*inet_vbox;
static GdkImage		*grid;

static gchar		*inet_data_dir;
static gchar		*text_format,
					*text_format_locale;

static gint			n_inet_monitors,
					update_interval  = 1;
static gint			style_id;



static ActiveTCP *
_tcp_alloc(void)
	{
	ActiveTCP	*tcp;

	if (free_tcp_list)
		{
		tcp = free_tcp_list->data;
		free_tcp_list = g_list_remove(free_tcp_list, tcp);
		}
	else
		tcp = g_new0(ActiveTCP, 1);
	return tcp;
	}

static ActiveTCP *
_log_active_port(ActiveTCP *tcp)
	{
	GList		*list;
	ActiveTCP	*active_tcp, *new_tcp;
	gchar		*ap, *aap;
	gint		slen;

	for (list = active_tcp_list; list; list = list->next)
		{
		active_tcp = (ActiveTCP *) (list->data);
		if (tcp->family == AF_INET)
			{
			ap = (char *)&tcp->remote_addr;
			aap = (char *)&active_tcp->remote_addr;
			slen = sizeof(struct in_addr);
			}
#if defined(INET6)
		else if (tcp->family == AF_INET6)
			{
			ap = (char *)&tcp->remote_addr6;
			aap = (char *)&active_tcp->remote_addr6;
			slen = sizeof(struct in6_addr);
			}
#endif
		else
			return 0;
		if (   memcmp(aap, ap, slen) == 0
			&& active_tcp->remote_port == tcp->remote_port
			&& active_tcp->local_port == tcp->local_port
		   )
			{
			active_tcp->state = TCP_ALIVE;
			return active_tcp;	/* Old hit still alive, not a new hit	*/
			}
		}
	tcp->state = TCP_ALIVE;
	tcp->new_hit = 1;
	new_tcp = _tcp_alloc();
	*new_tcp = *tcp;

	if (_GK.debug_level & DEBUG_INET)
		{
		ap = inet_ntoa(tcp->remote_addr);
		g_debug("inet  OO----->  %x %s:%x\n",
				tcp->local_port, ap, tcp->remote_port);
		}
	active_tcp_list = g_list_prepend(active_tcp_list, (gpointer) new_tcp);
	return new_tcp;		/* A new hit	*/
	}

void
gkrellm_inet_log_tcp_port_data(gpointer data)
	{
	GList		*list;
	InetMon		*in;
	ActiveTCP	*tcp, *active_tcp = NULL;
	gint		krell_hit;

	tcp = (ActiveTCP *) data;
	for (list = inet_mon_list; list; list = list->next)
		{
		krell_hit = 0;
		in = (InetMon *) list->data;
		if (   (!in->data0_is_range
				&& (   in->port0_0 == tcp->local_port
					|| in->port0_1 == tcp->local_port))
			|| (in->data0_is_range
				&& tcp->local_port >= in->port0_0
				&& tcp->local_port <= in->port0_1)
		   )
			{
			++in->active0;
			active_tcp = _log_active_port(tcp);
			krell_hit = active_tcp->new_hit;
			in->hits0_minute += krell_hit;
			in->hits0_hour   += krell_hit;
			}
		if (   (!in->data1_is_range
				&& (   in->port1_0 == tcp->local_port
					|| in->port1_1 == tcp->local_port))
			|| (in->data1_is_range
				&& tcp->local_port >= in->port1_0
				&& tcp->local_port <= in->port1_1)
		   )
			{
			++in->active1;
			active_tcp = _log_active_port(tcp);
			krell_hit = active_tcp->new_hit;
			in->hits1_minute += krell_hit;
			in->hits1_hour   += krell_hit;
			}
		in->krell_hits += krell_hit;
		}
	/* Defer setting new hit to 0 until here so multiple inet charts can
	|  monitor the same port number.  The above active_port will be the
	|  same if found for both data0 and data1.
	*/
	if (active_tcp)
		active_tcp->new_hit = 0;
	}


static gboolean
setup_inet_interface(void)
    {
	if (!read_tcp_data && !_GK.client_mode && gkrellm_sys_inet_init())
		{
		read_tcp_data = gkrellm_sys_inet_read_tcp_data;
		}
	return read_tcp_data ? TRUE : FALSE;
	}

void
gkrellm_inet_client_divert(void (*read_tcp_func)())
	{
	read_tcp_data = read_tcp_func;
	}


/* ======================================================================== */

#define DUMP_IN(in,tag)	g_debug("%s: %s %s %ld %ld %ld %ld\n", \
						tag, in->label0, in->label1, \
            			in->port0_0, in->port0_1, in->port1_0, in->port1_1);

static void
format_chart_text(InetMon *in, gchar *src_string, gchar *buf, gint size)
	{
	GList			*list;
	GkrellmChart	*cp;
	GkrellmChartdata *cd;
	gchar			c, *s, *s1;
	gint			len, value, n, i, w;

	if (!buf || size < 1)
		return;
	--size;
	*buf = '\0';
	if (!src_string)
		return;
	cp = in->chart;
	w = gkrellm_chart_width();
	for (s = text_format_locale; *s != '\0' && size > 0; ++s)
		{
		len = 1;
		if (*s == '$' && *(s + 1) != '\0')
			{
			value = -1;
			s1 = " ";
			if ((c = *(s + 1)) == 'M')
				value = gkrellm_get_chart_scalemax(cp);
			else if (c == 'a' && *in->label0)
				value = in->active0;
			else if (c == 'l' && *in->label0)
				s1 = in->label0;
			else if (c == 'c' && *(s + 2))
				{
				i = 0;
				sscanf(s + 2, "%d%n", &n, &i);
				s += i;
				--n;
				if (n > w || n < 0)
					n = w;
				list = in->hour_mode ?
						in->chart_hour->cd_list : in->chart_minute->cd_list;
				cd = (GkrellmChartdata *) list->data;
				if (*in->label0)
					{
					value = in->hour_mode ? in->hits0_hour : in->hits0_minute;
					for ( ; n > 0; --n)
						value += gkrellm_get_chartdata_data(cd, w - n);
					}
				}
			else if (c == 'A' && *in->label1)
				value = in->active1;
			else if (c == 'L' && *in->label1)
				s1 = in->label1;
			else if (c == 'C' && *(s + 2))
				{
				i = 0;
				sscanf(s + 2, "%d%n", &n, &i);
				s += i;
				--n;
				if (n > w || n < 0)
					n = w;
				list = in->hour_mode ?
						in->chart_hour->cd_list : in->chart_minute->cd_list;
				if (list->next)
					list = list->next;
				cd = (GkrellmChartdata *) list->data;
				if (*in->label1)
					{
					value = in->hour_mode ? in->hits1_hour : in->hits1_minute;
					for ( ; n > 0; --n)
						value += gkrellm_get_chartdata_data(cd, w - n);
					}
				}
			else if (c == 'H')
				s1 = gkrellm_sys_get_host_name();
			if (value >= 0)
				len = snprintf(buf, size, "%d", value);
			else
				len = snprintf(buf, size, "%s", s1);
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
draw_inet_extra(InetMon *in)
	{
	gchar	buf[128];

	if (!in->extra_info)
		return;
	format_chart_text(in, text_format_locale, buf, sizeof(buf));
	gkrellm_draw_chart_text(in->chart, style_id, buf);
	}

  /* Use the reserved area below the main chart to draw marks if any
  |  hits in second intervals.
  */
static void
draw_inet_mark_data(InetMon *in, gint minute_mark)
	{
	GkrellmChart	*cp;
	GdkGC			*gc1, *gc2, *gc3;
	gint			hits, x, y, n;

	cp = in->chart;
	in->mark_position = (in->mark_position + 1) % cp->w;
	if (minute_mark)
		{
		in->mark_data[in->mark_position] = -1;	/* minute flag in the data */
		return;
		}
	hits = in->hits0_minute + in->hits1_minute;
	in->mark_data[in->mark_position] = hits - in->mark_prev_hits;
	in->mark_prev_hits = hits;

	gc1 = gkrellm_draw_GC(1);
	gc2 = gkrellm_draw_GC(2);
	gc3 = gkrellm_draw_GC(3);

	/* Clear out the area and redraw the marks.
	*/
	y = cp->h - cp->y;
	gdk_draw_drawable(cp->pixmap, gc1, cp->bg_src_pixmap,
			0, y,  0, y,  cp->w, cp->y);
	gdk_gc_set_foreground(gc1, gkrellm_out_color());
	gdk_gc_set_foreground(gc2, gkrellm_in_color());
	gdk_gc_set_foreground(gc3, gkrellm_white_color());
	for (n = 0; n < cp->w; ++n)
		{
		x = (in->mark_position + n + 1) % cp->w;
		if (in->mark_data[x] > 0)
			gdk_draw_line(cp->pixmap, gc1,
						cp->x + n, cp->h - 1, cp->x + n, y);
		else if (in->mark_data[x] == -1)	/* Minute tick	*/
			gdk_draw_line(cp->pixmap, gc3,
						cp->x + n, cp->h - 1, cp->x + n, y);
		}
	gdk_draw_drawable(cp->drawing_area->window, gc1, cp->pixmap,
			0, y,  0, y,  cp->w, cp->y);
	}

static void
draw_inet_chart(InetMon *in)
	{
	struct tm			tm;
	GdkGC				*gc3;
	GkrellmChart		*cp;
	GkrellmTextstyle	*ts;
	GdkColor			tmp_color;
	gchar				buf[32];
	guint32				pixel0, pixel1;
	gint				y0, h4, w, h, n;

	cp = in->chart;
	gkrellm_draw_chartdata(cp);

	y0 = cp->h - cp->y;
	h4 = y0 / 4;
	gdk_drawable_get_size(in->chart->bg_grid_pixmap, &w, &h);
	if (grid == NULL)
		grid = gdk_drawable_get_image(in->chart->bg_grid_pixmap, 0, 0, w, h);
	ts = gkrellm_chart_alt_textstyle(style_id);

	tm = *gkrellm_get_current_time();
	gc3 = gkrellm_draw_GC(3);
	gdk_gc_set_foreground(gkrellm_draw_GC(3), gkrellm_white_color());
	if (in->hour_mode)
		{
		for (n = cp->w - 1; n >= 0; --n)
			{
			/* When hour ticked to 0, 23rd hour data was stored and a slot
			|  was skipped.
			*/
			if (tm.tm_hour == 0)	/* Draw day mark at midnight	*/
				{
				pixel0 = gdk_image_get_pixel(grid, cp->x + n, 0);
				tmp_color.pixel = pixel0;
				gdk_gc_set_foreground(gc3, &tmp_color);
				gdk_draw_line(cp->pixmap, gc3,
						cp->x + n - 1, y0 - 3, cp->x + n - 1, 3);
				if (h > 1)
					{
					pixel1 = gdk_image_get_pixel(grid, cp->x + n, 1);
					tmp_color.pixel = pixel1;
					gdk_gc_set_foreground(gc3, &tmp_color);
					gdk_draw_line(cp->pixmap, gc3,
							cp->x + n, y0 - 3, cp->x + n, 3);
					}
				}
			if (in->extra_info && tm.tm_hour == 1 && n < cp->w - 5)
				{
				strftime(buf, sizeof(buf), "%a", &tm);
				buf[1] = '\0';
				gkrellm_draw_chart_label(in->chart, ts,
						cp->x + n, in->chart->h - 4, buf);
				}
			if (--tm.tm_hour < 0)
				{
				tm.tm_hour = 24;		/* Extra hour for skipped slot	*/
				if (--tm.tm_wday < 0)
					tm.tm_wday = 6;
				}
			}
		}
	else
		{
		for (n = cp->w - 1; n >= 0; --n)
			{
			/* When minute ticked to 0, 59 minute data was stored and a slot
			|  was skipped.
			*/
			if (tm.tm_min == 0)		/* Draw hour mark	*/
				{
				pixel0 = gdk_image_get_pixel(grid, cp->x + n, 0);
				tmp_color.pixel = pixel0;
				gdk_gc_set_foreground(gc3, &tmp_color);
				gdk_draw_line(cp->pixmap, gc3,
						cp->x + n - 1, y0 - 3, cp->x + n - 1, y0 - h4);
				if (h > 1)
					{
					pixel1 = gdk_image_get_pixel(grid, cp->x + n, 1);
					tmp_color.pixel = pixel1;
					gdk_gc_set_foreground(gc3, &tmp_color);
					gdk_draw_line(cp->pixmap, gc3,
							cp->x + n, y0 - 3, cp->x + n, y0 - h4);
					}
				}
			if (--tm.tm_min < 0)
				tm.tm_min = 60;		/* extra minute for skipped slot */
			}
		}
	if (in->extra_info)
		draw_inet_extra(in);
	gkrellm_draw_chart_to_screen(cp);
	in->prev_active0 = in->active0;
	in->prev_active1 = in->active1;
	}

static void
select_hour_or_minute_chart(InetMon *in)
	{
	gkrellm_freeze_side_frame_packing();
	if (in->hour_mode && in->chart == in->chart_minute)
		{
		gkrellm_chart_hide(in->chart_minute, FALSE);
		gkrellm_chart_show(in->chart_hour, FALSE);
		in->chart = in->chart_hour;
		gkrellm_chartconfig_window_destroy(in->chart_minute);
		}
	else if (!in->hour_mode && in->chart == in->chart_hour)
		{
		gkrellm_chart_hide(in->chart_hour, FALSE);
		gkrellm_chart_show(in->chart_minute, FALSE);
		in->chart = in->chart_minute;
		gkrellm_chartconfig_window_destroy(in->chart_hour);
		}
	gkrellm_thaw_side_frame_packing();
	}

static void
update_inet(void)
	{
	InetMon			*in;
	ActiveTCP		*tcp;
	GList			*list;
	gchar			buf[32], *ap;
	gint			i;
	static gint		check_tcp;

	if (!inet_mon_list)
		return;

	if (GK.second_tick && check_tcp == 0)
		{
		for (list = inet_mon_list; list; list = list->next)
			{
			in = (InetMon *) list->data;
			in->active0 = 0;
			in->active1 = 0;
			}
		/* Assume all connections are dead, then read_tcp_data() will set
		|  still alive ones back to alive.  Then I can prune really dead ones.
		*/
		for (list = active_tcp_list; list; list = list->next)
			{
			tcp = (ActiveTCP *)(list->data);
			tcp->state = TCP_DEAD;
			}

		(*read_tcp_data)();

		for (list = active_tcp_list; list; )
			{
			tcp = (ActiveTCP *)(list->data);
			if (tcp->state == TCP_DEAD)
				{
				if (list == active_tcp_list)
					active_tcp_list = active_tcp_list->next;
				list = g_list_remove(list, tcp);
				if (_GK.debug_level & DEBUG_INET)
					{
					ap = inet_ntoa(tcp->remote_addr);
					g_debug("inet  XX----->  %x %s:%x\n",
							tcp->local_port, ap, tcp->remote_port);
					}
				free_tcp_list = g_list_prepend(free_tcp_list, tcp);
				}
			else
				list = list->next;
			}
		}
	if (GK.second_tick)
		check_tcp = (check_tcp + 1) % update_interval;

	for (list = inet_mon_list; list; list = list->next)
		{
		in = (InetMon *) list->data;
		if (GK.hour_tick)
			{
			if (!*in->label0)
				in->hits0_hour = in->hits1_hour;
			gkrellm_store_chartdata(in->chart_hour, 0,
					in->hits0_hour, in->hits1_hour);
			in->hits0_hour = in->hits1_hour = 0;
			if (GK.day_tick)	/* Make room for vertical day grid */
				{
				gkrellm_store_chartdata(in->chart_hour, 0, 0, 0);
				gkrellm_store_chartdata(in->chart_hour, 0, 0, 0);
				}
			}
		if (GK.minute_tick)
			{
			if (!*in->label0)
				in->hits0_minute = in->hits1_minute;
			gkrellm_store_chartdata(in->chart_minute, 0,
					in->hits0_minute, in->hits1_minute);
			in->hits0_minute = in->hits1_minute = 0;
			if (GK.hour_tick)	/* Make room for vertical hour grid */
				{
				gkrellm_store_chartdata(in->chart_minute, 0, 0, 0);
				gkrellm_store_chartdata(in->chart_minute, 0, 0, 0);
				}
			gkrellm_refresh_chart(in->chart);
			draw_inet_mark_data(in, 1);
			}
		else if (   GK.second_tick
				&& (   in->prev_active0 != in->active0
					|| in->prev_active1 != in->active1
				   )
				)
				draw_inet_chart(in);	/* Just to update extra info draw */

		if (GK.second_tick)
			draw_inet_mark_data(in, 0);

		if (in->busy && in->list_button->cur_index == D_MISC_BUTTON_OUT)
			i = D_MISC_BUTTON_ON;
		else
			i = D_MISC_BUTTON_OUT;
		gkrellm_set_decal_button_index(in->list_button, i);

		gkrellm_update_krell(in->panel, KRELL(in->panel), in->krell_hits);
		gkrellm_draw_panel_layers(in->panel);

		if (in->connection_string_event)
			{
			snprintf(buf, sizeof(buf), _("%s Connections"), in->name);
			gkrellm_message_dialog(buf, in->connection_string->str);
			in->connection_string_event = FALSE;
			in->busy = FALSE;
			}
		}
	}

static gboolean
tcp_port_is_monitored(ActiveTCP *tcp, gboolean range, gulong p0, gulong p1)
	{
	if (   (!range && (p0 == tcp->local_port || p1 == tcp->local_port))
		|| ( range && tcp->local_port >= p0 && tcp->local_port <= p1)
	   )
		return TRUE;
	return FALSE;
	}

static gpointer
get_connection_string_thread(void *data)
	{
	InetMon					*in	= (InetMon *) data;
	GList					*list;
	ActiveTCP				*tcp;
#if defined(INET6)
	union {
	    struct sockaddr_storage	ss;
	    struct sockaddr_in		sin;
	    struct sockaddr_in6	sin6;
	    struct sockaddr		sa;
	} ss;
	gint					salen, flag = 0;
	gchar					hbuf[NI_MAXHOST];
	gchar					buf[NI_MAXHOST + 10];
#else
	struct hostent			*hostent;
	gchar					buf[64];
#endif
	gchar					*remote_host, *udp_note;

	if (in->connection_string)
		in->connection_string = g_string_truncate(in->connection_string, 0);
	else
		in->connection_string = g_string_new("");
	for (list = in->tcp_save_list; list; list = list->next)
		{
		tcp = (ActiveTCP *) list->data;
#if defined(INET6)
		memset(&ss.ss, 0, sizeof(ss.ss));
		switch (tcp->family)
			{
			case AF_INET:
				salen = sizeof(struct sockaddr_in);
				memcpy(&ss.sin.sin_addr, &tcp->remote_addr, salen);
#if defined(SIN6_LEN)
				ss.sin.sin_len = salen;
#endif
				ss.sin.sin_family = tcp->family;
				break;
			case AF_INET6:
				salen = sizeof(struct sockaddr_in6);
				memcpy(&ss.sin6.sin6_addr, &tcp->remote_addr6, salen);
#if defined(SIN6_LEN)
				ss.sin6.sin6_len = salen;
#endif
				ss.sin6.sin6_family = tcp->family;
				/* XXX: We should mention about
				|  scope, too. */
				break;
			default:
				continue;
			}
		if (getnameinfo(&ss.sa, salen,
				hbuf, sizeof(hbuf), NULL, 0, flag))
			continue;
		remote_host = hbuf;
#else
		hostent = gethostbyaddr((char *) &tcp->remote_addr,
					sizeof(struct in_addr), AF_INET);
		if (hostent)
			remote_host = hostent->h_name;
		else
			remote_host = inet_ntoa(tcp->remote_addr);
#endif
		udp_note = tcp->is_udp ? " (UDP)" : "";
		snprintf(buf, sizeof(buf), "%6d:  %s%s\n",
				tcp->local_port, remote_host, udp_note);

		g_string_append(in->connection_string, buf);
		}
	if (in->connection_string->len == 0)
		g_string_append(in->connection_string, _("No current connections."));
	in->connection_string_event = TRUE;
	gkrellm_free_glist_and_data(&in->tcp_save_list);
	return NULL;
	}

static void
cb_list_button(GkrellmDecalbutton *button)
    {
	InetMon			*in	= (InetMon *) button->data;
	GList			*list;
	ActiveTCP		*tcp, *tcp_save;

	if (in->busy)
		return;
	in->busy = TRUE;

	/* Save a snapshot of active connections so I don't have to worry about
	|  the active_tcp_list changing while in the thread.
	*/
	for (list = active_tcp_list; list; list = list->next)
		{
		tcp = (ActiveTCP *) list->data;
		if (   tcp_port_is_monitored(tcp, in->data0_is_range,
						in->port0_0, in->port0_1)
			|| tcp_port_is_monitored(tcp, in->data1_is_range,
						in->port1_0, in->port1_1)
		   )
			{
			tcp_save = g_new0(ActiveTCP, 1);
			*tcp_save = *tcp;
			in->tcp_save_list = g_list_append(in->tcp_save_list, tcp_save);
			}
		}
	g_thread_new("get_connection_string", get_connection_string_thread, in);
	}

static gint
inet_expose_event(GtkWidget *widget, GdkEventExpose *ev)
	{
	InetMon		*in;
	GList		*list;
	GdkPixmap	*pixmap = NULL;

	for (list = inet_mon_list; list; list = list->next)
		{
		in = (InetMon *) list->data;
		if (widget == in->panel->drawing_area)
			pixmap = in->panel->pixmap;
		else if (widget == in->chart_minute->drawing_area)
			pixmap = in->chart_minute->pixmap;
		else if (widget == in->chart_hour->drawing_area)
			pixmap = in->chart_hour->pixmap;
		if (pixmap)
			{
			gdk_draw_drawable(widget->window, gkrellm_draw_GC(1), pixmap,
					ev->area.x, ev->area.y, ev->area.x, ev->area.y,
					ev->area.width, ev->area.height);
			break;
			}
		}
	return FALSE;
	}

static gint
cb_inet_extra(GtkWidget *widget, GdkEventButton *ev)
	{
	InetMon		*in;
	GList		*list;

	for (list = inet_mon_list; list; list = list->next)
		{
		in = (InetMon *) list->data;
		if (widget != in->chart->drawing_area)
			continue;
		if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS)
			{
			in->extra_info = !in->extra_info;
			gkrellm_refresh_chart(in->chart);
			gkrellm_config_modified();
			}
		else if (ev->button == 2)
			{
			in->hour_mode = !in->hour_mode;
			select_hour_or_minute_chart(in);
			gkrellm_rescale_chart(in->chart);
			}
		else if (   ev->button == 3
				 || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS)
				)
			gkrellm_chartconfig_window_create(in->chart);
		break;
		}
	return FALSE;
	}

static gint
cb_panel_press(GtkWidget *widget, GdkEventButton *ev)
	{
	if (ev->button == 3)
		gkrellm_open_config_window(mon_inet);
	return FALSE;
	}

  /* Lock the hour and minute heights together.
  */
static void
cb_inet_height(GkrellmChartconfig *cf, InetMon *in)
	{
	gint	h;

	h = gkrellm_get_chartconfig_height(cf);
	if (in->chart_minute->h != h)
		gkrellm_set_chart_height(in->chart_minute, h);
	if (in->chart_hour->h != h)
		gkrellm_set_chart_height(in->chart_hour, h);
	}

static void
destroy_inet_monitor(InetMon *in)
	{
	if (in->launch_table)
		gtk_widget_destroy(in->launch_table);
	g_free(in->name);
	g_free(in->label0);
	g_free(in->label1);
	if (in->launch.command)
		g_free(in->launch.command);
	if (in->launch.button)
		gkrellm_destroy_button(in->launch.button);
	g_free(in->mark_data);

	/* The panel doesn't live in the chart struct, so destroy it separately
	*/
	gkrellm_panel_destroy(in->panel);

	gkrellm_chartconfig_destroy(&in->chart_config_minute);
	gkrellm_chart_destroy(in->chart_minute);

	gkrellm_chartconfig_destroy(&in->chart_config_hour);
	gkrellm_chart_destroy(in->chart_hour);

	gtk_widget_destroy(in->vbox);
	g_free(in);
	}


#define	MIN_GRID_RES		2
#define	MAX_GRID_RES		1000000
#define DEFAULT_GRID_RES	10

static void
chart_create(InetMon *in, GkrellmChart *cp, GkrellmChartconfig **cfp,
				gint first_create)
	{
	GkrellmChartconfig	*cf;
	GkrellmChartdata	*cd;
	GdkPixmap			**src_pixmap, *grid_pixmap;

	cp->y = 3;
	gkrellm_chart_create(in->vbox, mon_inet, cp, cfp);
	cf = *cfp;

	/* I accumulate tcp hits myself, so I'm free to make the chartdata
	|  accumulate monotonically or not.  I choose not monotonic to make saving
	|  and loading data simpler.
	*/
	src_pixmap = gkrellm_data_out_pixmap();
	grid_pixmap = gkrellm_data_out_grid_pixmap();
	if (*in->label0)
		{
		cd = gkrellm_add_chartdata(cp, src_pixmap, grid_pixmap, in->label0);
		gkrellm_monotonic_chartdata(cd, FALSE);
		}
	src_pixmap = gkrellm_data_in_pixmap();
	grid_pixmap = gkrellm_data_in_grid_pixmap();
	if (*in->label1)
		{
		cd = gkrellm_add_chartdata(cp, src_pixmap, grid_pixmap, in->label1);
		gkrellm_monotonic_chartdata(cd, FALSE);
		}
	gkrellm_set_draw_chart_function(cp, draw_inet_chart, in);

	/* krell is not function of chart grids or resolution, so no interest
	|  in connecting to grid or resolution changes.
	*/
	gkrellm_chartconfig_height_connect(cf, cb_inet_height, in);
	gkrellm_chartconfig_grid_resolution_adjustment(cf, TRUE,
			0, (gfloat) MIN_GRID_RES, (gfloat) MAX_GRID_RES, 0, 0, 0, 70);
	if (gkrellm_get_chartconfig_grid_resolution(cf) < MIN_GRID_RES)
		gkrellm_set_chartconfig_grid_resolution(cf, DEFAULT_GRID_RES);

	/* Don't want to waste an hour priming the pump, and don't need to
	|  because data always starts at zero.
	*/
	cp->primed = TRUE;		/* XXX */
	gkrellm_alloc_chartdata(cp);

	if (first_create)
		{
		g_signal_connect(G_OBJECT (cp->drawing_area), "expose_event",
				G_CALLBACK(inet_expose_event), NULL);
		g_signal_connect(G_OBJECT(cp->drawing_area),"button_press_event",
				G_CALLBACK(cb_inet_extra), NULL);
		}
	}

static void
create_inet_monitor(GtkWidget *vbox1, InetMon *in, gint first_create)
	{
	GtkWidget		*vbox;
	GkrellmChart	*cp;
	GkrellmPanel	*p;
	GkrellmMargin	*m;
	GkrellmStyle	*style;
	gint			x;

	if (first_create)
		{
		vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(vbox1), vbox);
		in->vbox = vbox;
		in->chart_minute = gkrellm_chart_new0();
		in->chart_hour = gkrellm_chart_new0();
		in->panel = gkrellm_panel_new0();
		in->chart = in->chart_minute;
		in->name = g_strdup_printf(_("inet%d"), n_inet_monitors++);
		}
	else
		{
		vbox = in->vbox;
		gkrellm_destroy_decal_list(in->panel);
		gkrellm_destroy_krell_list(in->panel);
		}
	if (in->chart_config_hour && in->chart_config_minute)
		in->chart_config_hour->h = in->chart_config_minute->h;
	chart_create(in, in->chart_minute, &in->chart_config_minute, first_create);
	gkrellm_chartconfig_grid_resolution_label(in->chart_config_minute,
			_("TCP hits per minute"));
	chart_create(in, in->chart_hour, &in->chart_config_hour, first_create);
	gkrellm_chartconfig_grid_resolution_label(in->chart_config_hour,
			_("TCP hits per hour"));
	cp = in->chart;

	p = in->panel;
	style = gkrellm_panel_style(style_id);
	m = gkrellm_get_style_margins(style);
	if (style->label_position == 50 && gkrellm_chart_width() < 80)
		style->label_position = 40;		/* Not a kludge, an adjustment! */
	in->list_decal = gkrellm_create_decal_pixmap(p,
				gkrellm_decal_misc_pixmap(), gkrellm_decal_misc_mask(),
				N_MISC_DECALS, style, -1, -1);
	if (style->label_position <= 50)
		x = gkrellm_chart_width() - in->list_decal->w - m->right;
	else
		x = m->left;
	gkrellm_move_decal(p, in->list_decal, x, in->list_decal->y);

	gkrellm_create_krell(p, gkrellm_krell_panel_piximage(style_id), style);

	/* Inet krells are not related to chart scale_max.  Just give a constant
	|  full scale of 5.
	*/
	KRELL(p)->full_scale = 5;
	gkrellm_panel_configure(p, in->name, style);
	gkrellm_panel_create(vbox, mon_inet, p);

	/* At first_create both charts will be visible, but this will be
	|  undone below
	*/
	in->list_button = gkrellm_make_decal_button(p, in->list_decal,
			cb_list_button, in, D_MISC_BUTTON_OUT, D_MISC_BUTTON_IN);

	if (first_create)
		{
		g_signal_connect(G_OBJECT(p->drawing_area),"expose_event",
				G_CALLBACK(inet_expose_event), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area), "button_press_event",
				G_CALLBACK(cb_panel_press), NULL);

		gtk_widget_show(vbox);
		gkrellm_chart_hide(in->chart_hour, FALSE);
		}
	gkrellm_setup_launcher(p, &in->launch, CHART_PANEL_TYPE, 4);

	if (in->mark_data)
		g_free(in->mark_data);
	in->mark_data = g_new0(gshort, cp->w);

	if (! first_create)
		gkrellm_rescale_chart(in->chart);
	}

static void
create_inet(GtkWidget *vbox, gint first_create)
	{
	GList		*list;
	gint		new_data	= FALSE;
	gint		i;
	static gint	last_chart_width;

	inet_vbox = vbox;
	if (grid)
		{
		g_object_unref(G_OBJECT(grid));
		grid = NULL;
		}
	n_inet_monitors = 0;
	if (!first_create && last_chart_width != gkrellm_chart_width())
		{  /* Will be allocating new data arrays */
		gkrellm_inet_save_data();
		new_data = TRUE;
		last_chart_width = gkrellm_chart_width();
		}
	for (i = 0, list = inet_mon_list; list; ++i, list = list->next)
		create_inet_monitor(inet_vbox, (InetMon *)list->data, first_create);
	if (first_create || new_data)
		gkrellm_inet_load_data();
	if (inet_mon_list)
		gkrellm_spacers_show(mon_inet);
	else
		gkrellm_spacers_hide(mon_inet);
	}

static InetMon   *
lookup_inet(gchar *name)
	{
	InetMon	*in;
	GList	*list;

	for (list = inet_mon_list; list; list = list->next)
		{
		in = (InetMon *) list->data;
		if (name && in->name && !strcmp(in->name, name))
			return in;
		}
	return NULL;
	}

/* --------------------------------------------------------------------- */
#define	INET_CONFIG_KEYWORD		"inet"

static void
save_inet_config(FILE *f)
	{
	GList		*list;
	InetMon		*in;
	gchar		buf[128];
	gchar		*l0, *l1;
	gint		i;

	for (i = 0, list = inet_mon_list; list; list = list->next, ++i)
		{
		in = (InetMon *) list->data;
		l0 = (*in->label0) ? in->label0: "NONE";
		l1 = (*in->label1) ? in->label1: "NONE";
		fprintf(f, "%s monitor %s %s %lu %lu %s %lu %lu %d %d %d\n",
				INET_CONFIG_KEYWORD, in->name,
				l0, in->port0_0, in->port0_1,
				l1, in->port1_0, in->port1_1,
				in->extra_info, in->data0_is_range, in->data1_is_range);
		snprintf(buf, sizeof(buf), "%s:minute", in->name);
		gkrellm_save_chartconfig(f, in->chart_config_minute,
				INET_CONFIG_KEYWORD, buf);
		snprintf(buf, sizeof(buf), "%s:hour", in->name);
		gkrellm_save_chartconfig(f, in->chart_config_hour,
				INET_CONFIG_KEYWORD, buf);
		if (in->launch.command)
			fprintf(f, "%s launch %s %s\n", INET_CONFIG_KEYWORD,
						in->name, in->launch.command);
		if (in->launch.tooltip_comment)
			fprintf(f, "%s tooltip %s %s\n", INET_CONFIG_KEYWORD,
						in->name, in->launch.tooltip_comment);
		}
	fprintf(f, "%s text_format all %s\n", INET_CONFIG_KEYWORD, text_format);
	if (!_GK.client_mode)
		fprintf(f, "%s update_interval all %d\n",
					INET_CONFIG_KEYWORD, update_interval);
	}

static gint
fix_ports(InetMon *in)
	{
	gint	cd_length = 2;
	gulong	tmp;

	if (!*in->label0)
		{
		in->port0_1 = 0;
		in->data0_is_range = 0;
		--cd_length;
		}
	if (!*in->label1)
		{
		in->port1_1 = 0;
		in->data1_is_range = 0;
		--cd_length;
		}
	if (in->data0_is_range && (in->port0_1 < in->port0_0))
		{
		tmp = in->port0_1;
		in->port0_1 = in->port0_0;
		in->port0_0 = tmp;
		}
	if (in->data1_is_range && (in->port1_1 < in->port1_0))
		{
		tmp = in->port1_1;
		in->port1_1 = in->port1_0;
		in->port1_0 = tmp;
		}
	return cd_length;
	}

static void
load_inet_config(gchar *arg)
	{
	InetMon		*in;
	gchar		config[32], name[32];
	gchar		item[CFG_BUFSIZE];
	gchar		label0[16], label1[16];
	gchar		*hr_min;
	gint		n;

	if ((n = sscanf(arg, "%31s %31s %[^\n]", config, name, item)) != 3)
		return;
	hr_min = strrchr(name, (gint) ':');
	if (hr_min)
		*hr_min++ = '\0';
	if (!strcmp(config, "text_format"))
		{
		gkrellm_locale_dup_string(&text_format, item, &text_format_locale);
		return;
		}
	else if (!strcmp(config, "update_interval"))
		{
		sscanf(item, "%d", &update_interval);
		if (update_interval < 1)
			update_interval = 1;
		}
	else if (!strcmp(config, "monitor"))
		{
		in = g_new0(InetMon, 1);
		label0[0] = '\0';
		label1[0] = '\0';
		sscanf(item, "%15s %lu %lu %15s %lu %lu %d %d %d",
				label0, &in->port0_0, &in->port0_1,
				label1, &in->port1_0, &in->port1_1,
				&in->extra_info, &in->data0_is_range, &in->data1_is_range);
		if (!strcmp(label0, "NONE"))
			label0[0] = '\0';
		if (!strcmp(label1, "NONE"))
			label1[0] = '\0';
		in->label0 = g_strdup(label0);
		in->label1 = g_strdup(label1);
		in->cd_length = fix_ports(in);
		if (in->cd_length > 0)
			{
			in->name = g_strdup(name);
			in->chart_config_minute = gkrellm_chartconfig_new0();
			in->chart_config_hour = gkrellm_chartconfig_new0();
			inet_mon_list = g_list_append(inet_mon_list, in);
			}
		else	/* Bogus config line */
			{
			g_free(in->label0);
			g_free(in->label1);
			g_free(in);
			}
		return;
		}
	if ((in = lookup_inet(name)) == NULL)
		return;
	if (!strcmp(config, GKRELLM_CHARTCONFIG_KEYWORD))
		{
		if (hr_min && !strcmp(hr_min, "hour"))
			gkrellm_load_chartconfig(&in->chart_config_hour, item,
					in->cd_length);
		if (hr_min && !strcmp(hr_min, "minute"))
			gkrellm_load_chartconfig(&in->chart_config_minute, item,
					in->cd_length);
		}
	else if (!strcmp(config, "launch"))
		gkrellm_dup_string(&in->launch.command, item);
	else if (!strcmp(config, "tooltip"))
		gkrellm_dup_string(&in->launch.tooltip_comment, item);
	}


/* --------------------------------------------------------------------- */

  /* Read saved inet data (from a previous gkrellm process).  Return the
  |  number of missing data slots (skew).
  */
static gint
read_inet_data(GkrellmChart *cp, FILE *f, gint minute_chart,
			gint min, gint hour, gint yday, gint width)
	{
	struct tm	*tm;
	gchar		data[64];
	gint		n, in, out, cur_slot, skew, day;

	tm = gkrellm_get_current_time();
	day = tm->tm_yday - yday;

	/* Check for new years wrap around. I don't handle leap year here, will
	|  get some email, then be safe for four more years...
	*/
	if (day < 0)
		day = tm->tm_yday + ((yday < 365) ? 365 - yday : 0);

	cur_slot = day * 24 + tm->tm_hour;
	n = hour;
	if (minute_chart)
		{
		cur_slot = cur_slot * 60 + tm->tm_min;
		n = n * 60 + min;
		}
	skew = cur_slot - n;

	gkrellm_reset_chart(cp);
	for (n = 0; n < width; ++n)
		{
		if (fgets(data, sizeof(data), f) == NULL)
			break;

		if (skew >= cp->w)	/* All stored data is off the chart	*/
			continue;

		/* Use chart data storing routines to load in data so I don't care
		|  if current chart width is less or greater than stored data width.
		|  Charts will circular buff fill until data runs out.
		*/
		out = in = 0;
		sscanf(data, "%d %d", &out, &in);
		gkrellm_store_chartdata(cp, 0, out, in);
		}
	/* Need to store zero data for time slots not in read data to bring
	|  the chart up to date wrt current time.  As in update_inet() I need
	|  to skip slots for hour or minute ticks.
	|  Warning: skew can be negative if quit gkrellm, change system clock
	|  to earlier time, then restart gkrellm.
	*/
	if ((n = skew) < cp->w)		/* Do this only if some data was stored  */
		{
		while (n-- > 0)
			{
			gkrellm_store_chartdata(cp, 0, 0, 0);
			if (minute_chart && min++ == 0)
				{
				gkrellm_store_chartdata(cp, 0, 0, 0);
				gkrellm_store_chartdata(cp, 0, 0, 0);
				if (min == 60)
					min = 0;
				}
			else if (!minute_chart && hour++ == 0)
				{
				gkrellm_store_chartdata(cp, 0, 0, 0);
				gkrellm_store_chartdata(cp, 0, 0, 0);
				if (hour == 24)
					hour = 0;
				}
			}
		}
	return skew;
	}

static void
write_inet_data(GkrellmChart *cp, FILE *f)
	{
	GList	*list;
	gint	n;

	for (n = 0; n < cp->w; ++n)
		{
		for (list = cp->cd_list; list; list = list->next)
			fprintf(f, "%d ",
			   gkrellm_get_chartdata_data((GkrellmChartdata *) list->data, n));
		fprintf(f, "\n");
		}
	}

static gchar *
make_inet_data_fname(InetMon *in)
	{
	static gchar	idata_fname[256];
	gchar			c_sep0, c_sep1;

	c_sep0 = in->data0_is_range ? '-': '_';
	c_sep1 = in->data1_is_range ? '-': '_';
	snprintf(idata_fname, sizeof(idata_fname), "%s%cinet_%ld%c%ld_%ld%c%ld",
		inet_data_dir, G_DIR_SEPARATOR,
		in->port0_0, c_sep0, in->port0_1, in->port1_0, c_sep1, in->port1_1);
	return idata_fname;
	}

void
gkrellm_inet_save_data(void)
	{
	FILE		*f;
	struct tm	*tm;
	GList		*list;
	InetMon		*in;
	gchar		*fname, buf[64];

	tm = gkrellm_get_current_time();
	for (list = inet_mon_list; list; list = list->next)
		{
		in = (InetMon *) list->data;
		fname = make_inet_data_fname(in);
		if ((f = g_fopen(fname, "w")) == NULL)
			continue;

		fputs("minute hour yday width\n", f);
		snprintf(buf, sizeof(buf), "%d %d %d %d\n", tm->tm_min,
				tm->tm_hour, tm->tm_yday, in->chart->w);
		fputs(buf, f);

		/* Save any accumulated hits which have not been stored into the
		|  chart data array, and then save the chart data.
		*/
		fputs("hits0_minute hits1_minute hits0_hour hits1_hour\n", f);
		fprintf(f, "%ld %ld %ld %ld\n",
					in->hits0_minute, in->hits1_minute,
					in->hits0_hour, in->hits1_hour);
		write_inet_data(in->chart_minute, f);
		write_inet_data(in->chart_hour, f);
		fclose(f);
		}
	}

void
gkrellm_inet_load_data(void)
	{
	FILE		*f;
	GList		*list;
	InetMon		*in;
	gchar		buf[96], *fname;
	gint		min, hour, yday, len, skew;

	for (list = inet_mon_list; list; list = list->next)
		{
		in = (InetMon *) list->data;
		fname = make_inet_data_fname(in);
		if ((f = g_fopen(fname, "r")) == NULL)
			{
			gkrellm_reset_chart(in->chart);
			draw_inet_chart(in);
			continue;
			}
		fgets(buf, sizeof(buf), f);		/* Comment line */
		fgets(buf, sizeof(buf), f);
		sscanf(buf, "%d %d %d %d", &min, &hour, &yday, &len);
		fgets(buf, sizeof(buf), f);		/* Comment line */
		fgets(buf, sizeof(buf), f);
		sscanf(buf, "%lu %lu %lu %lu",
					&in->hits0_minute, &in->hits1_minute,
					&in->hits0_hour, &in->hits1_hour);

		skew = read_inet_data(in->chart_minute, f, 1, min, hour, yday, len);
		if (skew > 0)  /* Current minute slot is different from saved */
			in->hits0_minute = in->hits1_minute = 0;

		skew = read_inet_data(in->chart_hour, f, 0, min, hour, yday, len);
		if (skew > 0)  /* Current hour slot is different from saved */
			in->hits0_hour = in->hits1_hour = 0;

		fclose(f);
		gkrellm_rescale_chart(in->chart);
		}
	}

/* --------------------------------------------------------------------- */
#define	DEFAULT_TEXT_FORMAT	"\\t$a\\f $l\\N$A\\f $L"

enum
	{
	LABEL0_COLUMN,
	PORT00_COLUMN,
	PORT01_COLUMN,
	RANGE0_COLUMN,
	SPACER_COLUMN,
	LABEL1_COLUMN,
	PORT10_COLUMN,
	PORT11_COLUMN,
	RANGE1_COLUMN,
	DUMMY_COLUMN,
	INET_COLUMN,
	N_COLUMNS
	};

static GtkTreeView	*treeview;
static GtkTreeRowReference *row_reference;
static GtkTreeSelection	*selection;


static GtkWidget	*label0_entry,
					*label1_entry;
static GtkWidget	*port0_0_entry,
					*port0_1_entry,
					*port1_0_entry,
					*port1_1_entry;

static GtkWidget	*launch_vbox;

static GtkWidget	*data0_range_button,
					*data1_range_button;

static GtkWidget	*text_format_combo_box;


static void
set_list_store_model_data(GtkListStore *store, GtkTreeIter *iter, InetMon *in)
	{
	gchar			p00[16], p01[16], p10[16], p11[16];

	snprintf(p00, sizeof(p00), "%d", (int) in->port0_0);
	snprintf(p01, sizeof(p01), "%d", (int) in->port0_1);
	snprintf(p10, sizeof(p10), "%d", (int) in->port1_0);
	snprintf(p11, sizeof(p11), "%d", (int) in->port1_1);
	gtk_list_store_set(store, iter,
			LABEL0_COLUMN, in->label0,
			PORT00_COLUMN, p00,
			PORT01_COLUMN, p01,
			SPACER_COLUMN, "",
			RANGE0_COLUMN, in->data0_is_range,
			LABEL1_COLUMN, in->label1,
			PORT10_COLUMN, p10,
			PORT11_COLUMN, p11,
			RANGE1_COLUMN, in->data1_is_range,
			DUMMY_COLUMN, "",
			-1);
	}

static GtkTreeModel *
create_model(void)
	{
	GtkListStore	*store;
	GtkTreeIter		iter;
	GList			*list;
	InetMon			*in;

	store = gtk_list_store_new(N_COLUMNS,
				G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN,
				G_TYPE_STRING,
				G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN,
				G_TYPE_STRING, G_TYPE_POINTER);
	for (list = inet_mon_list; list; list = list->next)
		{
		in = (InetMon *) list->data;
		in->config_modified = FALSE;
		gtk_list_store_append(store, &iter);
		set_list_store_model_data(store, &iter, in);
		gtk_list_store_set(store, &iter, INET_COLUMN, in, -1);
		}
	return GTK_TREE_MODEL(store);
	}

static void
change_row_reference(GtkTreeModel *model, GtkTreePath *path)
	{
	gtk_tree_row_reference_free(row_reference);
	if (model && path)
		row_reference = gtk_tree_row_reference_new(model, path);
	else
		row_reference = NULL;
	}

static InetMon *
inet_new_from_model(GtkTreeModel *model, GtkTreeIter *iter, gchar *ports[])
	{
	InetMon		*in;
	gchar		*_ports[4];
	gint		i;
	gboolean	free_ports = FALSE;

	if (!ports)
		{
		ports = _ports;
		free_ports = TRUE;
		}
	in = g_new0(InetMon, 1);
	gtk_tree_model_get(model, iter,
			LABEL0_COLUMN, &in->label0,
			PORT00_COLUMN, &ports[0],
			PORT01_COLUMN, &ports[1],
			RANGE0_COLUMN, &in->data0_is_range,
			LABEL1_COLUMN, &in->label1,
			PORT10_COLUMN, &ports[2],
			PORT11_COLUMN, &ports[3],
			RANGE1_COLUMN, &in->data1_is_range,
			-1);
	in->port0_0 = atoi(ports[0]);
	in->port0_1 = atoi(ports[1]);
	in->port1_0 = atoi(ports[2]);
	in->port1_1 = atoi(ports[3]);
	for (i = 0; i < 4 && free_ports; ++i)
		g_free(ports[i]);

	return in;
	}

static void
reset_entries(void)
	{
	gtk_entry_set_text(GTK_ENTRY(label0_entry), "");
	gtk_entry_set_text(GTK_ENTRY(port0_0_entry), "0");
	gtk_entry_set_text(GTK_ENTRY(port0_1_entry), "0");
	gtk_entry_set_text(GTK_ENTRY(label1_entry), "");
	gtk_entry_set_text(GTK_ENTRY(port1_0_entry), "0");
	gtk_entry_set_text(GTK_ENTRY(port1_1_entry), "0");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data0_range_button), 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data1_range_button), 0);

	change_row_reference(NULL, NULL);
	gtk_tree_selection_unselect_all(selection);
	}


static void
cb_tree_selection_changed(GtkTreeSelection *selection, gpointer data)
	{
	GtkTreeIter		iter;
	GtkTreeModel	*model;
	GtkTreePath		*path;
	InetMon			*in;
	gchar			*ports[4];
	gint			i;

	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		{
		reset_entries();
		return;
		}
	path = gtk_tree_model_get_path(model, &iter);
	change_row_reference(model, path);
	gtk_tree_path_free(path);

	in = inet_new_from_model(model, &iter, ports);

	gtk_entry_set_text(GTK_ENTRY(label0_entry), in->label0);
	gtk_entry_set_text(GTK_ENTRY(port0_0_entry), ports[0]);
	gtk_entry_set_text(GTK_ENTRY(port0_1_entry), ports[1]);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data0_range_button),
			in->data0_is_range);

	gtk_entry_set_text(GTK_ENTRY(label1_entry), in->label1);
	gtk_entry_set_text(GTK_ENTRY(port1_0_entry), ports[2]);
	gtk_entry_set_text(GTK_ENTRY(port1_1_entry), ports[3]);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data1_range_button),
			in->data1_is_range);
	g_free(in->label0);
	g_free(in->label1);
	for (i = 0; i < 4; ++i)
		g_free(ports[i]);
	}

static void
cb_launch_entry(GtkWidget *widget, InetMon *in)
	{
	gkrellm_apply_launcher(&in->launch_entry, &in->tooltip_entry,
				in->panel, &in->launch, gkrellm_launch_button_cb);
    }

static void
add_launch_entry(GtkWidget *vbox, InetMon *in)
	{
	in->launch_table = gkrellm_gtk_launcher_table_new(vbox, 1);
	gkrellm_gtk_config_launcher(in->launch_table, 0,  &in->launch_entry,
				&in->tooltip_entry, in->name, &in->launch);
	g_signal_connect(G_OBJECT(in->launch_entry), "changed",
				G_CALLBACK(cb_launch_entry), in);
	g_signal_connect(G_OBJECT(in->tooltip_entry), "changed",
				G_CALLBACK(cb_launch_entry), in);
	gtk_widget_show_all(in->launch_table);
	}


static void
sync_inet_list(void)
	{
	GtkTreeModel	*model;
	GtkTreeIter		iter;
	InetMon			*in, *in_tmp;
	GList			*list, *new_inet_list;

	/* Just save all data and then later read it back in.  This avoids
	|  complicated detecting of name changes while ports the same, moving
	|  a inet down or up slots, etc.  Data is lost only if a port number
	|  for a monitor is changed.
	*/
	gkrellm_inet_save_data();
	new_inet_list = NULL;
	n_inet_monitors = 0;

	model = gtk_tree_view_get_model(treeview);
	if (gtk_tree_model_get_iter_first(model, &iter))
		{
		do
			{
			in = inet_new_from_model(model, &iter, NULL);
			new_inet_list = g_list_append(new_inet_list, in);
			gtk_tree_model_get(model, &iter, INET_COLUMN, &in_tmp, -1);
			fix_ports(in);

			/* If an existing inet still has the same port numbers, preserve
			|  its config.  Otherwise, it is same as making a new entry.
			|  (plus the data layers could go from 2 to 1 and then there would
			|   be an extra data layer in the config - not good).
			*/
			if (   in_tmp
				&& in_tmp->port0_0 == in->port0_0
				&& in_tmp->port0_1 == in->port0_1
				&& in_tmp->port1_0 == in->port1_0
				&& in_tmp->port1_1 == in->port1_1
			   )
				{
				in->chart_config_minute = in_tmp->chart_config_minute;
				in_tmp->chart_config_minute = NULL;
				in->chart_config_hour = in_tmp->chart_config_hour;
				in_tmp->chart_config_hour = NULL;
				in->extra_info = in_tmp->extra_info;
				in->hour_mode = in_tmp->hour_mode;
				}
			else
				{
				in->chart_config_minute = gkrellm_chartconfig_new0();
				in->chart_config_hour = gkrellm_chartconfig_new0();
				gkrellm_set_chartconfig_auto_grid_resolution(
							in->chart_config_minute, TRUE);
				gkrellm_set_chartconfig_auto_grid_resolution(
							in->chart_config_hour, TRUE);
				in->extra_info = TRUE;
				}
			if (in_tmp)
				{
				gkrellm_dup_string(&in->launch.command,
							in_tmp->launch.command);
				gkrellm_dup_string(&in->launch.tooltip_comment,
							in_tmp->launch.tooltip_comment);
				}
			gtk_list_store_set(GTK_LIST_STORE(model), &iter,
						INET_COLUMN, in, -1);
			}
		while (gtk_tree_model_iter_next(model, &iter));
		}
	while (inet_mon_list)
		{
		in = (InetMon *) inet_mon_list->data;
		destroy_inet_monitor(in);
		inet_mon_list = g_list_remove(inet_mon_list, in);
		}
	inet_mon_list = new_inet_list;
	for (list = inet_mon_list; list; list = list->next)
		create_inet_monitor(inet_vbox, (InetMon *)list->data, TRUE);

	gkrellm_inet_load_data();

	for (list = inet_mon_list; list; list = list->next)
		{
		in = (InetMon *) list->data;
		draw_inet_chart(in);
		add_launch_entry(launch_vbox, in);
		}
	if (inet_mon_list)
		gkrellm_spacers_show(mon_inet);
	else
		gkrellm_spacers_hide(mon_inet);
	}

static gboolean
cb_drag_end(GtkWidget *widget, GdkDragContext *context, gpointer data)
	{
	reset_entries();
	sync_inet_list();
	return FALSE;
	}

static void
cb_enter(GtkWidget *widget, gpointer data)
	{
	GtkTreeModel	*model;
	GtkTreePath		*path = NULL;
	GtkTreeIter		iter;
	InetMon			*in;

	in = g_new0(InetMon, 1);

	in->data0_is_range = GTK_TOGGLE_BUTTON(data0_range_button)->active;
	in->data1_is_range = GTK_TOGGLE_BUTTON(data1_range_button)->active;

	in->label0 = gkrellm_gtk_entry_get_text(&label0_entry);
	if (*(in->label0))
		{
		in->port0_0 = atoi(gkrellm_gtk_entry_get_text(&port0_0_entry));
		in->port0_1 = atoi(gkrellm_gtk_entry_get_text(&port0_1_entry));
		}
	in->label1 = gkrellm_gtk_entry_get_text(&label1_entry);
	if (*(in->label1))
		{
		in->port1_0 = atoi(gkrellm_gtk_entry_get_text(&port1_0_entry));
		in->port1_1 = atoi(gkrellm_gtk_entry_get_text(&port1_1_entry));
		}

	/* Validate the values
	*/
	if (   (!*(in->label0) && !*(in->label1))
		|| (*(in->label0) && in->port0_0 == 0 && in->port0_1 == 0)
		|| (*(in->label1) && in->port1_0 == 0 && in->port1_1 == 0)
	   )
		{
		g_free(in);
		return;
		}

	model = gtk_tree_view_get_model(treeview);
	if (row_reference)
		{
		path = gtk_tree_row_reference_get_path(row_reference);
		gtk_tree_model_get_iter(model, &iter, path);
		}
	else
		{
		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		gtk_list_store_set(GTK_LIST_STORE(model), &iter, INET_COLUMN, NULL,-1);
		}
	in->config_modified = TRUE;
	set_list_store_model_data(GTK_LIST_STORE(model), &iter, in);
	g_free(in);
	reset_entries();
	sync_inet_list();
	}

static void
cb_delete(GtkWidget *widget, gpointer data)
	{
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GtkTreeIter		iter;

	if (!row_reference)
		return;
	model = gtk_tree_view_get_model(treeview);
	path = gtk_tree_row_reference_get_path(row_reference);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

	reset_entries();
	sync_inet_list();
	}

static void
cb_text_format(GtkWidget *widget, gpointer data)
	{
	GList   *list;
	gchar   *s;
	GtkWidget *entry;

	entry = gtk_bin_get_child(GTK_BIN(text_format_combo_box));
	s = gkrellm_gtk_entry_get_text(&entry);
	gkrellm_locale_dup_string(&text_format, s, &text_format_locale);
	for (list = inet_mon_list; list; list = list->next)
		draw_inet_chart((InetMon *) list->data);
	}

static void
cb_update_interval(GtkWidget *entry, GtkSpinButton *spin)
    {
    update_interval = gtk_spin_button_get_value_as_int(spin);
    }

static gchar	*inet_info_text[] =
{
N_("Inet charts show historical TCP port hits on a minute or hourly\n"
	"chart. Below the chart there is a strip where marks are drawn for\n"
	"port hits in second intervals.   The inet krell has a full scale\n"
	"value of 5 hits and samples once per second.  The extra info\n"
	"display shows current TCP port connections.\n\n"
	"For each internet monitor, you can specify two labeled data sets with\n"
	"one or two non-zero port numbers entered for each data set.  Two\n"
	"ports are allowed because some internet ports are related and you\n"
	"might want to group them.  Check /etc/services for port numbers.\n\n"
	"For example, if you created an inet monitor:\n"),

N_("<i>\thttp 80 8080   ftp 21\n"),

N_("Http hits on the standard http port 80 and www web caching service\n"
	"on port 8080 are combined and plotted in the one color.  Ftp hits\n"
	"on the single ftp port 21 are plotted in another color.\n\n"),

N_("If the range button is checked, then all port numbers between Port0 and\n"
   "Port1 are monitored and included in the plot.\n\n"),

N_("<h>Chart Labels\n"),
N_("Substitution variables for the format string for chart labels:\n"),
N_("\t$M    maximum chart value\n"),
N_("\t$a    current active connections for Data0\n"),
N_("\t$cN   total connections in last N minutes (or hours) for Data0\n"),
N_("\t$l    label for Data0\n"),
N_("\t$A    current active connections for Data1\n"),
N_("\t$CN   total connections in last N minutes (or hours) for Data1\n"),
N_("\t$L    label for Data1\n"),

"\n",
N_("<h>Mouse Button Actions:\n"),
N_("<b>\tLeft "),
N_("click on an inet chart to toggle the extra info display of\n"
	"\t\tcurrent TCP port connections.\n"),
N_("<b>\tMiddle "),
N_("click on an inet chart to toggle hour/minute charts.\n")
};


static void
create_inet_tab(GtkWidget *tab_vbox)
	{
	GtkWidget		*tabs;
	GtkWidget		*table;
	GtkWidget		*hbox, *vbox, *vbox1;
	GtkWidget		*separator;
	GtkWidget		*scrolled;
	GtkWidget		*text;
	GtkWidget		*label;
	GtkWidget		*button;
	GtkTreeModel	*model;
	GtkCellRenderer	*renderer;
	GList			*list;
	InetMon			*in;
	gint			i;

	row_reference = NULL;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Ports"));

	table = gtk_table_new(6, 7, FALSE /*homogeneous*/);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 3);

	label = gtk_label_new(_("Data0"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 3, 0, 1);
	separator = gtk_hseparator_new();
	gtk_table_attach_defaults(GTK_TABLE(table), separator, 0, 3, 1, 2);
	label = gtk_label_new(_("Data1"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 4, 7, 0, 1);
	separator = gtk_hseparator_new();
	gtk_table_attach_defaults(GTK_TABLE(table), separator, 4, 7, 1, 2);

	separator = gtk_vseparator_new();
	gtk_table_attach_defaults(GTK_TABLE(table), separator, 3, 4, 0, 6);

	label = gtk_label_new(_("Label"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
	label = gtk_label_new(_("Port0"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 2, 2, 3);
	label = gtk_label_new(_("Port1"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 2, 3, 2, 3);
	label = gtk_label_new(_("Label"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 4, 5, 2, 3);
	label = gtk_label_new(_("Port0"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 5, 6, 2, 3);
	label = gtk_label_new(_("Port1"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 6, 7, 2, 3);

	label0_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(label0_entry), 8);
	gtk_widget_set_size_request(label0_entry, 32, -1);
	gtk_table_attach_defaults(GTK_TABLE(table), label0_entry, 0, 1, 3, 4);
	port0_0_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(port0_0_entry), 8);
	gtk_widget_set_size_request(port0_0_entry, 32, -1);
	gtk_table_attach_defaults(GTK_TABLE(table), port0_0_entry, 1, 2, 3, 4);
	port0_1_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(port0_1_entry), 8);
	gtk_widget_set_size_request(port0_1_entry, 32, -1);
	gtk_table_attach_defaults(GTK_TABLE(table), port0_1_entry, 2, 3, 3, 4);

	label1_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(label1_entry), 8);
	gtk_widget_set_size_request(label1_entry, 32, -1);
	gtk_table_attach_defaults(GTK_TABLE(table), label1_entry, 4, 5, 3, 4);
	port1_0_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(port1_0_entry), 8);
	gtk_widget_set_size_request(port1_0_entry, 32, -1);
	gtk_table_attach_defaults(GTK_TABLE(table), port1_0_entry, 5, 6, 3, 4);
	port1_1_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(port1_1_entry), 8);
	gtk_widget_set_size_request(port1_1_entry, 32, -1);
	gtk_table_attach_defaults(GTK_TABLE(table), port1_1_entry, 6, 7, 3, 4);

	hbox = gtk_hbox_new(FALSE, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 3, 4, 5);
	gkrellm_gtk_check_button(hbox, &data0_range_button, 0, TRUE, 0,
		_("Port0 - Port1 is a range"));

	hbox = gtk_hbox_new(FALSE, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), hbox, 4, 7, 4, 5);
	gkrellm_gtk_check_button(hbox, &data1_range_button, 0, TRUE, 0,
		_("Port0 - Port1 is a range"));

	separator = gtk_hseparator_new();
	gtk_table_attach_defaults(GTK_TABLE(table), separator, 0, 7, 5, 6);

	hbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(cb_delete), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

	/* everybody knows about CNTL click, right? */
//	button = gtk_button_new_from_stock(GTK_STOCK_NEW);
//	g_signal_connect(G_OBJECT(button), "clicked",
//			G_CALLBACK(reset_entries), NULL);
//	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(cb_enter), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

	separator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 2);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);


	model = create_model();
	treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(G_OBJECT(model));
	gtk_tree_view_set_rules_hint(treeview, TRUE);
	gtk_tree_view_set_reorderable(treeview, TRUE);
	g_signal_connect(G_OBJECT(treeview), "drag_end",
				G_CALLBACK(cb_drag_end), NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Label"),
				renderer,
				"text", LABEL0_COLUMN, NULL);
	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer), "xalign", 1.0, NULL);
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Port0"),
				renderer,
				"text", PORT00_COLUMN, NULL);
	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer), "xalign", 1.0, NULL);
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Port1"),
				renderer,
				"text", PORT01_COLUMN, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, "    ",
				renderer,
				"text", SPACER_COLUMN, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Label"),
				renderer,
				"text", LABEL1_COLUMN, NULL);
	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer), "xalign", 1.0, NULL);
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Port0"),
				renderer,
				"text", PORT10_COLUMN, NULL);
	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer), "xalign", 1.0, NULL);
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Port1"),
				renderer,
				"text", PORT11_COLUMN, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, "    ",
				renderer,
				"text", DUMMY_COLUMN, NULL);


	gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(treeview));
	selection = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed",
				G_CALLBACK(cb_tree_selection_changed), NULL);

/* --Setup tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));

	if (!_GK.client_mode)
		gkrellm_gtk_spin_button(vbox, NULL,
					(gfloat) update_interval, 1, 20, 1, 1, 0, 55,
            		cb_update_interval, NULL, FALSE,
            		_("Seconds between updates"));

	label = gtk_label_new("");	/* padding */
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Format String for Chart Labels"),
				4, 0, TRUE);

	text_format_combo_box = gtk_combo_box_entry_new_text();
	gtk_box_pack_start(GTK_BOX(vbox1), text_format_combo_box, FALSE, FALSE, 2);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
			text_format);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
			DEFAULT_TEXT_FORMAT);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
			"\\r\\f $M\\t$a\\f $l\\N$A\\f $L");
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
			"\\r\\f $M\\D1$a\\f $l\\D2$A\\f $L");
	gtk_combo_box_set_active(GTK_COMBO_BOX(text_format_combo_box), 0);
	g_signal_connect(G_OBJECT(GTK_COMBO_BOX(text_format_combo_box)), "changed",
			G_CALLBACK(cb_text_format), NULL);

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Launch Commands"),
				4, 0, TRUE);
	launch_vbox = gkrellm_gtk_scrolled_vbox(vbox1, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show(launch_vbox);
	gtk_widget_realize(launch_vbox);
	for (i = 0, list = inet_mon_list; list; list = list->next, ++i)
		{
		in = (InetMon *) list->data;
		add_launch_entry(launch_vbox, in);
		}

/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	for (i = 0; i < sizeof(inet_info_text)/sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(inet_info_text[i]));

	reset_entries();
	}



static GkrellmMonitor	monitor_inet =
	{
	N_("Internet"),			/* Name, for config tab.	*/
	MON_INET,			/* Id,  0 if a plugin		*/
	create_inet,		/* The create function		*/
	update_inet,		/* The update function		*/
	create_inet_tab,	/* The config tab create function	*/
	NULL,				/* Instant apply	*/

	save_inet_config,	/* Save user conifg			*/
	load_inet_config,	/* Load user config			*/
	"inet",				/* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_inet_monitor(void)
	{
	monitor_inet.name = _(monitor_inet.name);
	style_id = gkrellm_add_chart_style(&monitor_inet, INET_STYLE_NAME);
	gkrellm_locale_dup_string(&text_format, DEFAULT_TEXT_FORMAT,
					&text_format_locale);
	mon_inet = &monitor_inet;
	if (setup_inet_interface())
		{	/* Make the "data-suffix/inet" directory */
		inet_data_dir = gkrellm_make_data_file_name("inet", NULL);
		return &monitor_inet;
		}
	return NULL;
	}
