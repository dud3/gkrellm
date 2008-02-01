/* GKrellM
|  Copyright (C) 1999-2007 Bill Wilson
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
*/

#include "configure.h"

  /* Debugs for debug_level	*/
#define DEBUG_SYSDEP		0x1
#define DEBUG_MAIL			0x10
#define DEBUG_NET			0x20
#define DEBUG_TIMER			0x40
#define	DEBUG_SENSORS		0x80
#define	DEBUG_INET			0x800
#define	DEBUG_CLIENT		0x1000
#define DEBUG_GUI			0x2000
#define	DEBUG_POSITION		0x4000
#define	DEBUG_BATTERY		0x8000
#define	DEBUG_CHART_TEXT	0x10000
#define	DEBUG_PLUGIN		0x20000

#define	GKRELLM_FLOAT_FACTOR	1000.0

enum GkrellmState
	{
	INITIALIZING,
	INTERNAL,
	INIT_MONITOR,
	CREATE_MONITOR,
	UPDATE_MONITOR,
	CREATE_CONFIG,
	APPLY_CONFIG,
	SAVE_CONFIG,
	LOAD_CONFIG
	};


typedef	void (*GkrellmFunc)();


/* Private global extern declarations and function prototypes.
*/

struct	GkrellmConfig
	{
	gint		debug;
	gint		spare;
	gint		debug_level;
	gint		demo;
	gint		test;
	gboolean	nolock;
	gboolean	without_libsensors;
	gboolean	config_clean;

	gint		up_minutes;
	gint		base_uptime;
	time_t		start_time;
	time_t		time_now;

	GkrellmMonitor		*active_monitor;
	enum GkrellmState	gkrellm_state;

	gint		cpu_sys_activity;
	gint		sensor_temp_files;

	gboolean	initialized;
	gboolean	no_messages;
	gint		max_chart_height;
	gint		monitor_height,
				total_frame_height,
				w_display,
				h_display,
				y_position,
				x_position;
	gboolean	position_valid;
	
	gchar		*theme_path;
	gchar		*config_suffix;		/* Overrides host_configs below */
	gchar		*command_line_theme;
	gchar		*command_line_plugin;

	gchar		*gtk_theme_name;
	GtkSettings	*gtk_settings;

	gchar		*server;
	gint		server_port;
	gchar		*server_hostname;
	gchar		*server_sysname;
	gboolean	client_mode;
	gint		client_server_reconnect_timeout;
	gint		client_server_io_timeout;
	time_t		client_server_read_time;

	gboolean	force_host_config;
	gboolean	found_host_config;
	gboolean	no_config,
				allow_multiple_instances,
				allow_multiple_instances_real;

	gint		frame_left_width,
				frame_right_width,
				frame_top_height,
				frame_bottom_height;
	gint		chart_width_ref;
	gint		frame_left_chart_overlap,
				frame_right_chart_overlap,
				frame_left_panel_overlap,
				frame_right_panel_overlap,
				frame_left_spacer_overlap,
				frame_right_spacer_overlap;

	gboolean	need_frame_packing;
	gint		theme_reload_count;

	gint		chart_history_length;
	gint		chart_height_min;
	gint		chart_height_max;
	gint		allow_scaling;
	gboolean	chart_text_no_fill;
	gboolean	config_modified;
	gboolean	any_transparency;

	gboolean	track_gtk_theme_name;
	gchar		*default_track_theme;

	gchar		*session_id;

	gint		update_HZ;
	gint		chart_width;
	gboolean	save_position,
				withdrawn,
				on_top;
	gboolean	sticky_state,
				state_skip_taskbar,
				state_skip_pager,
				state_above,
				state_below,
				dock_type,
				is_dock_type,
				decorated,
				command_line_decorated;	/* Will override decorated */

	gboolean	enable_hostname,		/* No separate hostname config. */
				hostname_short,
				enable_system_name;

	gfloat		float_factor;			/* avoid config locale breakage */

	gint		mbmon_port;

	gint		rx_led_x;				/* Move these to net monitor */
	gint		rx_led_y;
	gint		tx_led_x;
	gint		tx_led_y;

	GkrellmStyle *krell_slider_style,
				*krell_mini_style;

	GkrellmPiximage
				*frame_top_piximage,
				*frame_bottom_piximage,
				*frame_left_piximage,
				*frame_right_piximage;

	GkrellmPiximage
				*button_panel_out_piximage,
				*button_panel_in_piximage,
				*button_meter_out_piximage,
				*button_meter_in_piximage;

	GkrellmPiximage
				*krell_slider_piximage,
				*krell_mini_piximage;

	GkrellmPiximage
				*spacer_top_piximage,
				*spacer_bottom_piximage,
				*spacer_top_chart_piximage,
				*spacer_bottom_chart_piximage,
				*spacer_top_meter_piximage,
				*spacer_bottom_meter_piximage;

	GkrellmPiximage
				*cap_top_left_chart_piximage,
				*cap_bottom_left_chart_piximage,
				*cap_top_right_chart_piximage,
				*cap_bottom_right_chart_piximage,
				*cap_top_left_meter_piximage,
				*cap_bottom_left_meter_piximage,
				*cap_top_right_meter_piximage,
				*cap_bottom_right_meter_piximage;

	GkrellmBorder
				frame_top_border,
				frame_bottom_border,
				frame_left_border,
				frame_right_border,
				button_panel_border,
				button_meter_border,
				frame_left_chart_border,
				frame_right_chart_border,
				frame_left_panel_border,
				frame_right_panel_border,
				spacer_top_border,
				spacer_bottom_border;

	gint		spacer_top_height_chart,
				spacer_bottom_height_chart,
				spacer_top_height_meter,
				spacer_bottom_height_meter;

	GkrellmPiximage
				*decal_misc_piximage;
	GdkPixmap	*decal_misc_pixmap;
	GdkBitmap	*decal_misc_mask;

	GkrellmPiximage
				*decal_button_piximage;

	/* These two vars should be handled in mail.c, but for historical reasons
	|  gkrellmrcs define them in a way that must be handled in config.c
	*/
	gint		decal_mail_frames,
				decal_mail_delay;


	GdkGC		*draw1_GC,
				*draw2_GC,
				*draw3_GC,
				*draw_stencil_GC,
				*text_GC;

	GdkGC		*bit1_GC,		/* Depth 1 GCs		*/
				*bit0_GC;

	PangoFontDescription
				*large_font,
				*normal_font,
				*small_font;

	gchar		*large_font_string,
				*normal_font_string,
				*small_font_string;

	gint		font_load_count;

	GkrellmPiximage *decal_alarm_piximage;
	GkrellmPiximage *decal_warn_piximage;
	gint		decal_alarm_frames,
				decal_warn_frames;

	GkrellmPiximage *bg_separator_piximage;
	GdkPixmap	*bg_separator_pixmap;
	gint		bg_separator_height;

	GkrellmPiximage *data_in_piximage,		/* Default data layers 0,2, ... */
				*data_in_grid_piximage;
	GdkPixmap	*data_in_pixmap,
				*data_out_pixmap;

	GkrellmPiximage *data_out_piximage,		/* Default data layers 1,3, ... */
				*data_out_grid_piximage;
	GdkPixmap	*data_in_grid_pixmap,
				*data_out_grid_pixmap;

	gchar		*chart_in_color,
				*chart_in_color_grid,
				*chart_out_color,
				*chart_out_color_grid;

	GdkColor	in_color;	/* For cpu user, disk read, rx data	*/
	GdkColor	out_color;	/* For cpu sys, disk writes, tx data	*/
	GdkColor	in_color_grid;
	GdkColor	out_color_grid;
	GdkColor	background_color;
	GdkColor	white_color;

	gint		bg_grid_mode;
	gint		theme_n_alternatives;
	gint		theme_alternative;
	gint		theme_scale;
	gint		m2;
	gint		use_top_bottom_margins;

	GList		*chart_name_list,		/* Move these lists to monitor_list */
				*meter_name_list,
				*custom_name_list,
				*bg_chart_piximage_list,
				*bg_grid_piximage_list,
				*bg_panel_piximage_list,
				*bg_meter_piximage_list,
				*krell_panel_piximage_list,
				*krell_meter_piximage_list,
				*chart_style_list,
				*panel_style_list,
				*meter_style_list,
				*custom_style_list;
	};

extern struct GkrellmConfig  _GK;

extern GList		*gkrellm_monitor_list;

extern struct tm	gkrellm_current_tm;
extern gint			gkrellm_w_display,
					gkrellm_h_display,
					gkrellm_y_position;

void	gkrellm_plugins_load(void);

void	gkrellm_record_state(enum GkrellmState state, GkrellmMonitor *mon);
void	gkrellm_plugins_config_create(GtkWidget *);
void	gkrellm_plugins_config_close(void);
void	gkrellm_menu_popup(void);

GkrellmMonitor *gkrellm_init_host_monitor(void);
GkrellmMonitor *gkrellm_init_cal_monitor(void);
GkrellmMonitor *gkrellm_init_clock_monitor(void);
GkrellmMonitor *gkrellm_init_cpu_monitor(void);
GkrellmMonitor *gkrellm_init_proc_monitor(void);
GkrellmMonitor *gkrellm_init_sensor_monitor(void);
GkrellmMonitor *gkrellm_init_disk_monitor(void);
GkrellmMonitor *gkrellm_init_inet_monitor(void);
GkrellmMonitor *gkrellm_init_net_monitor(void);
GkrellmMonitor *gkrellm_init_timer_monitor(void);
GkrellmMonitor *gkrellm_init_mem_monitor(void);
GkrellmMonitor *gkrellm_init_swap_monitor(void);
GkrellmMonitor *gkrellm_init_fs_monitor(void);
GkrellmMonitor *gkrellm_init_mail_monitor(void);
GkrellmMonitor *gkrellm_init_battery_monitor(void);
GkrellmMonitor *gkrellm_init_uptime_monitor(void);
GkrellmMonitor *gkrellm_init_sensors_config_monitor(void);

GkrellmMonitor *gkrellm_get_cpu_mon(void);
GkrellmMonitor *gkrellm_get_proc_mon(void);
GkrellmMonitor *gkrellm_get_sensors_mon(void);

void	gkrellm_init_hostname_monitor(void);	/* XXX */
GkrellmMonitor	*gkrellm_mon_host(void);
void			gkrellm_gkrellmd_disconnect_cb(GtkWidget *b, gpointer data);

GList	*gkrellm_get_chart_list(void);
GList	*gkrellm_get_panel_list(void);

void		gkrellm_alert_update(void);
void		gkrellm_alert_reset_all(void);
GdkPixbuf	*gkrellm_alert_pixbuf(void);

GkrellmMonitor	*gkrellm_monitor_from_style_name(gchar *);
GkrellmMonitor	*gkrellm_monitor_from_id(gint);

gboolean gkrellm_render_spacer(GkrellmSpacer *spacer, gint y_src, gint h_src,
			gint l_overlap, gint r_overlap);

void	gkrellm_spacers_hide(GkrellmMonitor *);
void	gkrellm_spacers_show(GkrellmMonitor *);

void	gkrellm_panel_button_signals_connect(GkrellmPanel *p);
void	gkrellm_panel_cleanup(void);

void	gkrellm_chart_setup(void);

void	gkrellm_build(void);
void	gkrellm_theme_config(void);
void	gkrellm_load_user_config(GkrellmMonitor *mon_only, gboolean);
void	gkrellm_save_user_config(void);
void	gkrellm_save_theme_config(void);
void	gkrellm_load_theme_config(void);
void	gkrellm_load_theme_piximages(void);
void	gkrellm_read_theme_event(GtkSettings *settings);
void	gkrellm_make_themes_list(void);
gchar	*gkrellm_get_large_font_string(void);
gchar	*gkrellm_get_normal_font_string(void);
gchar	*gkrellm_get_small_font_string(void);

gint	gkrellm_label_x_position(gint, gint, gint, gint);

void	gkrellm_inet_load_data(void);
void	gkrellm_inet_save_data(void);

void	gkrellm_net_save_data(void);
void	gkrellm_net_server_has_timer(void);

gint	gkrellm_effect_string_value(gchar *);

void	gkrellm_map_color_string(gchar *, GdkColor *);

void	gkrellm_add_plugin_config_page(GkrellmMonitor *);
void	gkrellm_remove_plugin_config_page(GkrellmMonitor *);

void	gkrellm_set_theme_alternatives_label(void);
void	gkrellm_start_timer(gint);

GtkItemFactory *gkrellm_create_item_factory_popup(void);

void	gkrellm_apply_hostname_config(void);
gboolean gkrellm_hostname_can_shorten(void);

gchar	*gkrellm_proc_get_sensor_panel_label(void);
gboolean gkrellm_proc_set_sensor(gpointer sr, gint type);
void	gkrellm_proc_draw_sensors(gpointer sr);

gchar	*gkrellm_cpu_get_sensor_panel_label(gint n);
gboolean gkrellm_cpu_set_sensor(gpointer sr, gint type, gint n);
void	gkrellm_cpu_draw_sensors(gpointer sr);


/* utils.c */
gchar	*gkrellm_cut_quoted_string(gchar *, gchar **);
gboolean gkrellm_getline_from_gstring(GString **, gchar *, gint);
void	gkrellm_free_glist_and_data(GList **);
GList	*gkrellm_string_in_list(GList *, gchar *);
gint	gkrellm_string_position_in_list(GList *list, gchar *s);
gboolean gkrellm_make_home_subdir(gchar *, gchar **);
gint	gkrellm_format_size_abbrev(gchar *, size_t, gfloat,
						GkrellmSizeAbbrev *, size_t);

/* sensors.c */
void		gkrellm_sensors_create_decals(GkrellmPanel *, gint,
						GkrellmDecal **, GkrellmDecal **);
gboolean	gkrellm_sensors_available(void);
void		gkrellm_sensor_draw_temperature_decal(GkrellmPanel *,
						GkrellmDecal *, gfloat, gchar);
void		gkrellm_sensor_draw_fan_decal(GkrellmPanel *, GkrellmDecal *,
						gfloat);
gint		gkrellm_sensor_read_temperature(gpointer sr, gfloat *, gchar *);
gint		gkrellm_sensor_read_fan(gpointer sr, gfloat *);
gint		gkrellm_sensor_read_voltage(gpointer sr, gfloat *);
GkrellmAlert *gkrellm_sensor_alert(gpointer sr);
void		gkrellm_sensor_alert_connect(gpointer sr,
						void (*fn)(), gpointer data);
void		gkrellm_sensor_reset_location(gpointer sr);
void		gkrellm_sensors_rebuild(gboolean do_temp, gboolean do_fan,
						gboolean do_volt);
void		gkrellm_sensors_model_update(void);
void		gkrellm_sensors_interface_remove(gint _interface);
void		gkrellm_sensors_sysdep_option(gchar *, gchar *, void (*func)());

/* pixops */
void		gkrellm_border_adjust(GkrellmBorder *border,
						gint l, gint r, gint t, gint b);


/* winops */
void	gkrellm_winop_reset(void);
void	gkrellm_winop_options(gint, gchar **);
void	gkrellm_winop_place_gkrellm(gchar *);
void	gkrellm_winop_flush_motion_events(void);
gboolean gkrellm_winop_updated_background(void);
void	gkrellm_winop_update_struts(void);
void	gkrellm_winop_withdrawn(void);
gboolean gkrellm_winop_draw_rootpixmap_onto_transparent_panel(GkrellmPanel *);
gboolean gkrellm_winop_draw_rootpixmap_onto_transparent_chart(GkrellmChart *);
void	gkrellm_winop_apply_rootpixmap_transparency(void);
void	gkrellm_winop_state_skip_taskbar(gboolean);
void	gkrellm_winop_state_skip_pager(gboolean);
void	gkrellm_winop_state_above(gboolean);
void	gkrellm_winop_state_below(gboolean);

/* client */
gint		gkrellm_connect_to(gchar *, gint);
gboolean	gkrellm_client_mode_connect(void);
void		gkrellm_client_mode_disconnect(void);
struct tm	*gkrellm_client_server_time(void);
gint		gkrellm_client_server_connect_state(void);
void		gkrellm_client_mode_connect_thread(void);
gint		gkrellm_client_server_get_net_timer(void);


