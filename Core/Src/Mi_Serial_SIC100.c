/*
 * Mi_Serial_SIC100.c
 *
 *  Version: 0.1 (2026-06-29)
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

oSerialHandler_t MiSerial_Handler = {
	.pTxBuffer = NULL,
	.SizeOfTxBuffer = MISERIAL_TX_BUFFER_SIZE,
	.pRxBuffer = MiSerial_RxBuffer,
	.SizeOfRxBuffer = MISERIAL_RX_BUFFER_SIZE,
};

//uint8_t MiSerial_TxBuffer[MISERIAL_TX_BUFFER_SIZE];
uint8_t MiSerial_RxBuffer[MISERIAL_RX_BUFFER_SIZE];

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

				switch(pParameter->ChannelConfig[i].TypeOfSensor)
				{
					case IoTSensorType_mV:
					case IoTSensorType_mA:
						oSerial_Printf(&MiSerial_Handler, "\"ErrorTolerance\" : \"%.3f\"", pParameter->ChannelConfig[i].ErrorTolerance);
						break;
					default:
						oSerial_Printf(&MiSerial_Handler, "\"ErrorTolerance\" : \"%.3f\", ", pParameter->ChannelConfig[i].ErrorTolerance);
						oSerial_Printf(&MiSerial_Handler, "\"SensorId\" : \"0x%02X\", ", (int)(pParameter->ChannelConfig[i].Properties.Array.SensorId));
						oSerial_Printf(&MiSerial_Handler, "\"SensorCount\" : \"%d\", ", (int)(pParameter->ChannelConfig[i].Properties.Array.CountOfSensor));
						oSerial_Printf(&MiSerial_Handler, "\"DisableSensorBit\" : \"%08X\"", (int)(pParameter->ChannelConfig[i].Properties.Array.DisableSensorBit));
						break;
				}

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

		switch(pParameter->ChannelConfig[i].TypeOfSensor)
		{
			case IoTSensorType_mV:
			case IoTSensorType_mA:
				oSerial_Printf(&MiSerial_Handler, "\"ErrorTolerance\" : \"%.3f\"\r\n", pParameter->ChannelConfig[i].ErrorTolerance);
				break;
			default:
				oSerial_Printf(&MiSerial_Handler, "\"ErrorTolerance\" : \"%.3f\",\r\n", pParameter->ChannelConfig[i].ErrorTolerance);
				oSerial_Printf(&MiSerial_Handler, "\"SensorId\" : \"0x%02X\",\r\n", (int)(pParameter->ChannelConfig[i].Properties.Array.SensorId));
				oSerial_Printf(&MiSerial_Handler, "\"SensorCount\" : \"%d\",\r\n", (int)(pParameter->ChannelConfig[i].Properties.Array.CountOfSensor));
				oSerial_Printf(&MiSerial_Handler, "\"DisableSensorBit\" : \"%08X\"\r\n", (int)(pParameter->ChannelConfig[i].Properties.Array.DisableSensorBit));
				break;
		}
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

		switch(pChannelConfig->TypeOfSensor)
		{
			case IoTSensorType_NULL:
				memset(pChannelConfig, 0, sizeof(*pChannelConfig));
				break;
			case IoTSensorType_mV:
			case IoTSensorType_mA:
				memset(&pChannelConfig->Properties, 0, sizeof(pChannelConfig->Properties));
				break;
			case IoTSensorType_ArrayDualTilt:
			case IoTSensorType_ArraySingleTilt:
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
				break;
			default:
				memset(pChannelConfig, 0, sizeof(*pChannelConfig));
				ErrorCount++;
				break;
		}

		if(pChannelConfig->SupplySource == 0){
			pChannelConfig->SupplySource = Channel;
		}
	}

	return ErrorCount == 0 ? RESULT_OK : RESULT_ERROR;
}


/* 단일 채널 데이터 본문 출력 함수 — 한 개의 "{...}" 형태 JSON object로 출력. comma/줄바꿈은 호출자가 처리 */
void MiSerial_PrintSensorData(IoT_DataPacket_t *pPacket)
{
	IoTDataSIC100_2C_t *pData;
	IoTDateAndTime_t *pTime;
	IoTSensorType_t tiltType;
	IoTSensorType_t analogType;
	uint8_t hasCh1, hasCh2;
	uint8_t validCount;
	uint8_t printedAny = 0;
	uint8_t i;
	uint8_t sensorCount;

	if(pPacket == NULL || !GPIOs.DI.UsbConnected){
		return;
	}

	if(pPacket->TypeOfData != IoTDataType_DataArray_Type2 || pPacket->DLC == 0){
		return;
	}

	pData = (IoTDataSIC100_2C_t *)&pPacket->Frame;
	pTime = &pData->Time;
	tiltType = pData->TiltArray.Type;
	analogType = pData->Analog.Type;

	hasCh1 = (tiltType == IoTSensorType_ArrayDualTilt || tiltType == IoTSensorType_ArraySingleTilt) ? 1 : 0;
	hasCh2 = (analogType == IoTSensorType_mV || analogType == IoTSensorType_mA) ? 1 : 0;

	validCount = hasCh1 + hasCh2;

	if(validCount == 0){
		return;
	}

/* Time은 첫 유효 함수 기준 — 같은 측정 사이클에서 채널 간 차이가 무시 가능 */
	oSerial_Printf(&MiSerial_Handler, "{\r\n");
	oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"SensorData\",\r\n");
	oSerial_Printf(&MiSerial_Handler, "\"Time\" : \"%04d-%02d-%02d %02d:%02d:%02d\",\r\n",
		(int)(pTime->Year + 2000), (int)pTime->Month, (int)pTime->Day,
		(int)pTime->Hour, (int)pTime->Minute, (int)pTime->Second);
	oSerial_Printf(&MiSerial_Handler, "\"Count\" : \"%d\",\r\n", (int)validCount);
	oSerial_Printf(&MiSerial_Handler, "\"DataArray\" : [\r\n");

	/* Channel 1 - TiltArray. 센서 개수는 패킷에서 추론 — 마지막 non-zero entry index+1 */
	if(hasCh1){
		sensorCount = pData->CountOfArraySensor;

		oSerial_Printf(&MiSerial_Handler, "{\r\n");
		oSerial_Printf(&MiSerial_Handler, "  \"Channel\" : \"1\",\r\n");
		oSerial_Printf(&MiSerial_Handler, "  \"Type\" : \"%d\",\r\n", (int)tiltType);
		oSerial_Printf(&MiSerial_Handler, "  \"SensorCount\" : \"%d\",\r\n", (int)sensorCount);
		oSerial_Printf(&MiSerial_Handler, "  \"Data\" : [\r\n");
		oSerial_Printf(&MiSerial_Handler, "    {\"Temp\" : \"%.2f\"}", MIIOT_DATA_DECODE_TEMP(pData->TiltArray.Temperature));

		if(tiltType == IoTSensorType_ArrayDualTilt){
			for(i=0; i<sensorCount; i++){
				oSerial_Printf(&MiSerial_Handler, ",\r\n    {\"X\" : \"%.3f\", \"Y\" : \"%.3f\"}",
					MIIOT_DATA_DECODE_ANGLE(pData->TiltArray.Dual[i].AxisX),
					MIIOT_DATA_DECODE_ANGLE(pData->TiltArray.Dual[i].AxisY));
			}
		}
		else{
			for(i=0; i<sensorCount; i++){
				oSerial_Printf(&MiSerial_Handler, ",\r\n    {\"Angle\" : \"%.3f\"}",
					MIIOT_DATA_DECODE_ANGLE(pData->TiltArray.Single[i].Axis));
			}
		}

		oSerial_Printf(&MiSerial_Handler, "\r\n  ]\r\n}");
		printedAny = 1;
	}

	/* Channel 2 - Analog (mV or mA) */
	if(hasCh2){
		double decoded;

		if(printedAny){
			oSerial_Printf(&MiSerial_Handler, ",\r\n");
		}

		if(analogType == IoTSensorType_mA){
			decoded = MIIOT_DATA_DECODE_mA(pData->Analog.Data);
		}
		else{
			decoded = MIIOT_DATA_DECODE_mV(pData->Analog.Data);
		}

		oSerial_Printf(&MiSerial_Handler, "{\r\n");
		oSerial_Printf(&MiSerial_Handler, "  \"Channel\" : \"2\",\r\n");
		oSerial_Printf(&MiSerial_Handler, "  \"Type\" : \"%d\",\r\n", (int)analogType);
		oSerial_Printf(&MiSerial_Handler, "  \"Data\" : \"%d\"\r\n}", (int)decoded);
	}

	oSerial_Printf(&MiSerial_Handler, "\r\n]\r\n");
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

	if(MiSerial_IsScan && ScanCh < 1){
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
