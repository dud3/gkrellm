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

/* These functions define the GKrellM interface to the system dependent
|  data reading module.  All gkrellm_sys_xxx() functions must exist in
|  an implementation.
|
|  The gkrellm_sys_init_xxx() functions will be called at GKrellM startup
|  and must return TRUE if the monitor is supported in the implementation.
|  Data initializations for a monitor interface may be done in these functions.
|
|  gkrellm_xxx_client_divert() functions provide for the system dependent
|  module to be implemented in a client module which will override the builtin
|  implementation and read data from a server.
*/

/* User registered ports 1024 - 49151
|  Dynamic/private ports 49152 - 65535
*/
#define GKRELLMD_SERVER_PORT	19150

/* ===================================================================== */
/* gkrellm_sys_main_init() is called at GKrellM startup before any other
|      gkrellm_sys_xxx_init() calls are made.
|  gkrellm_sys_main_cleanup() is called when GKrellM exits any time after
|     all gkrellm_sys_xxx_init() calls have been made.
*/

void	gkrellm_sys_main_init(void);
void	gkrellm_sys_main_cleanup(void);

	/* If the sysdep code needs to call a function for additional
	|  setup after the main() arguments are parsed and GTK is
	|  initialized, register the function by calling this from
	|  gkrellm_sys_main_init().
	*/
void	gkrellm_sys_setup_connect(void (*setup_func)());

	/* The setup function will be passed the main() argc and argv and
	|  should be declared like:  void XXX_setup(gint ac, gchar **av);
	*/

/* ===================================================================== */
/* Misc monitor interface functions */

gchar	*gkrellm_sys_get_host_name(void);
gchar	*gkrellm_sys_get_system_name(void);

gint	gkrellm_get_timer_ticks(void);

  /* If local mailboxes are not applicable, call this from
  |  gkrellm_sys_main_init().
  */
void	gkrellm_mail_local_unsupported(void);

GThread *gkrellm_mail_get_active_thread(void);

/* ===================================================================== */
/* CPU monitor interface
|
|  gkrellm_cpu_set_number_of_cpus() must be called in gkrellm_sys_cpu_init().
|
|  gkrellm_cpu_add_instance() should be used if the platform's cpus
|      have noncontiguous instance numbers.  It should be called once for
|      each cpu before gkrellm_cpu_set_number_of_cpus() is called.
|
|  gkrellm_cpu_assign_composite_data() may be optionally called if an
|      implementation has composite cpu data.  If this function is not called,
|      GKrellM will automatically accumulate composite cpu data.
|
|  gkrellm_cpu_assign_data() must be called for each real cpuN, N = {list of
|      all cpu instance numbers}
*/

gboolean gkrellm_sys_cpu_init(void);
void	gkrellm_sys_cpu_read_data(void);

void	gkrellm_cpu_client_divert(void (*read_func)());
void	gkrellm_cpu_set_number_of_cpus(gint n_cpus);
void	gkrellm_cpu_add_instance(gint inst);
void	gkrellm_cpu_nice_time_unsupported(void);
void	gkrellm_cpu_assign_composite_data(gulong user, gulong nice,
				gulong sys, gulong idle);
void	gkrellm_cpu_assign_data(gint n, gulong user, gulong nice,
				gulong sys, gulong idle);


/* ===================================================================== */
/* Proc monitor interface
*/

gboolean gkrellm_sys_proc_init(void);
void	gkrellm_sys_proc_read_data(void);
void	gkrellm_sys_proc_read_users(void);

void	gkrellm_proc_client_divert(void (*read_proc_func)(),
				void (*read_users_func)());
void	gkrellm_proc_assign_data(gint n_processes, gint n_running,
				gulong n_forks, gfloat load);
void	gkrellm_proc_assign_users(gint n_users);


/* ===================================================================== */
/* Disk monitor interface
|
|  An implementation can choose to assign disk data by one of three methods.
|  If gkrellm_disk_assign_data_by_device() is used, then the function
|      gkrellm_sys_disk_name_from_device() must provide a name mapping from
|      the device numbers to an identifying name.  It may also set an order
|      value for the name to control the stacking order of the disk chart.
|  If gkrellm_disk_assign_data_by_name() is used, then an order may be
|      optionally specifed with gkrellm_sys_disk_order_from_name().
|      (the default order of -1 simply appends disk charts as data for them
|      is reported).  After this function is called for a disk, the function
|      gkrellm_disk_subdisk_assign_data_by_name() may be called for subdisk
|      (partition) data for the disk if available.  Subdisk data is not
|      added to the composite disk since a subdisk data is expected to be
|      a portion of the previously reported whole disk data.
|  If gkrellm_disk_assign_data_nth() is used, default DiskA, DiskB, ... names
|      will be used.
*/

gboolean gkrellm_sys_disk_init(void);
void	gkrellm_sys_disk_read_data(void);
gchar	*gkrellm_sys_disk_name_from_device(gint device_number,
				gint unit_number, gint *order);
gint	gkrellm_sys_disk_order_from_name(const gchar *name);

void	gkrellm_disk_client_divert(void (*read_func)(),
			gchar *(*name_from_device_func)(), gint (*order_from_name_func)());
void    gkrellm_disk_add_by_name(const gchar *name, const gchar *label);
void	gkrellm_disk_assign_data_by_device(gint device_number,
				gint unit_number, guint64 r, guint64 w, gboolean virtual);
void	gkrellm_disk_assign_data_nth(gint n, guint64 r, guint64 w,
				gboolean virtual);
void	gkrellm_disk_assign_data_by_name(gchar *name,
				guint64 r, guint64 w, gboolean virtual);
void	gkrellm_disk_subdisk_assign_data_by_name(gchar *subdisk_name,
				gchar *disk_name, guint64 r, guint64 w);

void	gkrellm_disk_reset_composite(void);	/* Only Linux may need this */
void	gkrellm_disk_units_are_blocks(void);	/* Otherwise bytes */


/* ===================================================================== */
/* Inet monitor interface
|
|  System dependent code should #include "inet.h" and inet.h
|  should #define INET6 if IPV6 protocol is available.
*/

gboolean gkrellm_sys_inet_init(void);
void	gkrellm_sys_inet_read_tcp_data(void);

void	gkrellm_inet_client_divert(void (*read_tcp_func)());
void	gkrellm_inet_log_tcp_port_data(gpointer tcp);


/* ===================================================================== */
/* Net monitor interface
|
|  Works in one of two ways:
|  1) When gkrellm_sys_net_read_data() is called, simply loop through
|     available net interfaces and call gkrellm_net_assign_data() on them.
|     Each second the net code will check if data is still being assigned
|     to each interface and mark a net down if not.  For this mode do not
|     call gkrellm_net_use_routed() or gkrellm_net_routed_event(), and
|     gkrellm_sys_net_check_routes() should be an empty function.
|  2) If routed information is available separately from net data stats, the
|     net up/down state can be controlled by the system dependent code.  For
|     this mode, call gkrellm_net_use_routed(TRUE) from gkrellm_sys_net_init().
|     Then, each second the gkrellm_sys_net_check_routes() function will be
|     called and it should call gkrellm_net_routed_event() on any net
|     interfaces that have had a change in routed state.
*/
gboolean gkrellm_sys_net_init(void);
void	gkrellm_sys_net_read_data(void);
void	gkrellm_sys_net_check_routes(void);
gboolean gkrellm_sys_net_isdn_online(void);

void	gkrellm_net_client_divert(void (*read_func)(),
				void (*check_routes_func)(), gboolean (*isdn_online_func)());
void	gkrellm_net_assign_data(gchar *, gulong, gulong);
gchar	*gkrellm_net_mon_first(void);
gchar	*gkrellm_net_mon_next(void);
void	gkrellm_net_routed_event(gchar *name, gboolean routed);

  /* Routines optionally called from gkrellm_sys_net_init()
  *  or client_sys_net_init().
  */
void	gkrellm_net_use_routed(gboolean);
void	gkrellm_net_add_timer_type_ppp(gchar *);
void	gkrellm_net_add_timer_type_ippp(gchar *);
void	gkrellm_net_set_lock_directory(gchar *);


/* ===================================================================== */
/* Memory/Swap monitor interface
*/

gboolean gkrellm_sys_mem_init(void);
void	gkrellm_sys_mem_read_data(void);
void	gkrellm_sys_swap_read_data(void);

void	gkrellm_mem_client_divert(void (*read_mem_func)(),
				void (*read_swap_func)());
void	gkrellm_mem_assign_data(guint64 total, guint64 used, guint64 free,
				guint64 shared, guint64 buffers, guint64 cached);
void	gkrellm_swap_assign_data(guint64 total, guint64 used,
				gulong swap_in, gulong swap_out);


/* ===================================================================== */
/* FS monitor interface
|
|  gkrellm_sys_fs_init() should call gkrellm_fs_set_eject_commands() if the
|  commands are available.
*/

gboolean gkrellm_sys_fs_init(void);
void	gkrellm_sys_fs_get_fsusage(gpointer fs, gchar *dir);
void	gkrellm_sys_fs_get_mounts_list(void);
void	gkrellm_sys_fs_get_fstab_list(void);
gboolean gkrellm_sys_fs_fstab_modified(void);

void	gkrellm_fs_client_divert(void (*get_fsusage_func)(),
				void (*get_mounts_func)(), void (*get_fstab_func)(),
				gboolean (*fstab_modified_func)());
void	gkrellm_fs_assign_fsusage_data(gpointer fs,
				glong blocks, glong avail, glong free, glong bsize);
void	gkrellm_fs_add_to_fstab_list(gchar *dir, gchar *dev, gchar *type,
				gchar *options);
void	gkrellm_fs_add_to_mounts_list(gchar *dir, gchar *dev, gchar *type);
void	gkrellm_fs_setup_eject(gchar *eject_tray, gchar *close_tray,
				void (*eject_func)(), void (*close_func)());
void	gkrellm_fs_mounting_unsupported(void);


/* ===================================================================== */
/* Battery monitor interface
*/

#define	GKRELLM_BATTERY_COMPOSITE_ID	-1

gboolean gkrellm_sys_battery_init(void);
void	gkrellm_sys_battery_read_data(void);

void	gkrellm_battery_client_divert(void (*read_func)());
void	gkrellm_battery_assign_data(gint id, gboolean available,
				gboolean on_line, gboolean charging,
				gint percent, gint time_left /* minutes */);

/* ===================================================================== */
/* Uptime monitor interface
*/

gboolean gkrellm_sys_uptime_init(void);
time_t	gkrellm_sys_uptime_read_uptime(void);

void	gkrellm_uptime_client_divert(time_t (*read_func)());
void	gkrellm_uptime_set_base_uptime(time_t base_uptime);


/* ===================================================================== */
/* Sensor monitor interface
*/

gboolean gkrellm_sys_sensors_init(void);
gboolean gkrellm_sys_sensors_get_temperature(gchar *name, gint id,
			gint iodev, gint inter, gfloat *t);
gboolean gkrellm_sys_sensors_get_fan(gchar *name, gint id,
			gint iodev, gint inter, gfloat *f);
gboolean gkrellm_sys_sensors_get_voltage(gchar *name, gint id,
			gint iodev, gint inter, gfloat *v);

void	gkrellm_sensors_client_divert(gboolean (*get_temp_func)(),
            gboolean (*get_fan_func)(), gboolean (*get_volt_func)());
gpointer gkrellm_sensors_add_sensor(gint type, gchar *sensor_path,
			gchar *id_name,
        	gint id, gint iodev, gint inter,
        	gfloat factor, gfloat offset, gchar *vref, gchar *default_label);
void	gkrellm_sensors_set_group(gpointer sr, gint group);
void	gkrellm_sensors_sysdep_option(gchar *keyword, gchar *label,
			void (*func)());

  /* Common sensor reading from daemons.  Interfaces from 0 - 20 are available
  |  for system dependent interfaces.  Interfaces from 20 up are used here
  |  for the common daemon interfaces.
  */
#define HDDTEMP_INTERFACE	20
#define MBMON_INTERFACE		21

  /* Common hddtemp interface*/
void	gkrellm_sys_sensors_hddtemp_check(void);
gboolean gkrellm_sys_sensors_hddtemp_get_value(gchar *name, gfloat *value);

  /* Common mbmon interface*/
gboolean	gkrellm_sys_sensors_mbmon_supported(void);
gboolean	gkrellm_sys_sensors_mbmon_check(gboolean force);
gboolean	gkrellm_sys_sensors_mbmon_get_value(gchar *name, gfloat *value);
gboolean	gkrellm_sys_sensors_mbmon_port_change(gint port);

  /* Config migration helper */
void	gkrellm_sensors_config_migrate_connect(
				gboolean (*config_migrate_func)(), gint sysdep_private);
