 /* GKrellM
|  Copyright (C) 1999-2007 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|  Latest versions might be found at:  http://gkrellm.net
|
|  win32.c code is Copyright (C) Bill Nalen bill@nalens.com
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

#if defined(WIN32_CLIENT)
#include "../gkrellm.h"
#include "../gkrellm-sysdeps.h"
#include "../gkrellm-private.h"
#include "../win32-plugin.h"
	#include <gdk/gdkwin32.h>
#else
	#include "../../server/win32-plugin.h"
#endif

#include "../inet.h"

#include <limits.h>
#include <errno.h>
#include <largeint.h>
#include <winioctl.h>
#include <tchar.h>
#include <iphlpapi.h>

#include <pdh.h>
#include <pdhmsg.h>
#include <lmcons.h>
#include <lmerr.h>
#include <lmwksta.h>
#include <lmapibuf.h>

#include <ntdef.h>

#if defined(UNICODE) ^ defined(_UNICODE)
#error Inconsistent UNICODE and _UNICODE definition
#endif

#if !defined(ARRAYSIZE)
#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

// ***************************************************************************
// Needed to determine pagefile usage
// definitions were taken from MinGW include/ddk/ntapi.h because you cannot
// mix ddk includes with normal windows includes.
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#define SystemPagefileInformation 18

NTSTATUS
NTAPI
ZwQuerySystemInformation(
  /*IN*/ UINT SystemInformationClass,
  /*IN OUT*/ PVOID SystemInformation,
  /*IN*/ ULONG SystemInformationLength,
  /*OUT*/ PULONG ReturnLength /*OPTIONAL*/);

typedef struct _SYSTEM_PAGEFILE_INFORMATION {
	ULONG  NextEntryOffset;
	ULONG  CurrentSize;
	ULONG  TotalUsed;
	ULONG  PeakUsed;
	UNICODE_STRING  FileName;
} SYSTEM_PAGEFILE_INFORMATION, *PSYSTEM_PAGEFILE_INFORMATION;

// ***************************************************************************

#define MAX_NET_NAME 6
#define MAX_NET_ADAPTERS 10
#define MAX_DISK_NAME 6
#define MAX_DISKS 10
#define MAX_CPU 6

#define PerfKeysSize 12

typedef enum PerfKey_T
{
	CpuStart     = 0, 
	CpuTime      = 1,
	CpuSysTime   = 2,
	NumProcesses = 3,
	NumThreads   = 4,
	Uptime       = 5,
	NetDevStart  = 6,
	NetDevRecv   = 7,
	NetDevSend   = 8,
	DiskStart    = 9,
	DiskRead     = 10,
	DiskWrite    = 11
} PerfKey;

//******************************************************************

static gint		numCPUs;
//static gulong	swapin, swapout;
static char netName[MAX_NET_ADAPTERS + 1][MAX_NET_NAME + 1];
static char diskName[MAX_DISKS + 1][MAX_DISK_NAME + 1];
static gint numAdapters = 0;
static int rx[MAX_NET_ADAPTERS + 1];
static int tx[MAX_NET_ADAPTERS + 1];
static OSVERSIONINFO info;
static gchar* sname;
static gchar* hostname;

/// List of perflib counter strings, they are i18ned inside windows
/// so we have to fetch them by index at startup (readPerfKeys())
static TCHAR* perfKeyList[PerfKeysSize];

static HQUERY   pdhQueryHandle = 0;
static HCOUNTER cpuUserCounter[MAX_CPU + 1];
static HCOUNTER cpuSysCounter[MAX_CPU + 1];
static HCOUNTER processCounter;
static HCOUNTER threadCounter;
static HCOUNTER uptimeCounter;
static HCOUNTER diskReadCounter[MAX_DISKS + 1];
static HCOUNTER diskWriteCounter[MAX_DISKS + 1];
static HCOUNTER netRecCounter[MAX_NET_ADAPTERS + 1];
static HCOUNTER netSendCounter[MAX_NET_ADAPTERS + 1];
static PDH_STATUS status;



/* TODO: group static globals in structs
typedef struct _GK_DISC
	{
	gchar    name[MAX_DISK_NAME + 1];
	gulong   read;
	gulong   written;
	HCOUNTER read_counter;
	HCOUNTER diskWriteCounter;
	} GK_DISC;

static GK_DISC s_disk[MAX_DISKS + 1];
static guint   s_disk_cnt = 0;


typedef struct _GK_NET
	{
	gchar    name[MAX_NET_NAME + 1];
	gulong   rx, tx;
	HCOUNTER recv_counter, send_counter;
	} GK_NET;

static GK_NET s_net[MAX_NET_ADAPTERS + 1];
static guint  s_net_cnt = 0;
*/



// *****************************************************************

// local function protos
static void initPerfKeyList(void);

static void placePerfKeysFromReg(const PerfKey key, unsigned int index1,
	unsigned int index2);

static void placePerfKeyFromReg(const PerfKey key, unsigned int index,
	const TCHAR* prefix, const TCHAR* suffix);

static void placePerfKey(const PerfKey key, const TCHAR* value);


//***************************************************************************

void gkrellm_sys_main_init(void)
	{
	WSADATA wsdata;
	int err;

	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("Starting Winsock\n"));
	err = WSAStartup(MAKEWORD(1,1), &wsdata);
	if (err != 0)
		{ 
		_tprintf(_T("Starting Winsock failed with error code %i\n"), err);
		return;
		}

	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("Opening pdh-query\n"));
	status = PdhOpenQuery(NULL, 0, &pdhQueryHandle);
	if (status != ERROR_SUCCESS || pdhQueryHandle == 0)
		{
		if (_GK.debug_level & DEBUG_SYSDEP)
			_tprintf(_T("Opening pdh-query failed with error code %lu\n"), status);
		pdhQueryHandle = 0;
		}
	// get perflib localized key names
	initPerfKeyList();

	// do this once
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&info);

	// we don't have local mail on Windows (yet?)
	gkrellm_mail_local_unsupported();

	// initialize call back structure for plugins
	win32_init_callbacks();
	}


void gkrellm_sys_main_cleanup(void)
{
    int i;
#if defined(WIN32_CLIENT)
    NOTIFYICONDATA nid;
    // remove system tray icon
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = GDK_WINDOW_HWND(gkrellm_get_top_window()->window);
    nid.uID = 1;
    Shell_NotifyIcon(NIM_DELETE, &nid);
#endif // WIN32_CLIENT

    if (_GK.debug_level & DEBUG_SYSDEP)
        _tprintf(_T("Waiting for mail checking thread to end.\n"));

    while (gkrellm_mail_get_active_thread() != NULL)
    {
        // wait here till it finishes
        // in case we are trying to get mail info
        Sleep(500);
    }

    // Close PDH query-handle
    if (_GK.debug_level & DEBUG_SYSDEP)
        _tprintf(_T("Closing Pdh\n"));
    PdhCloseQuery(pdhQueryHandle);

    // free up these strings
    for (i = 0; i < PerfKeysSize; i++)
        free(perfKeyList[i]);

    g_free(sname);
    g_free(hostname);

    // stop winsock
    if (_GK.debug_level & DEBUG_SYSDEP)
        _tprintf(_T("Closing Winsock\n"));
    WSACleanup();
}

// only need to collect pdhQueryHandle data once for all those monitors that use it
static void win32_read_proc_stat(void)
	{
	static gint	data_read_tick	= -1;
	if (data_read_tick == gkrellm_get_timer_ticks())	/* One read per tick */
		return;
	data_read_tick = gkrellm_get_timer_ticks();
	if (pdhQueryHandle == 0)
		return;

	if (_GK.debug_level & DEBUG_SYSDEP)
		{
		_tprintf(_T("Collecting PDH query data\n"));
		}

	status = PdhCollectQueryData(pdhQueryHandle);
	if (status != ERROR_SUCCESS)
		{
		_tprintf(_T("Collecting PDH query data failed with status %lu\n"), status);
		}
	}


/* ===================================================================== */
/* Sensor interface */
/* ===================================================================== */

// interface to work with shared memory for MBM5

// ---------------------------------------------------------------------------
// --------------------------------------- Copyright 2001 A@majland.org ------
// --------------------------------------- Alteration for use in Visual C ----
// --------------------------------------- By Chris Zahrt techn0@iastate.edu -
// ---------------------------------------------------------------------------
//
//  Version     : 0.1
//  Date        : 02-27-2002
//
//  MBM         : version 5.1
//
//  Author      : Chris Zahrt techn0@iastate.edu (visual c alterations)
//                http://techn0.dhs.org/programming/vcmbmsm.html
//                Anders@Majland.org (author of original c code)
//                http://www.majland.org/sw/mbmcaf
//
//  Licence     : Cardware. (Send me a note/email if you find it usefull.)
//                Basically you may use it as you see fit as long as the origin
//                of the code remains clear
//
//  History     :
//		  0.1 02-27-2002 conversion of 0.3 borland to this version

// Update for MBM 5.1.9 by Bill Nalen bill@nalens.com

// ---------------------------------------------------------------------------

#define NrTemperature 32
#define NrVoltage 16
#define NrFan 16
#define NrCPU 4

static double temperatures[NrTemperature];
static int tempCount;
static double voltages[NrVoltage];
static int voltCount;
static double fans[NrFan];
static int fanCount;

//    enum Bus
#define BusType     char
#define ISA         0
#define SMBus       1
#define VIA686Bus   2
#define DirectIO    3

//    enum SMB
#define SMBType         char
#define smtSMBIntel     0
#define smtSMBAMD       1
#define smtSMBALi       2
#define smtSMBNForce    3
#define smtSMBSIS       4

// enum Sensor Types
#define SensorType      char
#define stUnknown       0
#define stTemperature   1
#define stVoltage       2
#define stFan           3
#define stMhz           4
#define stPercentage    5    

typedef struct {
    SensorType  iType;          // type of sensor
    int         Count;          // number of sensor for that type
} SharedIndex;

typedef struct {
    SensorType ssType;          // type of sensor
    unsigned char ssName[12];   // name of sensor
    char sspadding1[3];         // padding of 3 byte
    double ssCurrent;           // current value
    double ssLow;               // lowest readout
    double ssHigh;              // highest readout
    long ssCount;               // total number of readout
    char sspadding2[4];         // padding of 4 byte
    long double ssTotal;        // total amout of all readouts
    char sspadding3[6];         // padding of 6 byte
    double ssAlarm1;            // temp & fan: high alarm; voltage: % off;
    double ssAlarm2;            // temp: low alarm
} SharedSensor;

typedef struct {
    short siSMB_Base;            // SMBus base address
    BusType siSMB_Type;         // SMBus/Isa bus used to access chip
    SMBType siSMB_Code;         // SMBus sub type, Intel, AMD or ALi
    char siSMB_Addr;            // Address of sensor chip on SMBus
    unsigned char siSMB_Name[41];        // Nice name for SMBus
    short siISA_Base;            // ISA base address of sensor chip on ISA
    int siChipType;             // Chip nr, connects with Chipinfo.ini
    char siVoltageSubType;      // Subvoltage option selected
} SharedInfo;

typedef struct {
    double sdVersion;           // version number (example: 51090)
    SharedIndex sdIndex[10];     // Sensor index
    SharedSensor sdSensor[100];  // sensor info
    SharedInfo sdInfo;          // misc. info
    unsigned char sdStart[41];           // start time
    unsigned char sdCurrent[41];         // current time
    unsigned char sdPath[256];           // MBM path
} SharedData;


static gboolean ReadMBMSharedData(void);
static gboolean ReadSFSharedData(void);


static gboolean ReadSharedData(void)
	{
	static gint      sens_data_read_tick = -1;
	static gboolean  sens_data_valid = FALSE;

	if (sens_data_read_tick == gkrellm_get_timer_ticks())	/* One read per tick */
		return sens_data_valid;
	sens_data_read_tick = gkrellm_get_timer_ticks();

	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("Reading MBM or SpeedFan data\n"));

	// Try getting data from MBM
	sens_data_valid = ReadMBMSharedData();

	// Try SpeedFan in case MBM is absent
	if (!sens_data_valid)
		sens_data_valid = ReadSFSharedData();

	return sens_data_valid;
	} // ReadSharedData()


static gboolean ReadMBMSharedData(void)
{
	SharedData *ptr;
	SharedSensor *sens;
	HANDLE hSData;
	int i, j;
	int totalCount;

	hSData=OpenFileMapping(FILE_MAP_READ, FALSE, _T("$M$B$M$5$S$D$"));
	if (hSData == 0) 
		return FALSE;

	ptr = (SharedData *)MapViewOfFile(hSData, FILE_MAP_READ, 0, 0, 0);
	if (ptr == 0)
	{
		CloseHandle(hSData);
		return FALSE;
	}

	totalCount = 0;
	for (i = 0; i < 5; i++)
		{
		totalCount += ptr->sdIndex[i].Count;
		}

	tempCount = 0;
	voltCount = 0;
	fanCount = 0;
	for (j = 0; j < totalCount; j++)
		{
		sens = &(ptr->sdSensor[j]);
		switch (sens->ssType)
			{
			case stTemperature:
				temperatures[tempCount] = sens->ssCurrent;
				++tempCount;
				break;
			case stVoltage:
				voltages[voltCount] = sens->ssCurrent;
				++voltCount;
				break;
			case stFan:
				fans[fanCount] = sens->ssCurrent;
				++fanCount;
				break;
			default:
				break;
			}
		}
	UnmapViewOfFile(ptr);
	CloseHandle(hSData);
	return TRUE;
	}






/* ======================================================================== */
// SpeedFan

// Strucure of the shared block
#pragma pack(push, 1)
typedef struct
{
	unsigned short int version;
	unsigned short int flags;
	signed int         MemSize;
	HANDLE             handle;
	unsigned short int NumTemps;
	unsigned short int NumFans;
	unsigned short int NumVolts;
	signed int         temps[32];
	signed int         fans[32];
	signed int         volts[32];
} SFSharedMemory;
#pragma pack(pop)


static gboolean ReadSFSharedData(void)
	{
	SFSharedMemory *ptr;
	HANDLE hSData;
	int i;

	hSData = OpenFileMapping(FILE_MAP_READ, FALSE, TEXT("SFSharedMemory_ALM"));
	if (hSData == 0)
		return FALSE;

	ptr = (SFSharedMemory *)MapViewOfFile(hSData, FILE_MAP_READ, 0, 0, 0);
	if (ptr == 0)
	{
		CloseHandle(hSData);
		return FALSE;
	}

	tempCount = min(NrTemperature, ptr->NumTemps);
	for (i = 0; i < tempCount; i++)
		temperatures[i] = ptr->temps[i] / 100.0;

	voltCount = min(NrVoltage, ptr->NumVolts);
	for (i = 0; i < voltCount; i++)
		voltages[i] = ptr->volts[i] / 100.0;

	fanCount = min(NrFan, ptr->NumFans);
	for (i = 0; i < fanCount; i++)
		fans[i] = ptr->fans[i];

	UnmapViewOfFile(ptr);
	CloseHandle(hSData);
	return TRUE;
	}


/* ======================================================================== */


gboolean gkrellm_sys_sensors_get_voltage(gchar *device_name, gint id,
		gint iodev, gint inter, gfloat *volt)
{
	*volt = 0;
	if (iodev < 0 || iodev >= NrVoltage)
		return FALSE;
	if (ReadSharedData() == FALSE)
		return FALSE;
	*volt = voltages[iodev];
	return TRUE;
}

gboolean gkrellm_sys_sensors_get_fan(gchar *device_name, gint id,
		gint iodev, gint inter, gfloat *fan)
	{
	*fan = 0;
	if (iodev >= NrFan || iodev < 0)
		return FALSE;
	if (ReadSharedData() == FALSE)
		return FALSE;
	*fan = fans[iodev];    
	return TRUE;
	}

gboolean gkrellm_sys_sensors_get_temperature(gchar *device_name, gint id,
		gint iodev, gint inter, gfloat *temp)
	{
	*temp = 0;
	if (iodev >= NrTemperature || iodev < 0)
		return FALSE;
	if (ReadSharedData() == FALSE)
		return FALSE;
	*temp = temperatures[iodev];
	return TRUE;
	}

gboolean gkrellm_sys_sensors_init(void)
	{
	char buf[25];
	int i;

	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("INIT sensors\n"));

	//TODO: determine number of sensors at startup? This could cause
	//      user confusion in case mbm/speedfan is started after gkrellm
	tempCount = 0;
	voltCount = 0;
	fanCount = 0;

	for (i = 0; i < NrTemperature; i++)
		{
		//TODO: i18n?
		snprintf(buf, sizeof(buf), "Temp %i", i);
		gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE, NULL, buf, tempCount,
		                           tempCount, 0, 1, 0, NULL, buf);
		++tempCount;
		}

	for (i = 0; i < NrVoltage; i++)
		{
		snprintf(buf, sizeof(buf), "Volt %i", i);
		gkrellm_sensors_add_sensor(SENSOR_VOLTAGE, NULL, buf, voltCount,
		                           voltCount, 0, 1, 0, NULL, buf);
		++voltCount;
		}

	for (i = 0; i < NrFan; i++)
		{
		snprintf(buf, sizeof(buf), "Fan %i", i);
		gkrellm_sensors_add_sensor(SENSOR_FAN, NULL, buf, fanCount, fanCount,
		                           0, 1, 0, NULL, buf);
		++fanCount;
		}

	if (_GK.debug_level & DEBUG_SYSDEP)
		{
		printf("Initialized sensors for %i temps, %i volts and %i fans.\n",
		       tempCount, voltCount, fanCount);
		}

	return TRUE;
	}


/* ===================================================================== */
/* CPU monitor interface */
/* ===================================================================== */

/**
 * One routine reads cpu, disk, and swap data.  All three monitors will
 * call it, but only the first call per timer tick will do the work.
 **/	 
void gkrellm_sys_cpu_read_data(void)
	{
	static gulong user[MAX_CPU] = {0,0,0,0,0,0};
	static gulong sys[MAX_CPU]  = {0,0,0,0,0,0};
	static gulong idle[MAX_CPU] = {0,0,0,0,0,0};

	DWORD type;
	PDH_FMT_COUNTERVALUE value;
	int i;
	gulong userInt = 0;
	gulong idleInt = 0;
	gulong sysInt = 0;

	if (pdhQueryHandle == 0)
		return;

	win32_read_proc_stat();

	for (i = 0; i < numCPUs; i++)
		{
		status = PdhGetFormattedCounterValue(cpuUserCounter[i], PDH_FMT_LONG, &type, &value);
		if (status != ERROR_SUCCESS)
			{
			_tprintf(_T("Getting PDH-counter (cpu user time) failed with status %lu\n"), status);
			return;
			}
		else
			{
			userInt = value.longValue;
			}

		status = PdhGetFormattedCounterValue(cpuSysCounter[i], PDH_FMT_LONG, &type, &value);
		if (status != ERROR_SUCCESS)
			{
			_tprintf(_T("Getting PDH-counter (cpu sys time) failed with status %lu\n"), status);
			return;
			}
		else
			{
			sysInt = value.longValue;
			}

		// user time defined as total - system
		userInt -= sysInt;

		idleInt = 100 - userInt - sysInt;

		user[i] += userInt;
		sys[i] += sysInt;
		idle[i] += idleInt;

		gkrellm_cpu_assign_data(i, user[i], 0/*nice[i]*/, sys[i], idle[i]);
		}
	}


gboolean gkrellm_sys_cpu_init(void)
	{
	SYSTEM_INFO sysInfo;
	int i;
	TCHAR buf[PDH_MAX_COUNTER_PATH];
	TCHAR buf2[10];

	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("INIT cpu monitoring\n"));

	gkrellm_cpu_nice_time_unsupported();

	GetSystemInfo(&sysInfo);
	numCPUs = sysInfo.dwNumberOfProcessors;

	if (numCPUs < 1)
		numCPUs = 1;
	if (numCPUs > MAX_CPU)
		numCPUs = MAX_CPU;

	if (pdhQueryHandle != 0 && perfKeyList[CpuStart] != NULL && perfKeyList[CpuTime] != NULL)
		{
		for (i = 0; i < numCPUs; i++)
			{
			_tcscpy(buf, perfKeyList[CpuStart]);
			_itot(i, buf2, 10);
			_tcscat(buf, buf2);
			_tcscat(buf, perfKeyList[CpuTime]);
			status = PdhAddCounter(pdhQueryHandle, buf, 0, &cpuUserCounter[i]);
			if (status != ERROR_SUCCESS)
				_tprintf(_T("Failed adding cpu user-time pdh-counter for path '%s', status %ld\n"), buf, status);
			else if (_GK.debug_level & DEBUG_SYSDEP)
				_tprintf(_T("Added cpu user-time pdh-counter for path '%s'\n"), buf);

			_tcscpy(buf, perfKeyList[CpuStart]);
			_itot(i, buf2, 10);
			_tcscat(buf, buf2);
			_tcscat(buf, perfKeyList[CpuSysTime]);
			status = PdhAddCounter(pdhQueryHandle, buf, 0, &cpuSysCounter[i]);
			if (status != ERROR_SUCCESS)
				_tprintf(_T("Failed adding cpu sys-time pdh-counter for path '%s', status %ld\n"), buf, status);
			else if (_GK.debug_level & DEBUG_SYSDEP)
				_tprintf(_T("Added cpu sys-time pdh-counter for path '%s'\n"), buf);
			}
		}

	gkrellm_cpu_set_number_of_cpus(numCPUs);

	return TRUE;
	}


/* ===================================================================== */
/* Net monitor interface */
/* ===================================================================== */


void gkrellm_sys_net_read_data(void)
{
	gint i;
	DWORD type;
	PDH_FMT_COUNTERVALUE value;

	if (pdhQueryHandle == 0)
		return;

	win32_read_proc_stat();

	for (i = 0; i < numAdapters; i++)
		{
		status = PdhGetFormattedCounterValue(netRecCounter[i], PDH_FMT_LONG, &type, &value);
		if (status != ERROR_SUCCESS)
			{
			_tprintf(_T("Getting pdh-counter (net recv counter) failed with status %lu\n"), status);
			return;
			}
		else
			{
			rx[i] += value.longValue / _GK.update_HZ;
			}

		status = PdhGetFormattedCounterValue(netSendCounter[i], PDH_FMT_LONG, &type, &value);
		if (status != ERROR_SUCCESS)
			{
			_tprintf(_T("Getting pdh-counter (net send counter) failed with status %lu\n"), status);
			return;
			}
		else
			{
			tx[i] += value.longValue / _GK.update_HZ;
			}

		gkrellm_net_assign_data(netName[i], rx[i], tx[i]);
		}
	}

void gkrellm_sys_net_check_routes(void)
{
	//TODO
}


gboolean gkrellm_sys_net_isdn_online(void)
{
	return FALSE;	/* ISDN is off line */
}


gboolean gkrellm_sys_net_init(void)
	{
	DWORD objectSize = 0;
	DWORD instanceSize = 0;
	int strSize;
	int i;
	int adapter = -1;

	numAdapters = 0;

	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("INIT network monitoring\n"));

	if (pdhQueryHandle != 0)
		{
		TCHAR pdhIface[128];
		DWORD pdhIfaceSz = 128;
		PDH_STATUS stat;

		stat = PdhLookupPerfNameByIndex(NULL, 510, pdhIface, &pdhIfaceSz);
		if (stat != ERROR_SUCCESS) // fall back to non-translated pdh string
			_tcscpy(pdhIface, _T("Network Interface"));

		stat = PdhEnumObjectItems(NULL, NULL, pdhIface, NULL, &objectSize, NULL,
		                          &instanceSize, PERF_DETAIL_WIZARD, 0);
		if (stat != PDH_MORE_DATA && stat != ERROR_SUCCESS)
			{
			_tprintf(_T("Could not enumerate net pdh-counters for path '%s', error %lu\n"), pdhIface, stat);
			}
		else if (instanceSize > 0)
			{
			TCHAR buf[1024];
			LPTSTR objects = NULL;
			LPTSTR instances = NULL;
			LPTSTR instance = NULL;

			++instanceSize;
			++objectSize;

			objects = (LPTSTR) malloc(sizeof(TCHAR) * objectSize);
			instances = (LPTSTR) malloc(sizeof(TCHAR) * instanceSize);

			PdhEnumObjectItems(NULL, NULL, pdhIface, objects, &objectSize,
			                   instances, &instanceSize, PERF_DETAIL_WIZARD, 0);

			for (instance = instances; *instance != 0; instance += lstrlen(instance) + 1)
				{
				++adapter;
				if (adapter >= MAX_NET_ADAPTERS)
					{
					if (_GK.debug_level & DEBUG_SYSDEP)
						_tprintf(_T("Hit maximum number of network adapters.\n"));
					break;
					}

				strSize = MAX_NET_NAME;
				if (_tcslen(instance) < MAX_NET_NAME)
					{
					strSize = _tcslen(instance);
					}

				for (i = 0; i < strSize; i++)
					{
					if (instance[i] == _T(' ')) 
						netName[adapter][i] = _T('_');
					else
						netName[adapter][i] = instance[i];
					}

				netName[adapter][strSize] = _T('\0');
				/* TODO: determine network type */
				gkrellm_net_add_timer_type_ppp(netName[adapter]);

				_tcscpy(buf, perfKeyList[NetDevStart]);
				_tcscat(buf, instance);
				_tcscat(buf, perfKeyList[NetDevRecv]);
				status = PdhAddCounter(pdhQueryHandle, buf, 0, &(netRecCounter[adapter]));
				if (status != ERROR_SUCCESS)
					_tprintf(_T("Failed adding net recv pdh-counter for path '%s', status %ld\n"), buf, status);
				else if (_GK.debug_level & DEBUG_SYSDEP)
					_tprintf(_T("Added net recv pdh-counter for path '%s'\n"), buf);
				rx[adapter] = 0;

				_tcscpy(buf, perfKeyList[NetDevStart]);
				_tcscat(buf, instance);
				_tcscat(buf, perfKeyList[NetDevSend]);
				status = PdhAddCounter(pdhQueryHandle, buf, 0, &(netSendCounter[adapter]));
				if (status != ERROR_SUCCESS)
					_tprintf(_T("Failed adding net send pdh-counter for path '%s', status %ld\n"), buf, status);
				else if (_GK.debug_level & DEBUG_SYSDEP)
					_tprintf(_T("Added net send pdh-counter for path '%s'\n"), buf);
				tx[adapter] = 0;
				}

			/* Final number of network adapters */
			numAdapters = adapter + 1;

			free(objects);
			free(instances);
			}
		}

	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("Found %i interfaces for monitoring.\n"), numAdapters);

	return (numAdapters == 0 ? FALSE : TRUE);
	}


/* ===================================================================== */
/* Disk monitor interface */
/* ===================================================================== */

static gint numDisks = 0;
static long diskread[MAX_DISKS], diskwrite[MAX_DISKS];

gchar * gkrellm_sys_disk_name_from_device(gint device_number, gint unit_number,
			gint *order)
{
	static gchar name[32];

	//TODO: i18n?
	snprintf(name, sizeof(name), "Disk%s", diskName[device_number]);
	*order = device_number;
	return name;
}

gint gkrellm_sys_disk_order_from_name(gchar *name)
{
	return 0;	/* Disk by name not implemented in Windows */
}

void gkrellm_sys_disk_read_data(void)
	{
	/* One routine reads cpu, disk, and swap data.  All three monitors will
	| call it, but only the first call per timer tick will do the work.
	*/
	DWORD type;
	PDH_FMT_COUNTERVALUE value;
	double readInt = 0;
	double writeInt = 0;
	int i;

	if (pdhQueryHandle == 0)
		return;

	win32_read_proc_stat();

	for (i = 0; i < numDisks; i++)
		{
		status = PdhGetFormattedCounterValue(diskReadCounter[i], PDH_FMT_DOUBLE, &type, &value);
		if (status != ERROR_SUCCESS)
			{
			_tprintf(_T("Getting PDH-counter (disk read cnt) failed with status %lu\n"), status);
			return;
			}
		readInt = value.doubleValue / _GK.update_HZ;

		status = PdhGetFormattedCounterValue(diskWriteCounter[i], PDH_FMT_DOUBLE, &type, &value);
		if (status != ERROR_SUCCESS)
			{
			_tprintf(_T("Getting PDH-counter (disk write cnt) failed with status %lu\n"), status);
			return;
			}
		writeInt = value.doubleValue / _GK.update_HZ;

		diskread[i] += readInt;
		diskwrite[i] += writeInt;
		}

	for (i = 0; i < numDisks; i++)
		{
		//gkrellm_disk_assign_data_nth(i, diskread[i], diskwrite[i]);
		gkrellm_disk_assign_data_by_device(i, 0, diskread[i], diskwrite[i],
		                                   FALSE);
		}
	}

gboolean gkrellm_sys_disk_init(void)
	{
	DWORD size = 0;
	DWORD isize = 0;
	TCHAR buf[1024];
	int strSize;
	int i;
	int diskIndex = -1;

	numDisks = 0;

	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("INIT disk monitoring\n"));

	if (pdhQueryHandle != 0)
		{
		TCHAR pdhDisk[128];
		DWORD pdhDiskSz = 128;
		PDH_STATUS stat;
		
		stat = PdhLookupPerfNameByIndex(NULL, 234, pdhDisk, &pdhDiskSz);
		if (stat != ERROR_SUCCESS) // Fall back to non-translated pdh string
			_tcscpy(pdhDisk, TEXT("PhysicalDisk"));

		// Get number of disks that can be queried
		stat = PdhEnumObjectItems(NULL, NULL, pdhDisk, NULL, &size, NULL, &isize, PERF_DETAIL_WIZARD, 0);
		if (stat != PDH_MORE_DATA && stat != ERROR_SUCCESS)
			{
			_tprintf(_T("Could not enumerate disk pdh-counters for path '%s', error %lu\n"), pdhDisk, stat);
			}
		else if (size > 0)
			{
			LPTSTR objects   = NULL;
			LPTSTR instances = NULL;
			LPTSTR instance  = NULL;

			++isize;
			++size;

			objects = (LPTSTR) malloc(sizeof(TCHAR) * size);
			instances = (LPTSTR) malloc(sizeof(TCHAR) * isize);

			// get information about disks
			PdhEnumObjectItems(NULL, NULL, pdhDisk, objects, &size, instances, &isize, PERF_DETAIL_WIZARD, 0);
			for (instance = instances; *instance != 0; instance += lstrlen(instance) + 1)
				{
				// Ignore total counter, gkrellm provides that functionality
				if (_strnicmp(_T("_Total"), instance, 6) == 0)
					continue;

				++diskIndex;
				if (diskIndex >= MAX_DISKS)
					break;

				strSize = min(_tcsclen(instance), MAX_DISK_NAME);

				for (i = 0; i < strSize; i++)
					{
					if (instance[i] == _T(' ')) 
						diskName[diskIndex][i] = _T('_');
					else
						diskName[diskIndex][i] = instance[i];
					}
				diskName[diskIndex][strSize] = _T('\0');

				// assemble object name to pdhQueryHandle
				_tcscpy(buf, perfKeyList[DiskStart]);
				_tcscat(buf, instance);
				_tcscat(buf, perfKeyList[DiskRead]);
				status = PdhAddCounter(pdhQueryHandle, buf, 0, &(diskReadCounter[diskIndex]));
				if (status != ERROR_SUCCESS)
					_tprintf(_T("Failed adding disk read pdh-counter for path '%s', status %ld\n"), buf, status);
				else if (_GK.debug_level & DEBUG_SYSDEP)
					_tprintf(_T("Added disk read pdh-counter for path '%s'\n"), buf);
				diskread[diskIndex] = 0;

				_tcscpy(buf, perfKeyList[DiskStart]);
				_tcscat(buf, instance);
				_tcscat(buf, perfKeyList[DiskWrite]);
				status = PdhAddCounter(pdhQueryHandle, buf, 0, &(diskWriteCounter[diskIndex]));
				if (status != ERROR_SUCCESS)
					_tprintf(_T("Failed adding disk write pdh-counter for path '%s', status %ld\n"), buf, status);
				else if (_GK.debug_level & DEBUG_SYSDEP)
					_tprintf(_T("Added disk write pdh-counter for path '%s'\n"), buf);
				diskwrite[diskIndex] = 0;
				}
			numDisks = diskIndex + 1;

			free(objects);
			free(instances);
			}
		}

	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("Found %i disks for monitoring\n"), numDisks);

	return (numDisks == 0 ? FALSE : TRUE);
	}


/* ===================================================================== */
/* Proc monitor interface */
/* ===================================================================== */

void gkrellm_sys_proc_read_data(void)
	{
	static gulong last_n_forks = 0;
	static gfloat	fload = 0;

	DWORD type;
	PDH_FMT_COUNTERVALUE value;
	gint n_running = 0, n_processes = 0;
	gulong n_forks = 0;
	gulong new_forks;
	gfloat a;

	if (pdhQueryHandle == 0)
		return;

	win32_read_proc_stat();

	status = PdhGetFormattedCounterValue(processCounter, PDH_FMT_LONG, &type, &value);
	if (status != ERROR_SUCCESS)
		{
		_tprintf(_T("Getting PDH-counter (process cnt) failed with status %ld\n"), status);
		return;
		}
	n_processes = value.longValue;
	
	status = PdhGetFormattedCounterValue(threadCounter, PDH_FMT_LONG, &type, &value);
		if (status != ERROR_SUCCESS)
			{
			_tprintf(_T("Getting PDH-counter (thread cnt) failed with status %ld\n"), status);
			return;
			}
	n_forks = value.longValue;

	n_running = n_processes;

    //fload - is the system load average, an exponential moving average over a period
    //    of a minute of n_running.  It measures how heavily a system is loaded
    //    with processes or threads competing for cpu time slices.

    //All the unix OSs have a system call for getting the load average.  But if
    //you don't and can get a n_running number, you can calculate fload.  An
    //exponential moving average (ema) is done like:

    //    a = 2 / (period + 1)
    //    ema = ema + a * (new_value - ema)


    a = 2. / ((_GK.update_HZ * 60.) + 1.);
    new_forks = n_forks - last_n_forks;
    if (new_forks < 0)
        new_forks = 0;
    fload = fload + a * (new_forks - fload);
    if (fload < 0) 
        fload = 0;

	gkrellm_proc_assign_data(n_processes, n_running, n_forks, fload);

    last_n_forks = n_forks;
	}


void gkrellm_sys_proc_read_users(void)
	{
	gint n_users = 1;
	DWORD entriesRead;
	DWORD totalEntries;
	LPBYTE ptr;
	NET_API_STATUS nerr;

	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("Getting number of logged in users.\n"));

	nerr = NetWkstaUserEnum(NULL, 0, &ptr, MAX_PREFERRED_LENGTH, &entriesRead,
	                        &totalEntries, NULL);
	if (nerr == NERR_Success)
		n_users = entriesRead;

	NetApiBufferFree(ptr);

	gkrellm_proc_assign_users(n_users);
	}


gboolean gkrellm_sys_proc_init(void)
	{
	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("INIT process monitoring\n"));

	if (pdhQueryHandle == 0)
		return FALSE;

	status = PdhAddCounter(pdhQueryHandle, perfKeyList[NumProcesses], 0, &processCounter);
	if (status != ERROR_SUCCESS)
		_tprintf(_T("Failed adding process pdh-counter for path '%s', status %ld\n"), perfKeyList[NumProcesses], status);
	else if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("Added process pdh-counter for path '%s'\n"), perfKeyList[NumProcesses]);

	status = PdhAddCounter(pdhQueryHandle, perfKeyList[NumThreads], 0, &threadCounter);
	if (status != ERROR_SUCCESS)
		_tprintf(_T("Failed adding thread pdh-counter for path '%s', status %ld\n"), perfKeyList[NumThreads], status);
	else if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("Added thread pdh-counter for path '%s'\n"), perfKeyList[NumThreads]);

	return TRUE;
	}


/* ===================================================================== */
/* Memory/Swap monitor interface */
/* ===================================================================== */

void gkrellm_sys_mem_read_data(void)
{
	MEMORYSTATUSEX memStatus;
	memStatus.dwLength = sizeof(memStatus);

	if (GlobalMemoryStatusEx(&memStatus) != 0)
	{
		guint64  physUsed;
		guint64  shared;
		guint64  buffers;
		DWORDLONG physAvail, physTot;
		DWORDLONG cached;

		physTot   = memStatus.ullTotalPhys;
		physAvail = memStatus.ullAvailPhys;

		shared  = 0; //TODO: how to determine?
		buffers = 0; //TODO: how to determine?
		// TODO: don't know if this is correct (update: no, it's not)
		cached = 0; //(virtTot - virtAvail) - (pageTot - pageAvail);
		//if (cached < 0)
		//	cached = 0;

		physUsed = (physTot - physAvail) - buffers - cached;

		gkrellm_mem_assign_data(physTot, physUsed, physAvail, shared, buffers, cached);
	}
}


void gkrellm_sys_swap_read_data(void)
	{
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	NTSTATUS ntstatus;
	ULONG  szBuf = 3*sizeof(SYSTEM_PAGEFILE_INFORMATION);
	LPVOID pBuf  = NULL;

	// it is difficult to determine beforehand which size of the
	// buffer will be enough to retrieve all information, so we
	// start with a minimal buffer and increase its size until we get
	// the information successfully
	do
	{
		pBuf = malloc(szBuf);
		if (pBuf == NULL)
			break;

		ntstatus = ZwQuerySystemInformation(SystemPagefileInformation, pBuf, szBuf, NULL);
		if (ntstatus == STATUS_INFO_LENGTH_MISMATCH)
		{
			free(pBuf);
			szBuf *= 2;
		}
		else if (!NT_SUCCESS(ntstatus))
		{	// give up
			break;
		}
	}
	while (ntstatus == STATUS_INFO_LENGTH_MISMATCH);

	guint64 swapTotal = 0;
	guint64 swapUsed  = 0;
	gulong  swapIn    = 0; //TODO: calculate value
	gulong  swapOut   = 0; //TODO: calculate value

	// iterate over information for all pagefiles
	PSYSTEM_PAGEFILE_INFORMATION pInfo = (PSYSTEM_PAGEFILE_INFORMATION)pBuf;
	for (;;)
	{
		swapTotal += pInfo->CurrentSize * si.dwPageSize;
		swapUsed  += pInfo->TotalUsed * si.dwPageSize; 
		if (pInfo->NextEntryOffset == 0)
			break;
		// get pointer to next struct
		pInfo = (PSYSTEM_PAGEFILE_INFORMATION)((PBYTE)pInfo +
			pInfo->NextEntryOffset);
	}
	free(pBuf);

	gkrellm_swap_assign_data(swapTotal, swapUsed, swapIn, swapOut);
	}


gboolean gkrellm_sys_mem_init(void)
	{
	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("INIT memory monitoring\n"));
	return TRUE;
	}


/* ===================================================================== */
/* Battery monitor interface */
/* ===================================================================== */

#define	L_NO_BATTERY	0x80
#define	L_ON_LINE		1
#define	L_CHARGING		8
#define L_UNKNOWN		0xFF

void gkrellm_sys_battery_read_data(void)
{
	gboolean available, on_line, charging;
	gint percent, time_left;
	SYSTEM_POWER_STATUS power;

	GetSystemPowerStatus(&power);
	if (   (power.BatteryFlag & L_NO_BATTERY) == L_NO_BATTERY
	    || (power.BatteryFlag & L_UNKNOWN) == L_UNKNOWN
	   )
		{
		available = FALSE;
		}
	else
		{
		available = TRUE;
		}

	on_line = ((power.ACLineStatus & L_ON_LINE) == L_ON_LINE) ? TRUE : FALSE;
	charging= ((power.BatteryFlag & L_CHARGING) == L_CHARGING) ? TRUE : FALSE;

	time_left = power.BatteryLifeTime;
	percent = power.BatteryLifePercent;

	gkrellm_battery_assign_data(0, available, on_line, charging, percent, time_left);
}


gboolean gkrellm_sys_battery_init()
	{
	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("INIT battery monitoring\n"));
	return TRUE;
	}


/* ============================================== */
/* FS monitor interfaces */
/* ===================================================================== */

gboolean gkrellm_sys_fs_fstab_modified(void)
	{
	return FALSE;
	}


void eject_win32_cdrom(gchar *device)
	{
	HANDLE hFile;
	BOOL err;
	TCHAR devPath[MAX_PATH];
	DWORD numBytes;

	if (!device || device[0] == 0)
		return;

	/* FIXME: This assumes device names like "D:" */
	_sntprintf(devPath, ARRAYSIZE(devPath), _T("\\\\.\\%c:"), device[0]);
	devPath[ARRAYSIZE(devPath) - 1] = 0;

	hFile = CreateFile(devPath, GENERIC_READ,
	                   FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0,
	                   NULL);
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
	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("INIT filesystem monitoring\n"));
	gkrellm_fs_mounting_unsupported();
	gkrellm_fs_setup_eject(NULL, NULL, eject_win32_cdrom, NULL);
	return TRUE;
	}


void gkrellm_sys_fs_get_fsusage(gpointer fs, gchar *dir)
	{
	BOOL err = 0;

	if (!dir || dir[0] == 0)
		return;

	ULARGE_INTEGER freeAvailableToCaller;
	ULARGE_INTEGER totalBytes;
	ULARGE_INTEGER freeBytes;
	gulong total, freeCaller, free;

	/* TODO: Unicode support? */
	err = GetDiskFreeSpaceExA(dir, &freeAvailableToCaller, &totalBytes, &freeBytes);
	if (err != 0)
		{
		total = EnlargedUnsignedDivide(totalBytes, 1024, 0);
		freeCaller = EnlargedUnsignedDivide(freeAvailableToCaller, 1024, 0);
		free = EnlargedUnsignedDivide(freeBytes, 1024, 0);
		// fs, blocks, avail, free, size
		gkrellm_fs_assign_fsusage_data(fs, total, freeCaller, free, 1024);
		}
	else
		{
		/* TODO: This may happen on cd/dvd drives, ignore for now */
		/*
		_tprintf(_T("GetDiskFreeSpaceEx() failed on drive %c:, error %d\n"),
		         *dir, err);
		*/
		}
	}


void gkrellm_sys_fs_get_mounts_list(void)
	{
	char driveStrings[512];
	char *drive;
	DWORD ret;
	/* TODO: Unicode support? */
	ret = GetLogicalDriveStringsA(ARRAYSIZE(driveStrings) - 1, driveStrings);
	if (ret > 0)
		{
		for (drive = driveStrings; *drive != 0; drive += lstrlenA(drive) + 1)
			{
			if (_strnicmp("A:\\", drive, 3) != 0
			    && _strnicmp("B:\\", drive, 3) != 0
			   )
				{
				if (_GK.debug_level & DEBUG_SYSDEP)
					printf(("Found mounted drive '%s'\n"), drive);
				gkrellm_fs_add_to_mounts_list(drive, drive, "");
				}
			}
		}
	}


void gkrellm_sys_fs_get_fstab_list(void)
	{
	char driveStrings[512];
	char *drive;
	DWORD ret;
	/* TODO: Unicode support? */
	ret = GetLogicalDriveStringsA(ARRAYSIZE(driveStrings) - 1, driveStrings);
	if (ret > 0)
		{
		for (drive = driveStrings; *drive != 0; drive += lstrlenA(drive) + 1)
			{
			if (_strnicmp("A:\\", drive, 3) != 0
			    && _strnicmp("B:\\", drive, 3) != 0
			   )
				{
				if (_GK.debug_level & DEBUG_SYSDEP)
					printf(("Found fstab drive '%s'\n"), drive);
				gkrellm_fs_add_to_fstab_list(drive, drive, "", "");
				}
			}
		}
	}


/* ===================================================================== */
/* INET monitor interfaces */
/* ===================================================================== */

gboolean gkrellm_sys_inet_init(void)
{
	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("INIT inet port monitoring\n"));
	return TRUE;
}


void gkrellm_sys_inet_read_tcp_data(void)
	{
	PMIB_TCPTABLE pTcpTable = NULL;
	DWORD dwTableSize = 0;

	// Make an initial call to GetTcpTable to
	// get the necessary size into the dwSize variable
	if (GetTcpTable(NULL, &dwTableSize, FALSE) == ERROR_INSUFFICIENT_BUFFER
	    && dwTableSize > 0
	   )
		{
		pTcpTable = (MIB_TCPTABLE *)malloc(dwTableSize);

		// Make a second call to GetTcpTable to get
		// the actual data we require
		if (GetTcpTable(pTcpTable, &dwTableSize, FALSE) == NO_ERROR)
			{
			ActiveTCP tcp;
			DWORD i;
			for (i = 0; i < pTcpTable->dwNumEntries; i++)
				{
				MIB_TCPROW *row = &pTcpTable->table[i];

				if (row->dwState != MIB_TCP_STATE_ESTAB)
					continue;

				tcp.family             = AF_INET;
				tcp.local_port         = htons(row->dwLocalPort);
				tcp.remote_port        = htons(row->dwRemotePort);
				tcp.remote_addr.s_addr = row->dwRemoteAddr;

				gkrellm_inet_log_tcp_port_data(&tcp);
				}
			}
		free(pTcpTable);
		}
	}


/* ===================================================================== */
/* Uptime monitor interface */
/* ===================================================================== */

time_t gkrellm_sys_uptime_read_uptime(void)
{
	DWORD type;
	PDH_FMT_COUNTERVALUE value;
	long l = 0;

	win32_read_proc_stat();

	if (pdhQueryHandle != 0)
		{
		status = PdhGetFormattedCounterValue(uptimeCounter, PDH_FMT_LONG, &type, &value);
		if (status != ERROR_SUCCESS)
			{
			_tprintf(_T("Getting uptime pdh-counter failed, status %ld\n"), status);
			return (time_t)0;
			}
		l = value.longValue;
		}

	return (time_t) l;
	}


gboolean gkrellm_sys_uptime_init(void)
	{
	if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("INIT uptime monitoring\n"));

	if (pdhQueryHandle == 0)
		return FALSE;
		
	status = PdhAddCounter(pdhQueryHandle, perfKeyList[Uptime], 0, &uptimeCounter);
	if (status != ERROR_SUCCESS)
		_tprintf(_T("Failed adding uptime pdh-counter for path '%s', status %ld\n"), perfKeyList[Uptime], status);
	else if (_GK.debug_level & DEBUG_SYSDEP)
		_tprintf(_T("Added uptime pdh-counter for path '%s'\n"), perfKeyList[Uptime]);

	return (status == ERROR_SUCCESS ? TRUE : FALSE);
	}


/* ===================================================================== */
/* hostname interface */
/* ===================================================================== */

gchar *gkrellm_sys_get_host_name(void)
	{
	static gboolean	host_name_fetched = FALSE;

	if (!host_name_fetched)
		{
		char buf[128];
		int err;

		if (_GK.debug_level & DEBUG_SYSDEP)
			_tprintf(_T("Retrieving host name.\n"));

		err = gethostname(buf, sizeof(buf));

		/* TODO: i18n */
		if (err != 0)
			hostname = g_strdup("Unknown");
		else
			hostname = g_strdup(buf);

		host_name_fetched = TRUE;
		}
	return hostname;
	}


/* ===================================================================== */
/* System name interface */
/* ===================================================================== */

gchar *gkrellm_sys_get_system_name(void)
	{
	static gboolean	system_name_fetched = FALSE;
	char osName[64];

	if (!system_name_fetched)
		{
		if (_GK.debug_level & DEBUG_SYSDEP)
			_tprintf(_T("Retrieving system name.\n"));
		strcpy(osName, "Unknown");
		if (info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
			{
			if (info.dwMinorVersion == 0)
				strcpy(osName, "Windows 95");
			else if (info.dwMinorVersion == 10)
				strcpy(osName, "Windows 98");
			else if (info.dwMinorVersion == 90)
				strcpy(osName, "Windows Me");
			}
		else if (info.dwPlatformId == VER_PLATFORM_WIN32_NT)
			{
			if (info.dwMinorVersion == 0)
				{
				if (info.dwMajorVersion == 6)
					strcpy(osName, "Windows Vista");
				else if (info.dwMajorVersion == 5)
					strcpy(osName, "Windows 2000");
				else if (info.dwMajorVersion == 4)
					strcpy(osName, "Windows NT 4.0");
				}
			else if (info.dwMinorVersion == 51)
				{
				strcpy(osName, "Windows NT 3.51");
				}
			else if (info.dwMinorVersion == 1)
				{
				strcpy(osName, "Windows XP");
				}
			}
		sname = g_strdup(osName);
		system_name_fetched = TRUE;
		}
	return sname;
	}


/* ===================================================================== */
/* MBMON interface */
/* ===================================================================== */

gboolean gkrellm_sys_sensors_mbmon_port_change(gint port)
{
	return FALSE;
}

gboolean gkrellm_sys_sensors_mbmon_supported(void)
{
	return FALSE;
}


/* ===================================================================== */
/* Misc functions */
/* ===================================================================== */


static void
initPerfKeyList(void)
	{
	int i;
	for (i = 0; i < PerfKeysSize; i++)
		{
		perfKeyList[i] = NULL; 
		}
		
	if (pdhQueryHandle == 0)
		return;

	placePerfKeysFromReg(NumProcesses , 2  , 248);
	placePerfKeysFromReg(NumThreads   , 2  , 250);
	placePerfKeysFromReg(Uptime       , 2  , 674);

	placePerfKeyFromReg(CpuStart      , 238, _T("\\") , _T("("));
	placePerfKeyFromReg(CpuTime       , 6  , _T(")\\"), NULL);
	placePerfKeyFromReg(CpuSysTime    , 144, _T(")\\"), NULL);
	placePerfKeyFromReg(NetDevStart   , 510, _T("\\") , _T("("));
	placePerfKeyFromReg(NetDevRecv    , 264, _T(")\\"), NULL);
	placePerfKeyFromReg(NetDevSend    , 896, _T(")\\"), NULL);
	placePerfKeyFromReg(DiskStart     , 234, _T("\\") , _T("("));
	placePerfKeyFromReg(DiskRead      , 220, _T(")\\"), NULL);
	placePerfKeyFromReg(DiskWrite     , 222, _T(")\\"), NULL);
	}


static void
placePerfKeysFromReg(const PerfKey key, unsigned int index1, unsigned int index2)
	{
	TCHAR buf[512];
	TCHAR perfName[512];
	PDH_STATUS stat;
	DWORD size = 512;

	stat = PdhLookupPerfNameByIndex(NULL, index1, perfName, &size);
	if (stat == ERROR_SUCCESS)
		{
		_tcsncpy(buf, _T("\\"), 512);
		_tcsncat(buf, perfName, 512);
		_tcsncat(buf, _T("\\"), 512);

		size = 512;
		stat = PdhLookupPerfNameByIndex(NULL, index2, perfName, &size);
		if (stat == ERROR_SUCCESS)
			{
			_tcsncat(buf, perfName, 512);
			placePerfKey(key, buf);
			}
		else
			{
			_tprintf(_T("could not find perflib index %d in registry\n"), index2);
			placePerfKey(key, NULL);
			}
		}
	else
		{
		_tprintf(_T("Could not find perflib index %d in registry\n"), index1);
		placePerfKey(key, NULL);
		}
	}


static void
placePerfKeyFromReg(const PerfKey key, unsigned int index, const TCHAR* prefix,
                    const TCHAR* suffix)
	{
	TCHAR buf[512];
	TCHAR perfName[512];
	PDH_STATUS stat;
	DWORD size = 512;

	stat = PdhLookupPerfNameByIndex(NULL, index, perfName, &size);
	if (stat == ERROR_SUCCESS)
		{
		if (prefix)
			{
			_tcsncpy(buf, prefix, 512);
			_tcsncat(buf, perfName, 512);
			}
		else
			{
			_tcsncpy(buf, perfName, 512);
			}
		
		if (suffix)
			{
			_tcsncat(buf, suffix, 512);
			}
		placePerfKey(key, buf);
		}
	else
		{
		_tprintf(_T("could not find index %d in registry\n"), index);
		placePerfKey(key, NULL);
		}
	}


static void
placePerfKey(const PerfKey key, const TCHAR* value)
	{
	size_t strSize;
	if (((int)key > -1) && ((int)key < PerfKeysSize))
		{
		free(perfKeyList[key]);
		if (value != NULL)
			{
			strSize = _tcsclen(value);
			perfKeyList[key] = malloc(sizeof(TCHAR) * strSize + 1);
			_tcscpy(perfKeyList[key], value);
			}
		else
			{
			perfKeyList[key] = NULL;
			}
		//_tprintf(_T("perfKeyList[ %d ] = '%s'\n"), key, perfKeyList[key]);
		}
	else
		{
		_tprintf(_T("Invalid placement for key %d; value was '%s'\n"), key, value);
		}
	}



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
