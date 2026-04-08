/*
 * Mi_Serial.c
 *
 *  Created on: Feb 28, 2025
 *      Author: JONE
 */
#include "stdarg.h"
#include "stdio.h"
#include "stdlib.h"
#include "ONE_Serial.h"
#include "Mi_SoftwareRevision.h"
#include "Mi_LoRa.h"
#include "Mi_Measurement.h"
#include "Mi_Main.h"
#include "Mi_Storage.h"
#include "Mi_Serial.h"

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

				switch(pParameter->ChannelConfig[i].TypeOfSensor)
				{
					default:
						break;
					case IoTSensorType_Differential:
					case IoTSensorType_mV:
					case IoTSensorType_mA:
						oSerial_Printf(&MiSerial_Handler, "\"WarmupTime\" : \"%s\", ", pParameter->ChannelConfig[i].WarmupTime);
						break;
					case IoTSensorType_Thermistor:
						oSerial_Printf(&MiSerial_Handler, "\"Mode\" : \"%s\", ", pParameter->ChannelConfig[i].Properties.Thermistor.Mode);
						oSerial_Printf(&MiSerial_Handler, "\"Min\" : \"%.1f\", ", (float)(pParameter->ChannelConfig[i].Properties.Thermistor.Min));
						oSerial_Printf(&MiSerial_Handler, "\"Max\" : \"%.1f\", ",  (float)(pParameter->ChannelConfig[i].Properties.Thermistor.Max));
						break;
					case IoTSensorType_VibratingWire:
						oSerial_Printf(&MiSerial_Handler, "\"Mode\" : \"%s\", ", pParameter->ChannelConfig[i].Properties.VibrationWrie.Mode);
						oSerial_Printf(&MiSerial_Handler, "\"StartFrequency\" : \"%.1f\", ", (float)(pParameter->ChannelConfig[i].Properties.VibrationWrie.StartFreq));
						oSerial_Printf(&MiSerial_Handler, "\"EndFrequency\" : \"%.1f\", ",  (float)(pParameter->ChannelConfig[i].Properties.VibrationWrie.EndFreq));
						break;
				}

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

		switch(pParameter->ChannelConfig[i].TypeOfSensor)
		{
			default:
				break;
			case IoTSensorType_Differential:
			case IoTSensorType_mV:
			case IoTSensorType_mA:
				oSerial_Printf(&MiSerial_Handler, "\"WarmupTime\" : \"%s\",\r\n", pParameter->ChannelConfig[i].WarmupTime);
				break;
			case IoTSensorType_Thermistor:
				oSerial_Printf(&MiSerial_Handler, "\"Mode\" : \"%s\",\r\n", pParameter->ChannelConfig[i].Properties.Thermistor.Mode);
				oSerial_Printf(&MiSerial_Handler, "\"Min\" : \"%.1f\",\r\n", (float)(pParameter->ChannelConfig[i].Properties.Thermistor.Min));
				oSerial_Printf(&MiSerial_Handler, "\"Max\" : \"%.1f\",\r\n",  (float)(pParameter->ChannelConfig[i].Properties.Thermistor.Max));
				break;
			case IoTSensorType_VibratingWire:
				oSerial_Printf(&MiSerial_Handler, "\"Mode\" : \"%s\",\r\n", pParameter->ChannelConfig[i].Properties.VibrationWrie.Mode);
				oSerial_Printf(&MiSerial_Handler, "\"StartFrequency\" : \"%.1f\",\r\n", (float)(pParameter->ChannelConfig[i].Properties.VibrationWrie.StartFreq));
				oSerial_Printf(&MiSerial_Handler, "\"EndFrequency\" : \"%.1f\",\r\n",  (float)(pParameter->ChannelConfig[i].Properties.VibrationWrie.EndFreq));
				break;
		}

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
	float fVal;
	long lVal;
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

		switch(pChannelConfig->TypeOfSensor)
		{
			default:
				break;
			case IoTSensorType_Thermistor:
				if(oJSON_GetValue(Message, "\"Mode\"", StrVal, sizeof(StrVal))){
					if(strlen(StrVal) > 0){
						sprintf(pChannelConfig->Properties.Thermistor.Mode, "%.6s", StrVal);
					}
					else{
						ErrorCount++;
					}
				}

				if(oJSON_GetValue(Message, "\"Min\"", StrVal, sizeof(StrVal))){
					fVal = strtof(StrVal, &endptr);
					if(endptr != StrVal){
						pChannelConfig->Properties.Thermistor.Min = fVal;
					}
					else{
						ErrorCount++;
					}
				}

				if(oJSON_GetValue(Message, "\"Max\"", StrVal, sizeof(StrVal))){
					fVal = strtof(StrVal, &endptr);
					if(endptr != StrVal){
						pChannelConfig->Properties.Thermistor.Max = fVal;
					}
					else{
						ErrorCount++;
					}
				}
				break;
			case IoTSensorType_VibratingWire:
				if(oJSON_GetValue(Message, "\"Mode\"", StrVal, sizeof(StrVal))){
					if(strlen(StrVal) > 0){
						sprintf(pChannelConfig->Properties.VibrationWrie.Mode, "%.6s", StrVal);
					}
					else{
						ErrorCount++;
					}
				}

				if(oJSON_GetValue(Message, "\"StartFrequency\"", StrVal, sizeof(StrVal))){
					fVal = strtof(StrVal, &endptr);  // 10진수 문자열 10진수 변환
					if(endptr != StrVal){
						pChannelConfig->Properties.VibrationWrie.StartFreq = fVal;
					}
					else{
						ErrorCount++;
					}
				}

				if(oJSON_GetValue(Message, "\"EndFrequency\"", StrVal, sizeof(StrVal))){
					fVal = strtof(StrVal, &endptr);  // 10진수 문자열 10진수 변환
					if(endptr != StrVal){
						pChannelConfig->Properties.VibrationWrie.EndFreq = fVal;
					}
					else{
						ErrorCount++;
					}
				}
				break;
		}
	}

	return ErrorCount == 0 ? RESULT_OK : RESULT_ERROR;
}

void MiSerial_PrintSensorData(IoT_DataPacket_t *pPacket)
{
	uint8_t comma = 0;
	int count;
	int i;

	if(pPacket == NULL || pPacket->TypeOfData != IoTDataType_DataArray){
		return;
	}

	IoTDataArray_t *pData = (IoTDataArray_t *)pPacket->Frame;

	count = MIIOT_PAYLOAD_TO_DATAARRAY_COUNT(pPacket->DLC);

	oSerial_Printf(&MiSerial_Handler, "{\r\n");
	oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"SensorData\",\r\n");
	oSerial_Printf(&MiSerial_Handler, "\"Time\" : \"%04d-%02d-%02d %02d:%02d:%02d\",\r\n", (int)(pData->Time.Year+2000), (int)pData->Time.Month, (int)pData->Time.Day, (int)pData->Time.Hour, (int)pData->Time.Minute, (int)pData->Time.Second);
	oSerial_Printf(&MiSerial_Handler, "\"Count\" : \"%d\",\r\n", count);
	oSerial_Printf(&MiSerial_Handler, "\"DataArray\" : [\r\n");

	for(i=0; i<count; i++){
		if(comma){
			oSerial_Printf(&MiSerial_Handler, ",\r\n");
		}

		oSerial_Printf(&MiSerial_Handler, "{");
		oSerial_Printf(&MiSerial_Handler, "\"Channel\" : \"%d\", ", (int)pData->Items[i].Channel);
		oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"%d\", ", (int)pData->Items[i].Type);

		switch(pData->Items[i].Type)
		{
			case IoTSensorType_Thermistor:
				oSerial_Printf(&MiSerial_Handler, "\"Data\" : \"%.1f\"", (float)MIIOT_DATA_DECODE_TEMP(pData->Items[i].Data));
				break;
			case IoTSensorType_VibratingWire:
				oSerial_Printf(&MiSerial_Handler, "\"Data\" : \"%.2f\"", (float)MIIOT_DATA_DECODE_FREQUENCY(pData->Items[i].Data));
				break;
			case IoTSensorType_mV:
			case IoTSensorType_Differential:
				oSerial_Printf(&MiSerial_Handler, "\"Data\" : \"%.2f\"", (float)MIIOT_DATA_DECODE_mV(pData->Items[i].Data));
				break;
			case IoTSensorType_mA:
				oSerial_Printf(&MiSerial_Handler, "\"Data\" : \"%.3f\"", (float)MIIOT_DATA_DECODE_mA(pData->Items[i].Data));
				break;
			default:
				oSerial_Printf(&MiSerial_Handler, "\"Data\" : \"0\"");
				break;
		}

		oSerial_Printf(&MiSerial_Handler, "}");
		comma = 1;
	}

	oSerial_Printf(&MiSerial_Handler, "]\r\n");
	oSerial_Printf(&MiSerial_Handler, "};\r\n");
}

oResult_t MiSerial_Specific(char *Message)
{
	return RESULT_NULL;
}

void MiSerial_Process()
{
	static uint8_t ReadAIChannel = 0;
	oResult_t result = RESULT_OK;
	int32_t Analog = 0;

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

	if(MiSerial_SamplingADCsTrig && ReadAIChannel <= 9)
	{
		if(ReadAIChannel == 0){
			if((result = Measurement_Supply(15)) == RESULT_OK){
				Analog = (int32_t)GPIOs.ADC.SystemSupply;
			}
		}
		else{
		}

		if(result != RESULT_RUN){
			if(ReadAIChannel == 0){
				oSerial_Printf(&MiSerial_Handler, "{\r\n");
				oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"SamplingADCs\",\r\n");
			}

			if(result == RESULT_OK){
				oSerial_Printf(&MiSerial_Handler, "\"ADCs[%d]\" : \"%d\"", (int)ReadAIChannel, (int)Analog);
			}

			if(ReadAIChannel != 9){
				oSerial_Printf(&MiSerial_Handler, ",\r\n");
			}
			else{
				oSerial_Printf(&MiSerial_Handler, "\r\n};\r\n");
			}

			ReadAIChannel++;
		}
	}
	else{
		ReadAIChannel = 0;
		MiSerial_SamplingADCsTrig = 0;
	}
}
