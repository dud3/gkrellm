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


#include "win32-plugin.h"

win32_plugin_callbacks gkrellm_callbacks;

void win32_init_callbacks(void)
{
    // Data structure allocation
    gkrellm_callbacks.gkrellm_chart_new0 = gkrellm_chart_new0;
    gkrellm_callbacks.gkrellm_chartconfig_new0 = gkrellm_chartconfig_new0;
    gkrellm_callbacks.gkrellm_panel_new0 = gkrellm_panel_new0;
    gkrellm_callbacks.gkrellm_krell_new0 = gkrellm_krell_new0;
    gkrellm_callbacks.gkrellm_decal_new0 = gkrellm_decal_new0;
    gkrellm_callbacks.gkrellm_label_new0 = gkrellm_label_new0;
    gkrellm_callbacks.gkrellm_style_new0 = gkrellm_style_new0;
    gkrellm_callbacks.gkrellm_copy_style = gkrellm_copy_style;
    gkrellm_callbacks.gkrellm_copy_style_values = gkrellm_copy_style_values;
    gkrellm_callbacks.gkrellm_textstyle_new0 = gkrellm_textstyle_new0;
    gkrellm_callbacks.gkrellm_copy_textstyle = gkrellm_copy_textstyle;

    // Chart functions
    gkrellm_callbacks.gkrellm_chart_create = gkrellm_chart_create;
    gkrellm_callbacks.gkrellm_chart_destroy = gkrellm_chart_destroy;
    gkrellm_callbacks.gkrellm_chart_hide = gkrellm_chart_hide;
    gkrellm_callbacks.gkrellm_chart_show = gkrellm_chart_show;
    gkrellm_callbacks.gkrellm_chart_enable_visibility = gkrellm_chart_enable_visibility;
    gkrellm_callbacks.gkrellm_is_chart_visible = gkrellm_is_chart_visible;
    gkrellm_callbacks.gkrellm_set_draw_chart_function = gkrellm_set_draw_chart_function;
    gkrellm_callbacks.gkrellm_draw_chart_to_screen = gkrellm_draw_chart_to_screen;
    gkrellm_callbacks.gkrellm_draw_chart_label = gkrellm_draw_chart_label;
    gkrellm_callbacks.gkrellm_draw_chart_text = gkrellm_draw_chart_text;
    gkrellm_callbacks.gkrellm_reset_chart = gkrellm_reset_chart;
    gkrellm_callbacks.gkrellm_reset_and_draw_chart = gkrellm_reset_and_draw_chart;
    gkrellm_callbacks.gkrellm_refresh_chart = gkrellm_refresh_chart;
    gkrellm_callbacks.gkrellm_rescale_chart = gkrellm_rescale_chart;
    gkrellm_callbacks.gkrellm_clear_chart = gkrellm_clear_chart;
    gkrellm_callbacks.gkrellm_clear_chart_pixmap = gkrellm_clear_chart_pixmap;
    gkrellm_callbacks.gkrellm_clean_bg_src_pixmap = gkrellm_clean_bg_src_pixmap;
    gkrellm_callbacks.gkrellm_draw_chart_grid_line = gkrellm_draw_chart_grid_line;
    gkrellm_callbacks.gkrellm_chart_bg_piximage_override = gkrellm_chart_bg_piximage_override;
    gkrellm_callbacks.gkrellm_chart_width = gkrellm_chart_width;
    gkrellm_callbacks.gkrellm_set_chart_height_default = gkrellm_set_chart_height_default;
    gkrellm_callbacks.gkrellm_set_chart_height = gkrellm_set_chart_height;
    gkrellm_callbacks.gkrellm_get_chart_scalemax = gkrellm_get_chart_scalemax;
    gkrellm_callbacks.gkrellm_render_data_pixmap = gkrellm_render_data_pixmap;
    gkrellm_callbacks.gkrellm_render_data_grid_pixmap = gkrellm_render_data_grid_pixmap;

      // ChartData functions
    gkrellm_callbacks.gkrellm_add_chartdata = gkrellm_add_chartdata;
    gkrellm_callbacks.gkrellm_add_default_chartdata = gkrellm_add_default_chartdata;
    gkrellm_callbacks.gkrellm_alloc_chartdata = gkrellm_alloc_chartdata;
    gkrellm_callbacks.gkrellm_store_chartdata = gkrellm_store_chartdata;
    gkrellm_callbacks.gkrellm_draw_chartdata = gkrellm_draw_chartdata;
    gkrellm_callbacks.gkrellm_monotonic_chartdata = gkrellm_monotonic_chartdata;
    gkrellm_callbacks.gkrellm_get_chartdata_hide = gkrellm_get_chartdata_hide;
    gkrellm_callbacks.gkrellm_get_current_chartdata = gkrellm_get_current_chartdata;
    gkrellm_callbacks.gkrellm_get_chartdata_data = gkrellm_get_chartdata_data;
    gkrellm_callbacks.gkrellm_set_chartdata_draw_style = gkrellm_set_chartdata_draw_style;
    gkrellm_callbacks.gkrellm_set_chartdata_draw_style_default = gkrellm_set_chartdata_draw_style_default;
    gkrellm_callbacks.gkrellm_set_chartdata_flags = gkrellm_set_chartdata_flags;
    gkrellm_callbacks.gkrellm_scale_chartdata = gkrellm_scale_chartdata;

      // ChartConfig functions
    gkrellm_callbacks.gkrellm_chartconfig_window_create = gkrellm_chartconfig_window_create;
    gkrellm_callbacks.gkrellm_chartconfig_window_destroy = gkrellm_chartconfig_window_destroy;
    gkrellm_callbacks.gkrellm_chartconfig_grid_resolution_adjustment = gkrellm_chartconfig_grid_resolution_adjustment;
    gkrellm_callbacks.gkrellm_set_chartconfig_grid_resolution = gkrellm_set_chartconfig_grid_resolution;
    gkrellm_callbacks.gkrellm_get_chartconfig_grid_resolution = gkrellm_get_chartconfig_grid_resolution;
    gkrellm_callbacks.gkrellm_chartconfig_grid_resolution_connect = gkrellm_chartconfig_grid_resolution_connect;
    gkrellm_callbacks.gkrellm_set_chartconfig_flags = gkrellm_set_chartconfig_flags;
    gkrellm_callbacks.gkrellm_chartconfig_grid_resolution_label = gkrellm_chartconfig_grid_resolution_label;
    gkrellm_callbacks.gkrellm_set_chartconfig_auto_grid_resolution = gkrellm_set_chartconfig_auto_grid_resolution;
    gkrellm_callbacks.gkrellm_set_chartconfig_auto_resolution_stick = gkrellm_set_chartconfig_auto_resolution_stick;
    gkrellm_callbacks.gkrellm_set_chartconfig_sequence_125 = gkrellm_set_chartconfig_sequence_125;
    gkrellm_callbacks.gkrellm_set_chartconfig_fixed_grids = gkrellm_set_chartconfig_fixed_grids;
    gkrellm_callbacks.gkrellm_get_chartconfig_fixed_grids = gkrellm_get_chartconfig_fixed_grids;
    gkrellm_callbacks.gkrellm_chartconfig_fixed_grids_connect = gkrellm_chartconfig_fixed_grids_connect;
    gkrellm_callbacks.gkrellm_get_chartconfig_height = gkrellm_get_chartconfig_height;
    gkrellm_callbacks.gkrellm_chartconfig_height_connect = gkrellm_chartconfig_height_connect;
    gkrellm_callbacks.gkrellm_save_chartconfig = gkrellm_save_chartconfig;
    gkrellm_callbacks.gkrellm_load_chartconfig = gkrellm_load_chartconfig;
    gkrellm_callbacks.gkrellm_chartconfig_destroy = gkrellm_chartconfig_destroy;

      // Panel functions
    gkrellm_callbacks.gkrellm_panel_configure = gkrellm_panel_configure;
    gkrellm_callbacks.gkrellm_panel_configure_add_height = gkrellm_panel_configure_add_height;
    gkrellm_callbacks.gkrellm_panel_configure_set_height = gkrellm_panel_configure_set_height;
    gkrellm_callbacks.gkrellm_panel_configure_get_height = gkrellm_panel_configure_get_height;
    gkrellm_callbacks.gkrellm_panel_create = gkrellm_panel_create;
    gkrellm_callbacks.gkrellm_panel_destroy = gkrellm_panel_destroy;
    gkrellm_callbacks.gkrellm_panel_hide = gkrellm_panel_hide;
    gkrellm_callbacks.gkrellm_panel_show = gkrellm_panel_show;
    gkrellm_callbacks.gkrellm_panel_enable_visibility = gkrellm_panel_enable_visibility;
    gkrellm_callbacks.gkrellm_is_panel_visible = gkrellm_is_panel_visible;
    gkrellm_callbacks.gkrellm_panel_keep_lists = gkrellm_panel_keep_lists;
    gkrellm_callbacks.gkrellm_draw_panel_label = gkrellm_draw_panel_label;
    gkrellm_callbacks.gkrellm_draw_panel_layers = gkrellm_draw_panel_layers;
    gkrellm_callbacks.gkrellm_draw_panel_layers_force = gkrellm_draw_panel_layers_force;
    gkrellm_callbacks.gkrellm_panel_bg_piximage_override = gkrellm_panel_bg_piximage_override;

      // Krell functions
    gkrellm_callbacks.gkrellm_create_krell = gkrellm_create_krell;
    gkrellm_callbacks.gkrellm_set_krell_full_scale = gkrellm_set_krell_full_scale;
    gkrellm_callbacks.gkrellm_set_style_krell_values = gkrellm_set_style_krell_values;
    gkrellm_callbacks.gkrellm_set_style_krell_values_default = gkrellm_set_style_krell_values_default;
    gkrellm_callbacks.gkrellm_set_style_slider_values_default = gkrellm_set_style_slider_values_default;
    gkrellm_callbacks.gkrellm_set_krell_margins = gkrellm_set_krell_margins;
    gkrellm_callbacks.gkrellm_set_krell_expand = gkrellm_set_krell_expand;
    gkrellm_callbacks.gkrellm_update_krell = gkrellm_update_krell;
    gkrellm_callbacks.gkrellm_monotonic_krell_values = gkrellm_monotonic_krell_values;
    gkrellm_callbacks.gkrellm_destroy_krell_list = gkrellm_destroy_krell_list;
    gkrellm_callbacks.gkrellm_destroy_krell = gkrellm_destroy_krell;
    gkrellm_callbacks.gkrellm_move_krell_yoff = gkrellm_move_krell_yoff;
    gkrellm_callbacks.gkrellm_remove_krell = gkrellm_remove_krell;
    gkrellm_callbacks.gkrellm_insert_krell = gkrellm_insert_krell;
    gkrellm_callbacks.gkrellm_insert_krell_nth = gkrellm_insert_krell_nth;

      // Decal and Decalbutton functions
    gkrellm_callbacks.gkrellm_create_decal_text = gkrellm_create_decal_text;
    gkrellm_callbacks.gkrellm_create_decal_pixmap = gkrellm_create_decal_pixmap;
    gkrellm_callbacks.gkrellm_draw_decal_pixmap = gkrellm_draw_decal_pixmap;
    gkrellm_callbacks.gkrellm_draw_decal_text = gkrellm_draw_decal_text;
    gkrellm_callbacks.gkrellm_draw_decal_on_chart = gkrellm_draw_decal_on_chart;
    gkrellm_callbacks.gkrellm_move_decal = gkrellm_move_decal;
    gkrellm_callbacks.gkrellm_decal_on_top_layer = gkrellm_decal_on_top_layer;
    gkrellm_callbacks.gkrellm_destroy_decal = gkrellm_destroy_decal;
    gkrellm_callbacks.gkrellm_make_decal_visible = gkrellm_make_decal_visible;
    gkrellm_callbacks.gkrellm_make_decal_invisible = gkrellm_make_decal_invisible;
    gkrellm_callbacks.gkrellm_is_decal_visible = gkrellm_is_decal_visible;
    gkrellm_callbacks.gkrellm_remove_decal = gkrellm_remove_decal;
    gkrellm_callbacks.gkrellm_insert_decal = gkrellm_insert_decal;
    gkrellm_callbacks.gkrellm_insert_decal_nth = gkrellm_insert_decal_nth;
    gkrellm_callbacks.gkrellm_destroy_decal_list = gkrellm_destroy_decal_list;
    gkrellm_callbacks.gkrellm_set_decal_button_index = gkrellm_set_decal_button_index;
    gkrellm_callbacks.gkrellm_make_decal_button = gkrellm_make_decal_button;
    gkrellm_callbacks.gkrellm_make_overlay_button = gkrellm_make_overlay_button;
    gkrellm_callbacks.gkrellm_put_decal_in_panel_button = gkrellm_put_decal_in_panel_button;
    gkrellm_callbacks.gkrellm_put_decal_in_meter_button = gkrellm_put_decal_in_meter_button;
    gkrellm_callbacks.gkrellm_put_label_in_panel_button = gkrellm_put_label_in_panel_button;
    gkrellm_callbacks.gkrellm_put_label_in_meter_button = gkrellm_put_label_in_meter_button;
    gkrellm_callbacks.gkrellm_make_scaled_button = gkrellm_make_scaled_button;
    gkrellm_callbacks.gkrellm_decal_is_button = gkrellm_decal_is_button;
    gkrellm_callbacks.gkrellm_set_in_button_callback = gkrellm_set_in_button_callback;
    gkrellm_callbacks.gkrellm_in_button = gkrellm_in_button;
    gkrellm_callbacks.gkrellm_in_decal = gkrellm_in_decal;
    gkrellm_callbacks.gkrellm_decal_button_connect = gkrellm_decal_button_connect;
    gkrellm_callbacks.gkrellm_decal_button_right_connect = gkrellm_decal_button_right_connect;
    gkrellm_callbacks.gkrellm_set_button_sensitive = gkrellm_set_button_sensitive;
    gkrellm_callbacks.gkrellm_hide_button = gkrellm_hide_button;
    gkrellm_callbacks.gkrellm_show_button = gkrellm_show_button;
    gkrellm_callbacks.gkrellm_destroy_button = gkrellm_destroy_button;

      // Pixops
    gkrellm_callbacks.gkrellm_load_piximage = gkrellm_load_piximage;
    gkrellm_callbacks.gkrellm_piximage_new_from_file = gkrellm_piximage_new_from_file;
    gkrellm_callbacks.gkrellm_piximage_new_from_xpm_data = gkrellm_piximage_new_from_xpm_data;
    gkrellm_callbacks.gkrellm_set_piximage_border = gkrellm_set_piximage_border;
    gkrellm_callbacks.gkrellm_scale_pixbuf_to_pixmap = gkrellm_scale_pixbuf_to_pixmap;
    gkrellm_callbacks.gkrellm_scale_piximage_to_pixbuf = gkrellm_scale_piximage_to_pixbuf;
    gkrellm_callbacks.gkrellm_scale_piximage_to_pixmap = gkrellm_scale_piximage_to_pixmap;
    gkrellm_callbacks.gkrellm_paste_piximage = gkrellm_paste_piximage;
    gkrellm_callbacks.gkrellm_paste_pixbuf = gkrellm_paste_pixbuf;
    gkrellm_callbacks.gkrellm_destroy_piximage = gkrellm_destroy_piximage;
    gkrellm_callbacks.gkrellm_clone_piximage = gkrellm_clone_piximage;
    gkrellm_callbacks.gkrellm_clone_pixmap = gkrellm_clone_pixmap;
    gkrellm_callbacks.gkrellm_clone_bitmap = gkrellm_clone_bitmap;
    gkrellm_callbacks.gkrellm_free_pixmap = gkrellm_free_pixmap;
    gkrellm_callbacks.gkrellm_free_bitmap = gkrellm_free_bitmap;

      // Misc support functions
    gkrellm_callbacks.gkrellm_get_top_window = gkrellm_get_top_window;
    gkrellm_callbacks.gkrellm_set_gkrellmrc_piximage_border = gkrellm_set_gkrellmrc_piximage_border;
    gkrellm_callbacks.gkrellm_get_gkrellmrc_integer = gkrellm_get_gkrellmrc_integer;
    gkrellm_callbacks.gkrellm_get_gkrellmrc_string = gkrellm_get_gkrellmrc_string;
    gkrellm_callbacks.gkrellm_get_gkrellmrc_piximage_border = gkrellm_get_gkrellmrc_piximage_border;
    gkrellm_callbacks.gkrellm_freeze_side_frame_packing = gkrellm_freeze_side_frame_packing;
    gkrellm_callbacks.gkrellm_thaw_side_frame_packing = gkrellm_thaw_side_frame_packing;
    gkrellm_callbacks.gkrellm_pack_side_frames = gkrellm_pack_side_frames;
    gkrellm_callbacks.gkrellm_draw_string = gkrellm_draw_string;
    gkrellm_callbacks.gkrellm_draw_text = gkrellm_draw_text;
    gkrellm_callbacks.gkrellm_apply_launcher = gkrellm_apply_launcher;
    gkrellm_callbacks.gkrellm_setup_launcher = gkrellm_setup_launcher;
    gkrellm_callbacks.gkrellm_setup_decal_launcher = gkrellm_setup_decal_launcher;
    gkrellm_callbacks.gkrellm_configure_tooltip = gkrellm_configure_tooltip;
    gkrellm_callbacks.gkrellm_launch_button_cb = gkrellm_launch_button_cb;
    gkrellm_callbacks.gkrellm_disable_plugin_connect = gkrellm_disable_plugin_connect;
    gkrellm_callbacks.gkrellm_get_pid = gkrellm_get_pid;
    gkrellm_callbacks.gkrellm_monitor_height_adjust = gkrellm_monitor_height_adjust;
    gkrellm_callbacks.gkrellm_using_default_theme = gkrellm_using_default_theme;
    gkrellm_callbacks.gkrellm_open_config_window = gkrellm_open_config_window;
    gkrellm_callbacks.gkrellm_config_window_shown = gkrellm_config_window_shown;
    gkrellm_callbacks.gkrellm_config_modified = gkrellm_config_modified;
    gkrellm_callbacks.gkrellm_get_style_margins = gkrellm_get_style_margins;
    gkrellm_callbacks.gkrellm_set_style_margins = gkrellm_set_style_margins;
    gkrellm_callbacks.gkrellm_get_top_bottom_margins = gkrellm_get_top_bottom_margins;
    gkrellm_callbacks.gkrellm_style_is_themed = gkrellm_style_is_themed;
    gkrellm_callbacks.gkrellm_message_dialog = gkrellm_message_dialog;
    gkrellm_callbacks.gkrellm_config_message_dialog = gkrellm_config_message_dialog;
    gkrellm_callbacks.gkrellm_spacers_set_types = gkrellm_spacers_set_types;

      // Alerts
    gkrellm_callbacks.gkrellm_alert_create = gkrellm_alert_create;
    gkrellm_callbacks.gkrellm_alert_destroy = gkrellm_alert_destroy;
    gkrellm_callbacks.gkrellm_check_alert = gkrellm_check_alert;
    gkrellm_callbacks.gkrellm_reset_alert = gkrellm_reset_alert;
    gkrellm_callbacks.gkrellm_reset_panel_alerts = gkrellm_reset_panel_alerts;
    gkrellm_callbacks.gkrellm_freeze_alert = gkrellm_freeze_alert;
    gkrellm_callbacks.gkrellm_thaw_alert = gkrellm_thaw_alert;
    gkrellm_callbacks.gkrellm_alert_trigger_connect = gkrellm_alert_trigger_connect;
    gkrellm_callbacks.gkrellm_alert_stop_connect = gkrellm_alert_stop_connect;
    gkrellm_callbacks.gkrellm_alert_config_connect = gkrellm_alert_config_connect;
    gkrellm_callbacks.gkrellm_render_default_alert_decal = gkrellm_render_default_alert_decal;
    gkrellm_callbacks.gkrellm_alert_config_window = gkrellm_alert_config_window;
    gkrellm_callbacks.gkrellm_alert_window_destroy = gkrellm_alert_window_destroy;
    gkrellm_callbacks.gkrellm_save_alertconfig = gkrellm_save_alertconfig;
    gkrellm_callbacks.gkrellm_load_alertconfig = gkrellm_load_alertconfig;
    gkrellm_callbacks.gkrellm_alert_set_triggers = gkrellm_alert_set_triggers;

      // GKrellM Styles and Textstyles
    gkrellm_callbacks.gkrellm_add_chart_style = gkrellm_add_chart_style;
    gkrellm_callbacks.gkrellm_add_meter_style = gkrellm_add_meter_style;
    gkrellm_callbacks.gkrellm_lookup_chart_style_id = gkrellm_lookup_chart_style_id;
    gkrellm_callbacks.gkrellm_lookup_meter_style_id = gkrellm_lookup_meter_style_id;
    gkrellm_callbacks.gkrellm_meter_style = gkrellm_meter_style;
    gkrellm_callbacks.gkrellm_panel_style = gkrellm_panel_style;
    gkrellm_callbacks.gkrellm_chart_style = gkrellm_chart_style;
    gkrellm_callbacks.gkrellm_meter_style_by_name = gkrellm_meter_style_by_name;
    gkrellm_callbacks.gkrellm_panel_style_by_name = gkrellm_panel_style_by_name;
    gkrellm_callbacks.gkrellm_chart_style_by_name = gkrellm_chart_style_by_name;
    gkrellm_callbacks.gkrellm_krell_slider_style = gkrellm_krell_slider_style;
    gkrellm_callbacks.gkrellm_krell_mini_style = gkrellm_krell_mini_style;
    gkrellm_callbacks.gkrellm_chart_textstyle = gkrellm_chart_textstyle;
    gkrellm_callbacks.gkrellm_panel_textstyle = gkrellm_panel_textstyle;
    gkrellm_callbacks.gkrellm_meter_textstyle = gkrellm_meter_textstyle;
    gkrellm_callbacks.gkrellm_chart_alt_textstyle = gkrellm_chart_alt_textstyle;
    gkrellm_callbacks.gkrellm_panel_alt_textstyle = gkrellm_panel_alt_textstyle;
    gkrellm_callbacks.gkrellm_meter_alt_textstyle = gkrellm_meter_alt_textstyle;

      // Accessing GKrellM GkrellmPiximages and pixmaps
    gkrellm_callbacks.gkrellm_bg_chart_piximage = gkrellm_bg_chart_piximage;
    gkrellm_callbacks.gkrellm_bg_grid_piximage = gkrellm_bg_grid_piximage;
    gkrellm_callbacks.gkrellm_bg_panel_piximage = gkrellm_bg_panel_piximage;
    gkrellm_callbacks.gkrellm_bg_meter_piximage = gkrellm_bg_meter_piximage;
    gkrellm_callbacks.gkrellm_krell_panel_piximage = gkrellm_krell_panel_piximage;
    gkrellm_callbacks.gkrellm_krell_meter_piximage = gkrellm_krell_meter_piximage;
    gkrellm_callbacks.gkrellm_krell_slider_piximage = gkrellm_krell_slider_piximage;
    gkrellm_callbacks.gkrellm_krell_mini_piximage = gkrellm_krell_mini_piximage;
    gkrellm_callbacks.gkrellm_get_decal_alarm_piximage = gkrellm_get_decal_alarm_piximage;
    gkrellm_callbacks.gkrellm_get_decal_warn_piximage = gkrellm_get_decal_warn_piximage;
    gkrellm_callbacks.gkrellm_data_in_pixmap = gkrellm_data_in_pixmap;
    gkrellm_callbacks.gkrellm_data_in_grid_pixmap = gkrellm_data_in_grid_pixmap;
    gkrellm_callbacks.gkrellm_data_out_pixmap = gkrellm_data_out_pixmap;
    gkrellm_callbacks.gkrellm_data_out_grid_pixmap = gkrellm_data_out_grid_pixmap;
    gkrellm_callbacks.gkrellm_decal_misc_pixmap = gkrellm_decal_misc_pixmap;
    gkrellm_callbacks.gkrellm_decal_misc_mask = gkrellm_decal_misc_mask;

    // Accessing other data from the GK struct
    gkrellm_callbacks.gkrellm_draw_GC = gkrellm_draw_GC;
    gkrellm_callbacks.gkrellm_bit_GC = gkrellm_bit_GC;
    gkrellm_callbacks.gkrellm_default_font = gkrellm_default_font;
    gkrellm_callbacks.gkrellm_white_color = gkrellm_white_color;
    gkrellm_callbacks.gkrellm_black_color = gkrellm_black_color;
    gkrellm_callbacks.gkrellm_in_color = gkrellm_in_color;
    gkrellm_callbacks.gkrellm_out_color = gkrellm_out_color;
    gkrellm_callbacks.gkrellm_demo_mode = gkrellm_demo_mode;
    gkrellm_callbacks.gkrellm_update_HZ = gkrellm_update_HZ;
    gkrellm_callbacks.gkrellm_get_theme_path = gkrellm_get_theme_path;
    gkrellm_callbacks.gkrellm_get_timer_ticks = gkrellm_get_timer_ticks;
    gkrellm_callbacks.gkrellm_ticks = gkrellm_ticks;
    gkrellm_callbacks.gkrellm_allow_scaling = gkrellm_allow_scaling;
    gkrellm_callbacks.gkrellm_plugin_debug = gkrellm_plugin_debug;

    // Wrappers around gtk widget functions to provide a convenience higher level
    //  interface for creating the config pages.
    gkrellm_callbacks.gkrellm_gtk_notebook_page = gkrellm_gtk_notebook_page;
    gkrellm_callbacks.gkrellm_gtk_framed_notebook_page = gkrellm_gtk_framed_notebook_page;
    gkrellm_callbacks.gkrellm_gtk_launcher_table_new = gkrellm_gtk_launcher_table_new;
    gkrellm_callbacks.gkrellm_gtk_config_launcher = gkrellm_gtk_config_launcher;
    gkrellm_callbacks.gkrellm_gtk_entry_get_text = gkrellm_gtk_entry_get_text;
    gkrellm_callbacks.gkrellm_gtk_spin_button = gkrellm_gtk_spin_button;
    gkrellm_callbacks.gkrellm_gtk_check_button = gkrellm_gtk_check_button;
    gkrellm_callbacks.gkrellm_gtk_check_button_connected = gkrellm_gtk_check_button_connected;
    gkrellm_callbacks.gkrellm_gtk_button_connected = gkrellm_gtk_button_connected;
    gkrellm_callbacks.gkrellm_gtk_scrolled_vbox = gkrellm_gtk_scrolled_vbox;
    gkrellm_callbacks.gkrellm_gtk_framed_vbox = gkrellm_gtk_framed_vbox;
    gkrellm_callbacks.gkrellm_gtk_framed_vbox_end = gkrellm_gtk_framed_vbox_end;
    gkrellm_callbacks.gkrellm_gtk_scrolled_text_view = gkrellm_gtk_scrolled_text_view;
    gkrellm_callbacks.gkrellm_gtk_text_view_append_strings = gkrellm_gtk_text_view_append_strings;
    gkrellm_callbacks.gkrellm_gtk_text_view_append = gkrellm_gtk_text_view_append;

    // Some utility functions
    gkrellm_callbacks.gkrellm_homedir = gkrellm_homedir;
    gkrellm_callbacks.gkrellm_dup_string = gkrellm_dup_string;
    gkrellm_callbacks.gkrellm_make_config_file_name = gkrellm_make_config_file_name;
    gkrellm_callbacks.gkrellm_make_data_file_name = gkrellm_make_data_file_name;
    gkrellm_callbacks.gkrellm_get_current_time = gkrellm_get_current_time;
    gkrellm_callbacks.gkrellm_125_sequence = gkrellm_125_sequence;
    gkrellm_callbacks.gkrellm_save_all = gkrellm_save_all;

    // ------- Some builtin monitor public functions --------

    // Functions exported by cpu.c
    gkrellm_callbacks.gkrellm_smp_cpus = gkrellm_smp_cpus;
    gkrellm_callbacks.gkrellm_cpu_stats = gkrellm_cpu_stats;

    // Functions exported by net.c
    gkrellm_callbacks.gkrellm_net_routes = gkrellm_net_routes;
    gkrellm_callbacks.gkrellm_net_stats = gkrellm_net_stats;
    gkrellm_callbacks.gkrellm_net_led_positions = gkrellm_net_led_positions;

    // Functions exported by the Mail monitor - see bottom of mail.c
    gkrellm_callbacks.gkrellm_get_mail_mute_mode = gkrellm_get_mail_mute_mode;
    gkrellm_callbacks.gkrellm_add_external_mbox = gkrellm_add_external_mbox;
    gkrellm_callbacks.gkrellm_destroy_external_mbox = gkrellm_destroy_external_mbox;
    gkrellm_callbacks.gkrellm_set_external_mbox_counts = gkrellm_set_external_mbox_counts;
    gkrellm_callbacks.gkrellm_set_external_mbox_tooltip = gkrellm_set_external_mbox_tooltip;

    // Functions new for 2.1.1
    gkrellm_callbacks.gkrellm_get_theme_scale = gkrellm_get_theme_scale;
    gkrellm_callbacks.gkrellm_offset_chartdata = gkrellm_offset_chartdata;
    gkrellm_callbacks.gkrellm_make_scaled_decal_pixmap = gkrellm_make_scaled_decal_pixmap;

    // Functions new for 2.1.8
    gkrellm_callbacks.gkrellm_panel_label_on_top_of_decals = gkrellm_panel_label_on_top_of_decals;
    gkrellm_callbacks.gkrellm_alert_is_activated = gkrellm_alert_is_activated;
    gkrellm_callbacks.gkrellm_alert_dup = gkrellm_alert_dup;
    gkrellm_callbacks.gkrellm_alert_config_create_connect = gkrellm_alert_config_create_connect;
    gkrellm_callbacks.gkrellm_alert_command_process_connect = gkrellm_alert_command_process_connect;
    gkrellm_callbacks.gkrellm_alert_decal_visible = gkrellm_alert_decal_visible;
    gkrellm_callbacks.gkrellm_alert_set_delay = gkrellm_alert_set_delay;
    gkrellm_callbacks.gkrellm_alert_delay_config = gkrellm_alert_delay_config;
    gkrellm_callbacks.gkrellm_gtk_alert_button = gkrellm_gtk_alert_button;

    // Functions new for 2.1.9
    gkrellm_callbacks.gkrellm_piximage_new_from_inline = gkrellm_piximage_new_from_inline;
    gkrellm_callbacks.gkrellm_load_piximage_from_inline = gkrellm_load_piximage_from_inline;
    gkrellm_callbacks.gkrellm_alert_commands_config = gkrellm_alert_commands_config;
    gkrellm_callbacks.gkrellm_reset_alert_soft = gkrellm_reset_alert_soft;

    // Functions new for 2.1.11
    gkrellm_callbacks.gkrellm_decal_text_clear = gkrellm_decal_text_clear;
    gkrellm_callbacks.gkrellm_decal_text_insert = gkrellm_decal_text_insert;
    gkrellm_callbacks.gkrellm_create_decal_text_with_height = gkrellm_create_decal_text_with_height;
    gkrellm_callbacks.gkrellm_chartconfig_callback_block = gkrellm_chartconfig_callback_block;

    // Functions new for 2.1.16
    gkrellm_callbacks.gkrellm_alert_get_alert_state = gkrellm_alert_get_alert_state;
    gkrellm_callbacks.gkrellm_alert_plugin_add = gkrellm_alert_plugin_add;
    gkrellm_callbacks.gkrellm_alert_plugin_alert_connect = gkrellm_alert_plugin_alert_connect;
    gkrellm_callbacks.gkrellm_alert_plugin_config_connect = gkrellm_alert_plugin_config_connect;
    gkrellm_callbacks.gkrellm_alert_plugin_config_get_id_string = gkrellm_alert_plugin_config_get_id_string;
    gkrellm_callbacks.gkrellm_alert_plugin_alert_attach = gkrellm_alert_plugin_alert_attach;
    gkrellm_callbacks.gkrellm_alert_plugin_alert_detach = gkrellm_alert_plugin_alert_detach;
    gkrellm_callbacks.gkrellm_alert_plugin_get_data = gkrellm_alert_plugin_get_data;
    gkrellm_callbacks.gkrellm_alert_plugin_command_process = gkrellm_alert_plugin_command_process;
    gkrellm_callbacks.gkrellm_gtk_category_vbox = gkrellm_gtk_category_vbox;
    gkrellm_callbacks.gkrellm_remove_launcher = gkrellm_remove_launcher;


    //---------------------------------------------------------------------------
    // new since 2.2.0

    gkrellm_callbacks.gkrellm_decal_get_size = gkrellm_decal_get_size;
    gkrellm_callbacks.gkrellm_decal_text_set_offset = gkrellm_decal_text_set_offset;
    gkrellm_callbacks.gkrellm_decal_text_get_offset = gkrellm_decal_text_get_offset;
    gkrellm_callbacks.gkrellm_chart_reuse_text_format = gkrellm_chart_reuse_text_format;
    gkrellm_callbacks.gkrellm_get_hostname = gkrellm_get_hostname;

    gkrellm_callbacks.gkrellm_decal_scroll_text_set_text = gkrellm_decal_scroll_text_set_text;
    gkrellm_callbacks.gkrellm_decal_scroll_text_get_size = gkrellm_decal_scroll_text_get_size;
    gkrellm_callbacks.gkrellm_decal_scroll_text_align_center = gkrellm_decal_scroll_text_align_center;
    gkrellm_callbacks.gkrellm_decal_scroll_text_horizontal_loop = gkrellm_decal_scroll_text_horizontal_loop;
    gkrellm_callbacks.gkrellm_decal_scroll_text_vertical_loop = gkrellm_decal_scroll_text_vertical_loop;

    gkrellm_callbacks.gkrellm_text_extents = gkrellm_text_extents;

    gkrellm_callbacks.gkrellm_gdk_string_width = gkrellm_gdk_string_width;
    gkrellm_callbacks.gkrellm_gdk_draw_string = gkrellm_gdk_draw_string;
    gkrellm_callbacks.gkrellm_gdk_draw_text = gkrellm_gdk_draw_text;

    gkrellm_callbacks.gkrellm_client_mode = gkrellm_client_mode;
    gkrellm_callbacks.gkrellm_client_plugin_get_setup = gkrellm_client_plugin_get_setup;
    gkrellm_callbacks.gkrellm_client_plugin_serve_data_connect = gkrellm_client_plugin_serve_data_connect;
    //gkrellm_callbacks.gkrellm_client_plugin_reconnect_connect = gkrellm_client_plugin_reconnect_connect;


    //---------------------------------------------------------------------------
    // new since 2.2.1

    gkrellm_callbacks.gkrellm_draw_decal_markup = gkrellm_draw_decal_markup;
    gkrellm_callbacks.gkrellm_decal_scroll_text_set_markup = gkrellm_decal_scroll_text_set_markup;


    //---------------------------------------------------------------------------
    // new since 2.2.2

    gkrellm_callbacks.gkrellm_panel_label_get_position = gkrellm_panel_label_get_position;


    //---------------------------------------------------------------------------
    // new since 2.2.5

    gkrellm_callbacks.gkrellm_client_send_to_server = gkrellm_client_send_to_server;

    gkrellm_callbacks.gkrellm_create_decal_text_markup = gkrellm_create_decal_text_markup;
    gkrellm_callbacks.gkrellm_decal_text_markup_insert = gkrellm_decal_text_markup_insert;

    gkrellm_callbacks.gkrellm_decal_text_nth_inserted_set_offset = gkrellm_decal_text_nth_inserted_set_offset;
    gkrellm_callbacks.gkrellm_decal_text_nth_inserted_get_offset = gkrellm_decal_text_nth_inserted_get_offset;
    gkrellm_callbacks.gkrellm_config_instant_apply = gkrellm_config_instant_apply;
    gkrellm_callbacks.gkrellm_gtk_scrolled_selection = gkrellm_gtk_scrolled_selection;
    gkrellm_callbacks.gkrellm_text_markup_extents = gkrellm_text_markup_extents;
    gkrellm_callbacks.gkrellm_gdk_string_markup_width = gkrellm_gdk_string_markup_width;
    gkrellm_callbacks.gkrellm_gdk_text_markup_width = gkrellm_gdk_text_markup_width;
    gkrellm_callbacks.gkrellm_gdk_draw_string_markup = gkrellm_gdk_draw_string_markup;
    gkrellm_callbacks.gkrellm_gdk_draw_text_markup = gkrellm_gdk_draw_text_markup;

    //---------------------------------------------------------------------------
    // new since 2.3.2

    gkrellm_callbacks.gkrellm_debug = gkrellm_debug;

} // win32_init_callbacks()

