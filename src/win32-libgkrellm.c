 /* GKrellM Windows Portion
|  Copyright (C) 2002 Bill Nalen
|                2007-2014 Stefan Gehn
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

__declspec(dllexport) win32_plugin_callbacks* callbacks = NULL;

  /* Data structure allocation
  */
GkrellmChart *gkrellm_chart_new0(void)
{
    return (*(callbacks->gkrellm_chart_new0))();
}
GkrellmChartconfig *gkrellm_chartconfig_new0(void)
{
    return (*(callbacks->gkrellm_chartconfig_new0))();
}
GkrellmPanel *gkrellm_panel_new0(void)
{
    return (*(callbacks->gkrellm_panel_new0))();
}
GkrellmKrell *gkrellm_krell_new0(void)
{
    return (*(callbacks->gkrellm_krell_new0))();
}
GkrellmDecal *gkrellm_decal_new0(void)
{
    return (*(callbacks->gkrellm_decal_new0))();
}
GkrellmLabel *gkrellm_label_new0(void)
{
    return (*(callbacks->gkrellm_label_new0))();
}
GkrellmStyle *gkrellm_style_new0(void)
{
    return (*(callbacks->gkrellm_style_new0))();
}
GkrellmStyle *gkrellm_copy_style(GkrellmStyle *a)
{
    return (*(callbacks->gkrellm_copy_style))(a);
}
void		gkrellm_copy_style_values(GkrellmStyle *a, GkrellmStyle *b)
{
    (*(callbacks->gkrellm_copy_style_values))(a, b);
}
GkrellmTextstyle *gkrellm_textstyle_new0(void)
{
    return (*(callbacks->gkrellm_textstyle_new0))();
}
GkrellmTextstyle *gkrellm_copy_textstyle(GkrellmTextstyle *a)
{
    return (*(callbacks->gkrellm_copy_textstyle))(a);
}

  /* Chart functions
  */
void		gkrellm_chart_create(GtkWidget *a, GkrellmMonitor *b,
						GkrellmChart *c, GkrellmChartconfig **d)
{
    (*(callbacks->gkrellm_chart_create))(a, b, c, d);
}
void		gkrellm_chart_destroy(GkrellmChart *a)
{
    (*(callbacks->gkrellm_chart_destroy))(a);
}
void		gkrellm_chart_hide(GkrellmChart *a, gboolean b)
{
    (*(callbacks->gkrellm_chart_hide))(a, b);
}
void		gkrellm_chart_show(GkrellmChart *a, gboolean b)
{
    (*(callbacks->gkrellm_chart_show))(a, b);
}
gboolean	gkrellm_chart_enable_visibility(GkrellmChart *cp, gboolean b,
						gboolean *c)
{
    return (*(callbacks->gkrellm_chart_enable_visibility))(cp, b, c);
}
gboolean	gkrellm_is_chart_visible(GkrellmChart *a)
{
    return (*(callbacks->gkrellm_is_chart_visible))(a);
}
void		gkrellm_set_draw_chart_function(GkrellmChart *a,
						void (*func)(), gpointer b)
{
    (*(callbacks->gkrellm_set_draw_chart_function))(a, func, b);
}
void		gkrellm_draw_chart_to_screen(GkrellmChart *a)
{
    (*(callbacks->gkrellm_draw_chart_to_screen))(a);
}
gint		gkrellm_draw_chart_label(GkrellmChart *a, GkrellmTextstyle *b,
						gint c, gint d,gchar *e)
{
    return (*(callbacks->gkrellm_draw_chart_label))(a, b, c, d, e);
}
void		gkrellm_draw_chart_text(GkrellmChart *a, gint b, gchar *c)
{
    (*(callbacks->gkrellm_draw_chart_text))(a, b, c);
}
void		gkrellm_reset_chart(GkrellmChart *a)
{
    (*(callbacks->gkrellm_reset_chart))(a);
}
void		gkrellm_reset_and_draw_chart(GkrellmChart *a)
{
    (*(callbacks->gkrellm_reset_and_draw_chart))(a);
}
void		gkrellm_refresh_chart(GkrellmChart *a)
{
    (*(callbacks->gkrellm_refresh_chart))(a);
}
void		gkrellm_rescale_chart(GkrellmChart *a)
{
    (*(callbacks->gkrellm_rescale_chart))(a);
}
void		gkrellm_clear_chart(GkrellmChart *a)
{
    (*(callbacks->gkrellm_clear_chart))(a);
}
void		gkrellm_clear_chart_pixmap(GkrellmChart *a)
{
    (*(callbacks->gkrellm_clear_chart_pixmap))(a);
}
void		gkrellm_clean_bg_src_pixmap(GkrellmChart *a)
{
    (*(callbacks->gkrellm_clean_bg_src_pixmap))(a);
}
void		gkrellm_draw_chart_grid_line(GkrellmChart *a, GdkPixmap *b, gint c)
{
    (*(callbacks->gkrellm_draw_chart_grid_line))(a, b, c);
}
void		gkrellm_chart_bg_piximage_override(GkrellmChart *a,
						GkrellmPiximage *b, GkrellmPiximage *c)
{
    (*(callbacks->gkrellm_chart_bg_piximage_override))(a, b, c);
}
gint		gkrellm_chart_width(void)
{
    return (*(callbacks->gkrellm_chart_width))();
}
void		gkrellm_set_chart_height_default(GkrellmChart *a, gint b)
{
    (*(callbacks->gkrellm_set_chart_height_default))(a, b);
}
void		gkrellm_set_chart_height(GkrellmChart *a, gint b)
{
    (*(callbacks->gkrellm_set_chart_height))(a, b);
}
gint		gkrellm_get_chart_scalemax(GkrellmChart *a)
{
    return (*(callbacks->gkrellm_get_chart_scalemax))(a);
}
void		gkrellm_render_data_pixmap(GkrellmPiximage *a, GdkPixmap **b,
						GdkColor *c, gint d)
{
    (*(callbacks->gkrellm_render_data_pixmap))(a, b, c, d);
}
void		gkrellm_render_data_grid_pixmap(GkrellmPiximage *a,
						GdkPixmap **b, GdkColor *c)
{
    (*(callbacks->gkrellm_render_data_grid_pixmap))(a, b, c);
}

  /* ChartData functions
  */
GkrellmChartdata *gkrellm_add_chartdata(GkrellmChart *a, GdkPixmap **b,
					GdkPixmap *c, gchar *d)
{
    return (*(callbacks->gkrellm_add_chartdata))(a, b, c, d);
}
GkrellmChartdata *gkrellm_add_default_chartdata(GkrellmChart *a, gchar *b)
{
    return (*(callbacks->gkrellm_add_default_chartdata))(a, b);
}
void		gkrellm_alloc_chartdata(GkrellmChart *a)
{
    (*(callbacks->gkrellm_alloc_chartdata))(a);
}
void		gkrellm_store_chartdata(GkrellmChart *cp, gulong total, ...)
{
    va_list args;
    va_start(args, total);
    callbacks->gkrellm_store_chartdatav(cp, total, args);
    va_end(args);
}
void		gkrellm_store_chartdatav(GkrellmChart *cp, gulong total, va_list args)
{
    callbacks->gkrellm_store_chartdatav(cp, total, args);
}
void		gkrellm_draw_chartdata(GkrellmChart *a)
{
    (*(callbacks->gkrellm_draw_chartdata))(a);
}
void		gkrellm_monotonic_chartdata(GkrellmChartdata *a, gboolean b)
{
    (*(callbacks->gkrellm_monotonic_chartdata))(a, b);
}
gboolean	gkrellm_get_chartdata_hide(GkrellmChartdata *a)
{
    return (*(callbacks->gkrellm_get_chartdata_hide))(a);
}
gint		gkrellm_get_current_chartdata(GkrellmChartdata *a)
{
    return (*(callbacks->gkrellm_get_current_chartdata))(a);
}
gint		gkrellm_get_chartdata_data(GkrellmChartdata *a, gint b)
{
    return (*(callbacks->gkrellm_get_chartdata_data))(a, b);
}
void		gkrellm_set_chartdata_draw_style(GkrellmChartdata *a, gint b)
{
    (*(callbacks->gkrellm_set_chartdata_draw_style))(a, b);
}
void		gkrellm_set_chartdata_draw_style_default(GkrellmChartdata *a, gint b)
{
    (*(callbacks->gkrellm_set_chartdata_draw_style_default))(a, b);
}
void		gkrellm_set_chartdata_flags(GkrellmChartdata *a, gint b)
{
    (*(callbacks->gkrellm_set_chartdata_flags))(a, b);
}
void		gkrellm_scale_chartdata(GkrellmChartdata *a, gint b, gint c)
{
    (*(callbacks->gkrellm_scale_chartdata))(a, b, c);
}
void		gkrellm_offset_chartdata(GkrellmChartdata *a, gint b)
{
    (*(callbacks->gkrellm_offset_chartdata))(a, b);
}

  /* ChartConfig functions
  */
void		gkrellm_chartconfig_window_create(GkrellmChart *a)
{
    (*(callbacks->gkrellm_chartconfig_window_create))(a);
}
void		gkrellm_chartconfig_window_destroy(GkrellmChart *a)
{
    (*(callbacks->gkrellm_chartconfig_window_destroy))(a);
}
void		gkrellm_chartconfig_grid_resolution_adjustment(
						GkrellmChartconfig *a, gboolean b, gfloat c, gfloat d, gfloat e,
						gfloat f, gfloat g, gint h, gint i)
{
    (*(callbacks->gkrellm_chartconfig_grid_resolution_adjustment))(a, b, c, d, e, f, g, h, i);
}
void		gkrellm_set_chartconfig_grid_resolution(GkrellmChartconfig *a,
						gint b)
{
    (*(callbacks->gkrellm_set_chartconfig_grid_resolution))(a, b);
}
gint		gkrellm_get_chartconfig_grid_resolution(GkrellmChartconfig *a)
{
    return (*(callbacks->gkrellm_get_chartconfig_grid_resolution))(a);
}
void		gkrellm_chartconfig_grid_resolution_connect(
						GkrellmChartconfig *a, void (*fn)(), gpointer b)
{
    (*(callbacks->gkrellm_chartconfig_grid_resolution_connect))(a, fn, b);
}
void		gkrellm_set_chartconfig_flags(GkrellmChartconfig *a, gint b)
{
    (*(callbacks->gkrellm_set_chartconfig_flags))(a, b);
}
void		gkrellm_chartconfig_grid_resolution_label(
						GkrellmChartconfig *a, gchar *b)
{
    (*(callbacks->gkrellm_chartconfig_grid_resolution_label))(a, b);
}
void		gkrellm_set_chartconfig_auto_grid_resolution(
						GkrellmChartconfig *a, gboolean b)
{
    (*(callbacks->gkrellm_set_chartconfig_auto_grid_resolution))(a, b);
}
void		gkrellm_set_chartconfig_auto_resolution_stick(
						GkrellmChartconfig *a, gboolean b)
{
    (*(callbacks->gkrellm_set_chartconfig_auto_resolution_stick))(a, b);
}
void		gkrellm_set_chartconfig_sequence_125(GkrellmChartconfig *a,
						gboolean b)
{
    (*(callbacks->gkrellm_set_chartconfig_sequence_125))(a, b);
}
void		gkrellm_set_chartconfig_fixed_grids(GkrellmChartconfig *a, gint b)
{
    (*(callbacks->gkrellm_set_chartconfig_fixed_grids))(a, b);
}
gint		gkrellm_get_chartconfig_fixed_grids(GkrellmChartconfig *a)
{
    return (*(callbacks->gkrellm_get_chartconfig_fixed_grids))(a);
}
void		gkrellm_chartconfig_fixed_grids_connect(GkrellmChartconfig *a,
						void (*fn)(), gpointer b)
{
    (*(callbacks->gkrellm_chartconfig_fixed_grids_connect))(a, fn, b);
}
gint		gkrellm_get_chartconfig_height(GkrellmChartconfig *a)
{
    return (*(callbacks->gkrellm_get_chartconfig_height))(a);
}
void		gkrellm_chartconfig_height_connect(GkrellmChartconfig *a,
						void (*fn)(), gpointer b)
{
    (*(callbacks->gkrellm_chartconfig_height_connect))(a, fn, b);
}
void		gkrellm_save_chartconfig(FILE *a, GkrellmChartconfig *b,
						gchar *c, gchar *d)
{
    (*(callbacks->gkrellm_save_chartconfig))(a, b, c, d);
}
void		gkrellm_load_chartconfig(GkrellmChartconfig **a, gchar *b, gint c)
{
    (*(callbacks->gkrellm_load_chartconfig))(a, b, c);
}
void		gkrellm_chartconfig_destroy(GkrellmChartconfig **a)
{
    (*(callbacks->gkrellm_chartconfig_destroy))(a);
}

  /* Panel functions
  */
void		gkrellm_panel_configure(GkrellmPanel *a, gchar *b, GkrellmStyle *c)
{
    (*(callbacks->gkrellm_panel_configure))(a, b, c);
}
void		gkrellm_panel_configure_add_height(GkrellmPanel *a, gint b)
{
    (*(callbacks->gkrellm_panel_configure_add_height))(a, b);
}
void		gkrellm_panel_configure_set_height(GkrellmPanel *p, gint h)
{
    (*(callbacks->gkrellm_panel_configure_set_height))(p, h);
}
gint		gkrellm_panel_configure_get_height(GkrellmPanel *p)
{
    return (*(callbacks->gkrellm_panel_configure_get_height))(p);
}
void		gkrellm_panel_create(GtkWidget *a, GkrellmMonitor *b, GkrellmPanel *c)
{
    (*(callbacks->gkrellm_panel_create))(a, b, c);
}
void		gkrellm_panel_destroy(GkrellmPanel *a)
{
    (*(callbacks->gkrellm_panel_destroy))(a);
}
void		gkrellm_panel_hide(GkrellmPanel *a)
{
    (*(callbacks->gkrellm_panel_hide))(a);
}
void		gkrellm_panel_show(GkrellmPanel *a)
{
    (*(callbacks->gkrellm_panel_show))(a);
}
gboolean	gkrellm_panel_enable_visibility(GkrellmPanel *p, gboolean a,
						gboolean *b)
{
    return (*(callbacks->gkrellm_panel_enable_visibility))(p, a, b);
}
gboolean	gkrellm_is_panel_visible(GkrellmPanel *a)
{
    return (*(callbacks->gkrellm_is_panel_visible))(a);
}
void		gkrellm_panel_keep_lists(GkrellmPanel *a, gboolean b)
{
    (*(callbacks->gkrellm_panel_keep_lists))(a, b);
}
void		gkrellm_draw_panel_label(GkrellmPanel *a)
{
    (*(callbacks->gkrellm_draw_panel_label))(a);
}
void		gkrellm_draw_panel_layers(GkrellmPanel *a)
{
    (*(callbacks->gkrellm_draw_panel_layers))(a);
}
void		gkrellm_draw_panel_layers_force(GkrellmPanel *a)
{
    (*(callbacks->gkrellm_draw_panel_layers_force))(a);
}
void		gkrellm_panel_bg_piximage_override(GkrellmPanel *a, GkrellmPiximage *b)
{
    (*(callbacks->gkrellm_panel_bg_piximage_override))(a, b);
}

  /* Krell functions
  */
GkrellmKrell *gkrellm_create_krell(GkrellmPanel *a, GkrellmPiximage *b,
						GkrellmStyle *c)
{
    return (*(callbacks->gkrellm_create_krell))(a, b, c);
}
void		gkrellm_set_krell_full_scale(GkrellmKrell *a, gint b, gint c)
{
    (*(callbacks->gkrellm_set_krell_full_scale))(a, b, c);
}
void		gkrellm_set_style_krell_values(GkrellmStyle *a, gint b, gint c, gint d,
						gint e, gint f, gint g, gint h)
{
    (*(callbacks->gkrellm_set_style_krell_values))(a, b, c, d, e, f, g, h);
}
void		gkrellm_set_style_krell_values_default(GkrellmStyle *s, gint yoff,
						gint depth, gint x_hot, gint expand, gint ema,
						gint left_margin, gint right_margin)
{
    (*(callbacks->gkrellm_set_style_krell_values_default))(s, yoff, depth, x_hot, expand, ema, left_margin, right_margin);
}
void		gkrellm_set_style_slider_values_default(GkrellmStyle *s, gint yoff,
						gint left_margin, gint right_margin)
{
    (*(callbacks->gkrellm_set_style_slider_values_default))(s, yoff, left_margin, right_margin);
}
void		gkrellm_set_krell_margins(GkrellmPanel *a, GkrellmKrell *k,
						gint b, gint c)
{
    (*(callbacks->gkrellm_set_krell_margins))(a, k, b, c);
}
void		gkrellm_set_krell_expand(GkrellmStyle *a, gchar *b)
{
    (*(callbacks->gkrellm_set_krell_expand))(a, b);
}
void		gkrellm_update_krell(GkrellmPanel *a, GkrellmKrell *b, gulong c)
{
    (*(callbacks->gkrellm_update_krell))(a, b, c);
}
void		gkrellm_monotonic_krell_values(GkrellmKrell *k, gboolean b)
{
    (*(callbacks->gkrellm_monotonic_krell_values))(k, b);
}
void		gkrellm_destroy_krell_list(GkrellmPanel *a)
{
    (*(callbacks->gkrellm_destroy_krell_list))(a);
}
void		gkrellm_destroy_krell(GkrellmKrell *a)
{
    (*(callbacks->gkrellm_destroy_krell))(a);
}
void		gkrellm_move_krell_yoff(GkrellmPanel *a, GkrellmKrell *b, gint c)
{
    (*(callbacks->gkrellm_move_krell_yoff))(a, b, c);
}
void		gkrellm_remove_krell(GkrellmPanel *a, GkrellmKrell *b)
{
    (*(callbacks->gkrellm_remove_krell))(a, b);
}
void		gkrellm_insert_krell(GkrellmPanel *a, GkrellmKrell *b, gboolean c)
{
    (*(callbacks->gkrellm_insert_krell))(a, b, c);
}
void		gkrellm_insert_krell_nth(GkrellmPanel *a, GkrellmKrell *b, gint c)
{
    (*(callbacks->gkrellm_insert_krell_nth))(a, b, c);
}

  /* Decal and Decalbutton functions
  */
GkrellmDecal *gkrellm_create_decal_text(GkrellmPanel *p, gchar *a,
						GkrellmTextstyle *b, GkrellmStyle *c, gint d, gint e, gint f)
{
    return (*(callbacks->gkrellm_create_decal_text))(p, a, b, c, d, e, f);
}
GkrellmDecal *gkrellm_create_decal_pixmap(GkrellmPanel *a, GdkPixmap *b,
						GdkBitmap *c, gint d, GkrellmStyle *e, gint f, gint g)
{
    return (*(callbacks->gkrellm_create_decal_pixmap))(a, b, c, d, e, f, g);
}
GkrellmDecal *gkrellm_make_scaled_decal_pixmap(GkrellmPanel *p,
						GkrellmPiximage *im, GkrellmStyle *style,
						gint depth, gint x, gint y, gint w, gint h)
{
    return (*(callbacks->gkrellm_make_scaled_decal_pixmap))(p, im, style,
						depth, x, y, w, h);
}
void		gkrellm_draw_decal_pixmap(GkrellmPanel *a, GkrellmDecal *b, gint c)
{
    (*(callbacks->gkrellm_draw_decal_pixmap))(a, b, c);
}
void		gkrellm_draw_decal_text(GkrellmPanel *a, GkrellmDecal *b, gchar *c,
						gint d)
{
    (*(callbacks->gkrellm_draw_decal_text))(a, b, c, d);
}
void		gkrellm_draw_decal_on_chart(GkrellmChart *a, GkrellmDecal *b,
						gint c, gint d)
{
    (*(callbacks->gkrellm_draw_decal_on_chart))(a, b, c, d);
}
void		gkrellm_move_decal(GkrellmPanel *a, GkrellmDecal *b, gint c, gint d)
{
    (*(callbacks->gkrellm_move_decal))(a, b, c, d);
}
void		gkrellm_decal_on_top_layer(GkrellmDecal *a , gboolean b)
{
    (*(callbacks->gkrellm_decal_on_top_layer))(a, b);
}
void		gkrellm_destroy_decal(GkrellmDecal *a)
{
    (*(callbacks->gkrellm_destroy_decal))(a);
}
void		gkrellm_make_decal_visible(GkrellmPanel *a, GkrellmDecal *b)
{
    (*(callbacks->gkrellm_make_decal_visible))(a, b);
}
void		gkrellm_make_decal_invisible(GkrellmPanel *a, GkrellmDecal *b)
{
    (*(callbacks->gkrellm_make_decal_invisible))(a, b);
}
gint		gkrellm_is_decal_visible(GkrellmDecal *a)
{
    return (*(callbacks->gkrellm_is_decal_visible))(a);
}
void		gkrellm_remove_decal(GkrellmPanel *a, GkrellmDecal *b)
{
    (*(callbacks->gkrellm_remove_decal))(a, b);
}
void		gkrellm_insert_decal(GkrellmPanel *a, GkrellmDecal *b, gboolean c)
{
    (*(callbacks->gkrellm_insert_decal))(a, b, c);
}
void		gkrellm_insert_decal_nth(GkrellmPanel *a, GkrellmDecal *b, gint c)
{
    (*(callbacks->gkrellm_insert_decal_nth))(a, b, c);
}
void		gkrellm_destroy_decal_list(GkrellmPanel *a)
{
    (*(callbacks->gkrellm_destroy_decal_list))(a);
}
void		gkrellm_set_decal_button_index(GkrellmDecalbutton *a, gint b)
{
    (*(callbacks->gkrellm_set_decal_button_index))(a, b);
}
GkrellmDecalbutton *gkrellm_make_decal_button(GkrellmPanel *a, GkrellmDecal *b,
						void (*func)(), void *c, gint d, gint e)
{
    return (*(callbacks->gkrellm_make_decal_button))(a, b, func, c, d, e);
}
GkrellmDecalbutton *gkrellm_make_overlay_button(GkrellmPanel *a,
						void (*func)(), void *b, gint c, gint d, gint e, gint f,
						GkrellmPiximage *g, GkrellmPiximage *h)
{
    return (*(callbacks->gkrellm_make_overlay_button))(a, func, b, c, d, e, f, g, h);
}
GkrellmDecalbutton *gkrellm_put_decal_in_panel_button(GkrellmPanel *a,
						GkrellmDecal *b, void (*func)(), void *c,
						GkrellmMargin *d)
{
    return (*(callbacks->gkrellm_put_decal_in_panel_button))(a, b, func, c, d);
}
GkrellmDecalbutton *gkrellm_put_decal_in_meter_button(GkrellmPanel *a,
						GkrellmDecal *b, void (*func)(), void *c,
						GkrellmMargin *d)
{
    return (*(callbacks->gkrellm_put_decal_in_meter_button))(a, b, func, c, d);
}
GkrellmDecalbutton *gkrellm_put_label_in_panel_button(GkrellmPanel *a,
						void (*func)(), void *b, gint pad)
{
    return (*(callbacks->gkrellm_put_label_in_panel_button))(a, func, b, pad);
}
GkrellmDecalbutton *gkrellm_put_label_in_meter_button(GkrellmPanel *a,
						void (*func)(), void *b, gint pad)
{
    return (*(callbacks->gkrellm_put_label_in_meter_button))(a, func, b, pad);
}
GkrellmDecalbutton *gkrellm_make_scaled_button(GkrellmPanel *p,
						GkrellmPiximage *im, void (*func)(), void *data,
						gboolean auto_hide, gboolean set_default_border,
						gint depth, gint cur_index, gint pressed_index,
						gint x, gint y, gint w, gint h)
{
    return (*(callbacks->gkrellm_make_scaled_button))(p, im, func, data,
        auto_hide, set_default_border, depth, cur_index, pressed_index,
        x, y, w, h);
}
GkrellmDecalbutton *gkrellm_decal_is_button(GkrellmDecal *a)
{
    return (*(callbacks->gkrellm_decal_is_button))(a);
}
void		gkrellm_set_in_button_callback(GkrellmDecalbutton *a,
						gint (*func)(), gpointer data)
{
    (*(callbacks->gkrellm_set_in_button_callback))(a, func, data);
}
gboolean	gkrellm_in_button(GkrellmDecalbutton *button, GdkEventButton *a)
{
    return (*(callbacks->gkrellm_in_button))(button, a);
}
gboolean	gkrellm_in_decal(GkrellmDecal *a, GdkEventButton *b)
{
    return (*(callbacks->gkrellm_in_decal))(a, b);
}
void		gkrellm_decal_button_connect(GkrellmDecalbutton *a, void (*func)(),
						void *b)
{
    (*(callbacks->gkrellm_decal_button_connect))(a, func, b);
}
void		gkrellm_decal_button_right_connect(GkrellmDecalbutton *a,
						void (*func)(), void *b)
{
    (*(callbacks->gkrellm_decal_button_right_connect))(a, func, b);
}
void		gkrellm_set_button_sensitive(GkrellmDecalbutton *a, gboolean b)
{
    (*(callbacks->gkrellm_set_button_sensitive))(a, b);
}
void		gkrellm_hide_button(GkrellmDecalbutton *a)
{
    (*(callbacks->gkrellm_hide_button))(a);
}
void		gkrellm_show_button(GkrellmDecalbutton *a)
{
    (*(callbacks->gkrellm_show_button))(a);
}
void		gkrellm_destroy_button(GkrellmDecalbutton *a)
{
    (*(callbacks->gkrellm_destroy_button))(a);
}

  /* Pixops
  */
gboolean	gkrellm_load_piximage(gchar *a, gchar **b, GkrellmPiximage **c, gchar *d)
{
    return (*(callbacks->gkrellm_load_piximage))(a, b, c, d);
}
GkrellmPiximage	*gkrellm_piximage_new_from_file(gchar *fname)
{
    return (*(callbacks->gkrellm_piximage_new_from_file))(fname);
}
GkrellmPiximage	*gkrellm_piximage_new_from_xpm_data(gchar **data)
{
    return (*(callbacks->gkrellm_piximage_new_from_xpm_data))(data);
}
void		gkrellm_set_piximage_border(GkrellmPiximage *piximage,
						GkrellmBorder *border)
{
    (*(callbacks->gkrellm_set_piximage_border))(piximage, border);
}
gboolean	gkrellm_scale_pixbuf_to_pixmap(GdkPixbuf *src_pixbuf,
						GdkPixmap **pixmap, GdkBitmap **mask,
						gint w_dst, gint h_dst)
{
    return (*(callbacks->gkrellm_scale_pixbuf_to_pixmap))(src_pixbuf, pixmap,
        mask, w_dst, h_dst);
}
GdkPixbuf	*gkrellm_scale_piximage_to_pixbuf(GkrellmPiximage *piximage,
						gint w_dst, gint h_dst)
{
    return (*(callbacks->gkrellm_scale_piximage_to_pixbuf))(piximage, w_dst, h_dst);
}
gboolean	gkrellm_scale_piximage_to_pixmap(GkrellmPiximage *a, GdkPixmap **b,
						GdkBitmap **c, gint d, gint e)
{
    return (*(callbacks->gkrellm_scale_piximage_to_pixmap))(a, b, c, d, e);
}
void		gkrellm_paste_piximage(GkrellmPiximage *src_piximage,
						GdkDrawable *drawable,
						gint x_dst, gint y_dst, gint w_dst, gint h_dst)
{
    (*(callbacks->gkrellm_paste_piximage))(src_piximage, drawable, x_dst, y_dst, w_dst, h_dst);
}
void		gkrellm_paste_pixbuf(GdkPixbuf *src_pixbuf, GdkDrawable *drawable,
						gint x_dst, gint y_dst, gint w_dst, gint h_dst)
{
    (*(callbacks->gkrellm_paste_pixbuf))(src_pixbuf, drawable, x_dst, y_dst, w_dst, h_dst);
}
void		gkrellm_destroy_piximage(GkrellmPiximage *piximage)
{
    (*(callbacks->gkrellm_destroy_piximage))(piximage);
}
GkrellmPiximage	*gkrellm_clone_piximage(GkrellmPiximage *src_piximage)
{
    return (*(callbacks->gkrellm_clone_piximage))(src_piximage);
}
gboolean	gkrellm_clone_pixmap(GdkPixmap **a, GdkPixmap **b)
{
    return (*(callbacks->gkrellm_clone_pixmap))(a, b);
}
gboolean	gkrellm_clone_bitmap(GdkBitmap **a, GdkBitmap **b)
{
    return (*(callbacks->gkrellm_clone_bitmap))(a, b);
}
void		gkrellm_free_pixmap(GdkPixmap **a)
{
    (*(callbacks->gkrellm_free_pixmap))(a);
}
void		gkrellm_free_bitmap(GdkBitmap **a)
{
    (*(callbacks->gkrellm_free_bitmap))(a);
}

  /* Misc support functions
  */
GtkWidget	*gkrellm_get_top_window(void)
{
    return (*(callbacks->gkrellm_get_top_window))();
}
gboolean	gkrellm_set_gkrellmrc_piximage_border(gchar *a, GkrellmPiximage *b,
						GkrellmStyle *c)
{
    return (*(callbacks->gkrellm_set_gkrellmrc_piximage_border))(a, b, c);
}
gboolean	gkrellm_get_gkrellmrc_integer(gchar *a, gint *b)
{
    return (*(callbacks->gkrellm_get_gkrellmrc_integer))(a, b);
}
gchar		*gkrellm_get_gkrellmrc_string(gchar *a)
{
    return (*(callbacks->gkrellm_get_gkrellmrc_string))(a);
}
gboolean	gkrellm_get_gkrellmrc_piximage_border(gchar *image_name,
						GkrellmPiximage *image, GkrellmBorder *border)
{
    return (*(callbacks->gkrellm_get_gkrellmrc_piximage_border))(image_name, image, border);
}
void		gkrellm_freeze_side_frame_packing(void)
{
    (*(callbacks->gkrellm_freeze_side_frame_packing))();
}
void		gkrellm_thaw_side_frame_packing(void)
{
    (*(callbacks->gkrellm_thaw_side_frame_packing))();
}
void		gkrellm_pack_side_frames(void)
{
    (*(callbacks->gkrellm_pack_side_frames))();
}
void		gkrellm_draw_string(GdkDrawable *a, GkrellmTextstyle *b, gint c, gint d,
						gchar *e)
{
    (*(callbacks->gkrellm_draw_string))(a, b, c, d, e);
}
void		gkrellm_draw_text(GdkDrawable *a, GkrellmTextstyle *b, gint c, gint d,
						gchar *e, gint f)
{
    (*(callbacks->gkrellm_draw_text))(a, b, c, d, e, f);
}
void		gkrellm_apply_launcher(GtkWidget **a, GtkWidget **b, GkrellmPanel *c,
						GkrellmLauncher *d, void (*func)())
{
    (*(callbacks->gkrellm_apply_launcher))(a, b, c, d, func);
}
void		gkrellm_setup_launcher(GkrellmPanel *a, GkrellmLauncher *b, gint c,
						gint d)
{
    (*(callbacks->gkrellm_setup_launcher))(a, b, c, d);
}
void		gkrellm_setup_decal_launcher(GkrellmPanel *a, GkrellmLauncher *b,
						GkrellmDecal *c)
{
    (*(callbacks->gkrellm_setup_decal_launcher))(a, b, c);
}
void		gkrellm_configure_tooltip(GkrellmPanel *a, GkrellmLauncher *b)
{
    (*(callbacks->gkrellm_configure_tooltip))(a, b);
}
void		gkrellm_launch_button_cb(GkrellmDecalbutton *a)
{
    (*(callbacks->gkrellm_launch_button_cb))(a);
}
void		gkrellm_disable_plugin_connect(GkrellmMonitor *a, void (*func)())
{
    (*(callbacks->gkrellm_disable_plugin_connect))(a, func);
}
pid_t		gkrellm_get_pid(void)
{
    return (*(callbacks->gkrellm_get_pid))();
}
void		gkrellm_monitor_height_adjust(gint a)
{
    (*(callbacks->gkrellm_monitor_height_adjust))(a);
}
gboolean	gkrellm_using_default_theme(void)
{
    return (*(callbacks->gkrellm_using_default_theme))();
}
gfloat	gkrellm_get_theme_scale(void)
{
    return (*(callbacks->gkrellm_get_theme_scale))();
}
void		gkrellm_open_config_window(GkrellmMonitor *a)
{
    (*(callbacks->gkrellm_open_config_window))(a);
}
gboolean	gkrellm_config_window_shown(void)
{
    return (*(callbacks->gkrellm_config_window_shown))();
}
void		gkrellm_config_modified(void)
{
    (*(callbacks->gkrellm_config_modified))();
}
void		gkrellm_message_dialog(gchar *title, gchar *message)
{
    (*(callbacks->gkrellm_message_dialog))(title, message);
}
void		gkrellm_config_message_dialog(gchar *title, gchar *message)
{
    (*(callbacks->gkrellm_config_message_dialog))(title, message);
}
void		gkrellm_spacers_set_types(GkrellmMonitor *mon, gint top, gint bot)
{
    (*(callbacks->gkrellm_spacers_set_types))(mon, top, bot);
}
//void		gkrellm_message_window(gchar *title, gchar *message, GtkWidget *a)
//{
//    (*(callbacks->gkrellm_message_window))(title, message, a);
//}
//void		gkrellm_config_message_window(gchar *title, gchar *message,
//						GtkWidget *a)
//{
//    (*(callbacks->gkrellm_config_message_window))(title, message, a);
//}
GkrellmMargin *gkrellm_get_style_margins(GkrellmStyle *a)
{
    return (*(callbacks->gkrellm_get_style_margins))(a);
}
void		gkrellm_set_style_margins(GkrellmStyle *a, GkrellmMargin *b)
{
    (*(callbacks->gkrellm_set_style_margins))(a, b);
}
void		gkrellm_get_top_bottom_margins(GkrellmStyle *a, gint *b, gint *c)
{
    (*(callbacks->gkrellm_get_top_bottom_margins))(a, b, c);
}
gboolean	gkrellm_style_is_themed(GkrellmStyle *a, gint query)
{
    return (*(callbacks->gkrellm_style_is_themed))(a, query);
}

  /* Alerts
  */
GkrellmAlert *gkrellm_alert_create(GkrellmPanel *a, gchar *b, gchar *c,
						gboolean d, gboolean e, gboolean f,
						gfloat g, gfloat h, gfloat i, gfloat j, gint k)
{
    return (*(callbacks->gkrellm_alert_create))(a, b, c, d, e, f, g, h, i, j, k);
}
void		gkrellm_alert_destroy(GkrellmAlert **a)
{
    (*(callbacks->gkrellm_alert_destroy))(a);
}
void		gkrellm_check_alert(GkrellmAlert *a, gfloat b)
{
    (*(callbacks->gkrellm_check_alert))(a, b);
}
void		gkrellm_reset_alert(GkrellmAlert *a)
{
    (*(callbacks->gkrellm_reset_alert))(a);
}
void		gkrellm_reset_panel_alerts(GkrellmPanel *a)
{
    (*(callbacks->gkrellm_reset_panel_alerts))(a);
}
void		gkrellm_freeze_alert(GkrellmAlert *a)
{
    (*(callbacks->gkrellm_freeze_alert))(a);
}
void		gkrellm_thaw_alert(GkrellmAlert *a)
{
    (*(callbacks->gkrellm_thaw_alert))(a);
}
void		gkrellm_alert_trigger_connect(GkrellmAlert *a, void (*func)(),
						gpointer b)
{
    (*(callbacks->gkrellm_alert_trigger_connect))(a, func, b);
}
void		gkrellm_alert_stop_connect(GkrellmAlert *a, void (*func)(),
						gpointer b)
{
    (*(callbacks->gkrellm_alert_stop_connect))(a, func, b);
}
void		gkrellm_alert_config_connect(GkrellmAlert *a, void (*func)(),
						gpointer b)
{
    (*(callbacks->gkrellm_alert_config_connect))(a, func, b);
}
void		gkrellm_render_default_alert_decal(GkrellmAlert *a)
{
    (*(callbacks->gkrellm_render_default_alert_decal))(a);
}
void		gkrellm_alert_config_window(GkrellmAlert **a)
{
    (*(callbacks->gkrellm_alert_config_window))(a);
}
void		gkrellm_alert_window_destroy(GkrellmAlert **a)
{
    (*(callbacks->gkrellm_alert_window_destroy))(a);
}
void		gkrellm_save_alertconfig(FILE *a, GkrellmAlert *b, gchar *c, gchar *d)
{
    (*(callbacks->gkrellm_save_alertconfig))(a, b, c, d);
}
void		gkrellm_load_alertconfig(GkrellmAlert **a, gchar *b)
{
    (*(callbacks->gkrellm_load_alertconfig))(a, b);
}

void		gkrellm_alert_set_triggers(GkrellmAlert *a, gfloat b, gfloat c,
						gfloat d, gfloat e)
{
    (*(callbacks->gkrellm_alert_set_triggers))(a, b, c, d, e);
}

  /* GKrellM Styles and Textstyles
  */
gint		gkrellm_add_chart_style(GkrellmMonitor *a, gchar *b)
{
    return (*(callbacks->gkrellm_add_chart_style))(a, b);
}
gint		gkrellm_add_meter_style(GkrellmMonitor *a, gchar *b)
{
    return (*(callbacks->gkrellm_add_meter_style))(a, b);
}
gint		gkrellm_lookup_chart_style_id(gchar *a)
{
    return (*(callbacks->gkrellm_lookup_chart_style_id))(a);
}
gint		gkrellm_lookup_meter_style_id(gchar *a)
{
    return (*(callbacks->gkrellm_lookup_meter_style_id))(a);
}
GkrellmStyle *gkrellm_meter_style(gint a)
{
    return (*(callbacks->gkrellm_meter_style))(a);
}
GkrellmStyle *gkrellm_panel_style(gint a)
{
    return (*(callbacks->gkrellm_panel_style))(a);
}
GkrellmStyle *gkrellm_chart_style(gint a)
{
    return (*(callbacks->gkrellm_chart_style))(a);
}
GkrellmStyle *gkrellm_meter_style_by_name(gchar *a)
{
    return (*(callbacks->gkrellm_meter_style_by_name))(a);
}
GkrellmStyle *gkrellm_panel_style_by_name(gchar *a)
{
    return (*(callbacks->gkrellm_panel_style_by_name))(a);
}
GkrellmStyle *gkrellm_chart_style_by_name(gchar *a)
{
    return (*(callbacks->gkrellm_chart_style_by_name))(a);
}
GkrellmStyle *gkrellm_krell_slider_style(void)
{
    return (*(callbacks->gkrellm_krell_slider_style))();
}
GkrellmStyle *gkrellm_krell_mini_style(void)
{
    return (*(callbacks->gkrellm_krell_mini_style))();
}
GkrellmTextstyle *gkrellm_chart_textstyle(gint a)
{
    return (*(callbacks->gkrellm_chart_textstyle))(a);
}
GkrellmTextstyle *gkrellm_panel_textstyle(gint a)
{
    return (*(callbacks->gkrellm_panel_textstyle))(a);
}
GkrellmTextstyle *gkrellm_meter_textstyle(gint a)
{
    return (*(callbacks->gkrellm_meter_textstyle))(a);
}
GkrellmTextstyle *gkrellm_chart_alt_textstyle(gint a)
{
    return (*(callbacks->gkrellm_chart_alt_textstyle))(a);
}
GkrellmTextstyle *gkrellm_panel_alt_textstyle(gint a)
{
    return (*(callbacks->gkrellm_panel_alt_textstyle))(a);
}
GkrellmTextstyle *gkrellm_meter_alt_textstyle(gint a)
{
    return (*(callbacks->gkrellm_meter_alt_textstyle))(a);
}

  /* Accessing GKrellM GkrellmPiximages and pixmaps
  */
GkrellmPiximage	*gkrellm_bg_chart_piximage(gint a)
{
    return (*(callbacks->gkrellm_bg_chart_piximage))(a);
}
GkrellmPiximage *gkrellm_bg_grid_piximage(gint a)
{
    return (*(callbacks->gkrellm_bg_grid_piximage))(a);
}
GkrellmPiximage *gkrellm_bg_panel_piximage(gint a)
{
    return (*(callbacks->gkrellm_bg_panel_piximage))(a);
}
GkrellmPiximage *gkrellm_bg_meter_piximage(gint a)
{
    return (*(callbacks->gkrellm_bg_meter_piximage))(a);
}
GkrellmPiximage *gkrellm_krell_panel_piximage(gint a)
{
    return (*(callbacks->gkrellm_krell_panel_piximage))(a);
}
GkrellmPiximage *gkrellm_krell_meter_piximage(gint a)
{
    return (*(callbacks->gkrellm_krell_meter_piximage))(a);
}
GkrellmPiximage *gkrellm_krell_slider_piximage(void)
{
    return (*(callbacks->gkrellm_krell_slider_piximage))();
}
GkrellmPiximage *gkrellm_krell_mini_piximage(void)
{
    return (*(callbacks->gkrellm_krell_mini_piximage))();
}
void		gkrellm_get_decal_alarm_piximage(GkrellmPiximage **a, gint *b)
{
    (*(callbacks->gkrellm_get_decal_alarm_piximage))(a, b);
}
void		gkrellm_get_decal_warn_piximage(GkrellmPiximage **a, gint *b)
{
    (*(callbacks->gkrellm_get_decal_warn_piximage))(a, b);
}
GdkPixmap	**gkrellm_data_in_pixmap(void)
{
    return (*(callbacks->gkrellm_data_in_pixmap))();
}
GdkPixmap	*gkrellm_data_in_grid_pixmap(void)
{
    return (*(callbacks->gkrellm_data_in_grid_pixmap))();
}
GdkPixmap	**gkrellm_data_out_pixmap(void)
{
    return (*(callbacks->gkrellm_data_out_pixmap))();
}
GdkPixmap	*gkrellm_data_out_grid_pixmap(void)
{
    return (*(callbacks->gkrellm_data_out_grid_pixmap))();
}
GdkPixmap	*gkrellm_decal_misc_pixmap(void)
{
    return (*(callbacks->gkrellm_decal_misc_pixmap))();
}
GdkBitmap	*gkrellm_decal_misc_mask(void)
{
    return (*(callbacks->gkrellm_decal_misc_mask))();
}

  /* Accessing other data from the GK struct
  */
GdkGC		*gkrellm_draw_GC(gint a)
{
    return (*(callbacks->gkrellm_draw_GC))(a);
}
GdkGC		*gkrellm_bit_GC(gint a)
{
    return (*(callbacks->gkrellm_bit_GC))(a);
}
PangoFontDescription *gkrellm_default_font(gint a)
{
    return callbacks->gkrellm_default_font(a);
}
GdkColor	*gkrellm_white_color(void)
{
    return (*(callbacks->gkrellm_white_color))();
}
GdkColor	*gkrellm_black_color(void)
{
    return (*(callbacks->gkrellm_black_color))();
}
GdkColor	*gkrellm_in_color(void)
{
    return (*(callbacks->gkrellm_in_color))();
}
GdkColor	*gkrellm_out_color(void)
{
    return (*(callbacks->gkrellm_out_color))();
}
gboolean	gkrellm_demo_mode(void)
{
    return (*(callbacks->gkrellm_demo_mode))();
}
gint		gkrellm_update_HZ(void)
{
    return (*(callbacks->gkrellm_update_HZ))();
}
gchar		*gkrellm_get_theme_path(void)
{
    return (*(callbacks->gkrellm_get_theme_path))();
}
gint		gkrellm_get_timer_ticks(void)
{
    return (*(callbacks->gkrellm_get_timer_ticks))();
}
GkrellmTicks *gkrellm_ticks(void)
{
    return (*(callbacks->gkrellm_ticks))();
}
void		gkrellm_allow_scaling(gboolean *a, gint *b)
{
    (*(callbacks->gkrellm_allow_scaling))(a, b);
}
gint		gkrellm_plugin_debug(void)
{
    return (*(callbacks->gkrellm_plugin_debug))();
}

  /* Wrappers around gtk widget functions to provide a convenience higher level
  |  interface for creating the config pages.
  */
GtkWidget	*gkrellm_gtk_notebook_page(GtkWidget *a, gchar *b)
{
    return (*(callbacks->gkrellm_gtk_notebook_page))(a, b);
}
GtkWidget	*gkrellm_gtk_framed_notebook_page(GtkWidget *a, char *b)
{
    return (*(callbacks->gkrellm_gtk_framed_notebook_page))(a, b);
}
GtkWidget	*gkrellm_gtk_launcher_table_new(GtkWidget *a, gint b)
{
    return (*(callbacks->gkrellm_gtk_launcher_table_new))(a, b);
}
void		gkrellm_gtk_config_launcher(GtkWidget *a, gint b, GtkWidget **c,
						GtkWidget **d, gchar *e, GkrellmLauncher *f)
{
    (*(callbacks->gkrellm_gtk_config_launcher))(a, b, c, d, e, f);
}
gchar		*gkrellm_gtk_entry_get_text(GtkWidget **a)
{
    return (*(callbacks->gkrellm_gtk_entry_get_text))(a);
}
void		gkrellm_gtk_spin_button(GtkWidget *a, GtkWidget **b, gfloat c, gfloat d,
						gfloat e, gfloat f, gfloat g, gint h, gint i, void (*func)(),
						gpointer j, gboolean k, gchar *l)
{
    (*(callbacks->gkrellm_gtk_spin_button))(a, b, c, d, e, f, g, h, i, func, j, k, l);
}
void		gkrellm_gtk_check_button(GtkWidget *a, GtkWidget **b, gboolean c,
						gboolean d, gint e, gchar *f)
{
    (*(callbacks->gkrellm_gtk_check_button))(a, b, c, d, e, f);
}
void		gkrellm_gtk_check_button_connected(GtkWidget *a, GtkWidget **b,
						gboolean c, gboolean d, gboolean e, gint f, void (*func)(),
						gpointer g, gchar *h)
{
    (*(callbacks->gkrellm_gtk_check_button_connected))(a, b, c, d, e, f, func, g, h);
}
void		gkrellm_gtk_button_connected(GtkWidget *a, GtkWidget **b, gboolean c,
						gboolean d, gint e, void (*func)(), gpointer f, gchar *g)
{
    (*(callbacks->gkrellm_gtk_button_connected))(a, b, c, d, e, func, f, g);
}
GtkWidget	*gkrellm_gtk_scrolled_vbox(GtkWidget *a, GtkWidget **b,
						GtkPolicyType c, GtkPolicyType d)
{
    return (*(callbacks->gkrellm_gtk_scrolled_vbox))(a, b, c, d);
}
GtkWidget	*gkrellm_gtk_framed_vbox(GtkWidget *a, gchar *b, gint c, gboolean d,
						gint e, gint f)
{
    return (*(callbacks->gkrellm_gtk_framed_vbox))(a, b, c, d, e, f);
}
GtkWidget	*gkrellm_gtk_framed_vbox_end(GtkWidget *a, gchar *b, gint c, gboolean d,
						gint e, gint f)
{
    return (*(callbacks->gkrellm_gtk_framed_vbox_end))(a, b, c, d, e, f);
}
GtkWidget	*gkrellm_gtk_scrolled_text_view(GtkWidget *a, GtkWidget **b,
						GtkPolicyType c, GtkPolicyType d)
{
    return (*(callbacks->gkrellm_gtk_scrolled_text_view))(a, b, c, d);
}
void		gkrellm_gtk_text_view_append_strings(GtkWidget *a, gchar **b, gint c)
{
    (*(callbacks->gkrellm_gtk_text_view_append_strings))(a, b, c);
}
void		gkrellm_gtk_text_view_append(GtkWidget *a, gchar *b)
{
    (*(callbacks->gkrellm_gtk_text_view_append))(a, b);
}

  /* Some utility functions
  */
gchar		*gkrellm_homedir(void)
{
    return (*(callbacks->gkrellm_homedir))();
}
gboolean	gkrellm_dup_string(gchar **a, gchar *b)
{
    return (*(callbacks->gkrellm_dup_string))(a, b);
}
gboolean	gkrellm_locale_dup_string(gchar **a, gchar *b, gchar **c)
{
    return (*(callbacks->gkrellm_locale_dup_string))(a, b, c);
}
gchar		*gkrellm_make_config_file_name(gchar *a, gchar *b)
{
    return (*(callbacks->gkrellm_make_config_file_name))(a, b);
}
gchar		*gkrellm_make_data_file_name(gchar *a, gchar *b)
{
    return (*(callbacks->gkrellm_make_data_file_name))(a, b);
}
struct tm	*gkrellm_get_current_time(void)
{
    return (*(callbacks->gkrellm_get_current_time))();
}
gint		gkrellm_125_sequence(gint a, gboolean b, gint c, gint d,
						gboolean e, gboolean f)
{
    return (*(callbacks->gkrellm_125_sequence))(a, b, c, d, e, f);
}

  /* Session manager plugin helpers
  */
//gint		gkrellm_get_sm_argc(void)
//{
//    return (*(callbacks->gkrellm_get_sm_argc))();
//}
//gchar		**gkrellm_get_sm_argv(void)
//{
//    return (*(callbacks->gkrellm_get_sm_argv))();
//}
//gint		gkrellm_get_restart_options(gchar **a, gint b)
//{
//    return (*(callbacks->gkrellm_get_restart_options))(a, b);
//}
void		gkrellm_save_all(void)
{
    (*(callbacks->gkrellm_save_all))();
}

  /* ------- Some builtin monitor public functions -------- */

  /* Functions exported by cpu.c
  */
gint		gkrellm_smp_cpus(void)
{
    return (*(callbacks->gkrellm_smp_cpus))();
}
gboolean	gkrellm_cpu_stats(gint n, gulong *a, gulong *b, gulong *c, gulong *d)
{
    return (*(callbacks->gkrellm_cpu_stats))(n, a, b, c, d);
}


  /* Functions exported by net.c
  */
gint		gkrellm_net_routes(void)
{
    return (*(callbacks->gkrellm_net_routes))();
}
gboolean	gkrellm_net_stats(gint n, gchar *a, gulong *b, gulong *c)
{
    return (*(callbacks->gkrellm_net_stats))(n, a, b, c);
}
void		gkrellm_net_led_positions(gint *x_rx_led, gint *y_rx_led,
					gint *x_tx_led, gint *y_tx_led)
{
    (*(callbacks->gkrellm_net_led_positions))(x_rx_led, y_rx_led, x_tx_led, y_tx_led);
}


  /* Functions exported by the Mail monitor - see bottom of mail.c
  */
gboolean	gkrellm_get_mail_mute_mode(void)
{
    return (*(callbacks->gkrellm_get_mail_mute_mode))();
}
gpointer 	gkrellm_add_external_mbox(gint (*func)(), gboolean a, gpointer b)
{
    return (*(callbacks->gkrellm_add_external_mbox))(func, a, b);
}
void		gkrellm_destroy_external_mbox(gpointer a)
{
    (*(callbacks->gkrellm_destroy_external_mbox))(a);
}
void		gkrellm_set_external_mbox_counts(gpointer a, gint b, gint c)
{
    (*(callbacks->gkrellm_set_external_mbox_counts))(a, b, c);
}
void		gkrellm_set_external_mbox_tooltip(gpointer a, gchar *b)
{
    (*(callbacks->gkrellm_set_external_mbox_tooltip))(a, b);
}


      /* Functions new for 2.1.8
      */
void		gkrellm_panel_label_on_top_of_decals(GkrellmPanel *p,
					gboolean mode)
{
    (*(callbacks->gkrellm_panel_label_on_top_of_decals))(p, mode);
}
gboolean	gkrellm_alert_is_activated(GkrellmAlert *alert)
{
    return (*(callbacks->gkrellm_alert_is_activated))(alert);
}
void		gkrellm_alert_dup(GkrellmAlert **ap, GkrellmAlert *a_src)
{
    (*(callbacks->gkrellm_alert_dup))(ap, a_src);
}
void		gkrellm_alert_config_create_connect(GkrellmAlert *alert,
					void (*func)(), gpointer data)
{
    (*(callbacks->gkrellm_alert_config_create_connect))(alert, func, data);
}
void		gkrellm_alert_command_process_connect(GkrellmAlert *alert,
					void (*func)(), gpointer data)
{
    (*(callbacks->gkrellm_alert_command_process_connect))(alert, func, data);
}
gboolean	gkrellm_alert_decal_visible(GkrellmAlert *alert)
{
    return (*(callbacks->gkrellm_alert_decal_visible))(alert);
}
void		gkrellm_alert_set_delay(GkrellmAlert *alert, gint delay)
{
    (*(callbacks->gkrellm_alert_set_delay))(alert, delay);
}
void		gkrellm_alert_delay_config(GkrellmAlert *alert, gint step,
					gint high, gint low)
{
    (*(callbacks->gkrellm_alert_delay_config))(alert, step, high, low);
}
void		gkrellm_gtk_alert_button(GtkWidget *box, GtkWidget **button,
					gboolean expand, gboolean fill, gint pad,
					gboolean pack_start, void (*cb_func)(), gpointer data)
{
    (*(callbacks->gkrellm_gtk_alert_button))(box, button, expand, fill, pad, pack_start, cb_func, data);
}

      /* Functions new for 2.1.9
      */
GkrellmPiximage *gkrellm_piximage_new_from_inline(const guint8 *data,
						gboolean copy_pixels)
{
    return (*(callbacks->gkrellm_piximage_new_from_inline))(data, copy_pixels);
}
gboolean    gkrellm_load_piximage_from_inline(gchar *name,
						const guint8 *data, GkrellmPiximage **image,
						gchar *subdir, gboolean copy_pixels)
{
    return (*(callbacks->gkrellm_load_piximage_from_inline))(name, data, image, subdir, copy_pixels);
}
void		gkrellm_alert_commands_config(GkrellmAlert *alert,
                        gboolean alarm, gboolean warn)
{
    (*(callbacks->gkrellm_alert_commands_config))(alert, alarm, warn);
}
void		gkrellm_reset_alert_soft(GkrellmAlert *alert)
{
    (*(callbacks->gkrellm_reset_alert_soft))(alert);
}

/* Functions new for 2.1.11
  */
void		gkrellm_decal_text_clear(GkrellmDecal *d)
{
    (*(callbacks->gkrellm_decal_text_clear))(d);
}
void		gkrellm_decal_text_insert(GkrellmDecal *d, gchar *s,
					GkrellmTextstyle *ts, gint x_off, gint y_off)
{
    (*(callbacks->gkrellm_decal_text_insert))(d, s, ts, x_off, y_off);
}
GkrellmDecal *gkrellm_create_decal_text_with_height(GkrellmPanel *p,
					GkrellmTextstyle *ts, GkrellmStyle *style,
					gint x, gint y, gint w, gint h, gint y_baseline)
{
    return (*(callbacks->gkrellm_create_decal_text_with_height))(p, ts, style, x, y, w, h, y_baseline);
}
void		gkrellm_chartconfig_callback_block(GkrellmChartconfig *a,
					gboolean b)
{
    (*(callbacks->gkrellm_chartconfig_callback_block))(a, b);
}

  /* Functions new for 2.1.16
  */
void		gkrellm_alert_get_alert_state(GkrellmAlert *alert,
					gboolean *alarm_state, gboolean *warn_state)
{
    (*(callbacks->gkrellm_alert_get_alert_state))(alert, alarm_state, warn_state);
}
GkrellmAlertPlugin	*gkrellm_alert_plugin_add(GkrellmMonitor *mon,
					gchar *name)
{
    return (*(callbacks->gkrellm_alert_plugin_add))(mon, name);
}
void		gkrellm_alert_plugin_alert_connect(GkrellmAlertPlugin *gap,
            		void (*alarm_func)(), void (*warn_func)(),
					void (*update_func)(), void (*check_func)(),
					void (*destroy_func)())
{
    (*(callbacks->gkrellm_alert_plugin_alert_connect))(gap, alarm_func, warn_func, update_func, check_func, destroy_func);
}
void		gkrellm_alert_plugin_config_connect(GkrellmAlertPlugin *gap,
					gchar *tab_name,
            		void (*config_create_func)(), void (*config_done_func),
            		void (*config_save_func)(),void (*config_load_func)())
{
    (*(callbacks->gkrellm_alert_plugin_config_connect))(gap, tab_name, config_create_func, config_done_func, config_save_func, config_load_func);
}
gchar		*gkrellm_alert_plugin_config_get_id_string(GkrellmAlert *alert)
{
    return (*(callbacks->gkrellm_alert_plugin_config_get_id_string))(alert);
}
void		gkrellm_alert_plugin_alert_attach(GkrellmAlertPlugin *gap,
            		GkrellmAlert *alert, gpointer data)
{
    (*(callbacks->gkrellm_alert_plugin_alert_attach))(gap, alert, data);
}
void		gkrellm_alert_plugin_alert_detach(GkrellmAlertPlugin *gap,
					GkrellmAlert *alert)
{
    (*(callbacks->gkrellm_alert_plugin_alert_detach))(gap, alert);
}
gpointer	gkrellm_alert_plugin_get_data(GkrellmAlertPlugin *gap,
					GkrellmAlert *alert)
{
    return (*(callbacks->gkrellm_alert_plugin_get_data))(gap, alert);
}
void		gkrellm_alert_plugin_command_process(GkrellmAlert *alert,
					gchar *src, gchar *dst, gint dst_size)
{
    (*(callbacks->gkrellm_alert_plugin_command_process))(alert, src, dst, dst_size);
}
GtkWidget	*gkrellm_gtk_category_vbox(GtkWidget *box, gchar *category_header,
        			gint header_pad, gint box_pad, gboolean pack_start)
{
    return (*(callbacks->gkrellm_gtk_category_vbox))(box, category_header, header_pad, box_pad, pack_start);
}
void		gkrellm_remove_launcher(GkrellmLauncher *launch)
{
    (*(callbacks->gkrellm_remove_launcher))(launch);
}


//---------------------------------------------------------------------------
// new since 2.2.0

void    gkrellm_decal_get_size(GkrellmDecal *d, gint *w, gint *h)
  {callbacks->gkrellm_decal_get_size(d,w,h);}
void    gkrellm_decal_text_set_offset(GkrellmDecal *d, gint x, gint y)
  {callbacks->gkrellm_decal_text_set_offset(d,x,y);}
void    gkrellm_decal_text_get_offset(GkrellmDecal *d, gint *x, gint *y)
  {callbacks->gkrellm_decal_text_get_offset(d, x, y);}
void    gkrellm_chart_reuse_text_format(GkrellmChart *cp)
  {callbacks->gkrellm_chart_reuse_text_format(cp);}
gchar  *gkrellm_get_hostname(void)
  {return gkrellm_get_hostname();}

void    gkrellm_decal_scroll_text_set_text(GkrellmPanel *p, GkrellmDecal *d, gchar *text)
  {callbacks->gkrellm_decal_scroll_text_set_text(p,d,text);}
void    gkrellm_decal_scroll_text_get_size(GkrellmDecal *d, gint *w, gint *h)
  {callbacks->gkrellm_decal_scroll_text_get_size(d, w, h);}
void    gkrellm_decal_scroll_text_align_center(GkrellmDecal *d, gboolean center)
  {callbacks->gkrellm_decal_scroll_text_align_center(d,center);}
void    gkrellm_decal_scroll_text_horizontal_loop(GkrellmDecal *d, gboolean loop)
  {callbacks->gkrellm_decal_scroll_text_horizontal_loop(d,loop);}
void    gkrellm_decal_scroll_text_vertical_loop(GkrellmDecal *d, gboolean loop)
  {callbacks->gkrellm_decal_scroll_text_vertical_loop(d, loop);}

void    gkrellm_text_extents(PangoFontDescription *font_desc, gchar *text, gint len, gint *width, gint *height, gint *baseline, gint *y_ink)
  {callbacks->gkrellm_text_extents(font_desc, text, len, width, height, baseline, y_ink);}

gint    gkrellm_gdk_string_width(PangoFontDescription *d, gchar *text)
  {return callbacks->gkrellm_gdk_string_width(d, text);}
void    gkrellm_gdk_draw_string(GdkDrawable *drawable, PangoFontDescription *font, GdkGC *gc, gint x, gint y, gchar *string)
  {callbacks->gkrellm_gdk_draw_string(drawable, font, gc, x, y, string);}
void    gkrellm_gdk_draw_text(GdkDrawable *drawable, PangoFontDescription *font, GdkGC *gc, gint x, gint y, gchar *string, gint len)
  {callbacks->gkrellm_gdk_draw_text(drawable, font, gc, x, y, string, len);}

gboolean  gkrellm_client_mode(void)
  {return callbacks->gkrellm_client_mode();}
void      gkrellm_client_plugin_get_setup(gchar *key_name, void (*setup_func_cb)(gchar *str))
  {callbacks->gkrellm_client_plugin_get_setup(key_name, setup_func_cb);}
void      gkrellm_client_plugin_serve_data_connect(GkrellmMonitor *mon, gchar *key_name, void (*func_cb)(gchar *line))
  {callbacks->gkrellm_client_plugin_serve_data_connect(mon, key_name, func_cb);}
/*void      gkrellm_client_plugin_reconnect_connect(gchar *key_name, void (*func_cb)())
  {callbacks->gkrellm_client_plugin_reconnect_connect(key_name, func_cb);}*/


//---------------------------------------------------------------------------
// new since 2.2.1

void    gkrellm_draw_decal_markup(GkrellmPanel *p, GkrellmDecal *d, gchar *text)
  {callbacks->gkrellm_draw_decal_markup(p,d,text);}
void    gkrellm_decal_scroll_text_set_markup(GkrellmPanel *p, GkrellmDecal *d, gchar *text)
  {callbacks->gkrellm_decal_scroll_text_set_markup(p,d,text);}


//---------------------------------------------------------------------------
// new since 2.2.2

void    gkrellm_panel_label_get_position(GkrellmStyle *style, gint *x_position, gint *y_off)
  {callbacks->gkrellm_panel_label_get_position(style, x_position, y_off);}


//---------------------------------------------------------------------------
// new since 2.2.5

gboolean      gkrellm_client_send_to_server(gchar *key_name, gchar *line)
  {return callbacks->gkrellm_client_send_to_server(key_name, line);}

GkrellmDecal *gkrellm_create_decal_text_markup(GkrellmPanel *p, gchar *string, GkrellmTextstyle *ts, GkrellmStyle *style, gint x, gint y, gint w)
  {return callbacks->gkrellm_create_decal_text_markup(p,string,ts,style,x,y,w);}
void          gkrellm_decal_text_markup_insert(GkrellmDecal *d, gchar *s, GkrellmTextstyle *ts, gint x_off, gint y_off)
  {callbacks->gkrellm_decal_text_markup_insert(d,s,ts,x_off,y_off);}

void          gkrellm_decal_text_nth_inserted_set_offset(GkrellmDecal *d, gint n, gint x_off, gint y_off)
  {callbacks->gkrellm_decal_text_nth_inserted_set_offset(d, n, x_off, y_off);}
void          gkrellm_decal_text_nth_inserted_get_offset(GkrellmDecal *d, gint n, gint *x_off, gint *y_off)
  {callbacks->gkrellm_decal_text_nth_inserted_get_offset(d,n,x_off,y_off);}
void          gkrellm_config_instant_apply(GkrellmMonitor *mon)
  {callbacks->gkrellm_config_instant_apply(mon);}
GtkTreeSelection *gkrellm_gtk_scrolled_selection(GtkTreeView *treeview, GtkWidget *box, GtkSelectionMode s_mode, GtkPolicyType h_policy, GtkPolicyType v_policy, void (*func_cb)(), gpointer data)
  {return callbacks->gkrellm_gtk_scrolled_selection(treeview, box, s_mode, h_policy, v_policy, func_cb, data);}
void          gkrellm_text_markup_extents(PangoFontDescription *font_desc, gchar *text, gint len, gint *width, gint *height, gint *baseline, gint *y_ink)
  {callbacks->gkrellm_text_markup_extents(font_desc, text, len, width, height, baseline, y_ink);}
gint          gkrellm_gdk_string_markup_width(PangoFontDescription *d, gchar *s)
  {return callbacks->gkrellm_gdk_string_markup_width(d,s);}
gint          gkrellm_gdk_text_markup_width(PangoFontDescription *font_desc, const gchar *string, gint len)
  {return callbacks->gkrellm_gdk_text_markup_width(font_desc, string, len);}
void          gkrellm_gdk_draw_string_markup(GdkDrawable *drawable, PangoFontDescription *font, GdkGC *gc, gint x, gint y, gchar *string)
  {callbacks->gkrellm_gdk_draw_string_markup(drawable, font, gc, x, y, string);}
void          gkrellm_gdk_draw_text_markup(GdkDrawable *drawable, PangoFontDescription *font, GdkGC *gc, gint x, gint y, gchar *string, gint len)
  {callbacks->gkrellm_gdk_draw_text_markup(drawable, font, gc, x, y, string, len);}

//---------------------------------------------------------------------------
// new since 2.3.2

void          gkrellm_debug(guint debug_level, const gchar *format, ...)
	{
  va_list arg;
  va_start(arg, format);
  callbacks->gkrellm_debugv(debug_level, format, arg);
  va_end(arg);
	}

void          gkrellm_debugv(guint debug_level, const gchar *format, va_list arg)
	{
  callbacks->gkrellm_debugv(debug_level, format, arg);
	}
