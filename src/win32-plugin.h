 /* GKrellM Windows Portion
|  Copyright (C) 2002 Bill Nalen
|
|  Author:  Bill Nalen    bill@nalens.com
|  Latest versions might be found at:  http://bill.nalens.com
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

#include "gkrellm.h"

typedef struct
{
      /* Data structure allocation
      */
    GkrellmChart *(*gkrellm_chart_new0)(void);
    GkrellmChartconfig *(*gkrellm_chartconfig_new0)(void);
    GkrellmPanel *(*gkrellm_panel_new0)(void);
    GkrellmKrell *(*gkrellm_krell_new0)(void);
    GkrellmDecal *(*gkrellm_decal_new0)(void);
    GkrellmLabel *(*gkrellm_label_new0)(void);
    GkrellmStyle *(*gkrellm_style_new0)(void);
    GkrellmStyle *(*gkrellm_copy_style)(GkrellmStyle *);
    void		(*gkrellm_copy_style_values)(GkrellmStyle *, GkrellmStyle *);
    GkrellmTextstyle *(*gkrellm_textstyle_new0)(void);
    GkrellmTextstyle *(*gkrellm_copy_textstyle)(GkrellmTextstyle *);

      /* Chart functions
      */
    void		(*gkrellm_chart_create)(GtkWidget *, GkrellmMonitor *,
						    GkrellmChart *, GkrellmChartconfig **);
    void		(*gkrellm_chart_destroy)(GkrellmChart *);
    void		(*gkrellm_chart_hide)(GkrellmChart *, gboolean);
    void		(*gkrellm_chart_show)(GkrellmChart *, gboolean);
    gboolean	(*gkrellm_chart_enable_visibility)(GkrellmChart *cp, gboolean,
						    gboolean *);
    gboolean	(*gkrellm_is_chart_visible)(GkrellmChart *);
    void		(*gkrellm_set_draw_chart_function)(GkrellmChart *,
						    void (*func)(), gpointer);
    void		(*gkrellm_draw_chart_to_screen)(GkrellmChart *);
    gint		(*gkrellm_draw_chart_label)(GkrellmChart *, GkrellmTextstyle *,
						    gint, gint,gchar *);
    void		(*gkrellm_draw_chart_text)(GkrellmChart *, gint, gchar *);
    void		(*gkrellm_reset_chart)(GkrellmChart *);
    void		(*gkrellm_reset_and_draw_chart)(GkrellmChart *);
    void		(*gkrellm_refresh_chart)(GkrellmChart *);
    void		(*gkrellm_rescale_chart)(GkrellmChart *);
    void		(*gkrellm_clear_chart)(GkrellmChart *);
    void		(*gkrellm_clear_chart_pixmap)(GkrellmChart *);
    void		(*gkrellm_clean_bg_src_pixmap)(GkrellmChart *);
    void		(*gkrellm_draw_chart_grid_line)(GkrellmChart *, GdkPixmap *, gint);
    void		(*gkrellm_chart_bg_piximage_override)(GkrellmChart *,
						    GkrellmPiximage *, GkrellmPiximage *);
    gint		(*gkrellm_chart_width)(void);
    void		(*gkrellm_set_chart_height_default)(GkrellmChart *, gint);
    void		(*gkrellm_set_chart_height)(GkrellmChart *, gint);
    gint		(*gkrellm_get_chart_scalemax)(GkrellmChart *);
    void		(*gkrellm_render_data_pixmap)(GkrellmPiximage *, GdkPixmap **,
						    GdkColor *, gint);
    void		(*gkrellm_render_data_grid_pixmap)(GkrellmPiximage *,
						    GdkPixmap **, GdkColor *);

      /* ChartData functions
      */
    GkrellmChartdata *(*gkrellm_add_chartdata)(GkrellmChart *, GdkPixmap **,
					    GdkPixmap *, gchar *);
    GkrellmChartdata *(*gkrellm_add_default_chartdata)(GkrellmChart *, gchar *);
    void		(*gkrellm_alloc_chartdata)(GkrellmChart *);
    // gkrellm_store_chartdata is not called from libgkrellm, only gkrellm_store_chartdatav
    void		(*gkrellm_store_chartdatav)(GkrellmChart *, gulong, ...);
    void		(*gkrellm_draw_chartdata)(GkrellmChart *);
    void		(*gkrellm_monotonic_chartdata)(GkrellmChartdata *, gboolean);
    gboolean	(*gkrellm_get_chartdata_hide)(GkrellmChartdata *);
    gint		(*gkrellm_get_current_chartdata)(GkrellmChartdata *);
    gint		(*gkrellm_get_chartdata_data)(GkrellmChartdata *, gint);
    void		(*gkrellm_set_chartdata_draw_style)(GkrellmChartdata *, gint);
    void		(*gkrellm_set_chartdata_draw_style_default)(GkrellmChartdata *, gint);
    void		(*gkrellm_set_chartdata_flags)(GkrellmChartdata *, gint);
    void		(*gkrellm_scale_chartdata)(GkrellmChartdata *, gint, gint);

      /* ChartConfig functions
      */
    void		(*gkrellm_chartconfig_window_create)(GkrellmChart *);
    void		(*gkrellm_chartconfig_window_destroy)(GkrellmChart *);
    void		(*gkrellm_chartconfig_grid_resolution_adjustment)(
						    GkrellmChartconfig *, gboolean, gfloat, gfloat, gfloat,
						    gfloat, gfloat, gint, gint);
    void		(*gkrellm_set_chartconfig_grid_resolution)(GkrellmChartconfig *,
						    gint);
    gint		(*gkrellm_get_chartconfig_grid_resolution)(GkrellmChartconfig *);
    void		(*gkrellm_chartconfig_grid_resolution_connect)(
						    GkrellmChartconfig *, void (*fn)(), gpointer);
    void		(*gkrellm_set_chartconfig_flags)(GkrellmChartconfig *, gint);

    void		(*gkrellm_chartconfig_grid_resolution_label)(
						    GkrellmChartconfig *, gchar *);
    void		(*gkrellm_set_chartconfig_auto_grid_resolution)(
						    GkrellmChartconfig *, gboolean);
    void		(*gkrellm_set_chartconfig_auto_resolution_stick)(
						    GkrellmChartconfig *, gboolean);
    void		(*gkrellm_set_chartconfig_sequence_125)(GkrellmChartconfig *,
						    gboolean);
    void		(*gkrellm_set_chartconfig_fixed_grids)(GkrellmChartconfig *, gint);
    gint		(*gkrellm_get_chartconfig_fixed_grids)(GkrellmChartconfig *);
    void		(*gkrellm_chartconfig_fixed_grids_connect)(GkrellmChartconfig *,
						    void (*fn)(), gpointer);
    gint		(*gkrellm_get_chartconfig_height)(GkrellmChartconfig *);
    void		(*gkrellm_chartconfig_height_connect)(GkrellmChartconfig *,
						    void (*fn)(), gpointer);
    void		(*gkrellm_save_chartconfig)(FILE *, GkrellmChartconfig *,
						    gchar *, gchar *);
    void		(*gkrellm_load_chartconfig)(GkrellmChartconfig **, gchar *, gint);
    void		(*gkrellm_chartconfig_destroy)(GkrellmChartconfig **);

      /* Panel functions
      */
    void		(*gkrellm_panel_configure)(GkrellmPanel *, gchar *, GkrellmStyle *);
    void		(*gkrellm_panel_configure_add_height)(GkrellmPanel *, gint);
    void		(*gkrellm_panel_configure_set_height)(GkrellmPanel *p, gint h);
    gint		(*gkrellm_panel_configure_get_height)(GkrellmPanel *p);
    void		(*gkrellm_panel_create)(GtkWidget *, GkrellmMonitor *,GkrellmPanel *);
    void		(*gkrellm_panel_destroy)(GkrellmPanel *);
    void		(*gkrellm_panel_hide)(GkrellmPanel *);
    void		(*gkrellm_panel_show)(GkrellmPanel *);
    gboolean	(*gkrellm_panel_enable_visibility)(GkrellmPanel *p, gboolean, gboolean *);
    gboolean	(*gkrellm_is_panel_visible)(GkrellmPanel *);
    void		(*gkrellm_panel_keep_lists)(GkrellmPanel *, gboolean);
    void		(*gkrellm_draw_panel_label)(GkrellmPanel *);
    void		(*gkrellm_draw_panel_layers)(GkrellmPanel *);
    void		(*gkrellm_draw_panel_layers_force)(GkrellmPanel *);
    void		(*gkrellm_panel_bg_piximage_override)(GkrellmPanel *, GkrellmPiximage *);

      /* Krell functions
      */
    GkrellmKrell *(*gkrellm_create_krell)(GkrellmPanel *, GkrellmPiximage *,
						    GkrellmStyle *);
    void		(*gkrellm_set_krell_full_scale)(GkrellmKrell *, gint, gint);
    void		(*gkrellm_set_style_krell_values)(GkrellmStyle *, gint, gint, gint,
						    gint, gint, gint, gint);
    void		(*gkrellm_set_style_krell_values_default)(GkrellmStyle *s, gint yoff,
						    gint depth, gint x_hot, gint expand, gint ema,
						    gint left_margin, gint right_margin);
    void		(*gkrellm_set_style_slider_values_default)(GkrellmStyle *s, gint yoff,
						    gint left_margin, gint right_margin);
    void		(*gkrellm_set_krell_margins)(GkrellmPanel *, GkrellmKrell *k,
						    gint, gint);
    void		(*gkrellm_set_krell_expand)(GkrellmStyle *, gchar *);
    void		(*gkrellm_update_krell)(GkrellmPanel *, GkrellmKrell *, gulong);
    void		(*gkrellm_monotonic_krell_values)(GkrellmKrell *k, gboolean);
    void		(*gkrellm_destroy_krell_list)(GkrellmPanel *);
    void		(*gkrellm_destroy_krell)(GkrellmKrell *);
    void		(*gkrellm_move_krell_yoff)(GkrellmPanel *, GkrellmKrell *, gint);
    void		(*gkrellm_remove_krell)(GkrellmPanel *, GkrellmKrell *);
    void		(*gkrellm_insert_krell)(GkrellmPanel *, GkrellmKrell *, gboolean);
    void		(*gkrellm_insert_krell_nth)(GkrellmPanel *, GkrellmKrell *, gint);

      /* Decal and Decalbutton functions
      */
    GkrellmDecal *(*gkrellm_create_decal_text)(GkrellmPanel *p, gchar *,
						    GkrellmTextstyle *, GkrellmStyle *, gint, gint, gint);
    GkrellmDecal *(*gkrellm_create_decal_pixmap)(GkrellmPanel *, GdkPixmap *,
						    GdkBitmap *, gint, GkrellmStyle *, gint, gint);
    void		(*gkrellm_draw_decal_pixmap)(GkrellmPanel *, GkrellmDecal *, gint);
    void		(*gkrellm_draw_decal_text)(GkrellmPanel *, GkrellmDecal *, gchar *,
						    gint);
    void		(*gkrellm_draw_decal_on_chart)(GkrellmChart *, GkrellmDecal *,
						    gint, gint);
    void		(*gkrellm_move_decal)(GkrellmPanel *, GkrellmDecal *, gint, gint);
    void		(*gkrellm_decal_on_top_layer)(GkrellmDecal *, gboolean);
    void		(*gkrellm_destroy_decal)(GkrellmDecal *);
    void		(*gkrellm_make_decal_visible)(GkrellmPanel *, GkrellmDecal *);
    void		(*gkrellm_make_decal_invisible)(GkrellmPanel *, GkrellmDecal *);
    gint		(*gkrellm_is_decal_visible)(GkrellmDecal *);
    void		(*gkrellm_remove_decal)(GkrellmPanel *, GkrellmDecal *);
    void		(*gkrellm_insert_decal)(GkrellmPanel *, GkrellmDecal *, gboolean);
    void		(*gkrellm_insert_decal_nth)(GkrellmPanel *, GkrellmDecal *, gint);
    void		(*gkrellm_destroy_decal_list)(GkrellmPanel *);
    void		(*gkrellm_set_decal_button_index)(GkrellmDecalbutton *, gint);
    GkrellmDecalbutton *(*gkrellm_make_decal_button)(GkrellmPanel *, GkrellmDecal *,
						    void (*func)(), void *, gint, gint);
    GkrellmDecalbutton *(*gkrellm_make_overlay_button)(GkrellmPanel *,
						    void (*func)(), void *, gint, gint, gint, gint,
						    GkrellmPiximage *, GkrellmPiximage *);
    GkrellmDecalbutton *(*gkrellm_put_decal_in_panel_button)(GkrellmPanel *,
						    GkrellmDecal *, void (*func)(), void *,
						    GkrellmMargin *);
    GkrellmDecalbutton *(*gkrellm_put_decal_in_meter_button)(GkrellmPanel *,
						    GkrellmDecal *, void (*func)(), void *,
						    GkrellmMargin *);
    GkrellmDecalbutton *(*gkrellm_put_label_in_panel_button)(GkrellmPanel *,
						    void (*func)(), void *, gint pad);
    GkrellmDecalbutton *(*gkrellm_put_label_in_meter_button)(GkrellmPanel *,
						    void (*func)(), void *, gint pad);
    GkrellmDecalbutton *(*gkrellm_make_scaled_button)(GkrellmPanel *p,
						    GkrellmPiximage *im, void (*func)(), void *data,
						    gboolean auto_hide, gboolean set_default_border,
						    gint depth, gint cur_index, gint pressed_index,
						    gint x, gint y, gint w, gint h);
    GkrellmDecalbutton *(*gkrellm_decal_is_button)(GkrellmDecal *);
    void        (*gkrellm_set_in_button_callback)(GkrellmDecalbutton *,
                            gint (*func)(), gpointer data);
    gboolean	(*gkrellm_in_button)(GkrellmDecalbutton *button, GdkEventButton *);
    gboolean	(*gkrellm_in_decal)(GkrellmDecal *, GdkEventButton *);
    void		(*gkrellm_decal_button_connect)(GkrellmDecalbutton *, void (*func)(),
						    void *);
    void		(*gkrellm_decal_button_right_connect)(GkrellmDecalbutton *,
						    void (*func)(), void *);
    void		(*gkrellm_set_button_sensitive)(GkrellmDecalbutton *, gboolean);
    void		(*gkrellm_hide_button)(GkrellmDecalbutton *);
    void		(*gkrellm_show_button)(GkrellmDecalbutton *);
    void		(*gkrellm_destroy_button)(GkrellmDecalbutton *);

      /* Pixops
      */
    gboolean	(*gkrellm_load_piximage)(gchar *, gchar **, GkrellmPiximage **, gchar *);
    GkrellmPiximage	*(*gkrellm_piximage_new_from_file)(gchar *fname);
    GkrellmPiximage	*(*gkrellm_piximage_new_from_xpm_data)(gchar **data);
    void		(*gkrellm_set_piximage_border)(GkrellmPiximage *piximage,
						    GkrellmBorder *border);
    gboolean	(*gkrellm_scale_pixbuf_to_pixmap)(GdkPixbuf *src_pixbuf,
						    GdkPixmap **pixmap, GdkBitmap **mask,
						    gint w_dst, gint h_dst);
    GdkPixbuf	*(*gkrellm_scale_piximage_to_pixbuf)(GkrellmPiximage *piximage,
						    gint w_dst, gint h_dst);
    gboolean	(*gkrellm_scale_piximage_to_pixmap)(GkrellmPiximage *, GdkPixmap **,
						    GdkBitmap **, gint, gint);
    void		(*gkrellm_paste_piximage)(GkrellmPiximage *src_piximage,
						    GdkDrawable *drawable,
						    gint x_dst, gint y_dst, gint w_dst, gint h_dst);
    void		(*gkrellm_paste_pixbuf)(GdkPixbuf *src_pixbuf, GdkDrawable *drawable,
						    gint x_dst, gint y_dst, gint w_dst, gint h_dst);
    void		(*gkrellm_destroy_piximage)(GkrellmPiximage *piximage);
    GkrellmPiximage	*(*gkrellm_clone_piximage)(GkrellmPiximage *src_piximage);
    gboolean	(*gkrellm_clone_pixmap)(GdkPixmap **, GdkPixmap **);
    gboolean	(*gkrellm_clone_bitmap)(GdkBitmap **, GdkBitmap **);
    void		(*gkrellm_free_pixmap)(GdkPixmap **);
    void		(*gkrellm_free_bitmap)(GdkBitmap **);

      /* Misc support functions
      */
    GtkWidget	*(*gkrellm_get_top_window)(void);
    gboolean	(*gkrellm_set_gkrellmrc_piximage_border)(gchar *, GkrellmPiximage *,
						    GkrellmStyle *);
    gboolean	(*gkrellm_get_gkrellmrc_integer)(gchar *, gint *);
    gchar		*(*gkrellm_get_gkrellmrc_string)(gchar *);
    gboolean	(*gkrellm_get_gkrellmrc_piximage_border)(gchar *image_name,
						    GkrellmPiximage *image, GkrellmBorder *border);
    void		(*gkrellm_freeze_side_frame_packing)(void);
    void		(*gkrellm_thaw_side_frame_packing)(void);
    void		(*gkrellm_pack_side_frames)(void);
    void		(*gkrellm_draw_string)(GdkDrawable *, GkrellmTextstyle *, gint, gint,
						    gchar *);
    void		(*gkrellm_draw_text)(GdkDrawable *, GkrellmTextstyle *, gint, gint,
						    gchar *, gint);
    void		(*gkrellm_apply_launcher)(GtkWidget **, GtkWidget **, GkrellmPanel *,
						    GkrellmLauncher *, void (*func)());
    void		(*gkrellm_setup_launcher)(GkrellmPanel *, GkrellmLauncher *, gint,
						    gint);
    void		(*gkrellm_setup_decal_launcher)(GkrellmPanel *, GkrellmLauncher *,
						    GkrellmDecal *);
    void		(*gkrellm_configure_tooltip)(GkrellmPanel *, GkrellmLauncher *);
    void		(*gkrellm_launch_button_cb)(GkrellmDecalbutton *);
    void		(*gkrellm_disable_plugin_connect)(GkrellmMonitor *, void (*func)());
    pid_t		(*gkrellm_get_pid)(void);
    void		(*gkrellm_monitor_height_adjust)(gint);
    gboolean	(*gkrellm_using_default_theme)(void);
    void		(*gkrellm_open_config_window)(GkrellmMonitor *);
    gboolean	(*gkrellm_config_window_shown)(void);
    void		(*gkrellm_config_modified)(void);
    GkrellmMargin *(*gkrellm_get_style_margins)(GkrellmStyle *);
    void		(*gkrellm_set_style_margins)(GkrellmStyle *, GkrellmMargin *);
    void		(*gkrellm_get_top_bottom_margins)(GkrellmStyle *, gint *, gint *);
    gboolean	(*gkrellm_style_is_themed)(GkrellmStyle *, gint);
    void		(*gkrellm_message_dialog)(gchar *title, gchar *message);
    void		(*gkrellm_config_message_dialog)(gchar *title, gchar *message);
    void		(*gkrellm_message_window)(gchar *title, gchar *message, GtkWidget *);
    void		(*gkrellm_config_message_window)(gchar *title, gchar *message,
						    GtkWidget *);
    void		(*gkrellm_spacers_set_types)(GkrellmMonitor *mon, gint top, gint bot);


      /* Alerts
      */
    GkrellmAlert *(*gkrellm_alert_create)(GkrellmPanel *, gchar *, gchar *,
						    gboolean, gboolean, gboolean,
						    gfloat, gfloat, gfloat, gfloat, gint);
    void		(*gkrellm_alert_destroy)(GkrellmAlert **);
    void		(*gkrellm_check_alert)(GkrellmAlert *, gfloat);
    void		(*gkrellm_reset_alert)(GkrellmAlert *);
    void		(*gkrellm_reset_panel_alerts)(GkrellmPanel *);
    void		(*gkrellm_freeze_alert)(GkrellmAlert *);
    void		(*gkrellm_thaw_alert)(GkrellmAlert *);
    void		(*gkrellm_alert_trigger_connect)(GkrellmAlert *, void (*func)(),
						    gpointer);
    void		(*gkrellm_alert_stop_connect)(GkrellmAlert *, void (*func)(),
						    gpointer);
    void		(*gkrellm_alert_config_connect)(GkrellmAlert *, void (*func)(),
						    gpointer);
    void		(*gkrellm_render_default_alert_decal)(GkrellmAlert *);
    void		(*gkrellm_alert_config_window)(GkrellmAlert **);
    void		(*gkrellm_alert_window_destroy)(GkrellmAlert **);
    void		(*gkrellm_save_alertconfig)(FILE *, GkrellmAlert *, gchar *, gchar *);
    void		(*gkrellm_load_alertconfig)(GkrellmAlert **, gchar *);
    void		(*gkrellm_alert_set_triggers)(GkrellmAlert *,
							gfloat, gfloat, gfloat, gfloat);

      /* GKrellM Styles and Textstyles
      */
    gint		(*gkrellm_add_chart_style)(GkrellmMonitor *, gchar *);
    gint		(*gkrellm_add_meter_style)(GkrellmMonitor *, gchar *);
    gint		(*gkrellm_lookup_chart_style_id)(gchar *);
    gint		(*gkrellm_lookup_meter_style_id)(gchar *);
    GkrellmStyle *(*gkrellm_meter_style)(gint);
    GkrellmStyle *(*gkrellm_panel_style)(gint);
    GkrellmStyle *(*gkrellm_chart_style)(gint);
    GkrellmStyle *(*gkrellm_meter_style_by_name)(gchar *);
    GkrellmStyle *(*gkrellm_panel_style_by_name)(gchar *);
    GkrellmStyle *(*gkrellm_chart_style_by_name)(gchar *);
    GkrellmStyle *(*gkrellm_krell_slider_style)(void);
    GkrellmStyle *(*gkrellm_krell_mini_style)(void);
    GkrellmTextstyle *(*gkrellm_chart_textstyle)(gint);
    GkrellmTextstyle *(*gkrellm_panel_textstyle)(gint);
    GkrellmTextstyle *(*gkrellm_meter_textstyle)(gint);
    GkrellmTextstyle *(*gkrellm_chart_alt_textstyle)(gint);
    GkrellmTextstyle *(*gkrellm_panel_alt_textstyle)(gint);
    GkrellmTextstyle *(*gkrellm_meter_alt_textstyle)(gint);

      /* Accessing GKrellM GkrellmPiximages and pixmaps
      */
    GkrellmPiximage	*(*gkrellm_bg_chart_piximage)(gint);
    GkrellmPiximage *(*gkrellm_bg_grid_piximage)(gint);
    GkrellmPiximage *(*gkrellm_bg_panel_piximage)(gint);
    GkrellmPiximage *(*gkrellm_bg_meter_piximage)(gint);
    GkrellmPiximage *(*gkrellm_krell_panel_piximage)(gint);
    GkrellmPiximage *(*gkrellm_krell_meter_piximage)(gint);
    GkrellmPiximage *(*gkrellm_krell_slider_piximage)(void);
    GkrellmPiximage *(*gkrellm_krell_mini_piximage)(void);
    void		(*gkrellm_get_decal_alarm_piximage)(GkrellmPiximage **, gint *);
    void		(*gkrellm_get_decal_warn_piximage)(GkrellmPiximage **, gint *);
    GdkPixmap	**(*gkrellm_data_in_pixmap)(void);
    GdkPixmap	*(*gkrellm_data_in_grid_pixmap)(void);
    GdkPixmap	**(*gkrellm_data_out_pixmap)(void);
    GdkPixmap	*(*gkrellm_data_out_grid_pixmap)(void);
    GdkPixmap	*(*gkrellm_decal_misc_pixmap)(void);
    GdkBitmap	*(*gkrellm_decal_misc_mask)(void);

      /* Accessing other data from the GK struct
      */
    GdkGC		*(*gkrellm_draw_GC)(gint);
    GdkGC		*(*gkrellm_bit_GC)(gint);
    PangoFontDescription		*(*gkrellm_default_font)(gint);
    GdkColor	*(*gkrellm_white_color)(void);
    GdkColor	*(*gkrellm_black_color)(void);
    GdkColor	*(*gkrellm_in_color)(void);
    GdkColor	*(*gkrellm_out_color)(void);
    gboolean	(*gkrellm_demo_mode)(void);
    gint		(*gkrellm_update_HZ)(void);
    gchar		*(*gkrellm_get_theme_path)(void);
    gint		(*gkrellm_get_timer_ticks)(void);
    GkrellmTicks *(*gkrellm_ticks)(void);
    void		(*gkrellm_allow_scaling)(gboolean *, gint *);
    gint		(*gkrellm_plugin_debug)(void);

      /* Wrappers around gtk widget functions to provide a convenience higher level
      |  interface for creating the config pages.
      */
    GtkWidget	*(*gkrellm_gtk_notebook_page)(GtkWidget *, gchar *);
    GtkWidget	*(*gkrellm_gtk_framed_notebook_page)(GtkWidget *, char *);
    GtkWidget	*(*gkrellm_gtk_launcher_table_new)(GtkWidget *, gint);
    void		(*gkrellm_gtk_config_launcher)(GtkWidget *, gint, GtkWidget **,
						    GtkWidget **, gchar *, GkrellmLauncher *);
    gchar		*(*gkrellm_gtk_entry_get_text)(GtkWidget **);
    void		(*gkrellm_gtk_spin_button)(GtkWidget *, GtkWidget **, gfloat, gfloat,
						    gfloat, gfloat, gfloat, gint, gint, void (*func)(),
						    gpointer, gboolean, gchar *);
    void		(*gkrellm_gtk_check_button)(GtkWidget *, GtkWidget **, gboolean,
						    gboolean, gint, gchar *);
    void		(*gkrellm_gtk_check_button_connected)(GtkWidget *, GtkWidget **,
						    gboolean, gboolean, gboolean, gint, void (*func)(),
						    gpointer, gchar *);
    void		(*gkrellm_gtk_button_connected)(GtkWidget *, GtkWidget **, gboolean,
						    gboolean, gint, void (*func)(), gpointer, gchar *);
    GtkWidget	*(*gkrellm_gtk_scrolled_vbox)(GtkWidget *, GtkWidget **,
						    GtkPolicyType, GtkPolicyType);
    GtkWidget	*(*gkrellm_gtk_framed_vbox)(GtkWidget *, gchar *, gint, gboolean,
						    gint, gint);
    GtkWidget	*(*gkrellm_gtk_framed_vbox_end)(GtkWidget *, gchar *, gint, gboolean,
						    gint, gint);
    GtkWidget	*(*gkrellm_gtk_scrolled_text_view)(GtkWidget *, GtkWidget **,
						    GtkPolicyType, GtkPolicyType);
    void		(*gkrellm_gtk_text_view_append_strings)(GtkWidget *, gchar **, gint);
    void		(*gkrellm_gtk_text_view_append)(GtkWidget *, gchar *);

      /* Some utility functions
      */
    gchar		*(*gkrellm_homedir)(void);
    gboolean	(*gkrellm_dup_string)(gchar **, gchar *);
    gboolean	(*gkrellm_locale_dup_string)(gchar **, gchar *, gchar **);
    gchar		*(*gkrellm_make_config_file_name)(gchar *, gchar *);
    gchar		*(*gkrellm_make_data_file_name)(gchar *, gchar *);
    struct tm	*(*gkrellm_get_current_time)(void);
    gint		(*gkrellm_125_sequence)(gint, gboolean, gint, gint,
						    gboolean, gboolean);
    void		(*gkrellm_save_all)(void);

      /* ------- Some builtin monitor public functions -------- */

      /* Functions exported by cpu.c
      */
    gint		(*gkrellm_smp_cpus)(void);
    gboolean	(*gkrellm_cpu_stats)(gint n, gulong *, gulong *, gulong *, gulong *);


      /* Functions exported by net.c
      */
    gint		(*gkrellm_net_routes)(void);
    gboolean	(*gkrellm_net_stats)(gint n, gchar *, gulong *, gulong *);
    void		(*gkrellm_net_led_positions)(gint *x_rx_led, gint *y_rx_led,
						gint *x_tx_led, gint *y_tx_led);


      /* Functions exported by the Mail monitor - see bottom of mail.c
      */
    gboolean	(*gkrellm_get_mail_mute_mode)(void);
    gpointer 	(*gkrellm_add_external_mbox)(gint (*func)(), gboolean, gpointer);
    void		(*gkrellm_destroy_external_mbox)(gpointer);
    void		(*gkrellm_set_external_mbox_counts)(gpointer, gint, gint);
    void		(*gkrellm_set_external_mbox_tooltip)(gpointer, gchar *);

      /* Functions new for 2.1.1
      */
    gfloat		(*gkrellm_get_theme_scale)(void);
    void		(*gkrellm_offset_chartdata)(GkrellmChartdata *, gint);
    GkrellmDecal *(*gkrellm_make_scaled_decal_pixmap)(GkrellmPanel *p,
                        GkrellmPiximage *im, GkrellmStyle *style, gint depth,
                        gint x, gint y, gint w, gint h);

      /* Functions new for 2.1.8
      */
	void		(*gkrellm_panel_label_on_top_of_decals)(GkrellmPanel *p,
						gboolean mode);
	gboolean	(*gkrellm_alert_is_activated)(GkrellmAlert *alert);
	void		(*gkrellm_alert_dup)(GkrellmAlert **ap, GkrellmAlert *a_src);
	void		(*gkrellm_alert_config_create_connect)(GkrellmAlert *alert,
						void (*func)(), gpointer data);
	void		(*gkrellm_alert_command_process_connect)(GkrellmAlert *alert,
						void (*func)(), gpointer data);
	gboolean	(*gkrellm_alert_decal_visible)(GkrellmAlert *alert);
	void		(*gkrellm_alert_set_delay)(GkrellmAlert *alert, gint delay);
	void		(*gkrellm_alert_delay_config)(GkrellmAlert *alert, gint step,
						gint high, gint low);
	void		(*gkrellm_gtk_alert_button)(GtkWidget *box, GtkWidget **button,
						gboolean expand, gboolean fill, gint pad,
						gboolean pack_start, void (*cb_func)(), gpointer data);

      /* Functions new for 2.1.9
      */
	GkrellmPiximage *(*gkrellm_piximage_new_from_inline)(const guint8 *data,
						gboolean copy_pixels);
    gboolean	(*gkrellm_load_piximage_from_inline)(gchar *name,
						const guint8 *data, GkrellmPiximage **image,
						gchar *subdir, gboolean copy_pixels);
	void		(*gkrellm_alert_commands_config)(GkrellmAlert *alert,
						gboolean alarm, gboolean warn);
    void		(*gkrellm_reset_alert_soft)(GkrellmAlert *);

      /* Functions new for 2.1.11
      */
	void		(*gkrellm_decal_text_clear)(GkrellmDecal *d);
	void		(*gkrellm_decal_text_insert)(GkrellmDecal *d, gchar *s,
						GkrellmTextstyle *ts, gint x_off, gint y_off);
	GkrellmDecal *(*gkrellm_create_decal_text_with_height)(GkrellmPanel *p,
						GkrellmTextstyle *ts, GkrellmStyle *style,
						gint x, gint y, gint w, gint h, gint y_baseline);
	void		(*gkrellm_chartconfig_callback_block)(GkrellmChartconfig *,
						gboolean);

      /* Functions new for 2.1.16
      */
	void		(*gkrellm_alert_get_alert_state)(GkrellmAlert *alert,
						gboolean *alarm_state, gboolean *warn_state);
	GkrellmAlertPlugin	*(*gkrellm_alert_plugin_add)(GkrellmMonitor *mon,
						gchar *name);
	void		(*gkrellm_alert_plugin_alert_connect)(GkrellmAlertPlugin *gap,
            			void (*alarm_func)(), void (*warn_func)(),
						void (*update_func)(), void (*check_func)(),
						void (*destroy_func)());
	void		(*gkrellm_alert_plugin_config_connect)(GkrellmAlertPlugin *gap,
						gchar *tab_name,
            			void (*config_create_func)(), void (*config_done_func),
            			void (*config_save_func)(),void (*config_load_func)());
	gchar		*(*gkrellm_alert_plugin_config_get_id_string)(GkrellmAlert *alert);
	void		(*gkrellm_alert_plugin_alert_attach)(GkrellmAlertPlugin *gap,
            			GkrellmAlert *alert, gpointer data);
	void		(*gkrellm_alert_plugin_alert_detach)(GkrellmAlertPlugin *gap,
						GkrellmAlert *alert);
	gpointer	(*gkrellm_alert_plugin_get_data)(GkrellmAlertPlugin *gap,
						GkrellmAlert *alert);
	void		(*gkrellm_alert_plugin_command_process)(GkrellmAlert *alert,
						gchar *src, gchar *dst, gint dst_size);
	GtkWidget	*(*gkrellm_gtk_category_vbox)(GtkWidget *box, gchar *category_header,
        				gint header_pad, gint box_pad, gboolean pack_start);
	void		(*gkrellm_remove_launcher)(GkrellmLauncher *launch);


  //---------------------------------------------------------------------------
  // new since 2.2.0

  void    (*gkrellm_decal_get_size)(GkrellmDecal *d, gint *w, gint *h);
  void    (*gkrellm_decal_text_set_offset)(GkrellmDecal *d, gint x, gint y);
  void    (*gkrellm_decal_text_get_offset)(GkrellmDecal *d, gint *x, gint *y);
  void    (*gkrellm_chart_reuse_text_format)(GkrellmChart *cp);
  gchar  *(*gkrellm_get_hostname)(void);

  void    (*gkrellm_decal_scroll_text_set_text)(GkrellmPanel *p, GkrellmDecal *d, gchar *text);
  void    (*gkrellm_decal_scroll_text_get_size)(GkrellmDecal *d, gint *w, gint *h);
  void    (*gkrellm_decal_scroll_text_align_center)(GkrellmDecal *d, gboolean center);
  void    (*gkrellm_decal_scroll_text_horizontal_loop)(GkrellmDecal *d, gboolean loop);
  void    (*gkrellm_decal_scroll_text_vertical_loop)(GkrellmDecal *d, gboolean loop);

  void    (*gkrellm_text_extents)(PangoFontDescription *font_desc, gchar *text, gint len, gint *width, gint *height, gint *baseline, gint *y_ink);

  gint    (*gkrellm_gdk_string_width)(PangoFontDescription *, gchar *);
  void    (*gkrellm_gdk_draw_string)(GdkDrawable *drawable, PangoFontDescription *font, GdkGC *gc, gint x, gint y, gchar *string);
  void    (*gkrellm_gdk_draw_text)(GdkDrawable *drawable, PangoFontDescription *font, GdkGC *gc, gint x, gint y, gchar *string, gint len);

  gboolean  (*gkrellm_client_mode)(void);
  void      (*gkrellm_client_plugin_get_setup)(gchar *key_name, void (*setup_func_cb)(gchar *str));
  void      (*gkrellm_client_plugin_serve_data_connect)(GkrellmMonitor *mon, gchar *key_name, void (*func_cb)(gchar *line));
  //void      (*gkrellm_client_plugin_reconnect_connect)(gchar *key_name, void (*func_cb)()); // FIXME: missing in gkrellm?


  //---------------------------------------------------------------------------
  // new since 2.2.1

  void    (*gkrellm_draw_decal_markup)(GkrellmPanel *p, GkrellmDecal *d, gchar *text);
  void    (*gkrellm_decal_scroll_text_set_markup)(GkrellmPanel *p, GkrellmDecal *d, gchar *text);


  //---------------------------------------------------------------------------
  // new since 2.2.2

  void    (*gkrellm_panel_label_get_position)(GkrellmStyle *style, gint *x_position, gint *y_off);


  //---------------------------------------------------------------------------
  // new since 2.2.5

  gboolean      (*gkrellm_client_send_to_server)(gchar *key_name, gchar *line);

  GkrellmDecal *(*gkrellm_create_decal_text_markup)(GkrellmPanel *p, gchar *string, GkrellmTextstyle *ts, GkrellmStyle *style, gint x, gint y, gint w);
  void          (*gkrellm_decal_text_markup_insert)(GkrellmDecal *d, gchar *s, GkrellmTextstyle *ts, gint x_off, gint y_off);

  void          (*gkrellm_decal_text_nth_inserted_set_offset)(GkrellmDecal *d, gint n, gint x_off, gint y_off);
  void          (*gkrellm_decal_text_nth_inserted_get_offset)(GkrellmDecal *d, gint n, gint *x_off, gint *y_off);
  void          (*gkrellm_config_instant_apply)(GkrellmMonitor *mon);
  GtkTreeSelection *(*gkrellm_gtk_scrolled_selection)(GtkTreeView *treeview, GtkWidget *box, GtkSelectionMode s_mode, GtkPolicyType h_policy, GtkPolicyType v_policy, void (*func_cb)(), gpointer data);
  void          (*gkrellm_text_markup_extents)(PangoFontDescription *font_desc, gchar *text, gint len, gint *width, gint *height, gint *baseline, gint *y_ink);
  gint          (*gkrellm_gdk_string_markup_width)(PangoFontDescription *, gchar *);
  gint          (*gkrellm_gdk_text_markup_width)(PangoFontDescription *font_desc, const gchar *string, gint len);
  void          (*gkrellm_gdk_draw_string_markup)(GdkDrawable *drawable, PangoFontDescription *font, GdkGC *gc, gint x, gint y, gchar *string);
  void          (*gkrellm_gdk_draw_text_markup)(GdkDrawable *drawable, PangoFontDescription *font, GdkGC *gc, gint x, gint y, gchar *string, gint len);

  //---------------------------------------------------------------------------
  // new since 2.3.2

  // gkrellm_debug is not called from libgkrellm, only gkrellm_debugv
  void (*gkrellm_debugv)(guint debug_level, const gchar *format, va_list arg);

} win32_plugin_callbacks;


/// part of win32-plugin.c
///
extern win32_plugin_callbacks gkrellm_callbacks;


/// \brief initializes \p gkrellm_callbacks
/// Has to be called at gkrellm startup before loading plugins.
/// \note only needed on win32
void win32_init_callbacks(void);

#endif // WIN32_PLUGIN_H
