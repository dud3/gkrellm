/* GKrellM
|  Copyright (C) 1999-2008 Bill Wilson
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
#include "gkrellm-sysdeps.h"
#include "gkrellm-private.h"

#define	PPP_LOCK_FILE	"LCK..modem"

#define TIMER_TYPE_NONE		0
#define TIMER_TYPE_PPP		1
#define TIMER_TYPE_IPPP		2
#define TIMER_TYPE_SERVER	3


#define		N_DAY_STATS		31
#define		N_WEEK_STATS	26
#define		N_MONTH_STATS	12

typedef struct
	{
	gchar		*date;
	gdouble		rx,
				tx;
	gint		connect_time;
	}
	NetStat;

enum StatType
	{
	DAY_STAT,
	WEEK_STAT,
	MONTH_STAT
	};


typedef struct
	{
	gchar			*name;
	GtkWidget		*vbox,
					*parent_vbox;
	GtkWidget		*enable_button,
					*force_button,
					*alert_button,
					*label_entry;
	GkrellmChart	*chart;
	GkrellmChartdata *rx_cd,
					*tx_cd;
	GkrellmDecal	*rxled,
					*txled;

	/* All known net interfaces are in the net_mon_list.  Only interfaces
	|  which are up or forced up and are config enabled will actually have
	|  a chart created, unless the interface is linked to the timer button.
	|  A linked interface always has a chart created for it regardless
	|  of the up state but it will have a chart that may not be
	|  visible if the interface is down (ppp) or the connect state is
	|  hangup (ippp).
	*/
	gboolean	locked;		/* True if linked to timer button or forced up */

	GkrellmChartconfig	*chart_config;
	gboolean			enabled,
						chart_labels,
						force_up,
						real;

	gchar				*label;

	GkrellmAlert		*alert;
	GtkWidget			*alert_config_rx_button,
						*alert_config_tx_button;
	gboolean			alert_uses_rx,
						alert_uses_tx;

	gulong				rx_old,
						tx_old;
	gint				rx_current,
						tx_current;
	gdouble				rx_totalA,
						rx_totalB,
						tx_totalA,
						tx_totalB;

	NetStat				day_stats[N_DAY_STATS],
						week_stats[N_WEEK_STATS],
						month_stats[N_MONTH_STATS];

	gboolean			show_totalB,
						totals_shown,
						reset_button_in,
						stats_button_in,
						mouse_in_chart,
						new_text_format;
	GkrellmLauncher		launch;
	GtkWidget			*launch_entry,
						*tooltip_entry;

	gboolean			up,
						up_prev,
						up_event,
						down_event;

	gulong				rx,
						tx;
	}
	NetMon;


static void cb_alert_config(GkrellmAlert *ap, NetMon *net);
static void	cb_alert_config_create(GkrellmAlert *ap, GtkWidget *vbox,
					NetMon *net);
static void	net_stat_init(NetMon *net);

typedef struct
	{
	gchar	*name;
	gint	type;
	}
	TimerType;


static GList	*net_mon_list;
static GList	*net_mon_sys_list;
static GList	*timer_defaults_list;
static gchar	*lock_directory;
static gchar	*net_data_dir;

static void		(*read_net_data)();
static void		(*check_net_routes)();
static gboolean	(*isdn_is_online)();

static gboolean	net_use_routed;
static gboolean	net_config_use_routed;

static gint		reset_mday;

static gint
strcmp_net_name(NetMon *net1, NetMon *net2)
	{
	gchar	*s;
	gint	n, n1, n2, len;

	for (s = net1->name; *s; ++s)
		if (isdigit((unsigned char)*s))
			break;
	if (!*s)
		return strcmp(net1->name, net2->name);
	n1 = atoi(s);
	len = s - net1->name;
	n = strncmp(net1->name, net2->name, len);
	if (n == 0)
		{
		n2 = atoi(net2->name + len);
		if (n1 < n2)
			n = -1;
		else if (n1 > n2)
			n = 1;
		}
	return n;
	}

static NetMon *
new_net(gchar *name)
	{
	NetMon		*net;

	net = g_new0(NetMon, 1);
	net->name = g_strdup(name);
	if (strcmp(name, "lo"))
		net->enabled = TRUE;		/* All except lo default to enabled */
	net->chart_labels = TRUE;
	net->label = g_strdup("");
	net->launch.command = g_strdup("");
	net->launch.tooltip_comment = g_strdup("");
	net->alert_uses_rx = net->alert_uses_tx = TRUE;
	net_mon_list = g_list_insert_sorted(net_mon_list, net,
				(GCompareFunc) strcmp_net_name);

	net_stat_init(net);

	return net;
	}

static NetMon	*
lookup_net(gchar *name)
	{
	NetMon	*net;
	GList	*list;

	if (!name)
		return NULL;
	for (list = net_mon_list; list; list = list->next)
		{
		net = (NetMon *) list->data;
		if (!strcmp(net->name, name))
			return net;
		}
	return NULL;
	}

/* ------------- Net monitor to system dependent interface ------------- */

void
gkrellm_net_client_divert(void (*read_func)(), void (*check_routes)(),
			gboolean (*isdn_online_func)())
	{
	read_net_data = read_func;
	check_net_routes = check_routes;
	isdn_is_online = isdn_online_func;
	}

static gboolean
setup_net_interface(void)
    {
	if (!read_net_data && !_GK.client_mode && gkrellm_sys_net_init())
		{
		read_net_data = gkrellm_sys_net_read_data;
		check_net_routes = gkrellm_sys_net_check_routes;
		isdn_is_online = gkrellm_sys_net_isdn_online;
		}
	return read_net_data ? TRUE : FALSE;
	}

gchar *
gkrellm_net_mon_first(void)
	{
	gchar	*name = NULL;

	net_mon_sys_list = net_mon_list;
	if (net_mon_sys_list)
		{
		name = ((NetMon *) (net_mon_sys_list->data))->name;
		net_mon_sys_list = net_mon_sys_list->next;
		}
	return name;
	}

gchar
*gkrellm_net_mon_next(void)
	{
	gchar	*name = NULL;

	if (net_mon_sys_list)
		{
		name = ((NetMon *) (net_mon_sys_list->data))->name;
		net_mon_sys_list = net_mon_sys_list->next;
		}
	return name;
	}

void
gkrellm_net_use_routed(gboolean real_routed)
	{
	/* real_routed should only ever be FALSE when called from client.c to
	|  handle the server sysdep net code not using routed mode while the
	|  client <-> server interface will always use routed mode.
	*/
	net_use_routed = TRUE;
	net_config_use_routed = real_routed;
	}

void
gkrellm_net_routed_event(gchar *name, gboolean routed)
	{
	NetMon	*net;

	if (!net_use_routed)
		return;
	if ((net = lookup_net(name)) == NULL)
		net = new_net(name);
	if (routed)
		net->up_event = TRUE;
	else
		net->down_event = TRUE;
	net->up = routed;
	net->real = TRUE;
	}

void
gkrellm_net_assign_data(gchar *name, gulong rx, gulong tx)
	{
	NetMon	*net;

	if ((net = lookup_net(name)) == NULL)
		net = new_net(name);
	if (GK.second_tick && !net_use_routed)
		net->up = TRUE;
	net->rx = rx;
	net->tx = tx;
	net->real = TRUE;
	}

void
gkrellm_net_add_timer_type_ppp(gchar *name)
	{
	TimerType	*t;

	if (!name || !*name || _GK.client_mode)
		return;
	t = g_new0(TimerType, 1);
	t->name = g_strdup(name);
	t->type = TIMER_TYPE_PPP;
	timer_defaults_list = g_list_append(timer_defaults_list, t);
	}

void
gkrellm_net_add_timer_type_ippp(gchar *name)
	{
	TimerType	*t;

	if (!name || !*name || _GK.client_mode)
		return;
	t = g_new0(TimerType, 1);
	t->name = g_strdup(name);
	t->type = TIMER_TYPE_IPPP;
	timer_defaults_list = g_list_append(timer_defaults_list, t);
	}

void
gkrellm_net_set_lock_directory(gchar *dir)
	{
	lock_directory = g_strdup(dir);
	}


/* ======================================================================== */
/* Exporting net data for plugins */

gint
gkrellm_net_routes(void)
	{
	return g_list_length(net_mon_list);
	}

gboolean
gkrellm_net_stats(gint n, gchar *name, gulong *rx, gulong *tx)
	{
	GList	*list;
	NetMon	*net;

	list = g_list_nth(net_mon_list, n);
	if (!list)
		return FALSE;
	net = (NetMon *) list->data;
	if (name)
		strcpy(name, net->name);
	if (rx)
		*rx = net->rx;
	if (tx)
		*tx = net->tx;
	return TRUE;
	}

void
gkrellm_net_led_positions(gint *x_rx_led, gint *y_rx_led,
			gint *x_tx_led, gint *y_tx_led)
	{
	if (x_rx_led)
		*x_rx_led = _GK.rx_led_x;
	if (y_rx_led)
		*y_rx_led = _GK.rx_led_y;
	if (x_tx_led)
		*x_tx_led = _GK.tx_led_x;
	if (y_tx_led)
		*y_tx_led = _GK.tx_led_y;
	}

/* ======================================================================== */
#include    "pixmaps/net/decal_net_leds.xpm"
#include    "pixmaps/timer/bg_timer.xpm"
#include    "pixmaps/timer/decal_timer_button.xpm"

/* ISO 8601 date format for network stats gui*/
#ifdef WIN32
#define GK_NET_ISO_DATE "%Y-%m-%d"
#else
#define GK_NET_ISO_DATE "%F"
#endif

#define	MIN_GRID_RES		5
#define	MAX_GRID_RES		100000000
#define	DEFAULT_GRID_RES 	20000

  /* States for the timer button are indexes to the corresponding
  |  timer button decal frame shown.
  */
#define	TB_NORMAL		0
#define	TB_PRESSED		1
#define	TB_STANDBY		2
#define	TB_ON			3
#define	N_TB_DECALS		4

#define	RX_LED	0
#define	TX_LED	1

#define	RX_OFF	0
#define	RX_ON	1
#define	TX_OFF	2
#define	TX_ON	3
#define	N_LEDS	4

static GkrellmMonitor	*mon_net;
static GkrellmMonitor	*mon_timer;


static NetMon		*net_timed;		/* Monitor linked to timer button  */

GkrellmPanel		*timer_panel;		/* For the timer and button	*/

static GtkWidget	*net_vbox;		/* Where all net monitors live */
static GtkWidget	*dynamic_net_vbox;
static GtkWidget	*timer_vbox;

static GkrellmPiximage *bg_timer_piximage,
					*decal_net_led_piximage,
					*decal_timer_button_piximage;

static GkrellmStyle	*bg_timer_style;

static GdkPixmap	*decal_net_led_pixmap;
static GdkBitmap	*decal_net_led_mask;

static GdkPixmap	*decal_timer_button_pixmap;
static GdkBitmap	*decal_timer_button_mask;

static GkrellmDecal	*time_decal,
					*seconds_decal,
					*button_decal;

  /* These 3 decals will be drawn on net charts when mouse is in a chart and
  |  the chart has a rx and/or tx total drawn.  They won't live in a panel
  |  so must be managed differently from decals on a panel.
  */
static GkrellmDecal	*decal_totalA,
					*decal_totalB,
					*decal_reset;
static GkrellmDecal	*decal_stats;

static gchar		*timer_on_command;
static gchar		*timer_off_command;
static gchar		*timer_button_iface;
static gint			timer_button_enabled;
static gint			last_time = -1;


static gint			timer_button_type = TIMER_TYPE_NONE,
					timer_button_state,
					timer_button_old_state,
					last_timer_command;

static gboolean		timer_seconds;

static gint			check_connect_state,
					net_stats_window_height;

static gint			ascent,
					ascent_alt,
					sec_pad;

static time_t		net_timer0;

static gint			net_style_id,
					timer_style_id;

static GkrellmSizeAbbrev    stats_bytes_abbrev[]    =
	{
	{ KB_SIZE(1),       1,              "%.0f" },
	{ KB_SIZE(10),      KB_SIZE(1),     "%.1fKB" },
	{ MB_SIZE(1),       KB_SIZE(1),     "%.2fKB" },
	{ MB_SIZE(10),      MB_SIZE(1),     "%.3fMB" },
	{ MB_SIZE(100),     MB_SIZE(1),     "%.2fMB" },
	{ GB_SIZE(1),       MB_SIZE(1),     "%.2fMB" },
	{ GB_SIZE(10),      GB_SIZE(1),     "%.3fGB" },
	{ GB_SIZE(100),     GB_SIZE(1),     "%.2fGB" },
	{ TB_SIZE(1),       GB_SIZE(1),     "%.2fGB" },
	{ TB_SIZE(10),      TB_SIZE(1),     "%.3fTB" },
	{ TB_SIZE(100),     TB_SIZE(1),     "%.3fTB" },
	{ TB_SIZE(1000),    TB_SIZE(1),     "%.2fTB" }
	};

#define	NET_DATA_VERSION	2

#define	SATURDAY	6
#define	SUNDAY		0

static gchar	days_in_month[12] =
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31  };


  /* Called from client.c
  */
void
gkrellm_net_server_has_timer(void)
	{
	timer_button_type = TIMER_TYPE_SERVER;
	}

static void
draw_led(NetMon *net, int rxtx, int led_index)
	{
	GkrellmPanel	*p;
	GkrellmDecal	*led;

	if (!net->chart)
		return;
	p = net->chart->panel;
	if (rxtx == RX_LED)
		led = net->rxled;
	else
		led = net->txled;

	gkrellm_draw_decal_pixmap(p, led, led_index);
	}

static void
draw_timer(GkrellmPanel *p, gint seconds, gint force)
	{
	static gint	prev_minute = -1,
				prev_hour   = -1;
	gint		minutes, hours, w;
	gchar		buf[32], buf_sec[16];

	last_time = seconds;
	hours = seconds / 60 / 60;
	minutes = (seconds / 60) % 60;
	seconds = seconds % 60;
	snprintf(buf, sizeof(buf), "%2d:%02d", hours, minutes);
	snprintf(buf_sec, sizeof(buf_sec), "%02d", seconds);

	if (prev_minute != minutes || prev_hour != hours || force)
		{
		w = gkrellm_gdk_string_width(time_decal->text_style.font, buf);
		if (timer_seconds && w + seconds_decal->w + sec_pad <= time_decal->w)
			{
			time_decal->x_off = time_decal->w - w - seconds_decal->w - sec_pad
						- time_decal->text_style.effect;
			gkrellm_make_decal_visible(p, seconds_decal);
			}
		else
			{
			time_decal->x_off = time_decal->w - w
						- time_decal->text_style.effect;
			if (time_decal->x_off < 0)
				time_decal->x_off = 0;
			gkrellm_make_decal_invisible(p, seconds_decal);
			}
		}
	prev_minute = minutes;
	prev_hour = hours;

	gkrellm_draw_decal_text(p, time_decal, buf,
				force ?  -1 : (hours * 60 + minutes));

	if (gkrellm_is_decal_visible(seconds_decal))
		gkrellm_draw_decal_text(p, seconds_decal, buf_sec,
					force ? -1 : seconds);

	gkrellm_draw_panel_layers(timer_panel);
	}

  /* --------- net stats window -----------
  */
enum
	{
	DATE_COLUMN,
	RX_COLUMN,
	TX_COLUMN,
	TOTAL_COLUMN,
	TIME_COLUMN,
	N_STATS_COLUMNS
	};

static GtkTreeModel *
stats_model_create(NetMon *net, NetStat *ns, gint n_stats)
	{
	GtkListStore	*store;
	GtkTreeIter		iter;
	gchar			rx[64], tx[64], total[64], time[64];
	gint			i, hours, minutes;

	store = gtk_list_store_new(N_STATS_COLUMNS,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING);

	for (i = 0; i < n_stats; ++i, ++ns)
		{
		if (i > 0 && !strncmp(ns->date, "---", 3))
			continue;
		gkrellm_format_size_abbrev(rx, sizeof(rx), (gfloat) ns->rx,
					&stats_bytes_abbrev[0],
					sizeof(stats_bytes_abbrev) / sizeof(GkrellmSizeAbbrev));
		gkrellm_format_size_abbrev(tx, sizeof(tx), (gfloat) ns->tx,
					&stats_bytes_abbrev[0],
					sizeof(stats_bytes_abbrev) / sizeof(GkrellmSizeAbbrev));
		gkrellm_format_size_abbrev(total, sizeof(total),
					(gfloat) (ns->rx + ns->tx),
					&stats_bytes_abbrev[0],
					sizeof(stats_bytes_abbrev) / sizeof(GkrellmSizeAbbrev));
		minutes = ns->connect_time / 60;
		hours = minutes / 60;
		minutes %= 60;
		snprintf(time, sizeof(time), "%4d:%02d", hours, minutes);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				DATE_COLUMN, ns->date,
				RX_COLUMN, rx,
				TX_COLUMN, tx,
				TOTAL_COLUMN, total,
				TIME_COLUMN, time,
				-1);
		}
	return GTK_TREE_MODEL(store);
	}

static gint
net_stats_window_configure_cb(GtkWidget *widget, GdkEventConfigure *ev,
			gpointer data)
	{
	net_stats_window_height = widget->allocation.height;
	gkrellm_config_modified();
	return FALSE;
	}

static void
net_stats_close_cb(GtkWidget *widget, GtkWidget *stats_window)
	{
	gtk_widget_destroy(stats_window);
	}

static void
net_stats_page(GtkWidget *vbox, NetMon *net,
			NetStat *ns, gint n_stats, gchar *period)
	{
	GtkTreeView			*treeview;
	GtkTreeModel		*model;
	GtkTreeSelection	*selection;
	GtkCellRenderer		*renderer;
	gint				i;

	model = stats_model_create(net, ns, n_stats);
	treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(model);
	gtk_tree_view_set_rules_hint(treeview, TRUE);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, period,
			renderer, "text", DATE_COLUMN, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Received"),
			renderer, "text", RX_COLUMN, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Transmitted"),
			renderer, "text", TX_COLUMN, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Total"),
			renderer, "text", TOTAL_COLUMN, NULL);

	for (i = 0; i < n_stats; ++i)
		if (ns[i].connect_time > 0)
			{
			renderer = gtk_cell_renderer_text_new();
			gtk_tree_view_insert_column_with_attributes(treeview, -1,
					_("Connect Time"),
					renderer, "text", TIME_COLUMN, NULL);
			break;
			}
	selection = gkrellm_gtk_scrolled_selection(treeview, vbox,
			GTK_SELECTION_NONE,
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC,
			NULL, NULL);
	}

static void
net_stats_window_show(NetMon *net)
	{
	GtkWidget			*stats_window, *tabs, *main_vbox;
	GtkWidget			*vbox, *hbox, *button, *sep;
	gchar				buf[64];

	snprintf(buf, sizeof(buf), _("%s Statistics"), net->name);

	stats_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(stats_window), buf);
	gtk_window_set_wmclass(GTK_WINDOW(stats_window),
				"Gkrellm_netstats", "Gkrellm");

	g_signal_connect(G_OBJECT(stats_window), "configure_event",
				G_CALLBACK(net_stats_window_configure_cb), NULL);
	gtk_window_set_default_size(GTK_WINDOW(stats_window),
				-1, net_stats_window_height);

	main_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 4);
	gtk_container_add(GTK_CONTAINER(stats_window), main_vbox);

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(main_vbox), tabs, TRUE, TRUE, 0);

	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Daily"));
	net_stats_page(vbox, net, &net->day_stats[0], N_DAY_STATS, _("Date"));

	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Weekly"));
	net_stats_page(vbox, net, &net->week_stats[0], N_WEEK_STATS,
				_("Week Ending"));

	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Monthly"));
	net_stats_page(vbox, net, &net->month_stats[0], N_MONTH_STATS,
				_("Month"));

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(main_vbox), sep, FALSE, FALSE, 3);

	hbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 4);
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(net_stats_close_cb), stats_window);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

	gtk_widget_show_all(stats_window);
	}


static gint
get_connect_state(void)
	{
	struct stat	st;
	gchar		buf[256];
	gint		state	= TB_NORMAL;
	static gint	old_state;

	switch (timer_button_type)
		{
		case TIMER_TYPE_NONE:
		case TIMER_TYPE_SERVER:
			break;
		case TIMER_TYPE_PPP:
			if (net_timed->up)
				state = TB_ON;
			else if (lock_directory && *lock_directory)
				{
				snprintf(buf, sizeof(buf),
					 "%s/%s", lock_directory, PPP_LOCK_FILE);
				if (g_stat(buf, &st) == 0)
					state = TB_STANDBY;
				else
					{
					/* If lock file is ttySx, then user can make a link:
					|  ln -s ~/.gkrellm2/LCK..modem lock_directory/LCK..ttySx
					*/
					snprintf(buf, sizeof(buf), "%s/%s/%s",
								gkrellm_homedir(), GKRELLM_DIR, PPP_LOCK_FILE);
					if (g_stat(buf, &st) == 0)
						state = TB_STANDBY;
					}
				}
			break;
		case TIMER_TYPE_IPPP:
			if (isdn_is_online && (*isdn_is_online)())
				state = net_timed->up ? TB_ON : TB_STANDBY;
			break;
		}
	if ((_GK.debug_level & DEBUG_TIMER) && state != old_state)
		g_print(_("get_connect_state changed from %d to %d  (check=%d)\n"),
				old_state, state, check_connect_state);
	old_state = state;
	return state;
	}

static void
get_connect_time(void)
	{
	struct stat	st;
	gchar		buf[256];
	time_t		t	= 0;

	switch (timer_button_type)
		{
		case TIMER_TYPE_NONE:
		case TIMER_TYPE_SERVER:
			break;
		case TIMER_TYPE_PPP:
			snprintf(buf, sizeof(buf), "/var/run/%s.pid", timer_button_iface);
			if (g_stat(buf, &st) == 0)
				t = st.st_mtime;
			break;
		case TIMER_TYPE_IPPP:
			break;
		}
	if (t > 0)
		net_timer0 = t;
	else
		time(&net_timer0);
	}

static void
set_timer_button_state(gint decal_state)
	{
	timer_button_old_state = timer_button_state;
	timer_button_state = decal_state;
	gkrellm_draw_decal_pixmap(timer_panel, button_decal, decal_state);
	gkrellm_draw_panel_layers(timer_panel);

	if (   (_GK.debug_level & DEBUG_TIMER)
		&& timer_button_state != timer_button_old_state
	   )
		g_print(_("set_timer_button_state from %d to %d (check=%d)\n"),
			timer_button_old_state, timer_button_state, check_connect_state);
	}

static void
update_timer_button_monitor(void)
	{
	if (timer_button_state == TB_PRESSED)
		return;
	if (   (_GK.debug_level & DEBUG_TIMER) && net_timed
		&& timer_button_type != TIMER_TYPE_NONE
		&& (net_timed->up_event || net_timed->down_event)
	   )
		g_print(_("update_timer_button net_timed old_state=%d new_state=%d\n"),
			   net_timed->up_event, net_timed->down_event);
	switch (timer_button_type)
		{
		case TIMER_TYPE_NONE:
			if (timer_button_state == TB_ON)
				draw_timer(timer_panel, (int) (time(0) - net_timer0), 0);
			break;

		case TIMER_TYPE_PPP:
			if (net_timed->up)
				{
				set_timer_button_state(TB_ON);
				check_connect_state = FALSE;
				if (net_timed->up_event)
					get_connect_time();
				}
			else if (net_timed->down_event)
				set_timer_button_state(TB_NORMAL);
			if (check_connect_state)
				set_timer_button_state(get_connect_state());

			if (net_timed->up)
				draw_timer(timer_panel, (int) (time(0) - net_timer0), 0);
			break;

		case TIMER_TYPE_IPPP:
			/* get all isdn status from get_connect_state because the
			|  net_timed->up can be UP even with isdn line not connected.
			*/
			set_timer_button_state(get_connect_state());
			if (   timer_button_state != TB_NORMAL
				&& timer_button_old_state == TB_NORMAL
			   )
				time(&net_timer0);  /* New session just started */
			if (timer_button_state != TB_NORMAL)
				draw_timer(timer_panel, (int) (time(0) - net_timer0), 0);
			break;

		case TIMER_TYPE_SERVER:
			/* Button state is independent of displayed timer
			*/
			draw_timer(timer_panel, gkrellm_client_server_get_net_timer(), 0);
			break;
		}
	if (net_timed && net_timed->up)
		{
		net_timed->day_stats[0].connect_time += 1;
		net_timed->week_stats[0].connect_time += 1;
		net_timed->month_stats[0].connect_time += 1;
		}
	}

static void
stale_pppd_files_debug(void)
	{
	struct stat st;
	gchar	buf[256];

	snprintf(buf, sizeof(buf), "/var/run/%s.pid", timer_button_iface);
	if (g_stat(buf, &st) == 0 && !net_timed->up)
		g_print(_("  **** Stale pppd pppX.pid file detected!\n"));
	}

static gint
in_button(GkrellmDecal *d, GdkEventButton *ev)
	{
	if (ev->x > d->x && ev->x <= d->x + d->w)
		return TRUE;
	return FALSE;
	}

static gint	save_tb_state;

static gint
cb_timer_button_press(GtkWidget *widget, GdkEventButton *ev)
	{
	if (ev->button != 1 || !in_button(DECAL(timer_panel), ev))
		return FALSE;
	if (timer_button_state != TB_PRESSED)		/* button bounce? */
		save_tb_state = timer_button_state;
	set_timer_button_state(TB_PRESSED);
	return FALSE;
	}

static gint
cb_timer_button_release(GtkWidget *widget, GdkEventButton *ev)
	{
	gint	tstate, timer_command;

	if (timer_button_state != TB_PRESSED)
		return FALSE;
	set_timer_button_state(save_tb_state);
	if (! in_button(DECAL(timer_panel), ev))
		return FALSE;

	switch (timer_button_type)
		{
		case TIMER_TYPE_NONE:
		case TIMER_TYPE_SERVER:
			if (timer_button_state == TB_NORMAL)
				{
				if (*timer_on_command != '\0')
					g_spawn_command_line_async(timer_on_command, NULL);
				set_timer_button_state(TB_ON);
				time(&net_timer0);
				}
			else
				{
				if (*timer_off_command != '\0')
					g_spawn_command_line_async(timer_off_command, NULL);
				set_timer_button_state(TB_NORMAL);
				}
			break;

		case TIMER_TYPE_PPP:
		case TIMER_TYPE_IPPP:
			check_connect_state = TRUE;
			tstate = get_connect_state();
			if (_GK.debug_level & DEBUG_TIMER)
				stale_pppd_files_debug();
			if (tstate == TB_NORMAL)
				timer_command = ON;
			else if (tstate == TB_ON)
				timer_command = OFF;
			else /* tstate == TB_STANDBY */
				{
				/* For some, pppd is leaving stale LCK..modem (and ppp0.pid)
				|  files which can fool gkrellm.  So the question is, do I
				|  launch off or on command here?  Since I can't trust
				|  TB_STANDBY to mean pppd is running, I'll just base it on
				|  state info.
				*/
				if (last_timer_command == ON)
					{
					timer_command = OFF;
					if (timer_button_type == TIMER_TYPE_PPP)
						set_timer_button_state(TB_NORMAL);
					check_connect_state = FALSE;
					draw_led(net_timed, RX_LED, RX_OFF);	/* Noise */
					draw_led(net_timed, TX_LED, TX_OFF);
					}
				else
					timer_command = ON;
				}
			if (timer_command == ON && *timer_on_command != '\0')
					g_spawn_command_line_async(timer_on_command, NULL);
			if (timer_command == OFF && *timer_off_command != '\0')
					g_spawn_command_line_async(timer_off_command, NULL);
			last_timer_command = timer_command;
			break;
		}
	return FALSE;
	}


  /* A timed_net (linked to the timer button) always has a panel
  |  visible, and the Chart visibility is toggled based on net up state.
  */
static void
timed_net_visibility(void)
	{
	NetMon		*net = net_timed;
	gboolean	net_is_up,
				chart_is_visible;

	if (!net || !net->chart)
		return;
	chart_is_visible = gkrellm_is_chart_visible(net->chart);

	/* For ippp, the route may always be up, so base a up state on the
	|  route being alive and the line status being connected (timer button
	|  state)
	*/
	if (timer_button_type == TIMER_TYPE_IPPP)
		net_is_up = (net->up && timer_button_state != TB_NORMAL);
	else
		net_is_up = net->up;

	if ((net->force_up || net_is_up) && !chart_is_visible)
		gkrellm_chart_show(net->chart, TRUE);	/* Make sure panel is shown */
	else if (!net->force_up && !net_is_up && chart_is_visible)
		{
		gkrellm_chart_hide(net->chart, FALSE);  /* Don't hide the panel */

		/* Save data whenever net has transitioned to a "not up" state.
		|  Don't have the info to do this when there's a force_up and in
		|  this case just rely on the every six hour tick data save.
		*/
		gkrellm_net_save_data();
		}
	}


static gint
net_expose_event(GtkWidget *widget, GdkEventExpose *ev)
	{
	GList		*list;
	NetMon		*net;
	GdkPixmap	*pixmap	= NULL;

	if (timer_panel->drawing_area == widget)
		pixmap = timer_panel->pixmap;
	else
		for (list = net_mon_list; list; list = list->next)
			{
			net = (NetMon *) list->data;
			if (!net->chart || !net->chart->panel)	/* A disabled iface */
				continue;
			if (net->chart->drawing_area == widget)
				pixmap = net->chart->pixmap;
			else if (net->chart->panel->drawing_area == widget)
				pixmap = net->chart->panel->pixmap;
			if (pixmap)
				break;
			}
	if (pixmap)
		gdk_draw_drawable(widget->window, gkrellm_draw_GC(1), pixmap,
			ev->area.x, ev->area.y, ev->area.x, ev->area.y,
			ev->area.width, ev->area.height);
	return FALSE;
	}

static gint
map_x(gint x, gint width)
	{
	gint	xnew, chart_width;

	xnew = x;
	chart_width = gkrellm_chart_width();
	if (x < 0)
		xnew += chart_width - width;
	return xnew;
	}

static gint
grid_resolution_default(NetMon *net)
	{
	gchar	*s 	= net->name;
	gint	res;

	if (! strncmp(s, "ppp", 3))
		res = 2000;
#if defined(__FreeBSD__)
	else if (! strncmp(s, "tun", 3))
		res = 2000;
#endif
	else if (! strncmp(s, "plip", 3) || ! strncmp(s, "ippp", 4))
		res = 5000;
	else if (! strncmp(s, "eth", 3))
		res = 20000;
	else
		res = 10000;
	return res;
	}

static void
setup_net_scaling(GkrellmChartconfig *cf, NetMon *net)
	{
	GkrellmChart	*cp  = net->chart;
	gint	grids, res;

	grids = gkrellm_get_chartconfig_fixed_grids(cf);
	if (!grids)
		grids = FULL_SCALE_GRIDS;

	res = gkrellm_get_chartconfig_grid_resolution(cf);
	KRELL(cp->panel)->full_scale = res * grids / gkrellm_update_HZ();
	}

static void
destroy_chart(NetMon *net)
	{
	GkrellmChart	*cp;

	if (!net)
		return;
	cp = net->chart;
	if (cp)
		{
		net->launch.button = NULL;
		net->launch.tooltip = NULL;
		g_free(cp->panel->textstyle);
		cp->panel->textstyle = NULL;
		gkrellm_chart_destroy(cp);
		gtk_widget_destroy(net->vbox);
		net->chart = NULL;
		net->vbox = NULL;
		net->parent_vbox = NULL;
		}
	net->locked = FALSE;
	}


static GkrellmSizeAbbrev	current_bytes_abbrev[]	=
	{
	{ KB_SIZE(1),		1,				"%.0f" },
	{ KB_SIZE(20),		KB_SIZE(1),		"%.1fK" },
	{ MB_SIZE(1),		KB_SIZE(1),		"%.0fK" },
	{ MB_SIZE(20),		MB_SIZE(1),		"%.1fM" },
	{ GB_SIZE(1),		MB_SIZE(1),		"%.0fM" },
	{ GB_SIZE(20),		GB_SIZE(1),		"%.1fG" },
	{ TB_SIZE(1),		GB_SIZE(1),		"%.0fG" },
	{ TB_SIZE(20),		TB_SIZE(1),		"%.1fT" },
	{ TB_SIZE(1000),	TB_SIZE(1),		"%.0fT" }
	};

static GkrellmSizeAbbrev	total_bytes_abbrev[]	=
	{
	{ KB_SIZE(100),		1,				"%.0f" },
	{ MB_SIZE(1),		KB_SIZE(1),		"%.1fK" },
	{ MB_SIZE(10),		MB_SIZE(1),		"%.3fM" },
	{ MB_SIZE(100),		MB_SIZE(1),		"%.2fM" },
	{ GB_SIZE(1),		MB_SIZE(1),		"%.1fM" },
	{ GB_SIZE(10),		GB_SIZE(1),		"%.3fG" },
	{ GB_SIZE(100),		GB_SIZE(1),		"%.2fG" },
	{ TB_SIZE(1),		GB_SIZE(1),		"%.1fG" },
	{ TB_SIZE(10),		TB_SIZE(1),		"%.3fT" },
	{ TB_SIZE(100),		TB_SIZE(1),		"%.2fT" },
	{ TB_SIZE(1000),	TB_SIZE(1),		"%.1fT" }
	};

#define	DEFAULT_TEXT_FORMAT	"$T\\b\\c\\f$L"

static gchar    *text_format,
				*text_format_locale;

static void
format_net_data(NetMon *net, gchar *src_string, gchar *buf, gint size)
	{
	GkrellmChart	*cp;
	gchar			c, *s, *s1, *result;
	gint			len, bytes;
	gdouble			fbytes;
	gboolean		month, day, week;

	if (!buf || size < 1)
		return;
	--size;
	*buf = '\0';
	result = buf;

	if (!src_string)
		return;
	cp = net->chart;
	net->totals_shown = FALSE;

	if ((_GK.debug_level & DEBUG_CHART_TEXT))
		printf("net chart text: %s\n", src_string);

	for (s = src_string; *s != '\0' && size > 0; ++s)
		{
		len = 1;
		month = week = day = FALSE;
		if (*s == '$' && *(s + 1) != '\0')
			{
			bytes = -1;
			fbytes = -1.0;
			if ((c = *(s + 2)) == 'm')	/* cumulative modifiers */
				month = TRUE;
			else if (c == 'w')
				week = TRUE;
			else if (c == 'd')
				day = TRUE;

			if ((c = *(s + 1)) == 'T')
				bytes = net->rx_current + net->tx_current;
			else if (c == 'M')
				bytes = gkrellm_get_chart_scalemax(cp);
			else if (c == 't')
				bytes = net->tx_current;
			else if (c == 'r')
				bytes = net->rx_current;
			else if (c == 'o')
				{
				if (month)
					fbytes = net->month_stats[0].tx;
				else if (week)
					fbytes = net->week_stats[0].tx;
				else if (day)
					fbytes = net->day_stats[0].tx;
				else
					fbytes = net->show_totalB
								? net->tx_totalB : net->tx_totalA;
				}
			else if (c == 'i')
				{
				if (month)
					fbytes = net->month_stats[0].rx;
				else if (week)
					fbytes = net->week_stats[0].rx;
				else if (day)
					fbytes = net->day_stats[0].rx;
				else
					fbytes = net->show_totalB
								? net->rx_totalB : net->rx_totalA;
				}
			else if (c == 'O')
				{
				if (month)
					fbytes = net->month_stats[0].rx + net->month_stats[0].tx;
				else if (week)
					fbytes = net->week_stats[0].rx + net->week_stats[0].tx;
				else if (day)
					fbytes = net->day_stats[0].rx + net->day_stats[0].tx;
				else
					fbytes = net->show_totalB
							? (net->rx_totalB + net->tx_totalB)
							: (net->rx_totalA + net->tx_totalA);
				}
			else if (c == 'L')
				{
				if (!*(net->label))
					s1 = " ";
				else
					s1 = net->label;
				len = snprintf(buf, size, "%s", s1);
				}
			else if (c == 'I')
				len = snprintf(buf, size, "%s", net->name);
			else if (c == 'H')
				len = snprintf(buf, size, "%s", gkrellm_sys_get_host_name());
			else if ((s1 = gkrellm_plugin_get_exported_label(mon_net, c,
						net->name)) != NULL)
				len = snprintf(buf, size, "%s", s1);
			else
				{
				*buf = *s;
				if (size > 1)
					{
					*(buf + 1) = *(s + 1);
					++len;
					}
				}
			if (bytes >= 0)
				len = gkrellm_format_size_abbrev(buf, size, (gfloat) bytes,
					&current_bytes_abbrev[0],
					sizeof(current_bytes_abbrev) / sizeof(GkrellmSizeAbbrev));
			else if (fbytes >= 0)
				{
				len = gkrellm_format_size_abbrev(buf, size, (gfloat) fbytes,
					&total_bytes_abbrev[0],
					sizeof(total_bytes_abbrev) /sizeof(GkrellmSizeAbbrev));
				if (!day && !week && !month)
					net->totals_shown = TRUE;
				}
			++s;
			if (day || week || month)
				++s;
			}
		else
			*buf = *s;
		size -= len;
		buf += len;
		}
	*buf = '\0';

	if ((_GK.debug_level & DEBUG_CHART_TEXT))
		printf("              : %s\n", result);
	}

static void
draw_net_chart_labels(NetMon *net)
	{
	GkrellmChart		*cp = net->chart;
	gchar		buf[128];

	if (!net->chart_labels)
		return;
	format_net_data(net, text_format_locale, buf, sizeof(buf));
	if (!net->new_text_format)
		gkrellm_chart_reuse_text_format(cp);
	net->new_text_format = FALSE;
	gkrellm_draw_chart_text(cp, net_style_id, buf);
	}

static void
cb_command_process(GkrellmAlert *alert, gchar *src, gchar *dst, gint len,
			NetMon *net)
	{
	format_net_data(net, src, dst, len);
	}

static void
draw_chart_buttons(NetMon *net)
	{
	gint	frame, x, y;

	if (   net->totals_shown && net->chart_labels
		&& decal_totalA && decal_totalB && decal_reset
	   )
		{
		x = gkrellm_chart_width() - 2 * decal_totalA->w;
		y = 2;

		frame = net->show_totalB ? D_MISC_BUTTON_OUT : D_MISC_BUTTON_IN;
		gkrellm_draw_decal_pixmap(NULL, decal_totalA, frame);
		gkrellm_draw_decal_on_chart(net->chart, decal_totalA, x, y);

		frame = net->show_totalB ? D_MISC_BUTTON_IN : D_MISC_BUTTON_OUT;
		gkrellm_draw_decal_pixmap(NULL, decal_totalB, frame);
		gkrellm_draw_decal_on_chart(net->chart, decal_totalB,
					x + decal_totalA->w, y);

		frame = net->reset_button_in ? D_MISC_BUTTON_IN : D_MISC_BUTTON_OUT;
		gkrellm_draw_decal_pixmap(NULL, decal_reset, frame);
		gkrellm_draw_decal_on_chart(net->chart, decal_reset,
					x + decal_totalA->w, y + decal_totalA->h + 4);
		}
	if (decal_stats)
		{
		x = gkrellm_chart_width() - decal_stats->w;
		y = net->chart->h - decal_stats->h;
		frame = net->stats_button_in ? D_MISC_BUTTON_IN : D_MISC_BUTTON_OUT;
		gkrellm_draw_decal_pixmap(NULL, decal_stats, frame);
		gkrellm_draw_decal_on_chart(net->chart, decal_stats, x, y);
		}
	}

static void
refresh_net_chart(NetMon *net)
	{
	if (!net->chart)
		return;
	gkrellm_draw_chartdata(net->chart);
	draw_net_chart_labels(net);
	if (net->mouse_in_chart)
		draw_chart_buttons(net);
	gkrellm_draw_chart_to_screen(net->chart);
	}


static gint
cb_chart_press(GtkWidget *widget, GdkEventButton *ev, NetMon *net)
	{
	gboolean	check_button;

	check_button = net->chart_labels && net->totals_shown;
	if (check_button && gkrellm_in_decal(decal_totalA, ev))
		{
		net->show_totalB = FALSE;
		refresh_net_chart(net);
		}
	else if (check_button && gkrellm_in_decal(decal_totalB, ev))
		{
		net->show_totalB = TRUE;
		refresh_net_chart(net);
		}
	else if (check_button && gkrellm_in_decal(decal_reset, ev))
		{
		net->reset_button_in = TRUE;
		refresh_net_chart(net);
		}
	else if (gkrellm_in_decal(decal_stats, ev))
		{
		net->stats_button_in = TRUE;
		refresh_net_chart(net);
		}
	else if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS)
		{
		net->chart_labels = !net->chart_labels;
		gkrellm_config_modified();
		refresh_net_chart(net);
		}
	else if (   ev->button == 3
			 || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS)
			)
			gkrellm_chartconfig_window_create(net->chart);

	return FALSE;
	}

static gint
cb_chart_release(GtkWidget *widget, GdkEventButton *ev, NetMon *net)
	{
	if (!net->reset_button_in && !net->stats_button_in)
		return FALSE;
	if (net->stats_button_in && gkrellm_in_decal(decal_stats, ev))
		net_stats_window_show(net);
	if (net->reset_button_in && gkrellm_in_decal(decal_reset, ev))
		{
		if (net->show_totalB)
			net->rx_totalB = net->tx_totalB = 0;
		else
			net->rx_totalA = net->tx_totalA = 0;
		}
	net->reset_button_in = FALSE;
	net->stats_button_in = FALSE;
	refresh_net_chart(net);
	return FALSE;
	}

static gint
cb_chart_enter(GtkWidget *w, GdkEventButton *ev, NetMon *net)
	{
	net->mouse_in_chart = TRUE;
	draw_chart_buttons(net);
	gkrellm_draw_chart_to_screen(net->chart);
	return FALSE;
	}

static gint
cb_chart_leave(GtkWidget *w, GdkEventButton *ev, NetMon *net)
	{
	net->mouse_in_chart = FALSE;
	net->reset_button_in = FALSE;
	net->stats_button_in = FALSE;
	refresh_net_chart(net);
	return FALSE;
	}

static gint
cb_panel_press(GtkWidget *widget, GdkEventButton *ev)
	{
	if (ev->button == 3)
		gkrellm_open_config_window(mon_net);
	return FALSE;
	}

  /* Make sure net charts appear in same order as the sorted net_mon_list
  */
static void
net_chart_reorder(NetMon *new_net)
	{
	NetMon		*net;
	GList		*list;
	gint		i;

	for (i = 0, list = net_mon_list; list; list = list->next)
		{
		net = (NetMon *) list->data;
		if (net->parent_vbox != new_net->parent_vbox)
			continue;
		if (net == new_net)
			break;
		++i;
		}
	if (!list || !list->next)
		return;
	gtk_box_reorder_child(GTK_BOX(new_net->parent_vbox), new_net->vbox, i);
	}

static void
create_net_monitor(GtkWidget *vbox, NetMon *net, gint first_create)
	{
	GkrellmStyle	*style;
	GkrellmChart	*cp;
	GkrellmPanel	*p;

	if (first_create)
		{
		net->parent_vbox = vbox;
		net->vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), net->vbox, FALSE, FALSE, 0);

		net->chart = gkrellm_chart_new0();
		net->chart->panel = gkrellm_panel_new0();
		net->chart->panel->textstyle = gkrellm_textstyle_new0();
		net_chart_reorder(net);
		}
	cp = net->chart;
	p = cp->panel;

	gkrellm_chart_create(net->vbox, mon_net, cp, &net->chart_config);
	net->tx_cd = gkrellm_add_default_chartdata(cp, _("tx bytes"));
	net->rx_cd = gkrellm_add_default_chartdata(cp, _("rx bytes"));
	gkrellm_set_draw_chart_function(cp, refresh_net_chart, net);

    gkrellm_chartconfig_fixed_grids_connect(cp->config,
                setup_net_scaling, net);
    gkrellm_chartconfig_grid_resolution_connect(cp->config,
                setup_net_scaling, net);
    gkrellm_chartconfig_grid_resolution_adjustment(cp->config, TRUE,
                0, (gfloat) MIN_GRID_RES, (gfloat) MAX_GRID_RES, 0, 0, 0, 0);

    gkrellm_chartconfig_grid_resolution_label(cp->config,
                _("rx/tx bytes per sec"));
    if (gkrellm_get_chartconfig_grid_resolution(cp->config) < MIN_GRID_RES)
        gkrellm_set_chartconfig_grid_resolution(cp->config,
				grid_resolution_default(net));
    gkrellm_alloc_chartdata(cp);

	style = gkrellm_panel_style(net_style_id);
	gkrellm_create_krell(p, gkrellm_krell_panel_piximage(net_style_id), style);

	net->rxled = gkrellm_create_decal_pixmap(p, decal_net_led_pixmap,
				decal_net_led_mask, N_LEDS, style, 0, _GK.rx_led_y);
	net->rxled->x = map_x(_GK.rx_led_x, net->rxled->w);

	net->txled = gkrellm_create_decal_pixmap(p, decal_net_led_pixmap,
				decal_net_led_mask, N_LEDS, style, 0, _GK.tx_led_y);
	net->txled->x = map_x(_GK.tx_led_x, net->txled->w);

	*(p->textstyle) = *gkrellm_panel_textstyle(net_style_id);
	if (strlen(net->name) > 5)
		p->textstyle->font = gkrellm_panel_alt_textstyle(net_style_id)->font;

	gkrellm_panel_configure(p, net->name, style);
	gkrellm_panel_create(net->vbox, mon_net, p);

	setup_net_scaling(cp->config, net);

	net->new_text_format = TRUE;
	if (first_create)
		{
		g_signal_connect(G_OBJECT (cp->drawing_area), "expose_event",
					G_CALLBACK(net_expose_event), NULL);
		g_signal_connect(G_OBJECT(cp->drawing_area), "button_press_event",
					G_CALLBACK(cb_chart_press), net);
		g_signal_connect(G_OBJECT(cp->drawing_area), "button_release_event",
					G_CALLBACK(cb_chart_release), net);
		g_signal_connect(G_OBJECT(cp->drawing_area), "enter_notify_event",
					G_CALLBACK(cb_chart_enter), net);
		g_signal_connect(G_OBJECT(cp->drawing_area), "leave_notify_event",
					G_CALLBACK(cb_chart_leave), net);

		g_signal_connect(G_OBJECT (p->drawing_area), "expose_event",
					G_CALLBACK(net_expose_event), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area), "button_press_event",
					G_CALLBACK(cb_panel_press), NULL);

		gtk_widget_show_all(net->vbox);
		}
	else
		refresh_net_chart(net);

	gkrellm_setup_launcher(p, &net->launch, CHART_PANEL_TYPE, 0);

	draw_led(net, RX_LED, RX_OFF);
	draw_led(net, TX_LED, TX_OFF);

	if (net->force_up)
		net->locked = TRUE;
	}

static void
net_timer_visibility(void)
	{
	if (timer_button_enabled)
		{
		gkrellm_panel_show(timer_panel);
		gkrellm_spacers_show(mon_timer);
		}
	else
		{
		gkrellm_panel_hide(timer_panel);
		gkrellm_spacers_hide(mon_timer);
		}
	}


static gint
days_in_year(gint y)
	{
	if((y % 4) == 0)
		return 366;
	return 365;
	}

  /* Adjust a tm date structure to reference its next day.
  */
static void
next_day(struct tm *t)
	{
	if ((t->tm_wday += 1) > SATURDAY)
		t->tm_wday = SUNDAY;
	if ((t->tm_yday += 1) >= days_in_year(t->tm_year) )
		{
		t->tm_year += 1;
		t->tm_yday = 0;
		}
	if ((t->tm_mday += 1) > days_in_month[t->tm_mon])
		{
		if (   t->tm_mon == 1
		    && (((gint)t->tm_year - 80) % 4) == 0
		    && t->tm_mday == 29)
			;
		else
			{
			if ((t->tm_mon += 1) > 11)
				t->tm_mon = 0;
			t->tm_mday = 1;
			}
		}
	}

static gboolean
net_accounting_month_new(struct tm *tm)
	{
	if (   tm->tm_mday == reset_mday
		|| (   reset_mday > days_in_month[tm->tm_mon]
			&& tm->tm_mday == 1
		   )
	   )
		return TRUE;
	return FALSE;
	}

void
gkrellm_net_save_data(void)
	{
	FILE		*f;
	GList		*list;
	struct tm	*tm  = gkrellm_get_current_time();
	NetMon		*net;
	gchar		fname[256];
	gint		i;

	for (list = net_mon_list; list; list = list->next)
		{
		net = (NetMon *) list->data;
		if (!net->enabled)
			continue;
		snprintf(fname, sizeof(fname), "%s%c%s", net_data_dir,
					G_DIR_SEPARATOR, net->name);
		if ((f = g_fopen(fname, "w")) == NULL)
			continue;
		fprintf(f, "%d\n", NET_DATA_VERSION);
		fputs("wday mday month yday year\n", f);
		fprintf(f, "%d %d %d %d %d\n", tm->tm_wday,
					tm->tm_mday, tm->tm_mon, tm->tm_yday, tm->tm_year);

		fputs("[daily]\n", f);
		for (i = 0; i < N_DAY_STATS; ++i)
			fprintf(f, "\"%s\" %.0f %.0f %d\n", net->day_stats[i].date,
					net->day_stats[i].rx, net->day_stats[i].tx,
					net->day_stats[i].connect_time);
		fputs("[weekly]\n", f);
		for (i = 0; i < N_WEEK_STATS; ++i)
			fprintf(f, "\"%s\" %.0f %.0f %d\n", net->week_stats[i].date,
					net->week_stats[i].rx, net->week_stats[i].tx,
					net->week_stats[i].connect_time);
		fputs("[monthly]\n", f);
			for (i = 0; i < N_MONTH_STATS; ++i)
		fprintf(f, "\"%s\" %.0f %.0f %d\n", net->month_stats[i].date,
					net->month_stats[i].rx, net->month_stats[i].tx,
					net->month_stats[i].connect_time);
		fclose(f);
		}
	}

static gchar *
utf8_string(gchar *string)
	{
	gchar	*utf8 = NULL;

	if (   g_utf8_validate(string, -1, NULL)
	    || (utf8 = g_locale_to_utf8(string, -1, NULL, NULL, NULL)) == NULL
	   )
		utf8 = g_strdup(string);

	return utf8;
	}

static void
net_stat_set_date_string(NetStat *ns, enum StatType stat_type, struct tm *t)
	{
	struct tm	tm  = *t,
				tmx;
	gchar		*utf8, buf[32], bufa[32], bufb[32];

	if (stat_type == DAY_STAT)
		{
		strftime(buf, sizeof(buf), GK_NET_ISO_DATE, &tm);
		}
	else if (stat_type == WEEK_STAT)
		{
		while (tm.tm_wday != SATURDAY)
			next_day(&tm);
		strftime(buf, sizeof(buf), GK_NET_ISO_DATE, &tm);
		}
	else if (stat_type == MONTH_STAT)
		{
		if (reset_mday == 1)
			strftime(buf, sizeof(buf), "%B", &tm);	/* full month name */
		else
			{
			tmx = tm;
			if (tm.tm_mday < reset_mday)
				{
				tmx.tm_mon -= 1;
				if (tmx.tm_mon < 0)
					{
					tmx.tm_mon = 11;
					tmx.tm_year -= 1;
					}
				}
			else
				{
				tm.tm_mon += 1;
				if (tm.tm_mon > 11)
					{
					tm.tm_mon = 0;
					tm.tm_year += 1;
					}
				}
			tmx.tm_mday = reset_mday;
			tm.tm_mday = reset_mday - 1;
			if (tmx.tm_mday > days_in_month[tmx.tm_mon])
				tmx.tm_mday = days_in_month[tmx.tm_mon];
			if (tm.tm_mday > days_in_month[tm.tm_mon])
				tm.tm_mday = days_in_month[tm.tm_mon];
			strftime(bufa, sizeof(bufa), "%b %e", &tmx);
			strftime(bufb, sizeof(bufb), "%b %e", &tm);
			snprintf(buf, sizeof(buf), "%s - %s", bufa, bufb);
			}
		}
	else
		return;

	g_free(ns->date);
	utf8 = utf8_string(buf);
	ns->date = utf8 ? utf8 : g_strdup("??");
	}

static void
net_stat_init(NetMon *net)
	{
	struct tm	*tm = gkrellm_get_current_time();
	gint		i;

	for (i = 0; i < N_DAY_STATS; ++i)
		net->day_stats[i].date = g_strdup("---");
	for (i = 0; i < N_WEEK_STATS; ++i)
		net->week_stats[i].date = g_strdup("---");
	for (i = 0; i < N_MONTH_STATS; ++i)
		net->month_stats[i].date = g_strdup("---");

	net_stat_set_date_string(&net->day_stats[0], DAY_STAT, tm);
	net_stat_set_date_string(&net->week_stats[0], WEEK_STAT, tm);
	net_stat_set_date_string(&net->month_stats[0], MONTH_STAT, tm);
	}

static void
net_stats_shift_down(NetStat *ns, gint n_stats, enum StatType stat_type)
	{
	gint	d;

	g_free(ns[n_stats - 1].date);

	for (d = n_stats - 1; d > 0; --d)
		ns[d] = ns[d - 1];
	ns->rx = ns->tx = 0.0;
	ns->connect_time = 0;
	ns->date = NULL;
	net_stat_set_date_string(ns, stat_type, gkrellm_get_current_time());
	}

static void
load_net_data(void)
	{
	FILE		*f;
	GList		*list;
	struct tm	*tm  = gkrellm_get_current_time();
	NetMon		*net;
	NetStat		*ns = NULL;
	gchar		buf[128], fname[256], date[32];
	gint		wday, mday, month, yday, year, version;
	gint		day_delta, month_delta, n_stats = 0;

	for (list = net_mon_list; list; list = list->next)
		{
		net = (NetMon *) list->data;
		snprintf(fname, sizeof(fname), "%s%c%s", net_data_dir,
					G_DIR_SEPARATOR, net->name);
		if ((f = g_fopen(fname, "r")) == NULL)
			continue;
		fgets(buf, sizeof(buf), f);
		if (sscanf(buf, "%d\n", &version) != 1)
			{
			fclose(f);
			continue;
			}

		fgets(buf, sizeof(buf), f);		/* Comment line */
		fgets(buf, sizeof(buf), f);
		sscanf(buf, "%d %d %d %d %d", &wday, &mday, &month, &yday, &year);

		if (version == 1)
			{
			struct tm	tmx = *gkrellm_get_current_time();

			tmx.tm_wday = wday;
			tmx.tm_mday = mday;
			tmx.tm_mon = month;
			tmx.tm_yday = yday;
			tmx.tm_year = year;

			fgets(buf, sizeof(buf), f);		/* day */
			fgets(buf, sizeof(buf), f);
			sscanf(buf, "%lf %lf",
						&net->day_stats[0].rx, &net->day_stats[0].tx);
			fgets(buf, sizeof(buf), f);		/* week */
			fgets(buf, sizeof(buf), f);
			sscanf(buf, "%lf %lf",
						&net->week_stats[0].rx, &net->week_stats[0].tx);
			fgets(buf, sizeof(buf), f);		/* month */
			fgets(buf, sizeof(buf), f);
			sscanf(buf, "%lf %lf",
						&net->month_stats[0].rx, &net->month_stats[0].tx);

			net_stat_set_date_string(&net->day_stats[0], DAY_STAT, &tmx);
			net_stat_set_date_string(&net->week_stats[0], WEEK_STAT, &tmx);
			net_stat_set_date_string(&net->month_stats[0], MONTH_STAT, &tmx);

			if (   net == net_timed
				&& fgets(buf, sizeof(buf), f)	/* connect day*/
			   )
				{
				fgets(buf, sizeof(buf), f);
				sscanf(buf, "%d %d %d",
						&net->day_stats[0].connect_time,
						&net->week_stats[0].connect_time,
						&net->month_stats[0].connect_time);
				}

			}
		else if (version == NET_DATA_VERSION)
			{
			while (fgets(buf, sizeof(buf), f) != NULL)
				{
				if (!strncmp(buf, "[daily]", 7))
					{
					ns = &net->day_stats[0];
					n_stats = N_DAY_STATS;
					}
				else if (!strncmp(buf, "[weekly]", 8))
					{
					ns = &net->week_stats[0];
					n_stats = N_WEEK_STATS;
					}
				else if (!strncmp(buf, "[monthly]", 9))
					{
					ns = &net->month_stats[0];
					n_stats = N_MONTH_STATS;
					}
				else if (   ns && n_stats > 0
				         && sscanf(buf, "\"%31[^\"]\" %lf %lf %d",
				               date, &ns->rx, &ns->tx, &ns->connect_time) == 4
				        )
					{
					gkrellm_dup_string(&ns->date, date);
					++ns;
					--n_stats;
					}
				}
			}
		else
			{
			fclose(f);
			continue;
			}

		fclose(f);

		month_delta = (tm->tm_year * 12 + tm->tm_mon) - (year * 12 + month);
		if (month_delta == 0)
			day_delta = tm->tm_mday - mday;
		else if (month_delta == 1)
			day_delta = days_in_month[month] - mday + tm->tm_mday;
		else
			day_delta = 7;		/* max compared to is 6 */

		if (day_delta > 0)
			net_stats_shift_down(&net->day_stats[0], N_DAY_STATS, DAY_STAT);

		if (wday + day_delta != tm->tm_wday)
			net_stats_shift_down(&net->week_stats[0], N_WEEK_STATS, WEEK_STAT);

		if (   (   tm->tm_mday < reset_mday
				&& (   (month_delta > 1)
					|| (month_delta == 1 && mday < reset_mday)
				   )
			   )
			|| (   tm->tm_mday >= reset_mday
				&& (   (month_delta > 0)
					|| (month_delta == 0 && mday < reset_mday)
				   )
			   )
		   )
			net_stats_shift_down(&net->month_stats[0], N_MONTH_STATS,
						MONTH_STAT);
		}
	}


#define SEC_PAD	0

static void
create_net_timer(GtkWidget *vbox, gint first_create)
	{
	GkrellmPanel		*p;
	GkrellmStyle		*style;
	GkrellmTextstyle	*ts, *ts_alt;
	GkrellmMargin		*m;
	GkrellmBorder		*tb;
	gint				top_margin, bot_margin;
	gint				x, y, w, h, w_avail;

	if (first_create)
		timer_panel = gkrellm_panel_new0();
	p = timer_panel;

	style = gkrellm_meter_style(timer_style_id);
	ts = gkrellm_meter_textstyle(timer_style_id);
	ts_alt = gkrellm_meter_alt_textstyle(timer_style_id);
	m = gkrellm_get_style_margins(style);
	tb = &bg_timer_style->border;

	button_decal = gkrellm_create_decal_pixmap(p, decal_timer_button_pixmap,
			decal_timer_button_mask, N_TB_DECALS, style, -1, -1);
	button_decal->x = gkrellm_chart_width() - m->right - button_decal->w;

	w_avail = button_decal->x - m->left - 3;
	if (bg_timer_piximage)
		w_avail -= tb->left + tb->right;
	w = gkrellm_gdk_string_width(ts->font, "0000:0000");
	sec_pad = gkrellm_gdk_string_width(ts->font, "0") * 2 / 3;
	if (w > w_avail)
		w = w_avail;

	time_decal = gkrellm_create_decal_text(p, "0:", ts, style, -1, -1, w);
	seconds_decal = gkrellm_create_decal_text(p, "00",
				ts_alt, style, -1, -1, 0);

	gkrellm_panel_configure(p, NULL, style);

	/* Some special work needed here if there is a bg_timer.  I have so
	|  far the time_decal and button_decal fitting inside top/bottom margins.
	|  If I add bg_timer borders to time_decal height and that ends up higher
	|  than button_decal, I will need to grow the panel height.
	*/
	h = time_decal->h;
	if (bg_timer_piximage)
		{
		gkrellm_get_top_bottom_margins(style, &top_margin, &bot_margin);
		h += tb->top + tb->bottom;
		time_decal->y += tb->top;
		time_decal->x += tb->left;
		if (h > button_decal->h)	/* Need to grow the height ? */
			{
			gkrellm_panel_configure_set_height(p, h + top_margin + bot_margin);
			button_decal->y += (h - button_decal->h) / 2;
			}
		else	/* button_decal->y is OK */
			time_decal->y += (button_decal->h - h) / 2;
		}
	else
		{
		/* time_decal and button_decal are initially at same y = top_margin
		*/
		if (time_decal->h > button_decal->h)
			button_decal->y += (time_decal->h - button_decal->h) / 2;
		else
			time_decal->y += (button_decal->h - time_decal->h) / 2;
		}

	seconds_decal->y = time_decal->y + time_decal->h - seconds_decal->h;
	gkrellm_panel_create(vbox, mon_timer, p);

	gkrellm_move_decal(p, seconds_decal,
				time_decal->x + time_decal->w - seconds_decal->w,
				seconds_decal->y);

	if (first_create)
		{
		g_signal_connect(G_OBJECT (p->drawing_area), "expose_event",
					G_CALLBACK(net_expose_event), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area), "button_release_event",
					G_CALLBACK(cb_timer_button_release), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area), "leave_notify_event",
					G_CALLBACK(cb_timer_button_release), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area), "button_press_event",
					G_CALLBACK(cb_timer_button_press), NULL);
		}

	if (bg_timer_piximage)
		{
		w += tb->left + tb->right;
		x = time_decal->x - tb->left;
		y = time_decal->y - tb->top;
		gkrellm_paste_piximage(bg_timer_piximage, p->pixmap, x, y, w, h);
		gkrellm_paste_piximage(bg_timer_piximage, p->bg_pixmap, x, y, w, h);
		gdk_draw_drawable(p->bg_text_layer_pixmap, _GK.draw1_GC, p->bg_pixmap,
                0, 0,  0, 0,  p->w, p->h);

		}

	set_timer_button_state(timer_button_state);
	}

static void
update_net(void)
	{
	GList			*list;
	NetMon			*net;
	struct tm		*tm;
	GkrellmPanel	*p;
	gint			bytes;
	gdouble			rxd, txd;

	/* If sysdep code is not reporting route up/down events, then
	|  gkrellm_net_assign_data() sets a net as up if data is assigned for it.
	|  So, once a second, compare a net up state before a data read to the
	|  state after a read and use that to internally generate up/down events.
	|  If sysdep code assigns data even if a net is not routed, then there
	|  will be no automatic chart toggling and charts will always be visible
	|  if enabled.
	*/
	if (GK.second_tick)
		{
		if (!net_use_routed)
			{
			for (list = net_mon_list; list; list = list->next)
				{
				net = (NetMon *) list->data;
				net->up_prev = net->up;
				net->up = FALSE;
				}
			}
		else
			(*check_net_routes)();
		}
	(*read_net_data)();
	if (GK.second_tick && !net_use_routed)
		{
		for (list = net_mon_list; list; list = list->next)
			{
			net = (NetMon *) list->data;
			if (net->up && !net->up_prev)
				net->up_event = TRUE;
			else if (!net->up && net->up_prev)
				net->down_event = TRUE;
			}
		}
	for (list = net_mon_list; list; list = list->next)
		{
		net = (NetMon *) list->data;
		if (!net->chart)
			continue;
		p = net->chart->panel;
		if (net->up || net->force_up)
			{
			if (net->rx > net->rx_old)
				draw_led(net, RX_LED, RX_ON);
			else
				draw_led(net, RX_LED, RX_OFF);
			if (net->tx > net->tx_old)
				draw_led(net, TX_LED, TX_ON);
			else
				draw_led(net, TX_LED, TX_OFF);
			}
		net->rx_old = net->rx;
		net->tx_old = net->tx;
		if (GK.second_tick)
			{
			gkrellm_store_chartdata(net->chart, 0, net->tx, net->rx);
			net->rx_current = gkrellm_get_current_chartdata(net->rx_cd);
			net->tx_current = gkrellm_get_current_chartdata(net->tx_cd);
			rxd = (gdouble) net->rx_current;
			txd = (gdouble) net->tx_current;
			net->rx_totalA += rxd;
			net->tx_totalA += txd;
			net->rx_totalB += rxd;
			net->tx_totalB += txd;

			if (GK.day_tick)
				{
				tm = gkrellm_get_current_time();
				net_stats_shift_down(&net->day_stats[0], N_DAY_STATS,
							DAY_STAT);

				if (tm->tm_wday == 0)
					net_stats_shift_down(&net->week_stats[0], N_WEEK_STATS,
								WEEK_STAT);

				if (net_accounting_month_new(tm))
					net_stats_shift_down(&net->month_stats[0], N_MONTH_STATS,
								MONTH_STAT);
				}
			net->day_stats[0].rx   += rxd;
			net->week_stats[0].rx  += rxd;
			net->month_stats[0].rx += rxd;

			net->day_stats[0].tx   += txd;
			net->week_stats[0].tx  += txd;
			net->month_stats[0].tx += txd;

			if (net->alert)
				{
				bytes = 0;
				if (net->alert_uses_rx)
					bytes += net->rx_current;
				if (net->alert_uses_tx)
					bytes += net->tx_current;
				gkrellm_check_alert(net->alert, bytes);
				}
			gkrellm_panel_label_on_top_of_decals(p,
						gkrellm_alert_decal_visible(net->alert));
			refresh_net_chart(net);
			}
		gkrellm_update_krell(p, KRELL(p), net->tx + net->rx);
		gkrellm_draw_panel_layers(p);
		}
	if (GK.second_tick)
		{
		update_timer_button_monitor();
		timed_net_visibility();
		if (net_timed && !net_timed->up && !net_timed->force_up)
			{
			draw_led(net_timed, RX_LED, RX_OFF);
			draw_led(net_timed, TX_LED, TX_OFF);
			}
		for (list = net_mon_list; list; list = list->next)
			{
			net = (NetMon *) list->data;
			if (!net->locked)
				{
				if (net->up_event && !net->chart && net->enabled)
					{
					create_net_monitor(dynamic_net_vbox, net, TRUE);
					gkrellm_pack_side_frames();
					}
				else if (net->down_event && net->chart)
					{
					destroy_chart(net);
					gkrellm_pack_side_frames();
					}
				}
			net->up_event = net->down_event = FALSE;
			}
		}
	if (GK.hour_tick)
		{
		tm = gkrellm_get_current_time();
		if ((tm->tm_hour % 6) == 0)
			gkrellm_net_save_data();
		}
	}

  /* A timed interface has its chart locked and is forced enabled.
  */
static void
create_timed_monitor(void)
	{
	GList		*list;
	NetMon		*net;
	TimerType	*tt;

	if (_GK.client_mode)
		return;

	net_timed = NULL;
	timer_button_type = TIMER_TYPE_NONE;
	time(&net_timer0);

	if (!*timer_button_iface || !strcmp(timer_button_iface, "none"))
		return;

	/* Making a timed mon out of one that is already up?  It needs to be
	|  moved to a different vbox, so destroy and create dance.
	*/
	for (list = net_mon_list; list; list = list->next)
		{
		net = (NetMon *) list->data;
		if (!strcmp(net->name, timer_button_iface))
			{
			destroy_chart(net);
			break;
			}
		}
	if ((net = lookup_net(timer_button_iface)) == NULL)
		net = new_net(timer_button_iface);

	net->enabled = TRUE;
	net->locked = TRUE;
	create_net_monitor(net_vbox, net, TRUE);
	if (!net->up)
		gkrellm_chart_hide(net->chart, FALSE);	/* Don't hide its panel */

	net_timed = net;
	for (list = timer_defaults_list; list; list = list->next)
		{
		tt = (TimerType *) list->data;
		if (!strncmp(timer_button_iface, tt->name, strlen(tt->name) - 1))
			{
			timer_button_type = tt->type;
			break;
			}
		}
	get_connect_time();
	}


static void
load_net_extra_piximages(void)
	{
	gchar			**xpm;
	gint			w, h;

	/* Check for theme_dir/net/decal_net_leds.png.
	*/
	gkrellm_load_piximage("decal_net_leds", decal_net_leds_xpm,
			&decal_net_led_piximage, NET_STYLE_NAME);

	w = gdk_pixbuf_get_width(decal_net_led_piximage->pixbuf);
	w *= gkrellm_get_theme_scale();

	h = gdk_pixbuf_get_height(decal_net_led_piximage->pixbuf) / N_LEDS;
	h *= gkrellm_get_theme_scale();

	gkrellm_scale_piximage_to_pixmap(decal_net_led_piximage,
				&decal_net_led_pixmap, &decal_net_led_mask, w, h * N_LEDS);

	/* Check for theme_dir/net/decal_timer_button.png
	*/
	gkrellm_load_piximage("decal_timer_button", decal_timer_button_xpm,
			&decal_timer_button_piximage, TIMER_STYLE_NAME);
	h = gdk_pixbuf_get_height(decal_timer_button_piximage->pixbuf)
				/ N_TB_DECALS;
	h *= gkrellm_get_theme_scale();
	gkrellm_scale_piximage_to_pixmap(decal_timer_button_piximage,
			&decal_timer_button_pixmap, &decal_timer_button_mask,
			-1, h * N_TB_DECALS);

	/* Here is where I define the net timer panel theme extensions.  I ask
	|  for a theme extension image:
	|      THEME_DIR/timer/bg_timer.png
	|  and for a border for it from the gkrellmrc in the format:
	|      set_piximage_border timer_bg_timer l,r,t,b
	| There is no default for bg_timer, ie it may end up being NULL.
	*/
	if (!bg_timer_style)		/* Used just for the bg_timer border */
		bg_timer_style = gkrellm_style_new0();
	if (bg_timer_piximage)
		gkrellm_destroy_piximage(bg_timer_piximage);
	bg_timer_piximage = NULL;
	xpm = (gkrellm_using_default_theme()) ? bg_timer_xpm : NULL;
	gkrellm_load_piximage("bg_timer", xpm,
				&bg_timer_piximage, TIMER_STYLE_NAME);
	gkrellm_set_gkrellmrc_piximage_border("timer_bg_timer",
				bg_timer_piximage, bg_timer_style);
	}

static void
net_spacer_visibility(void)
	{
	GList		*list;
	gboolean	enabled = FALSE;

	for (list = net_mon_list; list; list = list->next)
		if (((NetMon *) list->data)->enabled)
			enabled = TRUE;
	if (timer_button_enabled || enabled)
		gkrellm_spacers_show(mon_net);
	else
		gkrellm_spacers_hide(mon_net);
	}

static void
create_net(GtkWidget *vbox, gint first_create)
	{
	GList	*list;
	NetMon	*net;

	load_net_extra_piximages();

	/* Make a couple of vboxes here so I can control the net layout.
	|  I want interface linked to the timer button  to go last.
	*/
	if (first_create)
		{
		dynamic_net_vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), dynamic_net_vbox, FALSE, FALSE, 0);
		gtk_widget_show(dynamic_net_vbox);

		net_vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), net_vbox, FALSE, FALSE, 0);
		gtk_widget_show(net_vbox);

		(*(read_net_data))();		/* need net up states */

		create_timed_monitor();
		for (list = net_mon_list; list; list = list->next)
			{
			net = (NetMon *) list->data;
			if (net == net_timed)
				continue;
			if ((net->up || net->force_up) && net->enabled)
				create_net_monitor(dynamic_net_vbox, net, first_create);
			}
		load_net_data();
		}
	else
		{
		/* Some decals don't live in a panel, they will be drawn onto
		|  charts when needed.  So, must destroy them at create events (not
		|  done automatically in panel lists) and then recreate them with NULL
		|  panel and style pointers.
		*/
		gkrellm_destroy_decal(decal_totalA);
		gkrellm_destroy_decal(decal_totalB);
		gkrellm_destroy_decal(decal_reset);
		gkrellm_destroy_decal(decal_stats);
		decal_totalA = decal_totalB = decal_reset = decal_stats = NULL;
		for (list = net_mon_list; list; list = list->next)
			{
			net = (NetMon *) list->data;
			if (net->chart)
				create_net_monitor(NULL, net, 0);
			}
		}
	decal_totalA = gkrellm_create_decal_pixmap(NULL,
			gkrellm_decal_misc_pixmap(), gkrellm_decal_misc_mask(),
			N_MISC_DECALS, NULL, 0, 0);
	decal_totalB = gkrellm_create_decal_pixmap(NULL,
			gkrellm_decal_misc_pixmap(), gkrellm_decal_misc_mask(),
			N_MISC_DECALS, NULL, 0, 0);

	decal_reset = gkrellm_create_decal_pixmap(NULL,
			gkrellm_decal_misc_pixmap(), gkrellm_decal_misc_mask(),
			N_MISC_DECALS, NULL, 0, 0);

	decal_stats = gkrellm_create_decal_pixmap(NULL,
			gkrellm_decal_misc_pixmap(), gkrellm_decal_misc_mask(),
			N_MISC_DECALS, NULL, 0, 0);

	net_spacer_visibility();
	}

static void
create_timer(GtkWidget *vbox, gint first_create)
	{
	timer_vbox = vbox;
	create_net_timer(timer_vbox, first_create);
	ascent = 0;
	ascent_alt = 0;
	if (first_create)
		draw_timer(timer_panel, (int) (time(0) - net_timer0), 1);
	else
		draw_timer(timer_panel, last_time , 1);
	gkrellm_draw_panel_layers(timer_panel);
	net_timer_visibility();
	}

#define	NET_CONFIG_KEYWORD	"net"

static void
cb_alert_trigger(GkrellmAlert *alert, NetMon *net)
	{
	/* Full panel alert, default decal.
	*/
	alert->panel = net->chart->panel;
	}

static void
create_alert(NetMon *net)
	{
	net->alert = gkrellm_alert_create(NULL, net->name,
				_("Bytes per second"),
				TRUE, FALSE, TRUE,
				1e10, 1000, 1000, 10000, 0);
	gkrellm_alert_delay_config(net->alert, 1, 60 * 60, 0);

	gkrellm_alert_trigger_connect(net->alert, cb_alert_trigger, net);
	gkrellm_alert_config_connect(net->alert, cb_alert_config, net);
	gkrellm_alert_config_create_connect(net->alert,
							cb_alert_config_create, net);
	gkrellm_alert_command_process_connect(net->alert, cb_command_process, net);
	}


static void
save_net_config(FILE *f)
	{
	GList	*list;
	NetMon	*net;

	for (list = net_mon_list; list; list = list->next)
		{
		net = (NetMon *) list->data;
		if (!net->enabled  && !net->real)
			continue;
		fprintf(f, "%s enables %s %d %d %d\n", NET_CONFIG_KEYWORD, net->name,
				net->enabled, net->chart_labels, net->force_up);
		gkrellm_save_chartconfig(f, net->chart_config,
				NET_CONFIG_KEYWORD, net->name);
		if (*net->label)
			fprintf(f, "%s label %s %s\n", NET_CONFIG_KEYWORD,
					net->name, net->label);
		if (*(net->launch.command))
			fprintf(f, "%s launch %s %s\n", NET_CONFIG_KEYWORD,
					net->name, net->launch.command);
		if (*(net->launch.tooltip_comment))
			fprintf(f, "%s tooltip %s %s\n", NET_CONFIG_KEYWORD,
					net->name, net->launch.tooltip_comment);
		if (net->alert)
			{
			gkrellm_save_alertconfig(f, net->alert,
						NET_CONFIG_KEYWORD, net->name);
			fprintf(f, "%s extra_alert_config %s %d %d\n", NET_CONFIG_KEYWORD,
						net->name,
						net->alert_uses_rx, net->alert_uses_tx);
			}
		}

	if (!_GK.client_mode || timer_button_type != TIMER_TYPE_SERVER)
		fprintf(f, "%s timer_enabled %d\n", NET_CONFIG_KEYWORD,
						timer_button_enabled);
	fprintf(f, "%s timer_seconds %d\n", NET_CONFIG_KEYWORD, timer_seconds);
	if (!_GK.client_mode)
		fprintf(f, "%s timer_iface %s\n", NET_CONFIG_KEYWORD,
				timer_button_iface);
	fprintf(f, "%s timer_on %s\n", NET_CONFIG_KEYWORD, timer_on_command);
	fprintf(f, "%s timer_off %s\n", NET_CONFIG_KEYWORD, timer_off_command);
	fprintf(f, "%s text_format %s\n", NET_CONFIG_KEYWORD, text_format);
	fprintf(f, "%s reset_mday %d\n", NET_CONFIG_KEYWORD, reset_mday);
	fprintf(f, "%s net_stats_window_height %d\n", NET_CONFIG_KEYWORD,
				net_stats_window_height);
	}

static void
load_net_config(gchar *arg)
	{
	NetMon		*net;
	gchar		config[32], name[32];
	gchar		item[CFG_BUFSIZE], item1[CFG_BUFSIZE];
	gboolean	enable = TRUE, ch_labels = TRUE, force = FALSE;
	gint		n;

	n = sscanf(arg, "%31s %[^\n]", config, item);
	if (n != 2)
		return;

	if (!_GK.client_mode || timer_button_type != TIMER_TYPE_SERVER)
		if (!strcmp(config, "timer_enabled") && ! _GK.demo)
			sscanf(item, "%d", &timer_button_enabled);

	if (!strcmp(config, "timer_seconds"))
		sscanf(item, "%d", &timer_seconds);
	else if (!strcmp(config, "reset_mday"))
		sscanf(item, "%d", &reset_mday);
	else if (!strcmp(config, "net_stats_window_height"))
		sscanf(item, "%d", &net_stats_window_height);
	else if (!strcmp(config, "timer_iface") && !_GK.client_mode)
		{
		if (_GK.demo && !strcmp(item, "none"))
			gkrellm_dup_string(&timer_button_iface, "ppp0");
		else
			gkrellm_dup_string(&timer_button_iface, item);
		}
	else if (!strcmp(config, "timer_on"))
		gkrellm_dup_string(&timer_on_command, item);
	else if (!strcmp(config, "timer_off"))
		gkrellm_dup_string(&timer_off_command, item);
	else if (!strcmp(config, "text_format"))
		gkrellm_locale_dup_string(&text_format, item, &text_format_locale);
	else if (sscanf(item, "%31s %[^\n]", name, item1) == 2)
		{
		if (!strcmp(config, "iface"))	/* Hack to get some of 1.0 config */
			{	/* Can't get resolution, label, launch or tooltip */
			sscanf(item1, "%*d %d %*s %d %d", &enable, &ch_labels, &force);
			strcpy(config, "enables");
			sprintf(item1, "%d %d %d", enable, ch_labels, force);
			}
		/* Remaining configs will have a net name
		*/
		if (!strcmp(config, "enables"))
			{
			if ((net = lookup_net(name)) == NULL)
				net = new_net(name);
			sscanf(item1, "%d %d %d", &net->enabled, &net->chart_labels,
						&net->force_up);
			if (!net_use_routed)
				net->force_up = FALSE;
			return;
			}
		if ((net = lookup_net(name)) == NULL)
			return;
		if (!strcmp(config, GKRELLM_CHARTCONFIG_KEYWORD))
			gkrellm_load_chartconfig(&net->chart_config, item1, 2);
		else if (!strcmp(config, GKRELLM_ALERTCONFIG_KEYWORD))
			{
			if (!net->alert)
				create_alert(net);
			gkrellm_load_alertconfig(&net->alert, item1);
			}
		else if (!strcmp(config, "extra_alert_config"))
			sscanf(item1, "%d %d", &net->alert_uses_rx, &net->alert_uses_tx);
		else if (!strcmp(config, "label"))
			gkrellm_dup_string(&net->label, item1);
		else if (!strcmp(config, "launch"))
			gkrellm_dup_string(&net->launch.command, item1);
		else if (!strcmp(config, "tooltip"))
			gkrellm_dup_string(&net->launch.tooltip_comment, item1);
		}
	}


/* -------------- User config interface --------------------- */

#define STEP	1

static GtkWidget	*pon_entry,
					*poff_entry,
					*timer_iface_combo_box,
					*text_format_combo_box;

static GtkWidget	*enable_net_timer_button;
static GtkWidget	*net_timer_seconds_button;

static gboolean		enable_in_progress;


static void
sync_chart(NetMon *net)
	{
	/* If enabling, fake an up_event so update_net() will create the chart.
	*/
	if (net->enabled && !net->chart && (net->up || net->force_up))
		net->up_event = TRUE;
	else if (   net->chart
			 && (!net->enabled || (!net->locked && !net->force_up && !net->up))
			)
		destroy_chart(net);
	}

static void
cb_force(GtkWidget *button, NetMon *net)
	{
	if (enable_in_progress)
		return;
	net->force_up = GTK_TOGGLE_BUTTON(button)->active;
	if (net != net_timed)
		net->locked = FALSE;
	sync_chart(net);
	}

static void
cb_enable(GtkWidget *button, NetMon *net)
	{
	enable_in_progress = TRUE;
	net->enabled = GTK_TOGGLE_BUTTON(button)->active;
	if (net->force_button)
		{
		if (net->enabled)
			gtk_widget_set_sensitive(net->force_button, TRUE);
		else
			{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(net->force_button),
						FALSE);
			gtk_widget_set_sensitive(net->force_button, FALSE);
			net->force_up = FALSE;
			}
		}
	gtk_widget_set_sensitive(net->alert_button, net->enabled);
	if (net == net_timed && !net->enabled && timer_iface_combo_box)
		gtk_combo_box_set_active(GTK_COMBO_BOX(timer_iface_combo_box), 0);
	else
		sync_chart(net);
	enable_in_progress = FALSE;
	net_spacer_visibility();
	}

static void
cb_launch_entry(GtkWidget *widget, NetMon *net)
	{
	if (net->chart)
		gkrellm_apply_launcher(&net->launch_entry, &net->tooltip_entry,
				net->chart->panel, &net->launch, gkrellm_launch_button_cb);
	}

static void
cb_text_format(GtkWidget *widget, gpointer data)
	{
	GList   *list;
	NetMon	*net;
	gchar   *s;
	GtkWidget *entry;

	entry = gtk_bin_get_child(GTK_BIN(text_format_combo_box));
	s = gkrellm_gtk_entry_get_text(&entry);
	gkrellm_locale_dup_string(&text_format, s, &text_format_locale);
	for (list = net_mon_list; list; list = list->next)
		{
		net = (NetMon *) list->data;
		net->new_text_format = TRUE;
		refresh_net_chart(net);
		}
	}

static void
cb_label_entry(GtkWidget *widget, NetMon *net)
	{
	gkrellm_dup_string(&net->label,
						gkrellm_gtk_entry_get_text(&net->label_entry));
	refresh_net_chart(net);
	}

static void
cb_set_alert(GtkWidget *button, NetMon *net)
	{

#if 0
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	NetMon			*net;

	if (!row_reference)
		return;
	model = gtk_tree_view_get_model(treeview);
	path = gtk_tree_row_reference_get_path(row_reference);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, NETMON_COLUMN, &net, -1);
#endif

	if (!net->alert)
		create_alert(net);
	gkrellm_alert_config_window(&net->alert);

#if 0
	gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
				ALERT_COLUMN, net->alert, -1);
#endif
	}

  /* Callback for a created or destroyed alert.  Find the sensor in the model
  |  and set the IMAGE_COLUMN.
  */
static void
cb_alert_config(GkrellmAlert *ap, NetMon *net)
    {
#if 0
	GtkTreeModel	*model;
	GtkTreeIter		iter;
	NetMon			*net_test;
	GdkPixbuf		*pixbuf;
	gchar			node[2];
	gint			i;

	if (!gkrellm_config_window_shown())
		return;
	model = gtk_tree_view_get_model(treeview);
	pixbuf = ap->activated ? gkrellm_alert_pixbuf() : NULL;
	for (i = 0; i < 2; ++i)
		{
		node[0] = '0' + i;      /* toplevel Primary or Secondary node */
		node[1] = '\0';
		if (get_child_iter(model, node, &iter))
			do
				{
				gtk_tree_model_get(model, &iter, NETMON_COLUMN, &net_test, -1);
				if (net != net_test)
					continue;
				gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
							IMAGE_COLUMN, pixbuf, -1);
				return;
				}
			while (gtk_tree_model_iter_next(model, &iter));
		}
#endif
	net->alert_uses_rx =
			GTK_TOGGLE_BUTTON(net->alert_config_rx_button)->active;
	net->alert_uses_tx =
			GTK_TOGGLE_BUTTON(net->alert_config_tx_button)->active;
	}

static void
cb_alert_config_button(GtkWidget *button, NetMon *net)
	{
	gboolean	read, write;

	read = GTK_TOGGLE_BUTTON(net->alert_config_rx_button)->active;
	write = GTK_TOGGLE_BUTTON(net->alert_config_tx_button)->active;
	if (!read && !write)
		{
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(net->alert_config_rx_button), TRUE);
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(net->alert_config_tx_button), TRUE);
		}
	}

static void
cb_alert_config_create(GkrellmAlert *ap, GtkWidget *vbox, NetMon *net)
    {
    gkrellm_gtk_check_button_connected(vbox, &net->alert_config_rx_button,
                net->alert_uses_rx, FALSE, FALSE, 2,
                cb_alert_config_button, net, _("rx bytes"));
    gkrellm_gtk_check_button_connected(vbox, &net->alert_config_tx_button,
                net->alert_uses_tx, FALSE, FALSE, 2,
                cb_alert_config_button, net, _("tx bytes"));
    }


static void
cb_timer_enable(GtkWidget *button, gpointer data)
	{
	gkrellm_panel_enable_visibility(timer_panel,
				GTK_TOGGLE_BUTTON(enable_net_timer_button)->active,
				&timer_button_enabled);
	gtk_widget_set_sensitive(net_timer_seconds_button, timer_button_enabled);
	if (timer_iface_combo_box)
		{
		/* Reset to "none" combo box entry */
		if (!timer_button_enabled)
			gtk_combo_box_set_active(GTK_COMBO_BOX(timer_iface_combo_box), 0);
		gtk_widget_set_sensitive(timer_iface_combo_box, timer_button_enabled);
		}
	net_timer_visibility();
	}

static void
cb_timer_seconds(GtkWidget *button, gpointer data)
	{
	timer_seconds = GTK_TOGGLE_BUTTON(button)->active;
	gkrellm_panel_destroy(timer_panel);
	create_net_timer(timer_vbox, TRUE);
	draw_timer(timer_panel, last_time , 1);
	}

static void
cb_timer_iface(GtkWidget *widget, gpointer data)
	{
	gchar	*s;
	GtkWidget *entry;

	entry = gtk_bin_get_child(GTK_BIN(timer_iface_combo_box));
	s = gkrellm_gtk_entry_get_text(&entry);
	if (*s == '\0' || strcmp(s, _("none")) == 0)
		s = "none";
	if (gkrellm_dup_string(&timer_button_iface, s))
		{
		/* A new timer_button_iface, so destroy old one.  If the old one
		|  is up or forced up, a new chart for it needs to be created in
		|  another vbox, so fake an up_event.
		*/
		if (net_timed)
			{
			destroy_chart(net_timed);
			if (   net_timed->enabled
				&& (net_timed->up || net_timed->force_up)
			   )
				net_timed->up_event = TRUE;
			}
		create_timed_monitor();
		if (net_timed)
			gtk_toggle_button_set_active(
						GTK_TOGGLE_BUTTON(net_timed->enable_button), TRUE);
		draw_timer(timer_panel, (int) (time(0) - net_timer0), 1);
		gkrellm_draw_panel_layers(timer_panel);
		}
	}

static void
cb_pon_entry(GtkWidget *widget, gpointer data)
	{
	gchar	*s;

	s = gkrellm_gtk_entry_get_text(&pon_entry);
	gkrellm_dup_string(&timer_on_command, s);
	}

static void
cb_poff_entry(GtkWidget *widget, gpointer data)
	{
	gchar	*s;

	s = gkrellm_gtk_entry_get_text(&poff_entry);
	gkrellm_dup_string(&timer_off_command, s);
	}

static void
cb_reset_mday(GtkWidget *widget, GtkSpinButton *spin)
	{
	GList		*list;
	NetMon		*net;
	NetStat		*ns;
	struct tm	*tm;
	gint		year, month, mday, month_delta;

	reset_mday = gtk_spin_button_get_value_as_int(spin);

	for (list = net_mon_list; list; list = list->next)
		{
		net = (NetMon *) list->data;
		net->month_stats[0].rx = net->month_stats[0].tx = 0.0;
		net->month_stats[0].connect_time = 0;
		tm = gkrellm_get_current_time();
		net_stat_set_date_string(&net->month_stats[0], MONTH_STAT, tm);

		for (ns = &net->day_stats[0]; ns < &net->day_stats[N_DAY_STATS]; ++ns)
			{
			if (sscanf(ns->date, "%d-%d-%d", &year, &month, &mday) != 3)
				continue;
			year -= 1900;	/* Adjust to struct tm ranges */
			month -= 1;
			month_delta = (tm->tm_year * 12 + tm->tm_mon)
								- (year * 12 + month);
			if (   (   tm->tm_mday < reset_mday
			        && (   (month_delta > 1)
			            || (month_delta == 1 && mday < reset_mday)
			           )
			       )
			    || (   tm->tm_mday >= reset_mday
			        && (   (month_delta > 0)
			            || (month_delta == 0 && mday < reset_mday)
			           )
			       )
			   )
				continue;

			net->month_stats[0].rx += ns->rx;
			net->month_stats[0].tx += ns->tx;
			net->month_stats[0].connect_time += ns->connect_time;
			}
		}
	}

#if !defined(WIN32)
static gchar	*net_info_text0[] =
{
N_("<h>Timer Button\n"),
N_("\tThe timer button may be used as a stand alone timer with start and\n"
	"\tstop commands, but it is usually linked to a dial up net interface\n"
	"\twhere the commands can control the interface and different timer\n"
	"\tbutton colors can show connect states:\n"),
"\n",
#if defined(__FreeBSD__)
"<b>\ttun: ",
#else
"<b>\tppp: ",
#endif
N_("Standby state is while the modem phone line is locked while\n"
	"\tppp is connecting, and the on state is the ppp link connected.\n"
	"\tThe phone line lock is determined by the existence of the modem\n"
	"\tlock file /var/lock/LCK..modem.  If your pppd setup does not\n"
	"\tuse /dev/modem, then you can configure an alternative with:\n"),
N_("<i>\t\tln -s /var/lock/LCK..ttySx ~/.gkrellm2/LCK..modem\n"),
N_("\twhere ttySx is the tty device your modem uses.  The ppp on\n"
	"\tstate is detected by the existence of /var/run/pppX.pid and\n"
	"\tthe time stamp of this file is the base for the on line time.\n"),
#if defined(__linux__)
"\n",
"<b>\tippp: ",
N_("The timer button standby state is not applicable to isdn\n"
	"\tinterfaces that are always routed. The on state is isdn on line\n"
	"\twhile the ippp interface is routed.  The on line timer is reset\n"
	"\twhen the isdn interface transitions from a hangup to an on line\n"
	"\tstate\n"),
#endif
"\n"
};
#endif

static gchar	*net_info_text1[] =
{
N_("<h>Chart Labels\n"),
N_("Substitution variables for the format string for chart labels:\n"),
N_("\t$M    maximum chart value\n"),
N_("\t$T    receive + transmit bytes\n"),
N_("\t$r    receive bytes\n"),
N_("\t$t    transmit bytes\n"),
N_("\t$O    cumulative receive + transmit bytes\n"),
N_("\t$i    cumulative receive bytes\n"),
N_("\t$o    cumulative transmit bytes\n"),
N_("\t$L    the optional chart label\n"),
N_("\t$I    the net interface name\n"),
"\n",
N_("The cumulative variables may have a 'd', 'w', or 'm' qualifier for\n"
   "daily, weekly, or monthly totals.  For example:  $Ow for a\n"
   "cumulative weekly receive + transmit bytes.\n"),
"\n",
N_("Substitution variables may be used in alert commands.\n")
};

static void
create_net_tab(GtkWidget *tab_vbox)
	{
	GtkWidget		*tabs;
	GtkWidget		*table;
	GtkWidget		*vbox, *vbox1;
	GtkWidget		*hbox;
	GtkWidget		*label;
	GtkWidget		*text;
	GList			*list, *tlist;
	NetMon			*net;
	TimerType		*tt;
	gchar			buf[256];
	gint			i;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Timer Button"));

	if (timer_button_type != TIMER_TYPE_SERVER) /* enabled by gkrellmd.conf? */
		gkrellm_gtk_check_button_connected(vbox, &enable_net_timer_button,
					timer_button_enabled, FALSE, FALSE, 10,
					cb_timer_enable, NULL,
					_("Enable timer button"));

	gkrellm_gtk_check_button_connected(vbox, &net_timer_seconds_button,
				timer_seconds, FALSE, FALSE, 0,
				cb_timer_seconds, NULL,
				_("Show seconds"));
	gtk_widget_set_sensitive(net_timer_seconds_button, timer_button_enabled);
	hbox = gtk_hbox_new (FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 50);

	if (!_GK.client_mode)
		{
		timer_iface_combo_box = gtk_combo_box_entry_new_text();
		gtk_box_pack_start(GTK_BOX(hbox), timer_iface_combo_box, TRUE, TRUE, 0);
		gtk_widget_set_sensitive(timer_iface_combo_box, timer_button_enabled);
		gtk_combo_box_append_text(GTK_COMBO_BOX(timer_iface_combo_box),
			_("none"));
		for (tlist = timer_defaults_list; tlist; tlist = tlist->next)
			{
			tt = (TimerType *) tlist->data;
			gtk_combo_box_append_text(GTK_COMBO_BOX(timer_iface_combo_box),
				tt->name);
			}
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(
			GTK_BIN(timer_iface_combo_box))), timer_button_iface);
		g_signal_connect(G_OBJECT(timer_iface_combo_box), "changed",
			G_CALLBACK(cb_timer_iface), NULL);

		label = gtk_label_new(_("Interface to link to the timer button"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
		}
	vbox = gkrellm_gtk_framed_vbox_end(vbox, NULL, 4, FALSE, 0, 2);
	hbox = gtk_hbox_new (FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	vbox1 = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 3);
	gtk_container_add(GTK_CONTAINER(hbox), vbox1);
	label = gtk_label_new(_("Start Command"));
	gtk_box_pack_start(GTK_BOX (vbox1), label, FALSE, TRUE, 0);
	pon_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX (vbox1), pon_entry, FALSE, TRUE, 0);
	gtk_entry_set_text(GTK_ENTRY(pon_entry), timer_on_command);
	g_signal_connect(G_OBJECT(pon_entry),
				"changed", G_CALLBACK(cb_pon_entry), NULL);

	vbox1 = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 3);
	gtk_container_add(GTK_CONTAINER(hbox), vbox1);
	label = gtk_label_new(_("Stop Command"));
	gtk_box_pack_start(GTK_BOX (vbox1), label, FALSE, TRUE, 0);
	poff_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX (vbox1), poff_entry, FALSE, TRUE, 0);
	gtk_entry_set_text(GTK_ENTRY(poff_entry), timer_off_command);
	g_signal_connect(G_OBJECT(poff_entry),
				"changed", G_CALLBACK(cb_poff_entry), NULL);

	for (list = net_mon_list; list; list = list->next)
		{
		net = (NetMon *) list->data;

		vbox = gkrellm_gtk_framed_notebook_page(tabs, net->name);

		snprintf(buf, sizeof(buf), _("Enable %s"), net->name);
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);
		gkrellm_gtk_check_button_connected(hbox,
						&net->enable_button, net->enabled,
						FALSE, FALSE, 0,
						cb_enable, net, buf);
		gkrellm_gtk_alert_button(hbox, &net->alert_button, FALSE, FALSE, 4,
					FALSE, cb_set_alert, net);
		gtk_widget_set_sensitive(net->alert_button, net->enabled);


		if (net_config_use_routed)
			{
			gkrellm_gtk_check_button_connected(vbox, &net->force_button,
						net->enabled ? net->force_up : FALSE, FALSE, FALSE, 0,
						cb_force, net,
		_("Force chart to be always shown even if interface is not routed."));
			if (!net->enabled)
				gtk_widget_set_sensitive(net->force_button, FALSE);
			}

		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 12);

		net->label_entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(net->label_entry), 16);
		gtk_widget_set_size_request(net->label_entry, 70, -1);
		gtk_entry_set_text(GTK_ENTRY(net->label_entry), net->label);
		gtk_box_pack_start (GTK_BOX (hbox), net->label_entry, FALSE, TRUE, 2);
		label = gtk_label_new(_("Optional label for this interface."));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 4);
		g_signal_connect(G_OBJECT(net->label_entry), "changed",
					G_CALLBACK(cb_label_entry), net);

		vbox = gkrellm_gtk_category_vbox(vbox,
					_("Launch Commands"),
					4, 0, TRUE);
		table = gkrellm_gtk_launcher_table_new(vbox, 1);
		gkrellm_gtk_config_launcher(table, 0,  &net->launch_entry,
						&net->tooltip_entry, net->name, &net->launch);
		g_signal_connect(G_OBJECT(net->launch_entry), "changed",
					G_CALLBACK(cb_launch_entry), net);
		g_signal_connect(G_OBJECT(net->tooltip_entry), "changed",
					G_CALLBACK(cb_launch_entry), net);
		}

/* -- Setup tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Format String for Chart Labels"),
				4, 0, TRUE);
	text_format_combo_box = gtk_combo_box_entry_new_text();
	gtk_widget_set_size_request (GTK_WIDGET(text_format_combo_box), 300, -1);
	gtk_box_pack_start(GTK_BOX(vbox1), text_format_combo_box, FALSE, FALSE, 2);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box), text_format);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		DEFAULT_TEXT_FORMAT);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		"\\c\\f$M\\n$T\\b\\c\\f$L");
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		_("\\f\\ww\\c\\f$M\\n\\f\\at\\.$t\\n\\f\\ar\\.$r\\b\\c\\f$L"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		_("\\f\\ww\\c\\f$M\\n\\f\\at\\.$o\\n\\f\\ar\\.$i\\b\\c\\f$L"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		_("\\f\\ww\\c\\f$M\\D2\\f\\ar\\.$r\\D1\\f\\at\\.$t\\b\\c\\f$L"));
	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(text_format_combo_box))),
		text_format);
	g_signal_connect(G_OBJECT(GTK_COMBO_BOX(text_format_combo_box)), "changed",
			G_CALLBACK(cb_text_format), NULL);

	gkrellm_gtk_spin_button(vbox, NULL, (gfloat) reset_mday,
			1.0, 31.0, 1.0, 1.0, 0, 55,
			cb_reset_mday, NULL, FALSE,
			_("Start day for cumulative monthly transmit and receive bytes"));

/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
#if !defined(WIN32)
	if (!_GK.client_mode)
		for (i = 0; i < sizeof(net_info_text0)/sizeof(gchar *); ++i)
			gkrellm_gtk_text_view_append(text, _(net_info_text0[i]));
#endif
	for (i = 0; i < sizeof(net_info_text1)/sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(net_info_text1[i]));
	}



static GkrellmMonitor	monitor_net =
	{
	N_("Net"),			/* Name, for config tab.	*/
	MON_NET,			/* Id,  0 if a plugin		*/
	create_net,			/* The create function		*/
	update_net,			/* The update function		*/
	create_net_tab,		/* The config tab create function	*/
	NULL,				/* Instant apply */

	save_net_config,	/* Save user conifg			*/
	load_net_config,	/* Load user config			*/
	NET_CONFIG_KEYWORD,	/* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_net_monitor(void)
	{
	TimerType	*tt;

	if (!setup_net_interface())
		return NULL;
	time(&net_timer0);
	if (timer_defaults_list)
		{
		tt = (TimerType *) timer_defaults_list->data;
		timer_button_iface = g_strdup(tt->name);
		timer_button_type = tt->type;
		}
	else
		{
		timer_button_iface = g_strdup("none");
		}
	if (!_GK.client_mode)
		timer_button_enabled = TRUE;
	else
		timer_button_enabled = (timer_button_type == TIMER_TYPE_SERVER);

	timer_seconds = TRUE;
	timer_on_command = g_strdup("");
	timer_off_command = g_strdup("");
	gkrellm_locale_dup_string(&text_format, DEFAULT_TEXT_FORMAT,
					&text_format_locale);

	net_stats_window_height = 200;

    monitor_net.name = _(monitor_net.name);
	net_style_id = gkrellm_add_chart_style(&monitor_net, NET_STYLE_NAME);
	mon_net = &monitor_net;
	return &monitor_net;
	}


static GkrellmMonitor	monitor_timer =
	{
	N_("Net Timer"),	/* Name, for config tab.	*/
	MON_TIMER,			/* Id,  0 if a plugin		*/
	create_timer,		/* The create function		*/
	NULL,				/* The update function		*/
	NULL,				/* The config tab create function	*/
	NULL,				/* Apply the config function		*/

	NULL,				/* Save user conifg			*/
	NULL,				/* Load user config			*/
	NULL,				/* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_timer_monitor(void)
	{
	if (!mon_net)
		return NULL;
    monitor_timer.name = _(monitor_timer.name);
	timer_style_id = gkrellm_add_meter_style(&monitor_timer, TIMER_STYLE_NAME);
	mon_timer = &monitor_timer;
	net_data_dir = gkrellm_make_data_file_name("net", NULL);
	reset_mday = 1;
	return &monitor_timer;
	}

