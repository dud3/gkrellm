/* GKrellM
|  Copyright (C) 1999-2010 Bill Wilson
|                2002      Bill Nalen
|                2007-2010 Stefan Gehn
|
|  Authors:  Bill Wilson    billw@gkrellm.net
|            Bill Nalen     bill@nalens.com
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

#include "../inet.h" // For struct ActiveTCP
#if defined(GKRELLM_SERVER)
	#include "../../server/win32-plugin.h"
#else
	#include "../win32-plugin.h"
#endif

#include <winioctl.h> // For cdrom eject
#include <iphlpapi.h> // For tcp connection stats

// Following two are for cpu, proc, disk and network stats
// which are queried via "performance data counters"
#include <pdh.h>
#include <pdhmsg.h>

// Following two are used to determine number of logged in users and
// pagefile usage via NT-APIs
#include <subauth.h>
#include <ntsecapi.h>

#if _WIN32_WINNT >= 0x501 // Windows XP or newer
#include <wtsapi32.h>
#endif // _WIN32_WINNT >= 0x501


// ----------------------------------------------------------------------------
// Needed to determine pagefile usage
//
// These definitions were taken from MinGW include/ddk/ntapi.h because you
// cannot mix ddk includes with normal windows includes although you can call
// these functions without being a driver.
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#define SystemPagefileInformation 18

typedef NTSTATUS (NTAPI *pfZwQuerySystemInformation)(
  /*IN*/ UINT SystemInformationClass,
  /*IN OUT*/ VOID *SystemInformation,
  /*IN*/ ULONG SystemInformationLength,
  /*OUT*/ ULONG *ReturnLength /*OPTIONAL*/);

typedef struct _SYSTEM_PAGEFILE_INFORMATION
	{
	ULONG  NextEntryOffset;
	ULONG  CurrentSize;
	ULONG  TotalUsed;
	ULONG  PeakUsed;
	UNICODE_STRING  FileName;
	}
	SYSTEM_PAGEFILE_INFORMATION;


// ----------------------------------------------------------------------------
/* Structs and typedefs used to determine the number of logged in users.
 * These should be in ntsecapi.h but are missing in MinGW currently, they
 * are present in the headers provided by mingw-w64.
 * Docs: http://msdn.microsoft.com/en-us/library/aa378290(VS.85).aspx
 */ 
#if defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
typedef struct _SECURITY_LOGON_SESSION_DATA
	{
	ULONG Size;
	LUID LogonId;
	LSA_UNICODE_STRING UserName;
	LSA_UNICODE_STRING LogonDomain;
	LSA_UNICODE_STRING AuthenticationPackage;
	ULONG LogonType;
	ULONG Session;
	PSID Sid;
	LARGE_INTEGER LogonTime;
	LSA_UNICODE_STRING LogonServer;
	LSA_UNICODE_STRING DnsDomainName;
	LSA_UNICODE_STRING Upn;
	}
	SECURITY_LOGON_SESSION_DATA;
#endif

// Definitions for function pointers (functions resolved manually at runtime)
typedef NTSTATUS (NTAPI *pfLsaEnumerateLogonSessions)(
		ULONG *LogonSessionCount, LUID **LogonSessionList);
typedef NTSTATUS (NTAPI *pfLsaGetLogonSessionData)(
		LUID *LogonId, SECURITY_LOGON_SESSION_DATA **ppLogonSessionData);
typedef NTSTATUS (NTAPI *pfLsaFreeReturnBuffer)(VOID *Buffer);


// ----------------------------------------------------------------------------
// Missing definitions from MinGW iphlpapi.h

typedef struct {
  DWORD dwState; // one of MIB_TCP_STATE_*
  IN6_ADDR LocalAddr;
  DWORD dwLocalScopeId;
  DWORD dwLocalPort;
  IN6_ADDR RemoteAddr;
  DWORD dwRemoteScopeId;
  DWORD dwRemotePort;
} MIB_TCP6ROW, *PMIB_TCP6ROW;

typedef struct {
  DWORD dwNumEntries;
  MIB_TCP6ROW table[ANY_SIZE];
} MIB_TCP6TABLE, *PMIB_TCP6TABLE;

typedef DWORD (WINAPI *GetTcp6TableFunc)(PMIB_TCP6TABLE TcpTable,
	PDWORD SizePointer, BOOL Order);


// ----------------------------------------------------------------------------
// Max len of device names returned by clean_dev_name().
// Value taken from net.c load_net_config() and disk.c load_disk_config().
#define MAX_DEV_NAME 31

#define ARR_SZ(x) (sizeof(x) / sizeof(x[0]))

static PDH_HQUERY pdhQueryHandle = NULL;

static const wchar_t* PDHDLL = L"PDH.DLL";
static const wchar_t* NTDLL = L"NTDLL.DLL";
static const wchar_t* WTSAPI32 = L"WTSAPI32.DLL";


// ----------------------------------------------------------------------------
// Own cleanup functions, called in gkrellm_sys_main_cleanup() to cleanup
// resources allocated by gkrellm_sys_*_init()

static void gkrellm_sys_cpu_cleanup(void);
static void gkrellm_sys_disk_cleanup(void);
static void gkrellm_sys_mem_cleanup(void);
static void gkrellm_sys_inet_cleanup(void);
static void gkrellm_sys_net_cleanup(void);
static void gkrellm_sys_proc_cleanup(void);


// ----------------------------------------------------------------------------

//! print a warning and (if possible) decode a windows error number
static void
win32_warning(const wchar_t *dll_name, DWORD status, const gchar *format, ...)
	{
	va_list varargs;
	wchar_t *status_msg = NULL;
	gchar *formatted_msg = NULL;
	HMODULE dll_handle = NULL;
	DWORD flags =   FORMAT_MESSAGE_ALLOCATE_BUFFER
	              | FORMAT_MESSAGE_FROM_SYSTEM
	              | FORMAT_MESSAGE_IGNORE_INSERTS;

	va_start(varargs, format);
	// Format passed message string
	formatted_msg = g_strdup_vprintf(format, varargs);
	va_end(varargs);

	// Load library for message strings if one was passed
	if (dll_name != NULL)
	{
		dll_handle = LoadLibraryW(dll_name);
		if (dll_handle != NULL)
			flags |= FORMAT_MESSAGE_FROM_HMODULE;
	}

	// NOTE: yes, this call takes a wchar_t **, it's a known flaw in the
	//       WIN32 API, you can ignore any compiler warning about arg 5
	if (FormatMessageW(
		  flags // dwFlags
        , dll_handle // lpSource
        , status // dwMessageId
        , MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT) // dwLanguageId
		, (LPWSTR)&status_msg // lpBuffer
		, 0 // nSize
		, NULL // varargs
		) > 0)
		{
		g_log(NULL, G_LOG_LEVEL_WARNING, "%s; Error 0x%lX: %ls",
				formatted_msg, status, status_msg);
		LocalFree(status_msg);
		}
	else
		{
		g_log(NULL, G_LOG_LEVEL_WARNING, "%s; Error 0x%lX\n",
				formatted_msg, status);
		}

	g_free(formatted_msg);
	if (dll_handle != NULL)
		FreeLibrary(dll_handle);
	}


// ----------------------------------------------------------------------------

// Simple wrapper around PdhAddCounter with error/debug handling
static gboolean
add_counter(const wchar_t *counter_path, PDH_HCOUNTER *counter_handle)
	{
	PDH_STATUS st;

	if (pdhQueryHandle == NULL || !counter_path || !counter_handle)
		return FALSE;

	st = PdhAddCounterW(pdhQueryHandle, counter_path, 0, counter_handle);
	if (st != ERROR_SUCCESS)
		{
		win32_warning(PDHDLL, st, "Failed adding pdh-counter for path '%ls'",
				counter_path);
		return FALSE;
		}

	gkrellm_debug(DEBUG_SYSDEP, "Added pdh-counter for path '%ls'\n",
			counter_path);
	return TRUE;
	}

static gboolean
get_formatted_counter_value(
		PDH_HCOUNTER counter_handle, const gchar *counter_name,
		DWORD format, PDH_FMT_COUNTERVALUE *val)
	{
	PDH_STATUS st;

	st = PdhGetFormattedCounterValue(counter_handle, format, NULL, val);
	if ((st != ERROR_SUCCESS) || (val->CStatus != PDH_CSTATUS_VALID_DATA))
		{
		win32_warning(PDHDLL, st,
			"Getting pdh-counter (%s) failed; CStatus %lX",
			counter_name, val->CStatus);
		return FALSE;
		}
	return TRUE;
	}

// Simple wrapper around PdhLookupPerfNameByIndex with error handling
static gboolean
lookup_perfname(DWORD index, wchar_t *perfname, DWORD perfname_max_len)
	{
	PDH_STATUS st;

	if (!perfname || perfname_max_len == 0)
		return FALSE;

	st = PdhLookupPerfNameByIndexW(NULL, index, perfname, &perfname_max_len);
	if (st != ERROR_SUCCESS)
		{
		win32_warning(PDHDLL, st, "Could not lookup perfname for index %lu",
				index);
		return FALSE;
		}

	if (perfname[0] == 0)
		{
		g_warning("Got empty perfname for index %lu, performance counters " \
				"appear to be broken on this system!\n", index);
		return FALSE;
		}

	gkrellm_debug(DEBUG_SYSDEP, "Looked up perfname '%ls' for index %lu\n",
			perfname, index);
	return TRUE;
	}

typedef void (*add_counter_cb) (wchar_t *name, PDH_HCOUNTER *c1, PDH_HCOUNTER *c2);

static void
add_counter_list(guint object_index,
		guint counter_index1, guint counter_index2,
		add_counter_cb cb)
	{
	PDH_STATUS  st;
	wchar_t		obj_name[PDH_MAX_COUNTER_NAME];
	wchar_t		c1_name[PDH_MAX_COUNTER_NAME];
	wchar_t		c2_name[PDH_MAX_COUNTER_NAME];
	wchar_t *	obj_list = NULL;
	DWORD		obj_list_size = 0;
	wchar_t *	inst_list = NULL;
	DWORD		inst_list_size = 0;
	wchar_t		counter_path[PDH_MAX_COUNTER_PATH];
	wchar_t *	inst = NULL;
	PDH_HCOUNTER c1;
	PDH_HCOUNTER c2;

	gkrellm_debug(DEBUG_SYSDEP, "add_counter_list()\n");
	if (pdhQueryHandle == NULL)
		return;

	// Get translated name for object_index
	if (!lookup_perfname(object_index, obj_name, PDH_MAX_COUNTER_NAME))
		return;

	if (!lookup_perfname(counter_index1, c1_name, PDH_MAX_COUNTER_NAME))
		return;

	if (!lookup_perfname(counter_index2, c2_name, PDH_MAX_COUNTER_NAME))
		return;

	// Get number of counters/instances that can be queried
	st = PdhEnumObjectItemsW(NULL, NULL, obj_name,
			NULL, &obj_list_size,
			NULL, &inst_list_size,
			PERF_DETAIL_WIZARD, 0);

	if ((st != PDH_MORE_DATA) && (st != ERROR_SUCCESS))
	{
		// Either no data at all or other error
		win32_warning(PDHDLL, st,
				"Failed to get pdh-counter count for object '%ls'", obj_name);
		return;
	}

	// Do nothing if there's no counters
	if (inst_list_size == 0)
		return;

	// Allocate buffers to hold object and instance names
	obj_list = (wchar_t *)g_malloc(sizeof(wchar_t) * obj_list_size);
	inst_list = (wchar_t *)g_malloc(sizeof(wchar_t) * inst_list_size);

	//gkrellm_debug(DEBUG_SYSDEP, "Max instance list size: %lu\n", inst_list_size);
	// Get actual information about counters
	st = PdhEnumObjectItemsW(NULL, NULL, obj_name,
			obj_list, &obj_list_size,
			inst_list, &inst_list_size,
			PERF_DETAIL_WIZARD, 0);
	if (st != ERROR_SUCCESS)
		{
		// Either no data at all or other error
		win32_warning(PDHDLL, st,
				"Failed to enumerate pdh-counters for object '%ls'", obj_name);
		}
	else
		{
		/*gkrellm_debug(DEBUG_SYSDEP, "Returned instance list size: %lu\n",
				inst_list_size);*/
		for (inst = inst_list; *inst != 0; inst += wcslen(inst) + 1)
			{
			//gkrellm_debug(DEBUG_SYSDEP, "counter instance '%ls'\n", inst);

			// Ignore total counter, gkrellm provides that functionality
			if (wcsnicmp(L"_Total", inst, 6) == 0)
				continue;

			// "\Disks(DiskOne)\ReadBytes"
			_snwprintf(counter_path, ARR_SZ(counter_path), L"\\%ls(%ls)\\%ls",
					obj_name, inst, c1_name);
			if (!add_counter(counter_path, &c1))
				continue;

			// "\Disks(DiskOne)\WriteBytes"
			_snwprintf(counter_path, ARR_SZ(counter_path), L"\\%ls(%ls)\\%ls",
					obj_name, inst, c2_name);
			if (!add_counter(counter_path, &c2))
				continue;

			if (c1 && c2)
				cb(inst, &c1, &c2);
			}
		}
	g_free((gpointer)obj_list);
	g_free((gpointer)inst_list);
	}

static
gchar *clean_dev_name(const wchar_t *name)
{
	gchar *clean_name;
	gchar *p;

	clean_name = g_utf16_to_utf8(name, -1, NULL, NULL, NULL);
	//FIXME: handle clean_name being NULL

	p = clean_name;
	while (*p)
		{
		p = g_utf8_next_char(p);
		if (*p == ' ' || *p == '\t')
			*p = '_';
		}

	// limit length of device name, gkrellm can't handle longer names :(
	if (strlen(clean_name) > MAX_DEV_NAME)
		clean_name[MAX_DEV_NAME] = '\0';

	return clean_name;
}


// ----------------------------------------------------------------------------

void gkrellm_sys_main_init(void)
	{
	PDH_STATUS st;
	WSADATA wsdata;
	int err;

	// Remove current working directory from DLL search path
	SetDllDirectoryW(L"");

	gkrellm_debug(DEBUG_SYSDEP, "Starting Winsock\n");
	err = WSAStartup(MAKEWORD(1,1), &wsdata);
	if (err != 0)
		g_warning("Starting Winsock failed with error code %d\n", err);

	gkrellm_debug(DEBUG_SYSDEP, "Opening PDH query\n");
	st = PdhOpenQueryW(NULL, 0, &pdhQueryHandle);
	if ((st != ERROR_SUCCESS) || (pdhQueryHandle == NULL))
		{
		win32_warning(PDHDLL, st, "Opening PDH query failed");
		pdhQueryHandle = 0;
		}

	// we don't have local mail on Windows (yet?)
	gkrellm_mail_local_unsupported();

	// initialize call back structure for plugins
	win32_init_callbacks();
	}


void gkrellm_sys_main_cleanup(void)
	{
	int i;
	PDH_STATUS st;

	gkrellm_debug(DEBUG_SYSDEP, "Waiting for mail checking thread to end.\n");
	i = 0;
	while (gkrellm_mail_get_active_thread() != NULL && (i++ < 120))
		{
		// wait here till it finishes
		// in case we are trying to get mail info
		g_usleep(G_USEC_PER_SEC); // 1 second wait
		}

	// Close PDH query-handle
	gkrellm_debug(DEBUG_SYSDEP, "Closing PDH query\n");
	st = PdhCloseQuery(pdhQueryHandle);
	if (st != ERROR_SUCCESS)
		win32_warning(PDHDLL, st, "Closing PDH query handle failed");
	pdhQueryHandle = NULL;

	gkrellm_sys_cpu_cleanup();
	gkrellm_sys_disk_cleanup();
	gkrellm_sys_net_cleanup();
	gkrellm_sys_inet_cleanup();
	gkrellm_sys_proc_cleanup();
	gkrellm_sys_mem_cleanup();

	gkrellm_debug(DEBUG_SYSDEP, "Closing Winsock\n");
	i = WSACleanup();
	if (i != 0)
		g_warning("Stopping Winsock failed, error %d\n", i);
	}

// only need to collect pdhQueryHandle data once for all those monitors that use it
static
void win32_read_proc_stat(void)
	{
	static gint s_data_read_tick = -1;
	PDH_STATUS st;

	if (pdhQueryHandle == NULL)
		return;

	if (s_data_read_tick == gkrellm_get_timer_ticks())	// One read per tick
		return;
	s_data_read_tick = gkrellm_get_timer_ticks();

	gkrellm_debug(DEBUG_SYSDEP, "Collecting PDH query data\n");
	st = PdhCollectQueryData(pdhQueryHandle);
	if (st != ERROR_SUCCESS)
		win32_warning(PDHDLL, st, "Failed to collect PDH query data");
	}


/* ===================================================================== */
/* Sensor interface */
/* ===================================================================== */

#define MBM_INTERFACE 1 /* MotherBoardMonitor 5 */
#define SF_INTERFACE  2 /* SpeedFan */
#define CT_INTERFACE  3 /* CoreTemp */
#define GPUZ_INTERFACE 4 /* GPU-Z */

/* TODO: Build as separate object */
#include "win32-sensors.c"

gboolean
gkrellm_sys_sensors_get_voltage(gchar *device_name, gint id,
		gint iodev, gint inter, gfloat *volt)
{
	if (inter == MBM_INTERFACE)
		return gkrellm_sys_sensors_mbm_get_value(id, SENSOR_VOLTAGE, volt);
	if (inter == SF_INTERFACE)
		return gkrellm_sys_sensors_sf_get_value(id, SENSOR_VOLTAGE, volt);
	if (inter == GPUZ_INTERFACE)
		return gkrellm_sys_sensors_gpuz_get_value((guint)id, volt);
	return FALSE;
}

gboolean
gkrellm_sys_sensors_get_fan(gchar *device_name, gint id,
		gint iodev, gint inter, gfloat *fan)
	{
	if (inter == MBM_INTERFACE)
		return gkrellm_sys_sensors_mbm_get_value(id, SENSOR_FAN, fan);
	if (inter == SF_INTERFACE)
		return gkrellm_sys_sensors_sf_get_value(id, SENSOR_FAN, fan);
	if (inter == GPUZ_INTERFACE)
		return gkrellm_sys_sensors_gpuz_get_value((guint)id, fan);
	return FALSE;
	}

gboolean
gkrellm_sys_sensors_get_temperature(gchar *device_name, gint id,
		gint iodev, gint inter, gfloat *temp)
	{
	if (inter == MBM_INTERFACE)
		return gkrellm_sys_sensors_mbm_get_value(id, SENSOR_TEMPERATURE, temp);
	if (inter == SF_INTERFACE)
		return gkrellm_sys_sensors_sf_get_value(id, SENSOR_TEMPERATURE, temp);
	if (inter == CT_INTERFACE)
		return gkrellm_sys_sensors_ct_get_temp((guint)id, (guint)iodev, temp);
	if (inter == GPUZ_INTERFACE)
		return gkrellm_sys_sensors_gpuz_get_value((guint)id, temp);
	return FALSE;
	}

gboolean
gkrellm_sys_sensors_init(void)
	{
	gboolean init_ok = FALSE;

	gkrellm_debug(DEBUG_SYSDEP, "INIT sensors\n");
	init_ok |= gkrellm_sys_sensors_sf_init();
	init_ok |= gkrellm_sys_sensors_ct_init();
	init_ok |= gkrellm_sys_sensors_gpuz_init();
	init_ok |= gkrellm_sys_sensors_mbm_init();

	gkrellm_debug(DEBUG_SYSDEP, "INIT sensors finished, result is %d\n", init_ok);
	// returns true if at least one sensors interface has been found
	return init_ok;
	}


/* ===================================================================== */
/* CPU monitor interface */
/* ===================================================================== */

typedef struct _GK_CPU
	{
	PDH_HCOUNTER total_pdh_counter;
	PDH_HCOUNTER sys_pdh_counter;
	gulong user;
	gulong sys;
	gulong idle;
	} GK_CPU;

static GPtrArray *s_cpu_ptr_array = NULL;

void
gkrellm_sys_cpu_read_data(void)
	{
	PDH_FMT_COUNTERVALUE tot;
	PDH_FMT_COUNTERVALUE sys;
	gint i;
	GK_CPU *cpu;

	if (pdhQueryHandle == NULL)
		return;
	win32_read_proc_stat(); // eventually fetch new pdh data
	gkrellm_debug(DEBUG_SYSDEP, "Reading cpu data for %d CPUs\n",
			s_cpu_ptr_array->len);

	for (i = 0; i < s_cpu_ptr_array->len; i++)
		{
		cpu = (GK_CPU *)g_ptr_array_index(s_cpu_ptr_array, i);

		if (!get_formatted_counter_value(cpu->total_pdh_counter, "cpu total time", PDH_FMT_LONG, &tot))
			return;

		if (!get_formatted_counter_value(cpu->sys_pdh_counter, "cpu system time", PDH_FMT_LONG, &sys))
			return;

		// user time = (total time - system time)
		cpu->user += (tot.longValue - sys.longValue);
		cpu->sys  += sys.longValue;
		// idle time = 100% - total time - system time
		cpu->idle += (100 - tot.longValue - sys.longValue);

		gkrellm_cpu_assign_data(i, cpu->user, 0 /*nice*/, cpu->sys, cpu->idle);
		}
	}

static void
gkrellm_sys_cpu_add_cb(wchar_t *name, PDH_HCOUNTER *total, PDH_HCOUNTER *sys)
{
	GK_CPU *cpu;

	gkrellm_debug(DEBUG_SYSDEP, "Adding CPU '%ls'\n", name);

	cpu = g_new0(GK_CPU, 1);
	cpu->total_pdh_counter = *total;
	cpu->sys_pdh_counter = *sys;
	g_ptr_array_add(s_cpu_ptr_array, cpu);
}

gboolean
gkrellm_sys_cpu_init(void)
	{

	gkrellm_debug(DEBUG_SYSDEP, "INIT CPU Monitoring\n");

	s_cpu_ptr_array = g_ptr_array_new();

	gkrellm_cpu_nice_time_unsupported();

	add_counter_list(
			  238 // object_index
			, 6 // counter_index1, cpu time
			, 144 // counter_index2, system time
			, gkrellm_sys_cpu_add_cb);

	gkrellm_debug(DEBUG_SYSDEP, "Found %i CPUs for monitoring.\n",
			s_cpu_ptr_array->len);

	gkrellm_cpu_set_number_of_cpus(s_cpu_ptr_array->len);

	return (s_cpu_ptr_array->len == 0 ? FALSE : TRUE);
	}

static void
gkrellm_sys_cpu_cleanup(void)
	{
	guint i;
	if (!s_cpu_ptr_array)
		return;
	gkrellm_debug(DEBUG_SYSDEP, "Freeing counters for %u cpu(s)\n",
		s_cpu_ptr_array->len);
	for (i = 0; i < s_cpu_ptr_array->len; i++)
		g_free(g_ptr_array_index(s_cpu_ptr_array, i));
	g_ptr_array_free(s_cpu_ptr_array, TRUE);
	s_cpu_ptr_array = NULL;
	}

/* ===================================================================== */
/* Net monitor interface */
/* ===================================================================== */

typedef struct _GK_NET
	{
	PDH_HCOUNTER recv_pdh_counter;
	PDH_HCOUNTER send_pdh_counter;
	gchar *name;
	gulong recv;
	gulong send;
	}
	GK_NET;

static GPtrArray *s_net_ptr_array = NULL;

static GK_NET *
gk_net_new()
{
	return g_new0(GK_NET, 1);
}

static void
gk_net_free(GK_NET *net)
{
	g_free(net->name);
	g_free(net);
}

void
gkrellm_sys_net_read_data(void)
{
	gint i;
	GK_NET *net;
	PDH_FMT_COUNTERVALUE recvVal;
	PDH_FMT_COUNTERVALUE sendVal;

	if (pdhQueryHandle == NULL)
		return;
	win32_read_proc_stat();
	gkrellm_debug(DEBUG_SYSDEP, "Reading net data for %d network devices\n",
			s_net_ptr_array->len);

	for (i = 0; i < s_net_ptr_array->len; i++)
		{
		net = (GK_NET *)g_ptr_array_index(s_net_ptr_array, i);

		if (!get_formatted_counter_value(net->recv_pdh_counter, "net recv", PDH_FMT_LONG, &recvVal))
			continue;

		if (!get_formatted_counter_value(net->send_pdh_counter, "net send", PDH_FMT_LONG, &sendVal))
			continue;

		net->recv += recvVal.longValue / _GK.update_HZ;
		net->send += sendVal.longValue / _GK.update_HZ;

		gkrellm_net_assign_data(net->name, net->recv, net->send);
		}
	}

void
gkrellm_sys_net_check_routes(void)
{
	//TODO: Implement if possible, detects enable/disable of network-interfaces
}

gboolean
gkrellm_sys_net_isdn_online(void)
{
	return FALSE; //TODO: Implement if possible
}

static void
gkrellm_sys_net_add_cb(wchar_t *name, PDH_HCOUNTER *recv, PDH_HCOUNTER *send)
{
	GK_NET *net;
	guint i;
	gchar unique = '0';
	GK_NET *other_net;

	net = gk_net_new();
	net->name = clean_dev_name(name);
	for (i = 0; i < s_net_ptr_array->len; i++)
	{
		other_net = (GK_NET *)(g_ptr_array_index(s_net_ptr_array, i));
		while (strncmp(net->name, other_net->name, MAX_DEV_NAME) == 0)
		{
			gkrellm_debug(DEBUG_SYSDEP,
					"net names '%s' and '%s' conflict, renaming new one\n",
					net->name, other_net->name);
			net->name[strlen(net->name) - 1] = unique++;
			break;
		}
	}
	net->recv_pdh_counter = *recv;
	net->send_pdh_counter = *send;

	gkrellm_debug(DEBUG_SYSDEP, "Adding network interface %s\n", net->name);

	// TODO: determine network type
	gkrellm_net_add_timer_type_ppp(net->name);

	g_ptr_array_add(s_net_ptr_array, net);
}

gboolean
gkrellm_sys_net_init(void)
	{
	gkrellm_debug(DEBUG_SYSDEP, "INIT network monitoring\n");

	s_net_ptr_array = g_ptr_array_new();

	add_counter_list(
			  510 // object_index
			, 264 // counter_index1
			, 506 // counter_index2
			, gkrellm_sys_net_add_cb);

	gkrellm_debug(DEBUG_SYSDEP, "Found %i network adapters for monitoring.\n",
		s_net_ptr_array->len);

	return (s_net_ptr_array->len == 0 ? FALSE : TRUE);
	}

static void
gkrellm_sys_net_cleanup(void)
	{
	guint i;
	if (!s_net_ptr_array)
		return;
	gkrellm_debug(DEBUG_SYSDEP, "Freeing counters for %u network adapter(s)\n",
		s_net_ptr_array->len);
	for (i = 0; i < s_net_ptr_array->len; i++)
		gk_net_free((GK_NET *)g_ptr_array_index(s_net_ptr_array, i));
	g_ptr_array_free(s_net_ptr_array, TRUE);
	}

/* ===================================================================== */
/* Disk monitor interface */
/* ===================================================================== */

typedef struct _GK_DISK
	{
	PDH_HCOUNTER read_pdh_counter;
	PDH_HCOUNTER write_pdh_counter;
	gchar *name;
	gulong read;
	gulong write;
	} GK_DISK;

static GPtrArray *s_disk_ptr_array = NULL;

static GK_DISK *
gk_disk_new()
{
	return g_new0(GK_DISK, 1);
}

static void
gk_disk_free(GK_DISK *disk)
{
	g_free(disk->name);
	g_free(disk);
}

gchar *gkrellm_sys_disk_name_from_device(gint device_number, gint unit_number,
			gint *order)
{
	static gchar name[37];
	GK_DISK *disk;

	disk = g_ptr_array_index(s_disk_ptr_array, device_number);
	snprintf(name, sizeof(name), "Disk%s", disk->name);
	*order = device_number;

	return name;
}

gint gkrellm_sys_disk_order_from_name(const gchar *name)
{
	return 0; // Disk by name not implemented in Windows
}

void gkrellm_sys_disk_read_data(void)
	{
	guint i;
	GK_DISK *disk;
	PDH_FMT_COUNTERVALUE readVal;
	PDH_FMT_COUNTERVALUE writeVal;

	if (pdhQueryHandle == NULL)
		return;
	win32_read_proc_stat();
	gkrellm_debug(DEBUG_SYSDEP, "Reading disk data\n");

	for (i = 0; i < s_disk_ptr_array->len; i++)
		{
		disk = g_ptr_array_index(s_disk_ptr_array, i);

		if (!get_formatted_counter_value(disk->read_pdh_counter, "disk read", PDH_FMT_DOUBLE, &readVal))
			continue;

		if (!get_formatted_counter_value(disk->write_pdh_counter, "disk write", PDH_FMT_DOUBLE, &writeVal))
			continue;

		disk->read  += readVal.doubleValue / _GK.update_HZ;
		disk->write += writeVal.doubleValue / _GK.update_HZ;

		gkrellm_disk_assign_data_by_device(i, 0, disk->read, disk->write, FALSE);
		}
	}

static
void gkrellm_sys_disk_add_cb(wchar_t *name, PDH_HCOUNTER *read, PDH_HCOUNTER *write)
{
	GK_DISK *disk;
	GK_DISK *other_disk;
	guint i;
	gchar unique = '0';

	disk = gk_disk_new();
	disk->name = clean_dev_name(name);
	for (i = 0; i < s_disk_ptr_array->len; i++)
	{
		other_disk = (GK_DISK *)(g_ptr_array_index(s_disk_ptr_array, i));
		while (strncmp(disk->name, other_disk->name, MAX_DEV_NAME) == 0)
		{
			gkrellm_debug(DEBUG_SYSDEP,
					"disk names '%s' and '%s' conflict, renaming new one\n",
					disk->name, other_disk->name);
			disk->name[strlen(disk->name) - 1] = unique++;
			break;
		}
	}
	disk->read_pdh_counter = *read;
	disk->write_pdh_counter = *write;

	gkrellm_debug(DEBUG_SYSDEP, "Adding disk %s\n", disk->name);

	g_ptr_array_add(s_disk_ptr_array, disk);
}

gboolean
gkrellm_sys_disk_init(void)
	{
	gkrellm_debug(DEBUG_SYSDEP, "INIT disk monitoring\n");

	s_disk_ptr_array = g_ptr_array_new();

	add_counter_list(
			  234 // object_index
			, 220 // counter_index1
			, 222 // counter_index2
			, gkrellm_sys_disk_add_cb);


	gkrellm_debug(DEBUG_SYSDEP, "Found %i disk(s) for monitoring.\n",
		s_disk_ptr_array->len);

	return (s_disk_ptr_array->len == 0 ? FALSE : TRUE);
	}

static void
gkrellm_sys_disk_cleanup(void)
	{
	guint i;
	if (!s_disk_ptr_array)
		return;
	gkrellm_debug(DEBUG_SYSDEP, "Freeing counters for %u disk(s)\n",
		s_disk_ptr_array->len);
	for (i = 0; i < s_disk_ptr_array->len; i++)
		gk_disk_free(g_ptr_array_index(s_disk_ptr_array, i));
	g_ptr_array_free(s_disk_ptr_array, TRUE);
	}

/* ===================================================================== */
/* Proc monitor interface */
/* ===================================================================== */

// Counters for proc monitor interface
static PDH_HCOUNTER processCounter = NULL;
static PDH_HCOUNTER waitQueueCounter = NULL;
// Library handle for secur32.dll, lib is loaded at runtime
static HINSTANCE hSecur32 = NULL;
// Function pointers to various functions from secur32.dll
static pfLsaEnumerateLogonSessions pfLELS = NULL;
static pfLsaFreeReturnBuffer pfLFRB = NULL;
static pfLsaGetLogonSessionData pfLGLSD = NULL;

#if 0
// We need to subtract 1 on Win2k, the "idle process" seems to be counted
// as waiting on win2k (not on winxp though)
// TODO: check on vista, win2k3, win2k8 etc.
static long proc_load_correction_val = 0;
#endif

void
gkrellm_sys_proc_read_data(void)
	{
	static gulong last_num_processes = 0;
	static gfloat fload = 0;

	PDH_FMT_COUNTERVALUE value;
	LONG num_processes;
	LONG num_forks = 0;
	LONG num_waiting;
	gfloat a;

	if (pdhQueryHandle == NULL)
		return;
	win32_read_proc_stat();
	gkrellm_debug(DEBUG_SYSDEP, "Reading proc data\n");

	if (!get_formatted_counter_value(processCounter, "process count",
			PDH_FMT_LONG, &value))
		return;
	num_processes = value.longValue;
	if ((last_num_processes) > 0 && (last_num_processes < num_processes))
		num_forks = num_processes - last_num_processes;
	last_num_processes = num_processes;

	if (!get_formatted_counter_value(waitQueueCounter, "wait queue size",
			PDH_FMT_LONG, &value))
		return;
#if 0
	num_waiting = (value.longValue + proc_load_correction_val);
#else
	num_waiting = value.longValue;
#endif

	//fload - is the system load average, an exponential moving average over a
	//period of a minute of n_running. It measures how heavily a system is
	//loaded with processes or threads competing for cpu time slices.
	//
	//All the unix OSs have a system call for getting the load average. But if
	//you don't and can get a n_running number, you can calculate fload. An
	//exponential moving average (ema) is done like:
	//
	//    a = 2 / (period + 1)
	//    ema = ema + a * (new_value - ema)
	//
	// See also
	// http://en.wikipedia.org/wiki/Load_(computing)
	a = 2.0 / ((_GK.update_HZ * 60.) + 1.);
	fload += a * (num_waiting - fload);
	if (fload < 0)
		fload = 0;

	gkrellm_debug(DEBUG_SYSDEP, "num_forks %ld; num_waiting %ld; a %f; fload %f\n",
			num_forks, num_waiting, a, fload);

	gkrellm_proc_assign_data(num_processes, 0 /*n_running*/,
			num_forks /*n_forks*/, fload);
	}

void
gkrellm_sys_proc_read_users(void)
	{
	gint i;
	// Number of interactive users
	gint n_users = 0;

#if _WIN32_WINNT >= 0x501 // Windows XP or newer
	BOOL ret;
	WTS_SESSION_INFOW *pSessionList = NULL;
	DWORD sessionListCount = 0;
	WTS_SESSION_INFOW *pSession = NULL;

	gkrellm_debug(DEBUG_SYSDEP, "Enumerating terminal sessions...\n");
	// Returns list of terminal sessions in pSessionInfo[]
	ret = WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionList,
			&sessionListCount);
	if (!ret)
		{
		win32_warning(WTSAPI32,
				GetLastError(),
				"Enumerating terminal sessions failed");
		}
	else if (pSessionList != NULL)
		{
		gkrellm_debug(DEBUG_SYSDEP, "Found %d terminal sessions\n", sessionListCount);
		for (i = 0; i < sessionListCount; i++)
			{
			pSession = &pSessionList[i];
			gkrellm_debug(DEBUG_SYSDEP, "Session %d (%ls) has state %d\n",
			    pSession->SessionId, pSession->pWinStationName, pSession->State);
			if (pSession->State == WTSActive)
				n_users++;
			}
		WTSFreeMemory(pSessionList);
		}
#else // TODO: Remove this code-branch if nobody wants win2k-support anymore 
	// Return value for Lsa functions
	NTSTATUS ntstatus;
	// Arguments for LsaEnumerateLogonSessions()
	ULONG numSessions = 0;
	PLUID pSessions = NULL;
	// Argument for LsaGetLogonSessionData()
	SECURITY_LOGON_SESSION_DATA *pSessionData;
	wchar_t acc_name[256];
	wchar_t acc_dom[256];
	DWORD dwSize;
	SID_NAME_USE sid_type;

	/* Silently fail if secur32.dll is missing functions that we use */
	if ((pfLELS == NULL) || (pfLFRB == NULL) || (pfLGLSD == NULL))
		return;

	gkrellm_debug(DEBUG_SYSDEP, "Getting number of logged in users\n");

	ntstatus = pfLELS(&numSessions, &pSessions);
	if (NT_SUCCESS(ntstatus))
		{
		gkrellm_debug(DEBUG_SYSDEP, "Found %lu user-sessions\n", numSessions);
		for (i = 0; i < (int)numSessions; i++)
			{
			//gkrellm_debug(DEBUG_SYSDEP, "Fetching session-data for session %d\n", i);
			pSessionData = NULL;

			ntstatus = pfLGLSD(&pSessions[i], &pSessionData);
			if (NT_SUCCESS(ntstatus) && (pSessionData != NULL))
				{
				if ((SECURITY_LOGON_TYPE)pSessionData->LogonType == Interactive
					&& (pSessionData->UserName.Buffer != NULL))
					{
					gkrellm_debug(DEBUG_SYSDEP, "Interactive User %d; '%ls'\n",
							i, pSessionData->UserName.Buffer);

					dwSize = 256;
					if (LookupAccountSidW(NULL, pSessionData->Sid,
							acc_name, &dwSize,
							acc_dom, &dwSize,
							&sid_type))
						{
						if (sid_type == 1)
							{
							n_users++;
							}
						else
							{
							gkrellm_debug(DEBUG_SYSDEP,
									"SID type %d is not a normal account\n",
									(int)sid_type);
							}
						}
					else
						{
							win32_warning(NTDLL,
									GetLastError(),
									"Looking up user account id failed");
						}
					}
				}
			else
				{
				win32_warning(NTDLL, ntstatus,
					"Could not get session-data for session %d", i);
				}

			// Free session-data provided by OS, even if function returned
			// an error before
			pfLFRB(pSessionData);
			}
		}
	else
		{
		win32_warning(NTDLL, ntstatus, "Could not enumerate user-sessions\n");
		}

	// Free LUID list provided by OS, even if function returned an error before
	pfLFRB(pSessions);
#endif

	gkrellm_proc_assign_users(n_users);
	}

gboolean
gkrellm_sys_proc_init(void)
	{
	wchar_t system_name[PDH_MAX_COUNTER_NAME];
	wchar_t counter_name[PDH_MAX_COUNTER_NAME];
	wchar_t counter_path[128+128+3];
#if 0
	OSVERSIONINFOEXW vi;
#endif

	gkrellm_debug(DEBUG_SYSDEP, "INIT process monitoring\n");

	if (pdhQueryHandle == NULL)
		return FALSE;

	// Fetch prefix for both counter paths ("System" index is 2)
	if (!lookup_perfname(2, system_name, ARR_SZ(system_name)))
		return FALSE;

	// Add counter for number of processes (index is 248)
	if (!lookup_perfname(248, counter_name, 128))
		return FALSE;
	_snwprintf(counter_path, ARR_SZ(counter_path), L"\\%ls\\%ls",
			system_name, counter_name);
	if (!add_counter(counter_path, &processCounter))
		return FALSE;

	// --- Add counter for waiting queue size (index is 44)
	if (!lookup_perfname(44, counter_name, 128))
		return FALSE;
	_snwprintf(counter_path, ARR_SZ(counter_path), L"\\%ls\\%ls",
			system_name, counter_name);
	if (!add_counter(counter_path, &waitQueueCounter))
		return FALSE;

	// Dynamically load secur32.dll and lookup functions.
	// Needed to determine number of logged in users
	hSecur32 = LoadLibraryW(L"secur32.dll");
	if (hSecur32 != NULL)
		{
		gkrellm_debug(DEBUG_SYSDEP, "Loaded secur32.dll\n");

		pfLELS = (pfLsaEnumerateLogonSessions)GetProcAddress(hSecur32,
			"LsaEnumerateLogonSessions");
		if (pfLELS == NULL)
			{
			g_warning("Could not get address for " \
			"LsaEnumerateLogonSessions() in secur32.dll\n");
			}

		pfLFRB = (pfLsaFreeReturnBuffer)GetProcAddress(hSecur32,
			"LsaFreeReturnBuffer");
		if (pfLFRB == NULL)
			{
			g_warning("Could not get address for " \
				"LsaFreeReturnBuffer() in secur32.dll\n");
			}

		pfLGLSD = (pfLsaGetLogonSessionData)GetProcAddress(hSecur32,
			"LsaGetLogonSessionData");
		if (pfLGLSD == NULL)
			{
			g_warning("Could not get address for " \
				"LsaGetLogonSessionData() in secur32.dll\n");
			}
		}
	else
		{
		win32_warning(NULL, GetLastError(), "Could not load secur32.dll\n");
		}

	// Determine OS for proper load-average computation
	// (wait-queue value on win2k is off by one)
#if 0
	memset(&vi, 0, sizeof(OSVERSIONINFOEXW));
	vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
	if (GetVersionExW((OSVERSIONINFOW *)(&vi)))
		{
		gkrellm_debug(DEBUG_SYSDEP, "major %ld; minor %ld\n",
				vi.dwMajorVersion, vi.dwMinorVersion);
		if ((vi.dwMajorVersion == 5) && (vi.dwMinorVersion == 0))
			proc_load_correction_val = -1;
		}
#endif

	return TRUE;
	}

static void
gkrellm_sys_proc_cleanup(void)
	{
	gkrellm_debug(DEBUG_SYSDEP, "Cleanup process monitoring\n");
	// Unload secur32.dll and invalidate function pointers
	pfLELS = NULL;
	pfLFRB = NULL;
	pfLGLSD = NULL;
	if (hSecur32 != NULL)
		FreeLibrary(hSecur32);
	hSecur32 = NULL;
	}


/* ===================================================================== */
/* Memory/Swap monitor interface */
/* ===================================================================== */

typedef struct _PERFORMANCE_INFORMATION {
	DWORD cb;
	SIZE_T CommitTotal;
	SIZE_T CommitLimit;
	SIZE_T CommitPeak;
	SIZE_T PhysicalTotal;
	SIZE_T PhysicalAvailable;
	SIZE_T SystemCache;
	SIZE_T KernelTotal;
	SIZE_T KernelPaged;
	SIZE_T KernelNonpaged;
	SIZE_T PageSize;
	DWORD HandleCount;
	DWORD ProcessCount;
	DWORD ThreadCount;
} PERFORMANCE_INFORMATION;

typedef BOOL (WINAPI *pfGetPerformanceInfo)(PERFORMANCE_INFORMATION *, DWORD);

static HINSTANCE hPsapi = NULL;
static pfGetPerformanceInfo pGPI = NULL;
static DWORD page_size = 1;
static HINSTANCE hNtdll = NULL;
static pfZwQuerySystemInformation pZwQSI = NULL;

void
gkrellm_sys_mem_read_data(void)
	{
	gkrellm_debug(DEBUG_SYSDEP, "Checking memory utilization\n");

	guint64 total = 0;
	guint64 used  = 0;
	guint64 avail = 0;
	guint64 cache = 0;

	if (pGPI)
		{
		PERFORMANCE_INFORMATION pi;

		// See http://msdn.microsoft.com/en-us/library/ms684824(VS.85).aspx
		// for the confusing description of PERFORMANCE_INFORMATION
		//
		// total   = PhysicalTotal
		// used    = PhysicalTotal - PhysicalAvailable
		// NOTE: (avail value is not exactly correct but we can't know better)
		// avail   = PhysicalAvailable - SystemCache
		// cache   = SystemCache

		if (pGPI(&pi, sizeof(PERFORMANCE_INFORMATION)))
			{
			total = pi.PhysicalTotal * pi.PageSize;
			used  = (pi.PhysicalTotal - pi.PhysicalAvailable) * pi.PageSize;
			avail = (pi.PhysicalAvailable - pi.SystemCache) * pi.PageSize;
			cache = pi.SystemCache * pi.PageSize;
			}
		}
	else
		{
		MEMORYSTATUSEX ms;
		ms.dwLength = sizeof(ms);

		if (GlobalMemoryStatusEx(&ms))
			{
			total = ms.ullTotalPhys;
			used  = ms.ullTotalPhys - ms.ullAvailPhys;
			avail = ms.ullAvailPhys;
			}
		}

	gkrellm_mem_assign_data(total, used, avail, 0, 0, cache);
	}

void
gkrellm_sys_swap_read_data(void)
	{
	guint64 swapTotal = 0;
	guint64 swapUsed  = 0;
	NTSTATUS ntstatus;
	ULONG  szBuf = 3*sizeof(SYSTEM_PAGEFILE_INFORMATION);
	SYSTEM_PAGEFILE_INFORMATION *pInfo;
	LPVOID pBuf = NULL;

	if (pZwQSI == NULL)
		return;

	gkrellm_debug(DEBUG_SYSDEP, "Checking swap utilization\n");

	// it is difficult to determine beforehand which size of the
	// buffer will be enough to retrieve all information, so we
	// start with a minimal buffer and increase its size until we get
	// the information successfully
	do
	{
		pBuf = g_malloc(szBuf);

		ntstatus = pZwQSI(SystemPagefileInformation, pBuf, szBuf, NULL);
		if (ntstatus == STATUS_INFO_LENGTH_MISMATCH)
		{
			// Buffer was too small, double its size and try again
			g_free(pBuf);
			szBuf *= 2;
		}
		else if (!NT_SUCCESS(ntstatus))
		{
			win32_warning(NTDLL, ntstatus, "Could not determine swap usage");
			g_free(pBuf);
			// Some other error occurred, give up
			return;
		}
	}
	while (ntstatus == STATUS_INFO_LENGTH_MISMATCH);

	if (pBuf != NULL)
		{
		// iterate over information for all pagefiles
		pInfo = (SYSTEM_PAGEFILE_INFORMATION *)pBuf;
		for (;;)
			{
			swapTotal += pInfo->CurrentSize * page_size;
			swapUsed  += pInfo->TotalUsed * page_size;
			if (pInfo->NextEntryOffset == 0)
				break; // end of list
			// get pointer to next struct
			pInfo = (SYSTEM_PAGEFILE_INFORMATION *)((BYTE *)pInfo +
				pInfo->NextEntryOffset);
			}
		g_free(pBuf);

		// TODO: calculate swapin/swapout values
		gkrellm_swap_assign_data(swapTotal, swapUsed, 0, 0);
		}
	}

gboolean
gkrellm_sys_mem_init(void)
	{
	SYSTEM_INFO si;

	gkrellm_debug(DEBUG_SYSDEP, "INIT memory monitoring\n");

	GetSystemInfo(&si);
	page_size = si.dwPageSize;

	hPsapi = LoadLibraryW(L"psapi.dll");
	if (hPsapi)
		{
		gkrellm_debug(DEBUG_SYSDEP, "Loaded psapi.dll\n");
		pGPI = (pfGetPerformanceInfo)GetProcAddress(hPsapi, "GetPerformanceInfo");
		if (pGPI == NULL)
			{
			gkrellm_debug(DEBUG_SYSDEP, "No GetPerformanceInfo() in " \
					"psapi.dll, cache-memory will stay at 0!\n");
			}
		}
	else
		{
		win32_warning(NULL, GetLastError(), "Could not load PSAPI.DLL");
		}

	hNtdll = LoadLibraryW(L"ntdll.dll");
	if (hNtdll)
		{
		gkrellm_debug(DEBUG_SYSDEP, "Loaded ntdll.dll\n");
		pZwQSI = (pfZwQuerySystemInformation)GetProcAddress(hNtdll, "ZwQuerySystemInformation");
		if (pZwQSI == NULL)
			{
			gkrellm_debug(DEBUG_SYSDEP, "No ZwQuerySystemInformation() in " \
					"ntdll.dll, pagefile-usage cannot be determined.\n");
			}
		}
	else
		{
		win32_warning(NULL, GetLastError(), "Could not load ntdll.dll");
		}

	return TRUE;
	}

static void
gkrellm_sys_mem_cleanup(void)
	{
	gkrellm_debug(DEBUG_SYSDEP, "Cleanup memory monitoring\n");
	pGPI = NULL;
	if (hPsapi != NULL)
		FreeLibrary(hPsapi);
	hPsapi = NULL;
	
	pZwQSI = NULL;
	if (hNtdll != NULL)
		FreeLibrary(hNtdll);
	hNtdll = NULL;
	}


/* ===================================================================== */
/* Battery monitor interface */
/* ===================================================================== */

void gkrellm_sys_battery_read_data(void)
{
	gboolean available, on_line, charging;
	gint percent, time_left;
	SYSTEM_POWER_STATUS power;

	gkrellm_debug(DEBUG_SYSDEP, "Checking battery state\n");

	GetSystemPowerStatus(&power);
	if (   (power.BatteryFlag & BATTERY_FLAG_NO_BATTERY) == BATTERY_FLAG_NO_BATTERY
	    || (power.BatteryFlag & BATTERY_FLAG_UNKNOWN) == BATTERY_FLAG_UNKNOWN
	   )
		{
		available = FALSE;
		}
	else
		{
		available = TRUE;
		}

	on_line   = ((power.ACLineStatus & AC_LINE_ONLINE) == AC_LINE_ONLINE) ? TRUE : FALSE;
	charging  = ((power.BatteryFlag & BATTERY_FLAG_CHARGING) == BATTERY_FLAG_CHARGING) ? TRUE : FALSE;
	time_left = power.BatteryLifeTime;
	percent   = power.BatteryLifePercent;

	gkrellm_battery_assign_data(0, available, on_line, charging, percent,
		time_left);
}

gboolean gkrellm_sys_battery_init()
	{
	gkrellm_debug(DEBUG_SYSDEP, "INIT battery monitoring\n");
	return TRUE;
	}


/* ===================================================================== */
/* FS monitor interfaces */
/* ===================================================================== */

gboolean gkrellm_sys_fs_fstab_modified(void)
	{
	/* TODO: determine changes in available volumes on win32 using
	FindFirstVolume(), FindNextVolume() etc. */
	return FALSE;
	}

static
void eject_win32_cdrom(gchar *device)
	{
	HANDLE hFile;
	BOOL err;
	char device_path[MAX_PATH];
	DWORD numBytes;

	if (!device || *device == '\0')
		return;

	// FIXME: This assumes device names like "D:"
	snprintf(device_path, MAX_PATH, "\\\\.\\%c:", device[0]);

	hFile = CreateFileA(device_path, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, NULL);
	if (hFile != 0 && hFile != INVALID_HANDLE_VALUE)
		{
		// this should be safe for non-removable drives
		err = DeviceIoControl(hFile, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0,
		                      &numBytes, NULL);
		if (!err)
			{
			err = DeviceIoControl(hFile, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0,
			                      NULL, 0, &numBytes, NULL);
			}
		CloseHandle(hFile);
		}
	}

gboolean gkrellm_sys_fs_init(void)
	{
	gkrellm_debug(DEBUG_SYSDEP, "INIT filesystem monitoring\n");

	gkrellm_fs_mounting_unsupported();
	gkrellm_fs_setup_eject(NULL, NULL, eject_win32_cdrom, NULL);
	return TRUE;
	}

void gkrellm_sys_fs_get_fsusage(gpointer fs, gchar *dir)
	{
	BOOL err = 0;
	ULARGE_INTEGER availToCaller;
	ULARGE_INTEGER totalBytes;
	ULARGE_INTEGER freeBytes;
	gunichar2 *w_dir = NULL;

	if (!dir || *dir == '\0')
		return;
	gkrellm_debug(DEBUG_SYSDEP, "Checking fs usage for %s\n", dir);

	w_dir = g_utf8_to_utf16(dir, -1, NULL, NULL, NULL);
	if (!w_dir)
		return;

	err = GetDiskFreeSpaceExW(w_dir, &availToCaller, &totalBytes, &freeBytes);
	if (err != 0)
		{
		// fs, blocks, avail, free, size
		gkrellm_fs_assign_fsusage_data(fs
			, totalBytes.QuadPart / (ULONGLONG)1024
			, availToCaller.QuadPart / (ULONGLONG)1024 /* free to caller */
			, freeBytes.QuadPart / (ULONGLONG)1024 /* free */
			, 1024 /* block size */
			);
		}
	else
		{
		/* This may happen on cd/dvd drives, ignore error silently */
		}
	g_free(w_dir);
	}


void gkrellm_sys_fs_get_mounts_list(void)
	{
	wchar_t drive_list[4*26];
	wchar_t *drive;
	gchar *drive_utf8;
	DWORD ret;
	DWORD sz;
	UINT drive_type;

	gkrellm_debug(DEBUG_SYSDEP, "Getting list of mounted drives\n");

	drive_list[0] = '\0';
	sz = ARR_SZ(drive_list) - sizeof(drive_list[0]);
	ret = GetLogicalDriveStringsW(sz, drive_list);
	if (ret == 0 || ret > sz)
	{
		win32_warning(NULL, GetLastError(), "Failed enumerating mounted drives");
		return;
	}

	for (drive = drive_list; (*drive) != '\0'; drive += wcslen(drive) + 1)
		{
		drive_type = GetDriveTypeW(drive);

		if (   (drive_type == DRIVE_REMOVABLE)
			&& (!wcsncmp(drive, L"A:\\", 3) || !wcsncmp(drive, L"B:\\", 3)))
			continue;

		gkrellm_debug(DEBUG_SYSDEP, "Found mounted drive '%ls' of type %u\n",
				drive, drive_type);
		drive_utf8 = g_utf16_to_utf8(drive, -1, NULL, NULL, NULL);
		gkrellm_fs_add_to_mounts_list(drive_utf8, drive_utf8,
				(drive_type == DRIVE_REMOTE ? "smbfs" : ""));
		g_free(drive_utf8);
		}
	}

void gkrellm_sys_fs_get_fstab_list(void)
	{
	wchar_t drive_list[4*26];
	wchar_t *drive;
	gchar *drive_utf8;
	DWORD ret;
	DWORD sz;
	UINT drive_type;

	gkrellm_debug(DEBUG_SYSDEP, "Getting list of drives in fstab\n");

	drive_list[0] = '\0';
	sz = ARR_SZ(drive_list) - sizeof(drive_list[0]);
	ret = GetLogicalDriveStringsW(sz, drive_list);
	if (ret == 0 || ret > sz)
	{
		win32_warning(NULL, GetLastError(), "Failed enumerating fstab drives");
		return;
	}

	for (drive = drive_list; (*drive) != '\0'; drive += wcslen(drive) + 1)
		{
		drive_type = GetDriveTypeW(drive);

		if (   (drive_type == DRIVE_REMOVABLE)
			&& (!wcsncmp(drive, L"A:\\", 3) || !wcsncmp(drive, L"B:\\", 3)))
			continue;

		gkrellm_debug(DEBUG_SYSDEP, "Found fstab drive '%ls' of type %u\n",
				drive, drive_type);

		drive_utf8 = g_utf16_to_utf8(drive, -1, NULL, NULL, NULL);
		gkrellm_fs_add_to_fstab_list(drive_utf8, drive_utf8,
				(drive_type == DRIVE_REMOTE ? "smbfs" : ""), "");
		g_free(drive_utf8);
		}
	}


/* ===================================================================== */
/* INET monitor interfaces */
/* ===================================================================== */

// Library handle for Iphlpapi.dll, lib is loaded at runtime
static HINSTANCE hIphlpapi = NULL;
// Function pointer to GetTcp6Table() which only exists on Vista and newer
static GetTcp6TableFunc pfGetTcp6Table = NULL;

gboolean gkrellm_sys_inet_init(void)
	{
	gkrellm_debug(DEBUG_SYSDEP, "INIT inet port monitoring\n");
	hIphlpapi = LoadLibraryW(L"Iphlpapi.dll");
	if (hIphlpapi != NULL)
		{
		gkrellm_debug(DEBUG_SYSDEP, "Loaded Iphlpapi.dll\n");
		pfGetTcp6Table = (GetTcp6TableFunc) GetProcAddress(hIphlpapi,
			"GetTcp6Table");
		if (pfGetTcp6Table == NULL)
			{
			g_warning("Could not get address for " \
				"GetTcp6Table() in Iphlpapi.dll " \
				"(this is ok on windows versions older than vista)\n");
			}
		}
	else
		{
		win32_warning(NULL, GetLastError(), "Could not load Iphlpapi.dll\n");
		}
	return TRUE;
	}
static void
gkrellm_sys_inet_cleanup(void)
	{
	gkrellm_debug(DEBUG_SYSDEP, "Cleanup inet port monitoring\n");
	pfGetTcp6Table = NULL;
	if (hIphlpapi != NULL)
		FreeLibrary(hIphlpapi);
	hIphlpapi = NULL;
	}

static void win32_read_tcp_data(void)
	{
	PMIB_TCPTABLE pTcpTable = NULL;
	DWORD dwTableSize = 0;
	DWORD dwStatus;
	MIB_TCPROW *tcprow;
	ActiveTCP tcp;
	DWORD i;

	gkrellm_debug(DEBUG_SYSDEP, "Fetching list of IPv4 TCP connections\n");

	// Make an initial call to GetTcpTable to
	// get the necessary size into the dwSize variable
	dwStatus = GetTcpTable(NULL, &dwTableSize, FALSE);

	if ((dwStatus == ERROR_INSUFFICIENT_BUFFER) && (dwTableSize > 0))
		{
		pTcpTable = (MIB_TCPTABLE *)g_malloc(dwTableSize);

		// Make a second call to GetTcpTable to get
		// the actual data we require
		dwStatus = GetTcpTable(pTcpTable, &dwTableSize, FALSE);

		if (dwStatus == NO_ERROR)
			{
			for (i = 0; i < pTcpTable->dwNumEntries; i++)
				{
				tcprow = &pTcpTable->table[i];

				// Skip connections that are not fully established
				if (tcprow->dwState != MIB_TCP_STATE_ESTAB)
					continue;

				tcp.family             = AF_INET;
				tcp.local_port         = htons(tcprow->dwLocalPort);
				tcp.remote_addr.s_addr = tcprow->dwRemoteAddr;
#if defined(INET6)
				memset(&tcp.remote_addr6, 0, sizeof(struct in6_addr));
#endif
				tcp.remote_port        = htons(tcprow->dwRemotePort);
				tcp.is_udp             = FALSE;

				gkrellm_inet_log_tcp_port_data(&tcp);
				}
			}
			else
			{
				win32_warning(NULL, dwStatus,
					"Could not fetch list of IPv4 TCP connections");
			}

		g_free(pTcpTable);
		}
		else
		{
			win32_warning(NULL, dwStatus,
					"Could not fetch list of IPv4 TCP connections");
		}
	}

#if defined(INET6)
static void win32_read_tcp6_data(void)
	{
	PMIB_TCP6TABLE pTcpTable = NULL;
	DWORD dwTableSize = 0;
	DWORD dwStatus;
	MIB_TCP6ROW *tcprow;
	ActiveTCP tcp;
	DWORD i;

	if (pfGetTcp6Table == NULL)
		return; // missing GetTcp6Table() on this machine

	gkrellm_debug(DEBUG_SYSDEP, "Fetching list of IPv6 TCP connections\n");

	// Make an initial call to GetTcpTable to
	// get the necessary size into the dwSize variable
	dwStatus = pfGetTcp6Table(NULL, &dwTableSize, FALSE);

	if ((dwStatus == ERROR_INSUFFICIENT_BUFFER) && (dwTableSize > 0))
		{
		pTcpTable = (MIB_TCP6TABLE *)g_malloc(dwTableSize);

		// Make a second call to GetTcpTable to get
		// the actual data we require
		dwStatus = pfGetTcp6Table(pTcpTable, &dwTableSize, FALSE);

		if (dwStatus == NO_ERROR)
			{
			for (i = 0; i < pTcpTable->dwNumEntries; i++)
				{
				tcprow = &pTcpTable->table[i];

				// Skip connections that are not fully established
				if (tcprow->dwState != MIB_TCP_STATE_ESTAB)
					continue;

				tcp.family             = AF_INET6;
				tcp.local_port         = htons(tcprow->dwLocalPort);
				tcp.remote_addr.s_addr = 0;
				tcp.remote_addr6       = tcprow->RemoteAddr;
				tcp.remote_port        = htons(tcprow->dwRemotePort);
				tcp.is_udp             = FALSE;

				gkrellm_inet_log_tcp_port_data(&tcp);
				}
			}
			else
			{
				win32_warning(NULL, dwStatus,
					"Could not fetch list of IPv6 TCP connections");
			}

		g_free(pTcpTable);
		}
		else
		{
			win32_warning(NULL, dwStatus,
					"Could not fetch list of IPv6 TCP connections");
		}
		}
#endif

void gkrellm_sys_inet_read_tcp_data(void)
	{
	win32_read_tcp_data();
#if defined(INET6)
	win32_read_tcp6_data();
#endif
	}


/* ===================================================================== */
/* Uptime monitor interface */
/* ===================================================================== */

static PDH_HCOUNTER uptimeCounter = NULL;

time_t gkrellm_sys_uptime_read_uptime(void)
{
	PDH_FMT_COUNTERVALUE val;

	if (pdhQueryHandle == NULL)
		return (time_t)0;
	win32_read_proc_stat();
	gkrellm_debug(DEBUG_SYSDEP, "Reading system uptime\n");

	if (!get_formatted_counter_value(uptimeCounter, "uptime", PDH_FMT_LONG, &val))
		return (time_t)0;
	return (time_t)val.longValue;
	}

gboolean gkrellm_sys_uptime_init(void)
	{
	wchar_t system_name[PDH_MAX_COUNTER_NAME];
	wchar_t uptime_name[PDH_MAX_COUNTER_NAME];
	wchar_t counter_path[PDH_MAX_COUNTER_PATH];

	gkrellm_debug(DEBUG_SYSDEP, "INIT uptime monitoring\n");

	if (pdhQueryHandle == NULL)
		return FALSE;

	// Fetch prefix for counter ("System" index is 2)
	if (!lookup_perfname(2, system_name, ARR_SZ(system_name)))
		return FALSE;

	// Fetch name for uptime ("Uptime" index is 674)
	if (!lookup_perfname(674, uptime_name, ARR_SZ(uptime_name)))
		return FALSE;

	_snwprintf(counter_path, ARR_SZ(counter_path), L"\\%ls\\%ls",
			system_name, uptime_name);

	if (!add_counter(counter_path, &uptimeCounter))
		return FALSE;

	return TRUE;
	}


/* ===================================================================== */
/* System name interface */
/* ===================================================================== */


typedef void (WINAPI *PGetNativeSystemInfo)(SYSTEM_INFO *);

gchar *gkrellm_sys_get_system_name(void)
	{
	static gboolean	 have_sys_name;
	static gchar sysname[32];
	OSVERSIONINFOEXW vi;
	SYSTEM_INFO si;
	PGetNativeSystemInfo pGNSI;

	if (have_sys_name)
		return sysname;

	gkrellm_debug(DEBUG_SYSDEP, "Retrieving system name\n");

	// Default value for sysname
	g_strlcpy(sysname, "Unknown", sizeof(sysname));

	// Query version info
	memset(&vi, 0, sizeof(OSVERSIONINFOEXW));
	vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
	if (!GetVersionExW((OSVERSIONINFOW *)(&vi)))
		return sysname;

	// We actually only support decoding NT-based version info
	if (vi.dwPlatformId != VER_PLATFORM_WIN32_NT)
		return sysname;

	// Try to call native version first, as this allows detecting
	// 64bit hosts from within a 32bit process.
	pGNSI = (PGetNativeSystemInfo)GetProcAddress(
			GetModuleHandleW(L"kernel32.dll"), "GetNativeSystemInfo");

	if (pGNSI != NULL)
		pGNSI(&si);
	else
		GetSystemInfo(&si);

	if (vi.dwMajorVersion == 6 && vi.dwMinorVersion == 0)
		{
		// Windows 6.0 aka Vista or Server 2008

		if (vi.wProductType == VER_NT_WORKSTATION)
			g_strlcpy(sysname, "Windows Vista", sizeof(sysname));
		else
			g_strlcpy(sysname, "Windows Server 2008", sizeof(sysname));
		}
	else if (vi.dwMajorVersion == 5)
		{
		// Windows 5.x aka 2000, XP, Server 2003

		if (vi.dwMinorVersion == 0)
			{
	         if (vi.wProductType == VER_NT_WORKSTATION)
	        	 g_strlcpy(sysname, "Windows 2000 Professional", sizeof(sysname));
	         else
	        	 g_strlcpy(sysname, "Windows 2000 Server", sizeof(sysname));
			}
		else if (vi.dwMinorVersion == 1)
			{
			if (vi.wSuiteMask & VER_SUITE_PERSONAL)
				g_strlcpy(sysname, "Windows XP Home Edition", sizeof(sysname));
			else
				g_strlcpy(sysname, "Windows XP Professional", sizeof(sysname));
			}
		else if (vi.dwMinorVersion == 2)
			{
			if (GetSystemMetrics(SM_SERVERR2))
				g_strlcpy(sysname, "Windows Server 2003 R2", sizeof(sysname));
			else if (vi.wSuiteMask == VER_SUITE_STORAGE_SERVER)
				g_strlcpy(sysname, "Windows Storage Server 2003", sizeof(sysname));
			else if (   vi.wProductType == VER_NT_WORKSTATION
					  && si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
				g_strlcpy(sysname, "Windows XP Professional x64", sizeof(sysname));
			else
				g_strlcpy(sysname, "Windows Server 2003", sizeof(sysname));
			}
		}

	return sysname;
	}


/* ===================================================================== */
/* Misc functions */
/* ===================================================================== */

/*
* Copyright (c) 1983, 1990, 1993
* The Regents of the University of California. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* 3. All advertising materials mentioning features or use of this software
* must display the following acknowledgement:
* This product includes software developed by the University of
* California, Berkeley and its contributors.
* 4. Neither the name of the University nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

/*
* Portions Copyright (c) 1993 by Digital Equipment Corporation.
*
* Permission to use, copy, modify, and distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies, and that
* the name of Digital Equipment Corporation not be used in advertising or
* publicity pertaining to distribution of the document or software without
* specific, written prior permission.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
* WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL DIGITAL EQUIPMENT
* CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
* DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
* PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
* ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
* SOFTWARE.
*/

/*
* Portions Copyright (c) 1996-1999 by Internet Software Consortium.
*
* Permission to use, copy, modify, and distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
* ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
* CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
* DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
* PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
* ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
* SOFTWARE.
*/

/* BeOS doesn't yet have it's own inet_aton and Bind won't be ported
* until R5, so this is from a Bind 8 distribution. It's currently untested.
*/

int inet_aton(const char *cp, struct in_addr *addr) {
u_long val;
int base, n;
char c;
short parts[4];
short *pp = parts;
int digit;

c = *cp;
for (;;) {
/*
* Collect number up to ``.''.
* Values are specified as for C:
* 0x=hex, 0=octal, isdigit=decimal.
*/
if (!isdigit(c))
return (0);
val = 0; base = 10; digit = 0;
if (c == '0') {
c = *++cp;
if (c == 'x' || c == 'X')
base = 16, c = *++cp;
else {
base = 8;
digit = 1 ;
}
}
for (;;) {
if (isascii(c) && isdigit(c)) {
if (base == 8 && (c == '8' || c == '9'))
return (0);
val = (val * base) + (c - '0');
c = *++cp;
digit = 1;
} else if (base == 16 && isascii(c) && isxdigit(c)) {
val = (val << 4) |
(c + 10 - (islower(c) ? 'a' : 'A'));
c = *++cp;
digit = 1;
} else
break;
}
if (c == '.') {
/*
* Internet format:
* a.b.c.d
* a.b.c (with c treated as 16 bits)
* a.b (with b treated as 24 bits)
*/
if (pp >= parts + 3 || val > 0xff)
return (0);
*pp++ = val;
c = *++cp;
} else
break;
}
/*
* Check for trailing characters.
*/
if (c != '\0' && (!isascii(c) || !isspace(c)))
return (0);
/*
* Did we get a valid digit?
*/
if (!digit)
return (0);
/*
* Concoct the address according to
* the number of parts specified.
*/
n = pp - parts + 1;
switch (n) {
case 1: /* a -- 32 bits */
break;

case 2: /* a.b -- 8.24 bits */
if (val > 0xffffff)
return (0);
val |= parts[0] << 24;
break;

case 3: /* a.b.c -- 8.8.16 bits */
if (val > 0xffff)
return (0);
val |= (parts[0] << 24) | (parts[1] << 16);
break;

case 4: /* a.b.c.d -- 8.8.8.8 bits */
if (val > 0xff)
return (0);
val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
break;
}
if (addr != NULL)
addr->s_addr = htonl(val);
return (1);
}
