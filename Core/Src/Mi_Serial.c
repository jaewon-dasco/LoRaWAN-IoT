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

oSerialHandler_t MiSerial_Handler = {NULL, NULL, 0, (uint8_t *)&MiSerial_RxBuffer, MISERIAL_RX_BUFFER_SIZE, 0,0,0,0};

char StrBuffer[MISERIAL_RX_BUFFER_SIZE];
uint8_t MiSerial_TxBuffer[MISERIAL_TX_BUFFER_SIZE];
uint8_t MiSerial_RxBuffer[MISERIAL_RX_BUFFER_SIZE];

uint8_t MiSerial_UpdateSensorCmd;
uint8_t MiSerial_StopSensorCmd;
uint8_t MiSerial_SamplingADCsTrig;


char* MiSerial_LoraStateString()
{
	char *pChar;

	memset(&StrBuffer, 0, sizeof(StrBuffer));
	pChar = (char *)&StrBuffer;

	if(pLoRaDevice->IsNotworking){
		sprintf(pChar, "NotWorking");
	}
	else if(!MiLoRa_IsPowerOn){
		if(MILORA_OPEN_REMAINING_TIME != 0){
			sprintf(pChar, "OpenFailed | Remaining %dsec", (int)(MILORA_OPEN_REMAINING_TIME/1000));
		}
		else{
			sprintf(pChar, "PowerOff");
		}
	}
	else if(pLoRaDevice->Status.IsSending){
		sprintf(pChar, "Sending");
	}
	else if(pLoRaDevice->Status.IsSendFailed){
		sprintf(pChar, "SendFailed");
	}
	else if(pLoRaDevice->Status.IsSendComformed){
		sprintf(pChar, "SendComformed");
	}
	else if(pLoRaDevice->Status.IsTimeSynced){
		sprintf(pChar, "TimeSynced");
	}
	else if(pLoRaDevice->Status.IsJoinFail){
		sprintf(pChar, "JoinFail");
	}
	else if(pLoRaDevice->Status.IsJoined){
		sprintf(pChar, "Joined");
	}
	else if(pLoRaDevice->Status.IsOpened){
		sprintf(pChar, "TryJoin");
	}
	else if(pLoRaDevice->Status.IsOpening){
		sprintf(pChar, "TryOpen");
	}
	else if(MiLoRa_IsPowerOn){
		sprintf(pChar, "PowerOn");
	}

	return StrBuffer;
}


void MiSerial_PrintResponse(char *Type, char *Message)
{
	if(!GPIOs.DI.UsbConnected){
		return;
	}

	oSerial_Printf(&MiSerial_Handler, "{\r\n");
	oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"Response\",\r\n");
	oSerial_Printf(&MiSerial_Handler, "\"Request\" : \"%s\",\r\n", Type);
	oSerial_Printf(&MiSerial_Handler, "\"Value\" : \"%s\"\r\n", Message);
	oSerial_Printf(&MiSerial_Handler, "};\r\n");
}

oResult_t MiSerial_Set(char *Message)
{
	int ErrorCount = 0;
	char StrVal[125];
	double dVal[4];
	double dTmp;
	int len;
	long lVal;
	char *endptr;

	if(strstr(Message, "Set") == NULL || strstr(Message, "{") == NULL || strstr(Message, "}") == NULL){
		return RESULT_NULL;
	}

	if(strstr(Message, "\"Type\" : \"DeviceInfo\"") != NULL){
		switch(oJSON_GetValue(Message, "\"SerialNo\"", StrVal, sizeof(StrVal)))
		{
			default:
				break;
			case RESULT_OK:
				len = strlen(StrVal);
				if(len > 0 && len < sizeof(MiIoT_Parameter.Information.SerialNo)){
					if(strncmp(MiIoT_Parameter.Information.SerialNo, StrVal, len) != 0){
						memset(MiIoT_Parameter.Information.SerialNo, 0, sizeof(MiIoT_Parameter.Information.SerialNo));
						strncpy(MiIoT_Parameter.Information.SerialNo, StrVal, len);
						MiIoT_Status.IsUpdateInformation = 0;
					}
				}
				else{
					ErrorCount++;
				}
				break;
			case RESULT_ERROR:
				ErrorCount++;
				break;
		}

		switch(oJSON_GetValue(Message, "\"OperatingMode\"", StrVal, sizeof(StrVal)))
		{
			default:
				break;
			case RESULT_OK:
				lVal = strtol(StrVal, &endptr, 10);
				if(endptr != StrVal){
					MiIoT_Parameter.Operating.OperatingMode = (uint8_t)lVal;
				}
				else{
					ErrorCount++;
				}
				break;
			case RESULT_ERROR:
				ErrorCount++;
				break;
		}

		switch(oJSON_GetValue(Message, "\"SamplingTime\"", StrVal, sizeof(StrVal)))
		{
			default:
				break;
			case RESULT_OK:
				lVal = strtol(StrVal, &endptr, 10);
				if(endptr != StrVal){
					MiIoT_Parameter.Operating.SamplingInterval = (uint32_t)lVal;
				}
				else{
					ErrorCount++;
				}
				break;
			case RESULT_ERROR:
				ErrorCount++;
				break;
		}

		switch(oJSON_GetValue(Message, "\"UpdateTime\"", StrVal, sizeof(StrVal)))
		{
			default:
				break;
			case RESULT_OK:
				lVal = strtol(StrVal, &endptr, 10);
				if(endptr != StrVal){
					MiIoT_Parameter.Operating.UpdateInterval = (uint32_t)lVal;
				}
				else{
					ErrorCount++;
				}
				break;
			case RESULT_ERROR:
				ErrorCount++;
				break;
		}

		if(ErrorCount == 0){
			MiSerial_PrintResponse("Set/DeviceInfo", "OK");
		}
		else{
			MiSerial_PrintResponse("Set/DeviceInfo", "ERROR");
		}

		return RESULT_OK;
	}

	if(strstr(Message, "\"Type\" : \"LoRaConfig\"") != NULL)
	{
		switch(oJSON_GetValue(Message, "\"PowerSaveMode\"", StrVal, sizeof(StrVal)))
		{
			default:
				break;
			case RESULT_OK:
				lVal = strtol(StrVal, &endptr, 10);
				if(endptr != StrVal){
					MiIoT_IsPowerSaveMode = (uint8_t)lVal;
				}
				else{
					ErrorCount++;
				}
				break;
			case RESULT_ERROR:
				ErrorCount++;
				break;
		}

		if(ErrorCount == 0){
			MiSerial_PrintResponse("Set/LoRaConfig", "OK");
		}
		else{
			MiSerial_PrintResponse("Set/LoRaConfig", "ERROR");
		}

		return RESULT_OK;
	}

	if(strstr(Message, "\"Type\" : \"ChannelConfig\"") != NULL){

		switch(oJSON_GetValue(Message, "\"Channel\"", StrVal, sizeof(StrVal)))
		{
			default:
				break;
			case RESULT_OK:
				lVal = strtol(StrVal, &endptr, 10);
				if(endptr != StrVal){
					if(MiSerial_SetChannelConfig(Message, &MiIoT_Parameter, (uint8_t)lVal) != RESULT_OK){
						ErrorCount++;
					}
				}
				else{
					ErrorCount++;
				}
				break;
			case RESULT_ERROR:
				ErrorCount++;
				break;
		}

		if(ErrorCount == 0){
			MiSerial_PrintResponse("Set/ChannelConfig", "OK");
		}
		else{
			MiSerial_PrintResponse("Set/ChannelConfig", "ERROR");
		}

		return RESULT_OK;
	}

	if(strstr(Message, "\"Type\" : \"SystemConfig\"") != NULL)
	{
		switch(oJSON_GetValue(Message, "\"ActualVRef\"", StrVal, sizeof(StrVal)))
		{
			default:
				break;
			case RESULT_OK:
				dTmp = strtod(StrVal, &endptr);
				if(endptr != StrVal){
					MiIoT_Parameter.SystemConfig.ActualVRef = dTmp;
				}
				else{
					ErrorCount++;
				}
				break;
			case RESULT_ERROR:
				ErrorCount++;
				break;
		}

		switch(oJSON_GetValue(Message, "\"Actual5Vdc\"", StrVal, sizeof(StrVal)))
		{
			default:
				break;
			case RESULT_OK:
				dTmp = strtod(StrVal, &endptr);
				if(endptr != StrVal){
					MiIoT_Parameter.SystemConfig.Actual5Vdc = dTmp;
				}
				else{
					ErrorCount++;
				}
				break;
			case RESULT_ERROR:
				ErrorCount++;
				break;
		}

		for(int i=0; i<10; i++)
		{
			sprintf(StrVal, "\"ADCalibration[%d]\"", (int)i);

			len = oJSON_GetDoubleArray(Message, StrVal, (double *)&dVal, 4);

			if(len == 4){
				memcpy(&MiIoT_Parameter.SystemConfig.Calibration.ADC[i], &dVal, sizeof(dVal));
			}
			else if(len < 0){
				ErrorCount++;
			}
		}

		if(ErrorCount == 0){
			MiSerial_PrintResponse("Set/SystemConfig", "OK");
		}
		else{
			MiSerial_PrintResponse("Set/SystemConfig", "ERROR");
		}

		return RESULT_OK;
	}

	if(strstr(Message, "\"Type\" : \"Save\"") != NULL)
	{
		if(MiStorage_ParameterSaveCmd == 0){
			MiStorage_ParameterSaveCmd = 1;
		}
		return RESULT_OK;
	}

	if(strstr(Message, "\"Type\" : \"Reset\"") != NULL)
	{
		MiSerial_PrintResponse("Reset", "OK");
		HAL_NVIC_SystemReset();

		return RESULT_OK;
	}

	return RESULT_OK;
}

oResult_t MiSerial_Get(char *Message)
{
	char *pStr;

	if(strstr(Message, "Get/SensorData") != NULL){
		if(MiIoT_IsValidParameter(&MiIoT_Parameter) == RESULT_OK){
			MiSerial_UpdateSensorCmd = 1;
			MiSerial_PrintResponse("Get/SensorData", "OK");
		}
		else{
			MiSerial_UpdateSensorCmd = 0;
			MiSerial_PrintResponse("Get/SensorData", "InvalidParameterError");
		}

		return RESULT_OK;
	}

	if(strstr(Message, "Get/SamplingADCs") != NULL){
		MiSerial_SamplingADCsTrig = 1;
		MiSerial_PrintResponse("Get/SamplingADCs", "OK");
		return RESULT_OK;
	}

	if(strstr(Message, "Get/DeviceInfo") != NULL){
		oSerial_Printf(&MiSerial_Handler, "{\r\n");
		oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"DeviceInfo\",\r\n");
		oSerial_Printf(&MiSerial_Handler, "\"SoftwareVersion\" : \"%.1f\",\r\n", MI_SW_REVISION);
		oSerial_Printf(&MiSerial_Handler, "\"ProductCode\" : \"%d\",\r\n", (int)MiIoT_Parameter.Information.ProductCode);
		oSerial_Printf(&MiSerial_Handler, "\"SerialNo\" : \"%s\",\r\n", MiIoT_Parameter.Information.SerialNo);
		oSerial_Printf(&MiSerial_Handler, "\"OperatingMode\" : \"%d\",\r\n", (int)MiIoT_Parameter.Operating.OperatingMode);
		oSerial_Printf(&MiSerial_Handler, "\"SamplingTime\" : \"%d\",\r\n", (int)MiIoT_Parameter.Operating.SamplingInterval);
		oSerial_Printf(&MiSerial_Handler, "\"UpdateTime\" : \"%d\"\r\n", (int)MiIoT_Parameter.Operating.UpdateInterval);
		oSerial_Printf(&MiSerial_Handler, "};\r\n");

		return RESULT_OK;
	}

	if(strstr(Message, "Get/DeviceState") != NULL){
		oSerial_Printf(&MiSerial_Handler, "{\r\n");
		oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"DeviceState\",\r\n");
		oSerial_Printf(&MiSerial_Handler, "\"Time\" : \"%04d-%02d-%02d %02d:%02d:%02d\",\r\n", (int)MiIoT_DT.Year, (int)MiIoT_DT.Month, (int)MiIoT_DT.Day, (int)MiIoT_DT.Hour, (int)MiIoT_DT.Minute, (int)MiIoT_DT.Second);
		oSerial_Printf(&MiSerial_Handler, "\"LoRa\" : \"%s\",\r\n", MiSerial_LoraStateString());
		oSerial_Printf(&MiSerial_Handler, "\"SupplyVolt\" : \"%d\",\r\n", (int)GPIOs.ADC.SystemSupply);
		oSerial_Printf(&MiSerial_Handler, "\"ClockVolt\" : \"%d\"\r\n", (int)GPIOs.ADC.InternalBAT);
		oSerial_Printf(&MiSerial_Handler, "};\r\n");
		return RESULT_OK;
	}

	if((pStr=strstr(Message, "Get/ChannelConfig")) != NULL){
		int32_t ch = 0;
		pStr += strlen("Get/ChannelConfig");
		if(*pStr == '/'){
			ch = strtol(pStr + 1, NULL, 10);
		}
		MiSerial_PrintChannelConfig(ch, &MiIoT_Parameter);
		return RESULT_OK;
	}

	if(strstr(Message, "Get/LoRaState") != NULL){
		oSerial_Printf(&MiSerial_Handler, "{\r\n");
		oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"LoRaState\",\r\n");
		oSerial_Printf(&MiSerial_Handler, "\"State\" : \"%s\",\r\n", MiSerial_LoraStateString());
		oSerial_Printf(&MiSerial_Handler, "\"PowerSaveMode\" : \"%d\",\r\n", MiIoT_IsPowerSaveMode);
		oSerial_Printf(&MiSerial_Handler, "\"Connection\" : \"%s\",\r\n", MiLoRa_IsReachable ? "Connected" : "Disconnected");
		oSerial_Printf(&MiSerial_Handler, "\"RSSI\" : \"%d\",\r\n", (int)RAK3172Dev.RSSI);
		oSerial_Printf(&MiSerial_Handler, "\"SNR\" : \"%d\",\r\n", (int)RAK3172Dev.SNR);
		oSerial_Printf(&MiSerial_Handler, "\"DevEUI\" : \"%s\",\r\n", RAK3172Dev.Information.DevEUI);
		oSerial_Printf(&MiSerial_Handler, "\"JoinEUI\" : \"%s\",\r\n", RAK3172Dev.Information.AppEUI);
		oSerial_Printf(&MiSerial_Handler, "\"AppKEY\" : \"%s\"\r\n", RAK3172Dev.Information.AppKEY);
		oSerial_Printf(&MiSerial_Handler, "};\r\n");

		if(RAK3172Dev.IsNotworking){
			RAK3172Dev.IsNotworking = 0; //reset flag
		}

		return RESULT_OK;
	}

	if(strstr(Message, "Get/SystemConfig") != NULL){
		oSerial_Printf(&MiSerial_Handler, "{\r\n");
		oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"SystemConfig\",\r\n");
		oSerial_Printf(&MiSerial_Handler, "\"ActualVRef\" : \"%d\",\r\n", (int)MiIoT_Parameter.SystemConfig.ActualVRef);
		oSerial_Printf(&MiSerial_Handler, "\"Actual5Vdc\" : \"%d\",\r\n", (int)MiIoT_Parameter.SystemConfig.Actual5Vdc);

		for(int i=0; i<10; i++)
		{
			oSerial_Printf(&MiSerial_Handler, "\"ADCalibration[%d]\" : ", (int)i);
			oSerial_Printf(&MiSerial_Handler, "[");
			oSerial_Printf(&MiSerial_Handler, "\"%d\",", (int)MiIoT_Parameter.SystemConfig.Calibration.ADC[i].InMin);
			oSerial_Printf(&MiSerial_Handler, "\"%d\",", (int)MiIoT_Parameter.SystemConfig.Calibration.ADC[i].InMax);
			oSerial_Printf(&MiSerial_Handler, "\"%d\",", (int)MiIoT_Parameter.SystemConfig.Calibration.ADC[i].OutMin);
			oSerial_Printf(&MiSerial_Handler, "\"%d\"", (int)MiIoT_Parameter.SystemConfig.Calibration.ADC[i].OutMax);
			oSerial_Printf(&MiSerial_Handler, "]");

			if(i<9){
				oSerial_Printf(&MiSerial_Handler, ",\r\n");
			}
		}

		oSerial_Printf(&MiSerial_Handler, "\r\n};\r\n");

		return RESULT_OK;
	}

	if(strstr(Message, "Get/DataIndex") != NULL){
		oSerial_Printf(&MiSerial_Handler, "{\r\n");
		oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"DataIndex\",\r\n");
		oSerial_Printf(&MiSerial_Handler, "\"Initialized\" : \"%d\",\r\n", (int)MiStorage_NandHeader.Status.IsInitialized);
		oSerial_Printf(&MiSerial_Handler, "\"Count\" : \"%d\",\r\n", (int)MiStorage_NandHeader.SensorData.CountOfData);
		oSerial_Printf(&MiSerial_Handler, "\"OlderAddr\" : \"%d\",\r\n", (int)MiStorage_NandHeader.SensorData.OlderAddress);
		oSerial_Printf(&MiSerial_Handler, "\"OlderTime\" : \"%04d-%02d-%02d %02d:%02d:%02d\",\r\n",
			(int)MiStorage_NandHeader.SensorData.OlderDateTime.Year, (int)MiStorage_NandHeader.SensorData.OlderDateTime.Month,
			(int)MiStorage_NandHeader.SensorData.OlderDateTime.Day, (int)MiStorage_NandHeader.SensorData.OlderDateTime.Hour,
			(int)MiStorage_NandHeader.SensorData.OlderDateTime.Minute, (int)MiStorage_NandHeader.SensorData.OlderDateTime.Second);
		oSerial_Printf(&MiSerial_Handler, "\"NewerAddr\" : \"%d\",\r\n", (int)MiStorage_NandHeader.SensorData.NewerAddress);
		oSerial_Printf(&MiSerial_Handler, "\"NewerTime\" : \"%04d-%02d-%02d %02d:%02d:%02d\",\r\n",
			(int)MiStorage_NandHeader.SensorData.NewerDateTime.Year, (int)MiStorage_NandHeader.SensorData.NewerDateTime.Month,
			(int)MiStorage_NandHeader.SensorData.NewerDateTime.Day, (int)MiStorage_NandHeader.SensorData.NewerDateTime.Hour,
			(int)MiStorage_NandHeader.SensorData.NewerDateTime.Minute, (int)MiStorage_NandHeader.SensorData.NewerDateTime.Second);
		oSerial_Printf(&MiSerial_Handler, "\"SyncAddr\" : \"%d\",\r\n", (int)MiStorage_NandHeader.SensorData.SyncAddress);
		oSerial_Printf(&MiSerial_Handler, "\"IsFault\" : \"%d\",\r\n", (int)MiStorage_NandHeader.Status.IsFault);
		oSerial_Printf(&MiSerial_Handler, "\"IsError\" : \"%d\"\r\n", (int)MiStorage_NandHeader.Status.IsError);
		oSerial_Printf(&MiSerial_Handler, "};\r\n");

		return RESULT_OK;
	}

	if(strstr(Message, "Get/StoredData") != NULL){
		char StrVal[32];
		char *endptr;
		uint32_t Address = 0;

		if(oJSON_GetValue(Message, "\"Address\"", StrVal, sizeof(StrVal)) == RESULT_OK){
			long lAddr = strtol(StrVal, &endptr, 10);
			if(endptr != StrVal){
				Address = (uint32_t)lAddr;
			}
		}

		if(Address < MISTORAGE_DATA_STARTADR || Address > MISTORAGE_DATA_ENDADR){
			MiSerial_PrintResponse("Get/StoredData", "InvalidAddress");
			return RESULT_OK;
		}

		{
			IoT_DataPacket_t Packet;
			MiStorage_PageHeader_t Header;
			oResult_t r;

			while((r = MiStorage_ReadIoTData(Address, &Packet, &Header)) == RESULT_RUN);

			if(r == RESULT_OK && Packet.TypeOfData == IoTDataType_Tilt){
				IoTDataTilt_t *pTilt = (IoTDataTilt_t *)&Packet.Frame;

				oSerial_Printf(&MiSerial_Handler, "{\r\n");
				oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"StoredData\",\r\n");
				oSerial_Printf(&MiSerial_Handler, "\"Address\" : \"%d\",\r\n", (int)Address);
				oSerial_Printf(&MiSerial_Handler, "\"Time\" : \"%04d-%02d-%02d %02d:%02d:%02d\",\r\n",
					(int)(pTilt->Time.Year+2000), (int)pTilt->Time.Month, (int)pTilt->Time.Day,
					(int)pTilt->Time.Hour, (int)pTilt->Time.Minute, (int)pTilt->Time.Second);
				oSerial_Printf(&MiSerial_Handler, "\"Temp\" : \"%.2f\",\r\n", MIIOT_DATA_DECODE_TEMP(pTilt->Temperature));
				oSerial_Printf(&MiSerial_Handler, "\"TiltX\" : \"%.3f\",\r\n", MIIOT_DATA_DECODE_ANGLE(pTilt->Sensor.AxisX));
				oSerial_Printf(&MiSerial_Handler, "\"TiltY\" : \"%.3f\"\r\n", MIIOT_DATA_DECODE_ANGLE(pTilt->Sensor.AxisY));
				oSerial_Printf(&MiSerial_Handler, "};\r\n");
			}
			else{
				MiSerial_PrintResponse("Get/StoredData", "ERROR");
			}
		}

		return RESULT_OK;
	}

	return RESULT_ERROR;
}

oResult_t MiSerial_DebugMessage(char *Message)
{
	if(strstr(Message, "Debug/StartLog") != NULL){
		oSerial_LogEnagle(&MiSerial_Handler, 1);
		MiSerial_PrintResponse("Debug/StartLog", "OK");
		return RESULT_OK;
	}

	if(strstr(Message, "Debug/StopLog") != NULL){
		MiSerial_PrintResponse("Debug/StopLog", "OK");
		oSerial_LogEnagle(&MiSerial_Handler, 0);
		return RESULT_OK;
	}

	return RESULT_ERROR;
}

void MiSerial(UART_HandleTypeDef *pUART)
{
	char Message[MISERIAL_RX_BUFFER_SIZE];

	if(MiSerial_Handler.pUART == NULL){
		MiSerial_Handler.pUART = pUART;
		oSerial_LogEnagle(&MiSerial_Handler, 0);
		return;
	}
	else if(!GPIOs.DI.UsbConnected){
		oSerial_LogEnagle(&MiSerial_Handler, 0);

		if(MiSerial_Handler.pUART->gState != HAL_UART_STATE_RESET){
			HAL_UART_DeInit(MiSerial_Handler.pUART);
		}
		return;
	}
	else{
		if(MiSerial_Handler.pUART->gState == HAL_UART_STATE_RESET){
			HAL_UART_Init(MiSerial_Handler.pUART);

			#ifdef DEBUGMODE
			oSerial_LogEnagle(&MiSerial_Handler, 1);
			#endif
		}
	}

	MiSerial_Process();

	if(oSerial_ReadSplit(&MiSerial_Handler, Message, MISERIAL_RX_BUFFER_SIZE, ";\r\n") != RESULT_OK){
	//if(oSerial_Read(&MiSerial_Handler, Message, MISERIAL_RX_BUFFER_SIZE, 100) != RESULT_OK){
		return;
	}

	if(strlen(Message) <= 0){
		return;
	}

	if(MiSerial_DebugMessage(Message) == RESULT_OK){
		return;
	}

	if(MiSerial_Get(Message) == RESULT_OK){
		return;
	}

	if(MiSerial_Set(Message) == RESULT_OK){
		return;
	}

	if(MiSerial_Specific(Message) == RESULT_OK){
		return;
	}

	MiSerial_PrintResponse("UndefinedType", "ERROR");
}
