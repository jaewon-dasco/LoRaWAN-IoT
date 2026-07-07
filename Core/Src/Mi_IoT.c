/*
 * Mi_IoT.c
 *
 *  Version: 0.4 (2026-07-07)
 */

#include "Mi_Native.h"
#include "Mi_Serial.h"
#include "Mi_Storage.h"
#include "Mi_SoftwareRevision.h"
#include "Mi_Measurement.h"
#include "Mi_LoRa.h"
#include "Mi_IoT.h"

uint8_t MiIoT_IsPowerSaveMode = 1;

IoTDataPacketCallbackHandler_t MiIoT_MeasurementCallback;
IoTDataPacketCallbackHandler_t MiIoT_SamplingCallback;
IoTDataPacketCallbackHandler_t MiIoT_StatusCallback;
ResultCallbackHandler_t MiIoT_SleepCallback;
ResultCallbackHandler_t MiIoT_IOControlCallback;
VoidCallbackHandler_t MiIoT_GPIOInitCallback;
VoidCallbackHandler_t MiIoT_GPIODeInitCallback;

oDateAndTime_t MiIoT_DT;
oDateAndTime_t MiIoT_MeasurementDT;
IoTParameter_t MiIoT_Parameter;
IoTStatus_t MiIoT_Status;
IoTProcessState_t MiIoT_ProcessState;
uint8_t MiIoT_LED;
oTrig_t MiIoT_USBConnectTrig = TRIGGER_INITIALIZER(1);
oBlinker_t MiIoT_BlinkIdle  = BLINK_INITIALIZER(150, 0b0000000000000001, 300);
oBlinker_t MiIoT_BlinkBusy  = BLINK_INITIALIZER(100, 0b1000000000000001, 100);

char* MiIoT_DataTypeToString(IoTDataType_t Type)
{
	switch(Type)
	{
		case IoTDataType_NULL:
			return "NULL";
		case IoTDataType_DataArray_Type1:
			return "DataArray_Type1";
		case IoTDataType_DataArray_Type2:
			return "DataArray_Type2";
		case IoTDataType_ArrayDualTilt:
			return "ArrayDualTilt";
		case IoTDataType_ArraySingleTilt:
			return "ArraySingeTilt";
		case IoTDataType_Analog:
			return "Analog";
		case IoTDataType_Tilt:
			return "Tilt";
		case IoTDataType_Status:
			return "Status";
		case IoTDataType_Operating:
			return "Operating";
		default:
			return "Unknown";
	}
}

char* MiIoT_SensorTypeToString(IoTSensorType_t Type)
{
	switch(Type)
	{
		case IoTSensorType_NULL:
			return "NULL";
		case IoTSensorType_mV:
			return "mV";
		case IoTSensorType_FullBridge:
			return "FullBridge";
		case IoTSensorType_mA:
			return "mA";
		case IoTSensorType_Differential:
			return "Differential";
		case IoTSensorType_Resistance:
			return "Resistance";
		case IoTSensorType_Thermistor:
			return "Thermistor";
		case IoTSensorType_VibratingWire:
			return "VibratingWire";
		case IoTSensorType_ArraySingleTilt:
			return "ArraySingeAxis";
		case IoTSensorType_ArrayDualTilt:
			return "ArrayDualAxis";
		case IoTSensorType_Tilt:
			return "Tilt";
		default:
			return "unknown";
	}
}


void IoT_MailBox_Sort(IoT_Mailbox_t *pMailBox)
{
    if (pMailBox == NULL || pMailBox->Count <= 1){
        return;
    }

    uint16_t n = pMailBox->Count;

    for (uint16_t i = 1; i < n; i++)
    {
        uint16_t idx_i = (pMailBox->FirstIndex + i) % MIIOT_MAILBOX_MAXCOUNT;

        IoT_MailboxItem_t key = pMailBox->Item[idx_i];
        uint32_t keyValue = key.Timestamp + key.Timer;

        uint16_t j = i;

        while (j > 0U)
        {
            uint16_t idx_prev = (pMailBox->FirstIndex + (j - 1U)) % MIIOT_MAILBOX_MAXCOUNT;
            uint16_t idx_cur  = (pMailBox->FirstIndex + j) % MIIOT_MAILBOX_MAXCOUNT;

            uint32_t prevValue = pMailBox->Item[idx_prev].Timestamp + pMailBox->Item[idx_prev].Timer;

            if (prevValue <= keyValue){
                break;
            }

            // 앞 요소를 뒤로 한 칸 밀기
            pMailBox->Item[idx_cur] = pMailBox->Item[idx_prev];
            j--;
        }

        uint16_t idx_dst = (pMailBox->FirstIndex + j) % MIIOT_MAILBOX_MAXCOUNT;
        pMailBox->Item[idx_dst] = key;
    }
}


IoT_MailboxItem_t* MiIoT_MailBox_AddItem(IoT_Mailbox_t *pMailBox, IoT_MailboxItem_t *pItem)
{
	IoT_MailboxItem_t* pResult = &pMailBox->Item[pMailBox->LastIndex];

	if(pMailBox == NULL || pItem == NULL){
		return NULL;
	}

	pItem->Timestamp = oTMR_GetTick(TICKBASE_SYSTICK);
	pItem->IsBusy = 0;
	pItem->IsDone = 0;
	pItem->IsError = 0;
	pItem->Result = RESULT_NULL;

	if(pItem != pResult){
		*pResult = *pItem;
	}

	pMailBox->LastIndex = (pMailBox->LastIndex+1) % MIIOT_MAILBOX_MAXCOUNT;

	if(pMailBox->Count >= MIIOT_MAILBOX_MAXCOUNT){
		pMailBox->FirstIndex = (pMailBox->FirstIndex+1) % MIIOT_MAILBOX_MAXCOUNT;
	}
	else{
		pMailBox->Count++;
	}

	return pResult;
}

IoT_MailboxItem_t* MiIoT_MailBox_GetLastItem(IoT_Mailbox_t *pMailBox)
{
	return &pMailBox->Item[pMailBox->LastIndex];
}

IoT_MailboxItem_t* MiIoT_MailBox_NewItem(IoT_Mailbox_t *pMailBox, IoT_DataPacket_t *pPacket, uint8_t QoS, uint32_t Timer)
{
	IoT_MailboxItem_t Mail = MIIOT_MAILBOX_INITIALIZER;

	if(pMailBox == NULL || pPacket == NULL || pPacket->DLC == 0 || pPacket->TypeOfData == IoTDataType_NULL){
		return NULL;
	}

	Mail.Timer = Timer;
	Mail.Payload.DLC = pPacket->DLC;
	Mail.Payload.TypeOfData = pPacket->TypeOfData;
	Mail.QoS = QoS;

	memcpy(&Mail.Payload.Frame, &pPacket->Frame, pPacket->DLC);

	return MiIoT_MailBox_AddItem(pMailBox, &Mail);
}

IoT_MailboxItem_t* MiIoT_MailBox_GetItem(IoT_Mailbox_t* pMailBox)
{
	uint32_t i = pMailBox->FirstIndex;
	IoT_MailboxItem_t* pResult = NULL;

	if(pMailBox == NULL || pMailBox->Count <= 0){
		return NULL;
	}

	if(pMailBox->Item[i].Payload.DLC != 0 && pMailBox->Item[i].Payload.TypeOfData > IoTDataType_NULL && pMailBox->Item[i].Payload.TypeOfData < IoTDataType_Max){

		if((pMailBox->Item[i].Timer == 0 || oTMR_Elapsed(&pMailBox->Item[i].Timestamp, pMailBox->Item[i].Timer, TICKBASE_SYSTICK))){
			pResult = &pMailBox->Item[i];
		}

	}
	else{
		MiIoT_MailBox_Remove(pMailBox, &pMailBox->Item[i]);
	}

	return pResult;
}

void MiIoT_MailBox_Remove(IoT_Mailbox_t* pMailBox, IoT_MailboxItem_t *pItem)
{
	uint32_t i;

	if(pMailBox == NULL || pItem == NULL || pMailBox->Count <= 0){
		return;
	}

	if(&pMailBox->Item[pMailBox->FirstIndex] == pItem){
		pMailBox->Item[pMailBox->FirstIndex].Payload.DLC = 0;
		pMailBox->Item[pMailBox->FirstIndex].Result = RESULT_NULL;

		pMailBox->FirstIndex = (pMailBox->FirstIndex+1) % MIIOT_MAILBOX_MAXCOUNT;
		pMailBox->Count--;
	}
	else{
		for(i=0; i<MIIOT_MAILBOX_MAXCOUNT && pMailBox->Count; i++){
			if(&pMailBox->Item[i] == pItem){
				pMailBox->Item[i].Payload.DLC = 0;
				pMailBox->Item[i].Result = RESULT_NULL;
				pMailBox->Count--;
				break;
			}
		}
	}
}

IoT_MailboxItem_t *MiIoT_MailBox_Find(IoT_Mailbox_t *pMailBox, IoTDataType_t TypeOfData)
{
	uint32_t i, cnt;

	if(pMailBox == NULL || pMailBox->Count <= 0){
		return NULL;
	}

	i = pMailBox->FirstIndex % MIIOT_MAILBOX_MAXCOUNT;
	for(cnt=0; cnt < pMailBox->Count; cnt++){
		if(pMailBox->Item[i].Payload.DLC > 0 && pMailBox->Item[i].Payload.TypeOfData == TypeOfData){
			return &pMailBox->Item[i];
		}

		i = (i+1) % MIIOT_MAILBOX_MAXCOUNT;
	}

	return NULL;
}

oResult_t MiIoT_MailBox_IsExist(IoT_Mailbox_t *pMailBox, IoTDataType_t TypeOfData)
{
	return MiIoT_MailBox_Find(pMailBox, TypeOfData) != NULL ? RESULT_OK : RESULT_ERROR;
}

uint8_t MiIoT_IsSamplingTime()
{
	static uint8_t TimeToSampling = 0;
	uint8_t ChannelIndex = 0;
	uint32_t OffsetMinute = 0;
	uint32_t TotalWarmup = 5000; //Default lora/lte to server delay 5 minute

	if(oDT_IsEmpty(&MiIoT_DT) || MiIoT_Parameter.Operating.SamplingInterval <= 0){
		TimeToSampling = 0;
		return TimeToSampling;
	}

	for(ChannelIndex=0; ChannelIndex<ArrayLen(MiIoT_Parameter.ChannelConfig); ChannelIndex++){
		if(MiIoT_Parameter.ChannelConfig[ChannelIndex].TypeOfSensor != 0){
			TotalWarmup += (uint32_t)MiIoT_Parameter.ChannelConfig[ChannelIndex].WarmupTime;
		}
	}

	OffsetMinute =  MS_TO_MINUTE(TotalWarmup);

	if(((HOUR_TO_TOTAL_MINUTE(MiIoT_DT)+OffsetMinute) % MiIoT_Parameter.Operating.SamplingInterval) == 0){
		if(TimeToSampling == 0){
			oDT_GetNow(&MiIoT_MeasurementDT);
			oDT_AddMinute(&MiIoT_MeasurementDT, OffsetMinute);

			MiIoT_MeasurementDT.Second = 0;
		}

		TimeToSampling = 1;
	}
	else{
		TimeToSampling = 0;
	}

	return TimeToSampling;
}

void MiIoT_IOControl()
{
	//LED indicator
	oBlink(&MiIoT_BlinkBusy);
	oBlink(&MiIoT_BlinkIdle);

	if(MiIoT_Parameter.Operating.OperatingMode == IoTOperatingMode_Stop){
		MiIoT_LED = 0;
	}
	else if(!MiIoT_ProcessState.SleepMode){
		if(pLoRaDevice->Status.IsBusy){
			MiIoT_LED = MiIoT_BlinkBusy.Output;
		}
		else if(MiIoT_ProcessState.IsBusy != 0){
			MiIoT_LED = MiIoT_BlinkIdle.Output;
		}
		else{
			MiIoT_LED = 0;
		}
	}

	if(MiIoT_IOControlCallback){
		MiIoT_ProcessState.BusyGPIO = MiIoT_IOControlCallback() == RESULT_RUN || MiIoT_LED;
	}
	else{
		MiIoT_ProcessState.BusyGPIO = 0;
	}
}

oResult_t MiIoT_SamplingSensor()
{
	static uint8_t SamplingtStep = -1;
	static IoT_DataPacket_t *pDataPacket;
	static oResult_t SamplingResult = RESULT_RUN;
	oResult_t result = RESULT_RUN;

	if(!MiIoT_SamplingCallback){
		SamplingtStep = 0;
		MiIoT_ProcessState.SamplingSensor = 0;
		return RESULT_NULL;
	}

	switch(SamplingtStep)
	{
		default:
			MiSerial_SamplingSensorCmd = 0;
			SamplingtStep = 0; // @suppress("No break at end of case")
		case 0:
			MiIoT_ProcessState.SamplingSensor = 0;

			if(MiSerial_SamplingSensorCmd){
				SamplingtStep++;
			}
			else{
				result = RESULT_DONE;
			}
			break;
		case 1:
			if(!MiIoT_ProcessState.SleepMode && !MiIoT_ProcessState.SamplingPeriod && !MiIoT_ProcessState.SamplingSupply){
				SamplingtStep++;
			}
			break;
		case 2:
			if((SamplingResult=MiIoT_SamplingCallback(&pDataPacket)) != RESULT_RUN){
				MiIoT_ProcessState.SamplingSensor = 1;
				SamplingtStep++;
			}
			break;
		case 3:
			if(SamplingResult == RESULT_OK && pDataPacket != NULL && pDataPacket->TypeOfData != IoTDataType_NULL && pDataPacket->DLC){
				MiSerial_PrintSensorData(pDataPacket);

				if(MiLoRa_IsReachable){
					MiIoT_MailBox_NewItem(&MiIoT_LoRaMailBox, pDataPacket, MILORA_QOS_MAX, 0);
					oSerial_Log("MiIoT", "Sampling set mailbox\r\n");
				}
			}

			switch(SamplingResult)
			{
				case RESULT_DONE:
					result = RESULT_DONE;
					MiSerial_SamplingSensorCmd = 0;
					MiSerial_StopSensorCmd = 0;
					SamplingtStep = 0;
					break;
				case RESULT_WAIT:
					result = RESULT_WAIT;
					SamplingtStep = 1;
					break;
				case RESULT_OK:
					result = RESULT_RUN;
					SamplingtStep = 1;
					break;
				default:
					result = SamplingResult;
					MiSerial_SamplingSensorCmd = 0;
					MiSerial_StopSensorCmd = 0;
					SamplingtStep = 0;
					break;
			}
			break;
	}

	return result;
}

oResult_t MiIoT_UpdatePeriod()
{
	static uint8_t UpdatePeriodStep = -1;
	static uint8_t UpdatePeriodStarted = 0;
	static uint8_t UpdatedPeriod_IsOk;
	static uint32_t UpdatePeriodTimer;
	static IoT_DataPacket_t *pDataPacket;
	static oResult_t MeasurementResult = RESULT_RUN;
	uint32_t MeasurementSendDelay = 0;
	oResult_t result = RESULT_RUN;

	//Update measurement
	if(!MiIoT_MeasurementCallback){
		UpdatePeriodStep = 0;
		MiIoT_ProcessState.SamplingPeriod = 0;
		return RESULT_NULL;
	}

	switch(UpdatePeriodStep)
	{
		default:
			UpdatePeriodStep = 0; // @suppress("No break at end of case")
		case 0:
			MiIoT_ProcessState.SamplingPeriod = 0;
			UpdatePeriodTimer = 0;
			result = RESULT_DONE;

			if(MiIoT_Parameter.Operating.OperatingMode == IoTOperatingMode_Operating){
				if(!MiIoT_ProcessState.Pause && (MiLoRa_IsReachable || UpdatePeriodStarted)){
					UpdatePeriodStarted = 1;

					if(UpdatedPeriod_IsOk){
						if(!MiIoT_IsSamplingTime()){
							UpdatedPeriod_IsOk = 0;
						}
					}
					else{
						if(MiIoT_IsSamplingTime()){
							result = RESULT_RUN;
							UpdatedPeriod_IsOk = 1;
							UpdatePeriodStep++;

						}
					}
				}
			}
			else{
				UpdatePeriodStarted = 0;
			}
			break;
		case 1:
			if((UpdatePeriodTimer == 0 || oTMR_Elapsed(&UpdatePeriodTimer, 100, TICKBASE_SYSTICK)) && !MiIoT_ProcessState.SleepMode && !MiIoT_ProcessState.SamplingSensor && !MiIoT_ProcessState.SamplingSupply){
				MiIoT_ProcessState.SamplingPeriod = 1;
				UpdatePeriodStep++;					
			}
			break;
		case 2:
			if((MeasurementResult = MiIoT_MeasurementCallback(&pDataPacket)) != RESULT_RUN){
				UpdatePeriodStep++;
			}
			break;
		case 3:
			if(MeasurementResult == RESULT_OK && pDataPacket != NULL && pDataPacket->TypeOfData != IoTDataType_NULL && pDataPacket->DLC){
				MIIOT_DT_TO_IOTTIME(&MiIoT_MeasurementDT, &pDataPacket->Frame);

				if(GPIOs.DI.UsbConnected){
					MiSerial_PrintSensorData(pDataPacket);
				}

				//시리얼 번호 만큼 시간 옵셋을 줘서 lora data 겹치지 않게 한다.
				if(MiIoT_Parameter.Information.SerialNo[6] > '0' && MiIoT_Parameter.Information.SerialNo[6] <= '9'){
					MeasurementSendDelay = (uint32_t)(MiIoT_Parameter.Information.SerialNo[6] - '0') * SECOND_TO_MS(5);
				}

				MiIoT_MailBox_NewItem(&MiIoT_LoRaMailBox, pDataPacket, MILORA_QOS_MAX, MeasurementSendDelay);
				oSerial_Log("MiIoT", "Measurement set mailbox\r\n");
			}

			switch(MeasurementResult)
			{
				case RESULT_DONE:
					result = RESULT_DONE;
					UpdatePeriodStep = 0;
					break;
				case RESULT_WAIT:
					result = RESULT_WAIT;
					MiIoT_ProcessState.SamplingPeriod = 0;
					UpdatePeriodTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					UpdatePeriodStep = 1;
					break;
				case RESULT_OK:
					result = RESULT_RUN;
					UpdatePeriodStep = 1;
					break;
				default:
					result = MeasurementResult;
					UpdatePeriodStep = 0;
					break;
			}
			break;
	}

	return result;
}

oResult_t MiIoT_UpdateStatus()
{
	static uint8_t MiIoT_StatusStep = -1;
	static IoT_DataPacket_t *pDataPacket;
	static uint32_t StatusUpdateTimer = 0;
	static uint8_t StatusSendLoRa = 0;

	//Update status
	if(MiIoT_StatusCallback){
		switch(MiIoT_StatusStep)
		{
			default:
				MiIoT_StatusStep = 0; // @suppress("No break at end of case")
			case 0:
				MiIoT_ProcessState.SamplingSupply = 0;
				StatusSendLoRa = 0;

				if(MiIoT_USBConnectTrig.Output){
					MiIoT_StatusStep++;
				}
				else if(MiIoT_Parameter.Operating.OperatingMode != IoTOperatingMode_Stop && !MiIoT_ProcessState.Pause && !MiIoT_ProcessState.SleepMode && MiLoRa_IsReachable){
					StatusSendLoRa |= StatusUpdateTimer == 0;
					StatusSendLoRa |= oTMR_Elapsed(&StatusUpdateTimer, MINUTE_TO_MS(MATH_MAX(MiIoT_Parameter.Operating.UpdateInterval, HOUR_TO_MINUTE(6))), TICKBASE_SYSTICK);

					if(StatusSendLoRa){
						MiIoT_StatusStep++;
					}
				}
				break;
			case 1:
				if(!MiIoT_ProcessState.SleepMode && !MiIoT_ProcessState.SamplingSensor && !MiIoT_ProcessState.SamplingPeriod){
					MiIoT_ProcessState.SamplingSupply = 1;
					MiIoT_StatusStep++;
				}
				break;
			case 2:
				switch(MiIoT_StatusCallback(&pDataPacket))
				{
					case RESULT_RUN:
						break;
					case RESULT_OK:
					case RESULT_DONE:
						MiIoT_StatusStep++;
						break;
					default:
						MiIoT_StatusStep = 0;
						break;
				}
				break;
			case 3:
				//연결되어 있고 status 메시지가 이미 있지 않을때 전송
				if(StatusSendLoRa && MiIoT_MailBox_IsExist(&MiIoT_LoRaMailBox, IoTDataType_Status) != RESULT_OK){
					MiIoT_StatusStep++;
				}
				else{
					MiIoT_StatusStep = 0;
				}
				break;
			case 4:
				if(MiIoT_MailBox_NewItem(&MiIoT_LoRaMailBox, pDataPacket, 1, 0) != NULL){
					oSerial_Log("MiIoT", "Status set mailbox\r\n");
					StatusUpdateTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					MiIoT_StatusStep++;
				}
				break;
		}
	}
	else{
		MiIoT_StatusStep = 0;
		MiIoT_ProcessState.SamplingSupply = 0;
	}

	return MiIoT_StatusStep != 0 ? RESULT_RUN : RESULT_DONE;
}

void MiIoT_Sleep()
{
	static uint8_t MiIoT_SleepStep = 0;
	static uint32_t SleepTimer = 0;

	if(MiIoT_GPIODeInitCallback && MiIoT_GPIOInitCallback){
		//Sleep mode
		switch(MiIoT_SleepStep)
		{
			default:
				MiIoT_SleepStep = 0;
				MiIoT_ProcessState.SleepMode = 0;
				SleepTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				break;
			case 0:
				if(MiIoT_ProcessState.IsBusy == 0 && MiIoT_IsPowerSaveMode && oTMR_GetTick(TICKBASE_SYSTICK) > 3000){
					/* 5ms idle 유예 — 이 안에 period/sensor/status가 busy 비트를 선점할 수 있다.
					 * 유예 통과 시 SleepMode=1 설정과 step 전진이 반드시 한 몸으로 일어나야
					 * 태스크 가드(!SleepMode)가 sleep 시퀀스 전 구간을 막는다. */
					if(oTMR_Elapsed(&SleepTimer, 5, TICKBASE_SYSTICK)){
						MiIoT_ProcessState.SleepMode = 1;
						MiIoT_LED = 0;
						SleepTimer = oTMR_GetTick(TICKBASE_SYSTICK);
						MiIoT_SleepStep++;
					}
				}
				else{
					SleepTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				}
				break;
			case 1:
				MiIoT_LED = 1;
				if(oTMR_Elapsed(&SleepTimer, 20, TICKBASE_SYSTICK)){
					SleepTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					MiIoT_SleepStep++;
				}
				break;
			case 2:			
				MiIoT_LED = 0;
				if(oTMR_Elapsed(&SleepTimer, 200, TICKBASE_SYSTICK)){
					SleepTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					MiIoT_SleepStep++;
				}
				break;
			case 3:
				if(MiIoT_GPIODeInitCallback){
					MiIoT_GPIODeInitCallback();
				}

				Native_SleepMode(MIIOT_SLEEP_TIME);

				if(MiIoT_GPIOInitCallback){
					MiIoT_GPIOInitCallback();
				}

				MiIoT_SleepStep++;
				break;
			case 4:
				if(MiIoT_SleepCallback == NULL){
					MiIoT_SleepStep++;
				}
				else if(MiIoT_SleepCallback() == RESULT_OK){
					MiIoT_SleepStep++;
				}
				break;
		}
	}
}

void MiIoT(UART_HandleTypeDef *pLoRaUART)
{
	oDT_GetNow(&MiIoT_DT);
	oTrigger(&MiIoT_USBConnectTrig, GPIOs.DI.UsbConnected);

	MiLoRa(pLoRaUART);

	MiIoT_ProcessState.BusyLora = MiLoRa_IsBusy;
	MiIoT_ProcessState.BusyMemory = MiStorage_IsBusy;

	MiIoT_UpdatePeriod();
	MiIoT_SamplingSensor();
	MiIoT_UpdateStatus();

	MiIoT_IOControl();
	MiIoT_Sleep();

	MiIoT_ProcessState.Pause = 0;
}
