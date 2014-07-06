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

#include <windows.h>

static
HANDLE gkrellm_sys_sensors_open_shm_helper(const wchar_t *shm_name,
		const gchar *app_name)
{
	HANDLE hData = NULL;
	gboolean ret;
	GError *err = NULL;

	// Try to open shm-file and return if successful
	hData = OpenFileMappingW(FILE_MAP_READ, FALSE, shm_name);
	if (hData != 0)
		return hData;

	// shm-file could not be opened, try to start sensor-app
	ret = g_spawn_command_line_async(app_name, &err);
	if (!ret && err)
		{
		g_warning("Could not start sensor-app %s: %s\n",
				app_name, err->message);
		g_error_free(err);
		}
	else
		{
		gkrellm_debug(DEBUG_SYSDEP,
				"Started sensor-app %s, waiting for it to initialize\n",
				app_name);
		// 5 second wait to allow sensor-app init
		g_usleep(5 * G_USEC_PER_SEC);
		// Retry open of shm-file
		hData = OpenFileMappingW(FILE_MAP_READ, FALSE, shm_name);
		}
	return hData;
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
//  Licence     : Cardware. (Send me a note/email if you find it usefull.)
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
	HANDLE          hData;
	MBMSharedData   *pData;
	MBMSharedSensor *pSensor;
	gboolean        ret = FALSE;
	SensorType      st = gkrellm_sensor_type_to_mbm(sensor_type);

	if (st == stUnknown || sensor_id < 0 || sensor_id > 99)
		return FALSE; // id out of range

	hData = OpenFileMappingW(FILE_MAP_READ, FALSE, MBM_SHM_NAME);
	if (hData == 0)
		return FALSE;
	pData = (MBMSharedData *)MapViewOfFile(hData, FILE_MAP_READ, 0, 0, 0);
	if (pData != NULL)
		{
		gkrellm_debug(DEBUG_SYSDEP, "Fetching sensor value %d from MBM\n", sensor_id);
		pSensor = &(pData->sdSensor[sensor_id]);
		if (pSensor->ssType == st)
			{
				*value = pSensor->ssCurrent;
				ret = TRUE;
			}
		UnmapViewOfFile(pData);
		}
	CloseHandle(hData);
	return ret;
	}

static gboolean
gkrellm_sys_sensors_mbm_init(void)
{
	HANDLE          hData;
	MBMSharedData   *pData;
	MBMSharedSensor *pSensor;
	gboolean        ret = FALSE;
	gint i, sensorCount, tempCount, voltCount, fanCount;
	gchar *id_name;

	hData = gkrellm_sys_sensors_open_shm_helper(MBM_SHM_NAME, MBM_EXE_NAME);
	if (hData == 0)
		return FALSE;
	gkrellm_debug(DEBUG_SYSDEP, "Mapping MBM SHM file\n");
	pData = (MBMSharedData *)MapViewOfFile(hData, FILE_MAP_READ, 0, 0, 0);
	if (pData != NULL)
		{
		ret = TRUE; // MBM available, return TRUE

		sensorCount = 0;
		for (i = 0; i < 9; i++)
			sensorCount += pData->sdIndex[i].Count;

		tempCount = 0;
		voltCount = 0;
		fanCount = 0;
		for (i = 0; i < sensorCount; i++)
			{
			pSensor = &(pData->sdSensor[i]);
			switch (pSensor->ssType)
				{
				case stTemperature:
					id_name = g_strdup_printf("mbm-temp-%d", tempCount);

					gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE, /*sensor_path*/NULL,
						/*id_name*/id_name, /*id*/i, /*iodev*/0,
						/*inter*/MBM_INTERFACE, /*factor*/1, /*offset*/0,
						/*vref*/NULL, /*default_label*/(gchar *)pSensor->ssName);

		            g_free(id_name);
					++tempCount;
					break;
				case stVoltage:
					id_name = g_strdup_printf("mbm-volt-%d", voltCount);

					gkrellm_sensors_add_sensor(SENSOR_VOLTAGE, /*sensor_path*/NULL,
						/*id_name*/id_name, /*id*/i, /*iodev*/0,
						/*inter*/MBM_INTERFACE, /*factor*/1, /*offset*/0,
						/*vref*/NULL, /*default_label*/(gchar *)pSensor->ssName);

		            g_free(id_name);
					++voltCount;
					break;
				case stFan:
					id_name = g_strdup_printf("mbm-fan-%d", fanCount);

					gkrellm_sensors_add_sensor(SENSOR_FAN, /*sensor_path*/NULL,
						/*id_name*/id_name, /*id*/i, /*iodev*/0,
						/*inter*/MBM_INTERFACE, /*factor*/1, /*offset*/0,
						/*vref*/NULL, /*default_label*/(gchar *)pSensor->ssName);

		            g_free(id_name);
		            fanCount++;
					break;
				} /* switch() */
			} /* for() */

		UnmapViewOfFile(pData);
		}
	CloseHandle(hData);
	return ret;
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
	HANDLE          hData;
	SFSharedMemory *pData;
	gboolean        ret = FALSE;

	if (sensor_id < 0 || sensor_id > 31)
		return FALSE; // id out of range

	hData = OpenFileMappingW(FILE_MAP_READ, FALSE, SPEEDFAN_SHM_NAME);
	if (hData == 0)
		return FALSE;
	pData = (SFSharedMemory *)MapViewOfFile(hData, FILE_MAP_READ, 0, 0, 0);
	if (pData != NULL)
		{
		gkrellm_debug(DEBUG_SYSDEP, "Fetching sensor value %d from SpeedFan\n", sensor_id);
		switch(sensor_type)
			{
			case SENSOR_TEMPERATURE:
				if (sensor_id < pData->NumTemps)
					{
					*value = pData->temps[sensor_id] / 100.0;
					ret = TRUE;
					}
				break;
			case SENSOR_VOLTAGE:
				if (sensor_id < pData->NumVolts)
					{
					*value = pData->volts[sensor_id] / 100.0;
					ret = TRUE;
					}
				break;
			case SENSOR_FAN:
				if (sensor_id < pData->NumFans)
					{
					*value = pData->fans[sensor_id];
					ret = TRUE;
					}
				break;
			}
		UnmapViewOfFile(pData);
		}
	CloseHandle(hData);
	return ret;
	}

static gboolean
gkrellm_sys_sensors_sf_init(void)
	{
	HANDLE          hData;
	SFSharedMemory *pData;
	gboolean       ret = FALSE;
	gint           i;
	gchar          *id_name;
	gchar          *default_label;

	hData = gkrellm_sys_sensors_open_shm_helper(SPEEDFAN_SHM_NAME, SPEEDFAN_EXE_NAME);
	if (hData == 0)
		return FALSE;

	gkrellm_debug(DEBUG_SYSDEP, "Mapping SpeedFan SHM file\n");
	pData = (SFSharedMemory *)MapViewOfFile(hData, FILE_MAP_READ, 0, 0, 0);
	if (pData != NULL)
		{
		ret = TRUE; // Mark SpeedFan as available

		gkrellm_debug(DEBUG_SYSDEP, "Enumerating %hu temps, %hu voltages and %hu fans\n",
					pData->NumTemps, pData->NumVolts, pData->NumFans);

		for (i = 0; i < pData->NumTemps; i++)
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

		for (i = 0; i < pData->NumVolts; i++)
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

		for (i = 0; i < pData->NumFans; i++)
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

		UnmapViewOfFile(pData);
		}
	CloseHandle(hData);
	return ret;
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
	HANDLE          hData;
	CORE_TEMP_SHARED_DATA *pData;
	gboolean        ret = FALSE;
	guint           temp_index;

	if (core_index < 0 || core_index > 255 || cpu_index < 0 || cpu_index > 127)
		return FALSE; // core or cpu index out of range

	hData = OpenFileMappingW(FILE_MAP_READ, FALSE, CORE_TEMP_SHM_NAME);
	if (hData == 0)
		return FALSE;
	pData = (CORE_TEMP_SHARED_DATA *)MapViewOfFile(hData, FILE_MAP_READ, 0, 0, 0);
	if (pData != NULL)
		{
		gkrellm_debug(DEBUG_SYSDEP,
			"Fetching temp for core %d, cpu %d from CoreTemp\n", core_index,
			cpu_index);

		// 'core index' + ( 'cpu index' * 'number of cores per cpu' )
		temp_index = core_index + (cpu_index * pData->uiCoreCnt);

		// make absolute value from delta
		if (pData->ucDeltaToTjMax == '\1')
			*temp = pData->uiTjMax[cpu_index] - pData->fTemp[temp_index];
		else
			*temp = pData->fTemp[temp_index];

		// Convert Fahrenheit to Celsius
		if (pData->ucFahrenheit == '\1')
			*temp = (*temp - 32) * 5 / 9;

		UnmapViewOfFile(pData);
		}
	CloseHandle(hData);
	return ret;
	}

static gboolean
gkrellm_sys_sensors_ct_init(void)
	{
	HANDLE          hData;
	CORE_TEMP_SHARED_DATA *pData;
	gboolean       ret = FALSE;
	guint          uiCpu;
	guint          uiCore;
	gchar          *id_name;
	gchar          *default_label;

	hData = gkrellm_sys_sensors_open_shm_helper(CORE_TEMP_SHM_NAME, CORE_TEMP_EXE_NAME);
	if (hData == 0)
		return FALSE;
	gkrellm_debug(DEBUG_SYSDEP, "Mapping CoreTemp SHM file\n");
	pData = (CORE_TEMP_SHARED_DATA *)MapViewOfFile(hData, FILE_MAP_READ, 0, 0, 0);
	if (pData != NULL)
		{
		ret = TRUE; // Mark CoreTemp as available

		for (uiCpu = 0; uiCpu < pData->uiCPUCnt; uiCpu++)
			{
			for (uiCore = 0; uiCore < pData->uiCoreCnt; uiCore++)
				{
				id_name = g_strdup_printf("coretemp-cpu%u-core%u", uiCpu, uiCore);
				if (pData->uiCPUCnt == 1)
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
		UnmapViewOfFile(pData);
		}
	CloseHandle(hData);
	return ret;
	}

