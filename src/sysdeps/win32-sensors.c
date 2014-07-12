/* GKrellM
|  Copyright (C) 1999-2014 Bill Wilson
|
|  Authors:  Bill Wilson    billw@gkrellm.net
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

#include <wchar.h>
#include <windows.h>

typedef struct _ShmData
	{
	HANDLE handle;
	void *data;
	} ShmData;

static gboolean
shm_open(ShmData *shm, const wchar_t *shm_name)
	{
	shm->handle = OpenFileMappingW(FILE_MAP_READ, FALSE, shm_name);
	if (!shm->handle)
		{
		shm->data = NULL;
		return FALSE;
		}

	shm->data = MapViewOfFile(shm->handle, FILE_MAP_READ, 0, 0, 0);
	if (!shm->data)
		{
		CloseHandle(shm->handle);
		shm->handle = NULL;
		return FALSE;
		}
	return TRUE;
	}

static void
shm_close(ShmData *shm)
	{
	if (shm->data)
		UnmapViewOfFile(shm->data);
	if (shm->handle)
		CloseHandle(shm->handle);
	}

static gboolean
shm_open_or_start_app(ShmData *shm, const wchar_t *shm_name,
		const gchar *app_name)
	{
	guint retries;

	/* Try to open shared memory area and return if successful*/
	if (shm_open(shm, shm_name))
		return TRUE;

	/* shared memory area could not be opened, try to start sensor-app */
	GError *err = NULL;
	if (!g_spawn_command_line_async(app_name, &err))
		{
		g_warning("Could not start sensor-app %s: %s\n",
				app_name, err->message);
		g_error_free(err);
		return FALSE;
		}

	gkrellm_debug(DEBUG_SYSDEP,
			"Started sensor-app %s, waiting for it to initialize\n",
			app_name);

	/* Wait up to retries seconds for the shared memory area to be accessible */
	for (retries = 10; retries > 0; --retries)
		{
		if (shm_open(shm, shm_name))
			return TRUE;
		g_usleep(1 * G_USEC_PER_SEC); /* delay to allow sensor-app to startup */
		}

	return FALSE;
	}


// ---------------------------------------------------------------------------
// Interface to work with shared memory for MBM5
//
// Copyright 2001 A@majland.org
// Alteration for use in Visual C by Chris Zahrt techn0@iastate.edu
//
//  Version     : 0.1
//  Date        : 02-27-2002
//  MBM         : version 5.1
//
//  Author      : - Anders@Majland.org (author of original c code)
//                  http://www.majland.org/sw/mbmcaf
//                - Chris Zahrt techn0@iastate.edu (visual c alterations)
//                   http://techn0.dhs.org/programming/vcmbmsm.html
//
//  Licence     : Cardware. (Send me a note/email if you find it useful.)
//                Basically you may use it as you see fit as long as the origin
//                of the code remains clear
//
//  History     :
//		  0.1 02-27-2002 conversion of 0.3 borland to this version
//
// Update for MBM 5.1.9 by Bill Nalen bill@nalens.com
// ---------------------------------------------------------------------------

#define BusType         char
#define SMBType         char
#define SensorType      char
#define stUnknown       (char)(0)
#define stTemperature   (char)(1)
#define stVoltage       (char)(2)
#define stFan           (char)(3)
//#define stMhz           (char)(4)
//#define stPercentage    (char)(5)

typedef struct _MBMSharedIndex
	{
	SensorType iType; // type of sensor
	int Count; // number of sensor for that type
	}
	MBMSharedIndex;

typedef struct _MBMSharedSensor
	{
	SensorType ssType;        // type of sensor
	unsigned char ssName[12]; // name of sensor
	char sspadding1[3];       // padding of 3 byte
	double ssCurrent;         // current value
	double ssLow;             // lowest readout
	double ssHigh;            // highest readout
	long ssCount;             // total number of readout
	char sspadding2[4];       // padding of 4 byte
	long double ssTotal;      // total amout of all readouts
	char sspadding3[6];       // padding of 6 byte
	double ssAlarm1;          // temp & fan: high alarm; voltage: % off;
	double ssAlarm2;          // temp: low alarm
	}
	MBMSharedSensor;

typedef struct _MBMSharedInfo
	{
	short siSMB_Base; // SMBus base address
	BusType siSMB_Type; // SMBus/Isa bus used to access chip
	SMBType siSMB_Code; // SMBus sub type, Intel, AMD or ALi
	char siSMB_Addr; // Address of sensor chip on SMBus
	unsigned char siSMB_Name[41]; // Nice name for SMBus
	short siISA_Base; // ISA base address of sensor chip on ISA
	int siChipType; // Chip nr, connects with Chipinfo.ini
	char siVoltageSubType; // Subvoltage option selected
	}
	MBMSharedInfo;

typedef struct _MBMSharedData
	{
	double sdVersion; // version number (example: 51090)
	MBMSharedIndex sdIndex[10]; // Sensor index
	MBMSharedSensor sdSensor[100]; // sensor info
	MBMSharedInfo sdInfo; // misc. info
	unsigned char sdStart[41]; // start time
	unsigned char sdCurrent[41]; // current time
	unsigned char sdPath[256]; // MBM path
	}
	MBMSharedData;

static const wchar_t* MBM_SHM_NAME = L"$M$B$M$5$S$D$";
static const gchar*   MBM_EXE_NAME = "MBM5.exe";

static SensorType gkrellm_sensor_type_to_mbm(gint type)
{
	if (type == SENSOR_TEMPERATURE)
		return stTemperature;
	if (type == SENSOR_VOLTAGE)
		return stVoltage;
	if (type == SENSOR_FAN)
		return stFan;
	return stUnknown;
}

static gboolean
gkrellm_sys_sensors_mbm_get_value(gint sensor_id, gint sensor_type, gfloat *value)
	{
	ShmData          shm;
	MBMSharedData   *data;
	MBMSharedSensor *sensor;
	SensorType       st;

	st = gkrellm_sensor_type_to_mbm(sensor_type);
	if (st == stUnknown || sensor_id < 0 || sensor_id > 99)
		return FALSE; // id out of range

	if (!shm_open(&shm, MBM_SHM_NAME))
		return FALSE;
	data = (MBMSharedData*)(shm.data);

	gkrellm_debug(DEBUG_SYSDEP, "Fetching sensor value %d from MBM\n", sensor_id);

	sensor = &(data->sdSensor[sensor_id]);
	if (sensor->ssType != st)
		{
		shm_close(&shm);
		return FALSE;
		}

	*value = sensor->ssCurrent;
	shm_close(&shm);
	return TRUE;
	}

static gboolean
gkrellm_sys_sensors_mbm_init(void)
{
	ShmData          shm;
	MBMSharedData   *data;
	MBMSharedSensor *sensor;
	gint i, sensorCount, tempCount, voltCount, fanCount;
	gchar *id_name;

	if (!shm_open_or_start_app(&shm, MBM_SHM_NAME, MBM_EXE_NAME))
		return FALSE;
	data = (MBMSharedData*)(shm.data);

	sensorCount = 0;
	for (i = 0; i < 9; i++)
		sensorCount += data->sdIndex[i].Count;

	tempCount = 0;
	voltCount = 0;
	fanCount = 0;
	for (i = 0; i < sensorCount; i++)
		{
		sensor = &(data->sdSensor[i]);
		switch (sensor->ssType)
			{
			case stTemperature:
				id_name = g_strdup_printf("mbm-temp-%d", tempCount);

				gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE, /*sensor_path*/NULL,
						/*id_name*/id_name, /*id*/i, /*iodev*/0,
						/*inter*/MBM_INTERFACE, /*factor*/1, /*offset*/0,
						/*vref*/NULL, /*default_label*/(gchar *)sensor->ssName);

				g_free(id_name);
				++tempCount;
				break;
			case stVoltage:
				id_name = g_strdup_printf("mbm-volt-%d", voltCount);

				gkrellm_sensors_add_sensor(SENSOR_VOLTAGE, /*sensor_path*/NULL,
						/*id_name*/id_name, /*id*/i, /*iodev*/0,
						/*inter*/MBM_INTERFACE, /*factor*/1, /*offset*/0,
						/*vref*/NULL, /*default_label*/(gchar *)sensor->ssName);

				g_free(id_name);
				++voltCount;
				break;
			case stFan:
				id_name = g_strdup_printf("mbm-fan-%d", fanCount);

				gkrellm_sensors_add_sensor(SENSOR_FAN, /*sensor_path*/NULL,
						/*id_name*/id_name, /*id*/i, /*iodev*/0,
						/*inter*/MBM_INTERFACE, /*factor*/1, /*offset*/0,
						/*vref*/NULL, /*default_label*/(gchar *)sensor->ssName);

				g_free(id_name);
				++fanCount;
				break;
			} /* switch() */
		} /* for() */

	shm_close(&shm);
	return TRUE;
	}


/* ======================================================================== */
// SpeedFan

// Strucure of the shared block
#pragma pack(push, 1)
typedef struct _SFSharedMemory
{
	unsigned short int version;
	unsigned short int flags;
	signed int         MemSize;
	signed int         handle;
	unsigned short int NumTemps;
	unsigned short int NumFans;
	unsigned short int NumVolts;
	signed int         temps[32];
	signed int         fans[32];
	signed int         volts[32];
} SFSharedMemory;
#pragma pack(pop)

static const wchar_t* SPEEDFAN_SHM_NAME = L"SFSharedMemory_ALM";
static const gchar*   SPEEDFAN_EXE_NAME = "speedfan.exe";

static gboolean
gkrellm_sys_sensors_sf_get_value(gint sensor_id, gint sensor_type, gfloat *value)
{
	ShmData         shm;
	SFSharedMemory *data;
	gboolean        ret = FALSE;

	if (sensor_id < 0 || sensor_id > 31)
		return FALSE; // id out of range

	if (!shm_open(&shm, SPEEDFAN_SHM_NAME))
		return FALSE;
	data = (SFSharedMemory*)(shm.data);

	gkrellm_debug(DEBUG_SYSDEP, "Fetching sensor value %d from SpeedFan\n", sensor_id);
	switch(sensor_type)
		{
		case SENSOR_TEMPERATURE:
			if (sensor_id < data->NumTemps)
				{
				*value = data->temps[sensor_id] / 100.0;
				ret = TRUE;
				}
			break;
		case SENSOR_VOLTAGE:
			if (sensor_id < data->NumVolts)
				{
				*value = data->volts[sensor_id] / 100.0;
				ret = TRUE;
				}
			break;
		case SENSOR_FAN:
			if (sensor_id < data->NumFans)
				{
				*value = data->fans[sensor_id];
				ret = TRUE;
				}
			break;
		}
	shm_close(&shm);
	return ret;
	}

static gboolean
gkrellm_sys_sensors_sf_init(void)
	{
	ShmData         shm;
	SFSharedMemory *data;
	gint            i;
	gchar          *id_name;
	gchar          *default_label;

	if (!shm_open_or_start_app(&shm, SPEEDFAN_SHM_NAME, SPEEDFAN_EXE_NAME))
		return FALSE;
	data = (SFSharedMemory*)(shm.data);

	gkrellm_debug(DEBUG_SYSDEP, "Enumerating %hu temps, %hu voltages and %hu fans\n",
				data->NumTemps, data->NumVolts, data->NumFans);

	for (i = 0; i < data->NumTemps; i++)
		{
		id_name = g_strdup_printf("speedfan-temp-%d", i);
		default_label = g_strdup_printf("Temp %d", i+1);

		gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE, /*sensor_path*/NULL,
			/*id_name*/id_name, /*id*/i, /*iodev*/0,
			/*inter*/SF_INTERFACE, /*factor*/1, /*offset*/0,
			/*vref*/NULL, /*default_label*/default_label);

		g_free(id_name);
		g_free(default_label);
		}

	for (i = 0; i < data->NumVolts; i++)
		{
		id_name = g_strdup_printf("speedfan-volt-%d", i);
		default_label = g_strdup_printf("Voltage %d", i+1);

		gkrellm_sensors_add_sensor(SENSOR_VOLTAGE, /*sensor_path*/NULL,
			/*id_name*/id_name, /*id*/i, /*iodev*/0,
			/*inter*/SF_INTERFACE, /*factor*/1, /*offset*/0,
			/*vref*/NULL, /*default_label*/default_label);

		g_free(id_name);
		g_free(default_label);
		}

	for (i = 0; i < data->NumFans; i++)
		{
		id_name = g_strdup_printf("speedfan-fan-%d", i);
		default_label = g_strdup_printf("Fan %d", i+1);

		gkrellm_sensors_add_sensor(SENSOR_FAN, /*sensor_path*/NULL,
			/*id_name*/id_name, /*id*/i, /*iodev*/0,
			/*inter*/SF_INTERFACE, /*factor*/1, /*offset*/0,
			/*vref*/NULL, /*default_label*/default_label);

		g_free(id_name);
		g_free(default_label);
		}

	shm_close(&shm);
	return TRUE;
	}


/* ======================================================================== */
// CoreTemp

/**
 * ucFahrenheit and ucDeltaToTjMax represent boolean values. 0 = false, 1 = true.
 * If ucFahrenheit is set, the temperature is reported in Fahrenheit.
 * If ucDeltaToTjMax is set, the temperature reported respresents the distance
 * from TjMax.
 *
 * Information and struct taken from
 * http://www.alcpu.com/CoreTemp/developers.html
**/
typedef struct _CORE_TEMP_SHARED_DATA
{
	unsigned int  uiLoad[256];
	unsigned int  uiTjMax[128];
	unsigned int  uiCoreCnt;
	unsigned int  uiCPUCnt;
	float         fTemp[256];
	float         fVID;
	float         fCPUSpeed;
	float         fFSBSpeed;
	float         fMultipier;
	char          sCPUName[100];
	unsigned char ucFahrenheit;
	unsigned char ucDeltaToTjMax;
} CORE_TEMP_SHARED_DATA;

static const wchar_t* CORE_TEMP_SHM_NAME = L"CoreTempMappingObject";
static const gchar*   CORE_TEMP_EXE_NAME = "CoreTemp.exe";

static gboolean
gkrellm_sys_sensors_ct_get_temp(guint core_index, guint cpu_index, gfloat *temp)
	{
	ShmData                shm;
	CORE_TEMP_SHARED_DATA *data;
	guint                  temp_index;

	if (core_index < 0 || core_index > 255 || cpu_index < 0 || cpu_index > 127)
		return FALSE; // core or cpu index out of range

	if (!shm_open(&shm, CORE_TEMP_SHM_NAME))
		return FALSE;
	data = (CORE_TEMP_SHARED_DATA*)(shm.data);

	gkrellm_debug(DEBUG_SYSDEP,
		"Fetching temp for core %d, cpu %d from CoreTemp\n", core_index,
		cpu_index);

	// 'core index' + ( 'cpu index' * 'number of cores per cpu' )
	temp_index = core_index + (cpu_index * data->uiCoreCnt);

	// make absolute value from delta
	if (data->ucDeltaToTjMax == '\1')
		*temp = data->uiTjMax[cpu_index] - data->fTemp[temp_index];
	else
		*temp = data->fTemp[temp_index];

	// Convert Fahrenheit to Celsius
	if (data->ucFahrenheit == '\1')
		*temp = (*temp - 32) * 5 / 9;

	shm_close(&shm);
	return TRUE;
	}

static gboolean
gkrellm_sys_sensors_ct_init(void)
	{
	ShmData                shm;
	CORE_TEMP_SHARED_DATA *data;
	guint                  uiCpu;
	guint                  uiCore;
	gchar                 *id_name;
	gchar                 *default_label;

	if (!shm_open_or_start_app(&shm, CORE_TEMP_SHM_NAME, CORE_TEMP_EXE_NAME))
		return FALSE;
	data = (CORE_TEMP_SHARED_DATA*)(shm.data);

	for (uiCpu = 0; uiCpu < data->uiCPUCnt; uiCpu++)
		{
		for (uiCore = 0; uiCore < data->uiCoreCnt; uiCore++)
			{
			id_name = g_strdup_printf("coretemp-cpu%u-core%u", uiCpu, uiCore);
			if (data->uiCPUCnt == 1)
				default_label = g_strdup_printf("CPU Core %u", uiCore+1);
			else
				default_label = g_strdup_printf("CPU %u, Core %u", uiCpu+1, uiCore+1);

			gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE, /*sensor_path*/NULL,
				/*id_name*/id_name, /*id*/uiCore, /*iodev*/uiCpu,
				/*inter*/CT_INTERFACE, /*factor*/1, /*offset*/0,
				/*vref*/NULL, /*default_label*/default_label);

			g_free(id_name);
			g_free(default_label);
			}
		}

	shm_close(&shm);
	return TRUE;
	}


/**
 * GPU-Z sensor reading
 *
 * Information and struct taken from
 * http://www.techpowerup.com/forums/threads/gpu-z-shared-memory-layout.65258/
 **/
#define GPUZ_MAX_RECORDS 128

#pragma pack(push, 1)
typedef struct _GPUZ_RECORD
{
	WCHAR key[256];
	WCHAR value[256];
} GPUZ_RECORD;

typedef struct _GPUZ_SENSOR_RECORD
{
	WCHAR name[256];
	WCHAR unit[8];
	UINT32 digits;
	double value;
} GPUZ_SENSOR_RECORD;

typedef struct _GPUZ_SH_MEM
{
	UINT32 version; /* Version number, 1 for the struct here */
	volatile LONG busy; /* Is data being accessed? */
	UINT32 lastUpdate; /* GetTickCount() of last update */
	GPUZ_RECORD data[GPUZ_MAX_RECORDS];
	GPUZ_SENSOR_RECORD sensors[GPUZ_MAX_RECORDS];
} GPUZ_SH_MEM;
#pragma pack(pop)

static const wchar_t* GPUZ_SHM_NAME = L"GPUZShMem";
static const gchar*   GPUZ_EXE_NAME = "gpu-z.exe";

static gboolean
gpuz_sensor_unit_to_sensor_type(const wchar_t *sensor_unit, gint *sensor_type)
	{
	/* TODO: Support "°F" if needed and convert values to celsius */
	if (wcscmp(sensor_unit, L"°C") == 0)
		{
		*sensor_type = SENSOR_TEMPERATURE;
		return TRUE;
		}
	else if (wcscmp(sensor_unit, L"RPM") == 0)
		{
		*sensor_type = SENSOR_FAN;
		return TRUE;
		}
	else if (wcscmp(sensor_unit, L"V") == 0)
		{
		*sensor_type = SENSOR_VOLTAGE;
		return TRUE;
		}

	/* TODO: Handle more sensor types in gkrellm itself? */
	return FALSE;
	}

static void
gpuz_sensor_add(guint index, gint sensor_type, gchar *sensor_name)
{
	gchar *id_name;

	/* TODO: is index stable on changing hardware? */
	id_name = g_strdup_printf("gpuz-sensor-%u", index);

	gkrellm_sensors_add_sensor(sensor_type,
			/*sensor_path*/NULL,
			/*id_name*/id_name,
			/*id*/index,
			/*iodev*/0,
			/*inter*/GPUZ_INTERFACE,
			/*factor*/1,
			/*offset*/0,
			/*vref*/NULL,
			/*default_label*/sensor_name);

	g_free(id_name);
}

static gboolean
gkrellm_sys_sensors_gpuz_get_value(guint index, gfloat *value)
{
	ShmData             shm;
	GPUZ_SH_MEM        *data;
	GPUZ_SENSOR_RECORD *sensor;

	if (index >= GPUZ_MAX_RECORDS)
		return FALSE; /* index out of range */

	if (!shm_open(&shm, GPUZ_SHM_NAME))
		return FALSE;
	data = (GPUZ_SH_MEM*)(shm.data);

	if (data->busy != 0)
		{
		shm_close(&shm);
		return FALSE;
		}

	sensor = &(data->sensors[index]);
	*value = (gfloat)sensor->value;

	shm_close(&shm);
	return TRUE;
}

static gboolean
gkrellm_sys_sensors_gpuz_init(void)
	{
	ShmData             shm;
	GPUZ_SH_MEM        *data;
	guint               i;
	GPUZ_SENSOR_RECORD *sensor;
	gint                sensor_type;
	gchar              *sensor_name;

	if (!shm_open_or_start_app(&shm, GPUZ_SHM_NAME, GPUZ_EXE_NAME))
		return FALSE;
	data = (GPUZ_SH_MEM*)(shm.data);

	gkrellm_debug(DEBUG_SYSDEP,
			"Found GPU-Z shared memory area version %d, busy %d",
			data->version, data->busy);

	if (data->version != 1)
		{
		g_warning("Unexpected GPU-Z shared memory area version %u, "
				"skipping GPU-Z sensors.\n", data->version);
		shm_close(&shm);
		return FALSE;
		}

	/* TODO: wait for data->busy == 0? */

	for (i = 0; i < GPUZ_MAX_RECORDS; ++i)
		{
		sensor = &(data->sensors[i]);

		if (!sensor->name || sensor->name[0] == 0)
			{
			gkrellm_debug(DEBUG_SYSDEP,	"GPU-Z sensor %u has no name,"
					" assuming end of valid sensor information\n", i);
			break;
			}

		sensor_name = g_utf16_to_utf8(sensor->name, -1, NULL, NULL, NULL);

	    if (gpuz_sensor_unit_to_sensor_type(sensor->unit, &sensor_type))
			{
			gkrellm_debug(DEBUG_SYSDEP,
					"GPU-Z sensor %u \"%s\" has type %d, adding\n",
					i, sensor_name, sensor_type);
			gpuz_sensor_add(i, sensor_type, sensor_name);
			}
		else
			{
			gkrellm_debug(DEBUG_SYSDEP,
					"GPU-Z sensor %u \"%s\" has unsupported unit, skipping\n",
					i, sensor_name);
			}

		g_free(sensor_name);
		}

	shm_close(&shm);
	return TRUE;
	}
