/*
 * Mi_Serial.c
 *
 *  Created on: Feb 28, 2025
 *      Author: JONE
 */
#include "stdarg.h"
#include "stdio.h"
#include "stdlib.h"
#include "Mi_SoftwareRevision.h"
#include "Mi_LoRa.h"
#include "Mi_Measurement.h"
#include "Mi_Main.h"
#include "Mi_Storage.h"
#include "Mi_Serial.h"
#include "ONE_Serial.h"

uint8_t MiSerial_IsScan = 0;

void MiSerial_PrintChannelConfig(int32_t Channel, IoTParameter_t *pParameter)
{
	int i;
	int cofigcount = 0;

	if(pParameter == NULL || !GPIOs.DI.UsbConnected){
		return;
	}

	if(Channel > MEASUREMENT_CHANNEL_MAXCOUNT){
		MiSerial_PrintResponse("Get/ChannelConfig", "ERROR");
		return;
	}

	oSerial_Printf(&MiSerial_Handler, "{\r\n");
	oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"ChannelConfig\",\r\n");

	if(Channel <= 0){
		//Count
		for(i=0; i<MEASUREMENT_CHANNEL_MAXCOUNT; i++){
			if(pParameter->ChannelConfig[i].TypeOfSensor){
				cofigcount++;
			}
		}

		oSerial_Printf(&MiSerial_Handler, "\"Count\" : \"%d\",\r\n", cofigcount);

		//Data
		cofigcount = 0;
		oSerial_Printf(&MiSerial_Handler, "\"Data\" : [\r\n");

		for(i=0; i<MEASUREMENT_CHANNEL_MAXCOUNT; i++){
			if(pParameter->ChannelConfig[i].TypeOfSensor){
				if(cofigcount){
					oSerial_Printf(&MiSerial_Handler, ",\r\n");
				}

				oSerial_Printf(&MiSerial_Handler, "{");
				oSerial_Printf(&MiSerial_Handler, "\"Channel\" : \"%d\", ", (int)i+1);
				oSerial_Printf(&MiSerial_Handler, "\"SensorType\" : \"%d\", ", (int)pParameter->ChannelConfig[i].TypeOfSensor);
				oSerial_Printf(&MiSerial_Handler, "\"WarmupTime\" : \"%d\", ", (int)pParameter->ChannelConfig[i].WarmupTime);
				oSerial_Printf(&MiSerial_Handler, "\"SupplySource\" : \"%d\", ", (int)pParameter->ChannelConfig[i].SupplySource);
				oSerial_Printf(&MiSerial_Handler, "\"RetryCount\" : \"%d\", ", (int)pParameter->ChannelConfig[i].RetryCount);
				oSerial_Printf(&MiSerial_Handler, "\"RetryInterval\" : \"%d\", ", (int)pParameter->ChannelConfig[i].RetryInterval);
				oSerial_Printf(&MiSerial_Handler, "\"ErrorTolerance\" : \"%.3f\", ", pParameter->ChannelConfig[i].ErrorTolerance);
				oSerial_Printf(&MiSerial_Handler, "\"SensorId\" : \"%02X\", ", (int)(pParameter->ChannelConfig[i].Properties.Array.SensorId));
				oSerial_Printf(&MiSerial_Handler, "\"SensorCount\" : \"%d\", ", (int)(pParameter->ChannelConfig[i].Properties.Array.CountOfSensor));
				oSerial_Printf(&MiSerial_Handler, "\"DisableSensorBit\" : \"%08X\"", (int)(pParameter->ChannelConfig[i].Properties.Array.DisableSensorBit));

				oSerial_Printf(&MiSerial_Handler, "}");
				cofigcount++;
			}
		}

		oSerial_Printf(&MiSerial_Handler, "]\r\n");
	}
	else{
		i = Channel-1;
		oSerial_Printf(&MiSerial_Handler, "\"Channel\" : \"%d\",\r\n", (int)i+1);
		oSerial_Printf(&MiSerial_Handler, "\"SensorType\" : \"%d\",\r\n", (int)pParameter->ChannelConfig[i].TypeOfSensor);
		oSerial_Printf(&MiSerial_Handler, "\"WarmupTime\" : \"%d\",\r\n", (int)pParameter->ChannelConfig[i].WarmupTime);
		oSerial_Printf(&MiSerial_Handler, "\"SupplySource\" : \"%d\",\r\n", (int)pParameter->ChannelConfig[i].SupplySource);
		oSerial_Printf(&MiSerial_Handler, "\"RetryCount\" : \"%d\",\r\n", (int)pParameter->ChannelConfig[i].RetryCount);
		oSerial_Printf(&MiSerial_Handler, "\"RetryInterval\" : \"%d\",\r\n", (int)pParameter->ChannelConfig[i].RetryInterval);
		oSerial_Printf(&MiSerial_Handler, "\"ErrorTolerance\" : \"%.3f\",\r\n", pParameter->ChannelConfig[i].ErrorTolerance);
		oSerial_Printf(&MiSerial_Handler, "\"SensorId\" : \"%02X\",\r\n", (int)(pParameter->ChannelConfig[i].Properties.Array.SensorId));
		oSerial_Printf(&MiSerial_Handler, "\"SensorCount\" : \"%d\",\r\n", (int)(pParameter->ChannelConfig[i].Properties.Array.CountOfSensor));
		oSerial_Printf(&MiSerial_Handler, "\"DisableSensorBit\" : \"%08X\"\r\n", (int)(pParameter->ChannelConfig[i].Properties.Array.DisableSensorBit));
	}

	oSerial_Printf(&MiSerial_Handler, "};\r\n");
}

oResult_t MiSerial_SetChannelConfig(char *Message, IoTParameter_t *pParameter, uint8_t Channel)
{
	int ErrorCount = 0;
	IoTChannelConfig_t *pChannelConfig = NULL;
	long lVal;
	long long llVal;
	float fVal;
	char StrVal[125];
	char *endptr;

	if(Channel > 0 && strstr(Message, "\"Type\" : \"ChannelConfig\"") != NULL)
	{
		Channel -= 1;
		pChannelConfig = &pParameter->ChannelConfig[Channel];

		if(oJSON_GetValue(Message, "\"SensorType\"", StrVal, sizeof(StrVal))){
			lVal = strtol(StrVal, &endptr, 10);
			if(endptr != StrVal){
				pChannelConfig->TypeOfSensor = (IoTSensorType_t)(lVal & 0xFF);

				if(pChannelConfig->TypeOfSensor == IoTSensorType_Differential){
					if(Channel % 3 == 0){
						pParameter->ChannelConfig[Channel+1].TypeOfSensor = 0;
					}
					else if(Channel % 3 != 0){
						pParameter->ChannelConfig[Channel].TypeOfSensor = 0;
					}
				}
			}
			else{
				ErrorCount++;
			}
		}

		if(oJSON_GetValue(Message, "\"WarmupTime\"", StrVal, sizeof(StrVal))){
			lVal = strtol(StrVal, &endptr, 10);
			if(endptr != StrVal){
				pChannelConfig->WarmupTime = MATH_LIMIT((uint16_t)lVal, 0, SECOND_TO_MS(30));
			}
			else{
				ErrorCount++;
			}
		}

		if(oJSON_GetValue(Message, "\"SupplySource\"", StrVal, sizeof(StrVal))){
			lVal = strtol(StrVal, &endptr, 10);
			if(endptr != StrVal){
				pChannelConfig->SupplySource = MATH_LIMIT((uint8_t)lVal, 0, 2);
			}
			else{
				ErrorCount++;
			}
		}

		if(oJSON_GetValue(Message, "\"RetryCount\"", StrVal, sizeof(StrVal))){
			lVal = strtol(StrVal, &endptr, 10);
			if(endptr != StrVal){
				pChannelConfig->RetryCount = MATH_LIMIT((uint8_t)lVal, 0, 254);
			}
			else{
				ErrorCount++;
			}
		}

		if(oJSON_GetValue(Message, "\"RetryInterval\"", StrVal, sizeof(StrVal))){
			lVal = strtol(StrVal, &endptr, 10);
			if(endptr != StrVal){
				pChannelConfig->RetryInterval = MATH_LIMIT((uint8_t)lVal, 0, 254);
			}
			else{
				ErrorCount++;
			}
		}

		if(oJSON_GetValue(Message, "\"ErrorTolerance\"", StrVal, sizeof(StrVal))){
			fVal = strtof(StrVal, &endptr);
			if(endptr != StrVal){
				pChannelConfig->ErrorTolerance = fVal;
			}
			else{
				ErrorCount++;
			}
		}

		if(oJSON_GetValue(Message, "\"SensorId\"", StrVal, sizeof(StrVal))){
			lVal = strtol(StrVal, &endptr, 16);
			if(endptr != StrVal){
				if(lVal >= 0 && lVal <= 255){
					pChannelConfig->Properties.Array.SensorId = (uint16_t)(lVal);
				}
				else{
					ErrorCount++;
				}
			}
			else{
				ErrorCount++;
			}
		}

		if(oJSON_GetValue(Message, "\"SensorCount\"", StrVal, sizeof(StrVal))){
			lVal = strtol(StrVal, &endptr, 10);
			if(endptr != StrVal){
				if(lVal >= 0){
					pChannelConfig->Properties.Array.CountOfSensor = MATH_LIMIT((uint16_t)(lVal), 0, 60);
				}
				else{
					ErrorCount++;
				}
			}
			else{
				ErrorCount++;
			}
		}

		if(oJSON_GetValue(Message, "\"DisableSensorBit\"", StrVal, sizeof(StrVal))){
			llVal = strtoull(StrVal, &endptr, 16);
			if(endptr != StrVal){
				pChannelConfig->Properties.Array.DisableSensorBit = llVal;
			}
			else{
				ErrorCount++;
			}
		}

		if(pChannelConfig->SupplySource == 0){
			pChannelConfig->SupplySource = Channel;
		}
	}

	return ErrorCount == 0 ? RESULT_OK : RESULT_ERROR;
}


void MiSerial_PrintSensorData(IoT_DataPacket_t *pPacket)
{
	int i;
	uint32_t Count;

	if(pPacket == NULL || pPacket->DLC == 0 || pPacket->DLC < MIIOT_IOTDATA_SIZE_TIME || !GPIOs.DI.UsbConnected){
		return;
	}

	oSerial_Printf(&MiSerial_Handler, "{\r\n");
	oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"SensorData\",\r\n");
	oSerial_Printf(&MiSerial_Handler, "\"Time\" : \"%04d-%02d-%02d %02d:%02d:%02d\",\r\n", (int)(((IoTDateAndTime_t *)&pPacket->Frame)->Year+2000), (int)((IoTDateAndTime_t *)&pPacket->Frame)->Month, (int)((IoTDateAndTime_t *)&pPacket->Frame)->Day, (int)((IoTDateAndTime_t *)&pPacket->Frame)->Hour, (int)((IoTDateAndTime_t *)&pPacket->Frame)->Minute, (int)((IoTDateAndTime_t *)&pPacket->Frame)->Second);

	switch(pPacket->TypeOfData)
	{
		case IoTDataType_ArrayDualTilt:
			Count = (pPacket->DLC - MIIOT_IOTDATA_SIZE_ARRAYDUALTILT(0)) / MIIOT_SENSORDATA_SIZE_TILT_DUAL;

			oSerial_Printf(&MiSerial_Handler, "\"Channel\" : \"%d\",\r\n", (int)((IoTDataArrayDualTilt_t *)&pPacket->Frame)->Channel);
			oSerial_Printf(&MiSerial_Handler, "\"Temp\" : \"%.2f\",\r\n", MIIOT_DATA_DECODE_TEMP(((IoTDataArrayDualTilt_t *)&pPacket->Frame)->Temperature));
			oSerial_Printf(&MiSerial_Handler, "\"ArrayDualTilt\" : [\r\n");

			for(i=0; i<Count; i++){
				oSerial_Printf(&MiSerial_Handler, "{");
				oSerial_Printf(&MiSerial_Handler, "\"X\" : \"%.3f\", ", MIIOT_DATA_DECODE_ANGLE(((IoTDataArrayDualTilt_t *)&pPacket->Frame)->Sensor[i].AxisX));
				oSerial_Printf(&MiSerial_Handler, "\"Y\" : \"%.3f\"", MIIOT_DATA_DECODE_ANGLE(((IoTDataArrayDualTilt_t *)&pPacket->Frame)->Sensor[i].AxisY));
				oSerial_Printf(&MiSerial_Handler, "}");

				if(i<Count-1){
					oSerial_Printf(&MiSerial_Handler, ",");
				}

				oSerial_Printf(&MiSerial_Handler, "\r\n");
			}
			break;

		case IoTDataType_ArraySingleTilt:
			Count = (pPacket->DLC - MIIOT_IOTDATA_SIZE_ARRAYSINGLETILT(0)) / MIIOT_SENSORDATA_SIZE_TILT_SINGLE;

			oSerial_Printf(&MiSerial_Handler, "\"Channel\" : \"%d\",\r\n", (int)((IoTDataArraySingleTilt_t *)&pPacket->Frame)->Channel);
			oSerial_Printf(&MiSerial_Handler, "\"Temp\" : \"%.2f\",\r\n", MIIOT_DATA_DECODE_TEMP(((IoTDataArraySingleTilt_t *)&pPacket->Frame)->Temperature));
			oSerial_Printf(&MiSerial_Handler, "\"ArraySingleTilt\" : [\r\n");

			for(i=0; i<Count; i++){
				oSerial_Printf(&MiSerial_Handler, "{");
				oSerial_Printf(&MiSerial_Handler, "\"Angle\" : \"%.3f\"", MIIOT_DATA_DECODE_ANGLE(((IoTDataArraySingleTilt_t *)&pPacket->Frame)->Sensor[i].Axis));
				oSerial_Printf(&MiSerial_Handler, "}");

				if(i<Count-1){
					oSerial_Printf(&MiSerial_Handler, ",");
				}

				oSerial_Printf(&MiSerial_Handler, "\r\n");
			}
			break;

		default:
			break;
	}

	oSerial_Printf(&MiSerial_Handler, "]\r\n");
	oSerial_Printf(&MiSerial_Handler, "};\r\n");
}

oResult_t MiSerial_Specific(char *Message)
{
	oResult_t result = RESULT_ERROR;

	if(strstr(Message, "Get/Scan") != NULL){
		MiSerial_PrintResponse("Get/Scan", "OK");
		MiSerial_IsScan = 1;
		result = RESULT_OK;
	}

	return result;
}

void MiSerial_Process()
{
	static uint32_t ScanCh = 0;
	oResult_t result = RESULT_ERROR;
	uint8_t IsFirst;
	uint32_t i;
	uint32_t Count = 0;

	if(MiStorage_ParameterSaveCmd != 0){
		if(MiStorage_ParameterSaveCmd == 2){
			MiSerial_PrintResponse("Save", "OK");
			MiStorage_ParameterSaveCmd = 0;
		}
		else if(MiStorage_ParameterSaveCmd == -1){
			MiSerial_PrintResponse("Save", "ERROR");
			MiStorage_ParameterSaveCmd = 0;
		}
	}

	if(MiSerial_IsScan && ScanCh < 2){
		if(MiIoT_Parameter.ChannelConfig[ScanCh].TypeOfSensor == IoTSensorType_NULL){
			ScanCh++;
		}
		else if((result=Measurement_Scan(ScanCh+1, 1, 255, (uint8_t *)&IPIRequestData, 8, 1)) != RESULT_RUN){
			oSerial_Printf(&MiSerial_Handler, "{\r\n");
			oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"Scan\",\r\n");
			oSerial_Printf(&MiSerial_Handler, "\"Channel\" : \"%d\",\r\n", (int)ScanCh+1);

			if(result == RESULT_OK)
			{
				IsFirst = 1;

				oSerial_Printf(&MiSerial_Handler, "\"IDs\" : [");

				for(i=0; i<MEASUREMENT_NODE_MAXCOUNT; i++){
					if(Measure_ScanIdList[i] != 0){
						Count++;

						if(!IsFirst){
							oSerial_Printf(&MiSerial_Handler, ",");
						}

						oSerial_Printf(&MiSerial_Handler, "\"0x%02X\"", (int)Measure_ScanIdList[i]);
						IsFirst = 0;
					}
				}

				oSerial_Printf(&MiSerial_Handler, "],\r\n");
				oSerial_Printf(&MiSerial_Handler, "\"Count\" : \"%d\"\r\n", (int)Count);
			}
			else{
				oSerial_Printf(&MiSerial_Handler, "\"Result\" : \"ERROR\"\r\n");
			}

			oSerial_Printf(&MiSerial_Handler, "};\r\n");
			ScanCh++;
		}
	}
	else{
		ScanCh = 0;
		MiSerial_IsScan = 0;
	}
}
