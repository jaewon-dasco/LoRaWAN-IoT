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

// Export 상태머신 변수
uint8_t  MiSerial_ExportStep = 0;
int32_t  MiSerial_ExportAddr = 0;
int32_t  MiSerial_ExportCount = 0;
int32_t  MiSerial_ExportSeq = 0;
int32_t  MiSerial_ExportErrors = 0;

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
					oSerial_Printf(&MiSerial_Handler, ",\r\n"); //이전데이터 구분기호
				}

				oSerial_Printf(&MiSerial_Handler, "{");
				oSerial_Printf(&MiSerial_Handler, "\"Channel\" : \"%d\", ", (int)i+1);
				oSerial_Printf(&MiSerial_Handler, "\"SensorType\" : \"%d\", ", (int)pParameter->ChannelConfig[i].TypeOfSensor);
				oSerial_Printf(&MiSerial_Handler, "\"WarmupTime\" : \"%d\", ", (int)pParameter->ChannelConfig[i].WarmupTime);
				oSerial_Printf(&MiSerial_Handler, "\"SupplySource\" : \"%d\", ", (int)pParameter->ChannelConfig[i].SupplySource);
				oSerial_Printf(&MiSerial_Handler, "\"RetryCount\" : \"%d\", ", (int)pParameter->ChannelConfig[i].RetryCount);
				oSerial_Printf(&MiSerial_Handler, "\"RetryInterval\" : \"%d\", ", (int)pParameter->ChannelConfig[i].RetryInterval);
				oSerial_Printf(&MiSerial_Handler, "\"ErrorTolerance\" : \"%.3f\"", pParameter->ChannelConfig[i].ErrorTolerance);

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
		oSerial_Printf(&MiSerial_Handler, "\"ErrorTolerance\" : \"%.3f\"\r\n", pParameter->ChannelConfig[i].ErrorTolerance);
	}

	oSerial_Printf(&MiSerial_Handler, "};\r\n");
}

oResult_t MiSerial_SetChannelConfig(char *Message, IoTParameter_t *pParameter, uint8_t Channel)
{
	int ErrorCount = 0;
	IoTChannelConfig_t *pChannelConfig = NULL;
	char StrVal[125];
	char *endptr;
	long lVal;

	if(Channel > 0 && Channel <= MIIOT_CHANNEL_MAXCOUNT && strstr(Message, "\"Type\" : \"ChannelConfig\"") != NULL)
	{
		Channel -= 1;
		pChannelConfig = &pParameter->ChannelConfig[Channel];

		if(oJSON_GetValue(Message, "\"SensorType\"", StrVal, sizeof(StrVal))){
			lVal = strtol(StrVal, &endptr, 10);
			if(endptr != StrVal){
				pChannelConfig->TypeOfSensor = (IoTSensorType_t)(lVal & 0xFF);

				if(pChannelConfig->TypeOfSensor == IoTSensorType_Differential){
					if(Channel % 3 == 0 && (Channel+1) < MIIOT_CHANNEL_MAXCOUNT){
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
				pChannelConfig->WarmupTime = (uint16_t)lVal;
			}
			else{
				ErrorCount++;
			}
		}

		if(oJSON_GetValue(Message, "\"SupplySource\"", StrVal, sizeof(StrVal))){
			lVal = strtol(StrVal, &endptr, 10);
			if(endptr != StrVal){
				pChannelConfig->SupplySource = (uint8_t)lVal;
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
	if(pPacket == NULL || pPacket->DLC == 0 || pPacket->DLC < MIIOT_IOTDATA_SIZE_TIME || pPacket->TypeOfData != IoTDataType_Tilt){
		return;
	}

	oSerial_Printf(&MiSerial_Handler, "{\r\n");
	oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"SensorData\",\r\n");
	oSerial_Printf(&MiSerial_Handler, "\"Time\" : \"%04d-%02d-%02d %02d:%02d:%02d\",\r\n", (int)(((IoTDateAndTime_t *)&pPacket->Frame)->Year+2000), (int)((IoTDateAndTime_t *)&pPacket->Frame)->Month, (int)((IoTDateAndTime_t *)&pPacket->Frame)->Day, (int)((IoTDateAndTime_t *)&pPacket->Frame)->Hour, (int)((IoTDateAndTime_t *)&pPacket->Frame)->Minute, (int)((IoTDateAndTime_t *)&pPacket->Frame)->Second);

	oSerial_Printf(&MiSerial_Handler, "\"Temp\" : \"%.2f\",\r\n", MIIOT_DATA_DECODE_TEMP(((IoTDataTilt_t *)&pPacket->Frame)->Temperature));
	oSerial_Printf(&MiSerial_Handler, "\"Tilt\" : [\r\n");

	oSerial_Printf(&MiSerial_Handler, "{");
	oSerial_Printf(&MiSerial_Handler, "\"X\" : \"%.3f\", ", MIIOT_DATA_DECODE_ANGLE(((IoTDataTilt_t *)&pPacket->Frame)->Sensor.AxisX));
	oSerial_Printf(&MiSerial_Handler, "\"Y\" : \"%.3f\"", MIIOT_DATA_DECODE_ANGLE(((IoTDataTilt_t *)&pPacket->Frame)->Sensor.AxisY));
	oSerial_Printf(&MiSerial_Handler, "}\r\n");

	oSerial_Printf(&MiSerial_Handler, "]\r\n");
	oSerial_Printf(&MiSerial_Handler, "};\r\n");
}

oResult_t MiSerial_Specific(char *Message)
{
	if(strstr(Message, "Stop/Export") != NULL){
		if(MiSerial_ExportStep > 0){
			oSerial_Printf(&MiSerial_Handler, "{\"Type\":\"ExportDone\",\"Total\":\"%d\",\"Errors\":\"%d\",\"Stopped\":\"1\"};\r\n",
				(int)MiSerial_ExportSeq, (int)MiSerial_ExportErrors);
			MiSerial_ExportStep = 0;
		}
		MiSerial_PrintResponse("Stop/Export", "OK");
		return RESULT_OK;
	}

	return RESULT_ERROR;
}

void MiSerial_Process()
{
	oResult_t result;
	uint32_t Analog;

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

	if(MiSerial_SamplingADCsTrig)
	{
		if((result = Measurement_Supply(15)) == RESULT_OK){
			Analog = (int32_t)GPIOs.ADC.SystemSupply;
		}

		if(result != RESULT_RUN){
			oSerial_Printf(&MiSerial_Handler, "{\r\n");
			oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"SamplingADCs\",\r\n");

			if(result == RESULT_OK){
				oSerial_Printf(&MiSerial_Handler, "\"ADCs[%d]\" : \"%d\"", (int)0, (int)Analog);
			}

			oSerial_Printf(&MiSerial_Handler, "\r\n};\r\n");

			MiSerial_SamplingADCsTrig = 0;
		}
	}
}

/* History

2026-06-26 | v0.1
	- baseline (Mi_Serial_SIA100_SD.c)
*/
