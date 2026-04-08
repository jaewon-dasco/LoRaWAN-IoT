#include "ONE_CAN.h"
#include "ONE_Signal.h"
#include "ONE_Time.h"
#include "ONE_Memory.h"
#include "Mi_Native.h"
#include "Mi_LoRa.h"
#include "Mi_IoT.h"
#include "Mi_SoftwareRevision.h"
#include "Mi_Measurement.h"
#include "Mi_Storage.h"
#include "Mi_Serial.h"

#define SYSTEM_SUPPLY_LOW_LIMIT		3100
#define RTC_SUPPLY_LOW_LIMIT		2100

oIO_t DO_ADC_REF_EANBLE		= {DO_ADC_REF_ENABLE_GPIO_Port,		DO_ADC_REF_ENABLE_Pin,		IO_LOW};
oIO_t DO_NAND_ENABLE		= {DO_NAND_ENABLE_GPIO_Port,		DO_NAND_ENABLE_Pin,			IO_LOW};
oIO_t DO_POWEROUT_ENALBE0	= {DO_POWEROUT_ENABLE_0_GPIO_Port, 	DO_POWEROUT_ENABLE_0_Pin,	IO_LOW};
oIO_t DO_POWEROUT_ENALBE1	= {DO_POWEROUT_ENABLE_1_GPIO_Port, 	DO_POWEROUT_ENABLE_1_Pin,	IO_LOW};
oIO_t DO_CAN_EANBLE			= {DO_CAN_ENABLE_GPIO_Port,			DO_CAN_ENABLE_Pin,		 	IO_HIGH};
oIO_t DO_POWEROUT_SELECT	= {DO_POWEROUT_CH_SELECT_GPIO_Port,	DO_POWEROUT_CH_SELECT_Pin,	IO_HIGH};

oIO_t DO_LORA_ENABLE		= {DO_LORA_ENABLE_GPIO_Port,		DO_LORA_ENABLE_Pin,			IO_LOW};
oIO_t DO_LED_OPERATING		= {DO_LED_OPERATING_GPIO_Port,		DO_LED_OPERATING_Pin,		IO_HIGH};
oIO_t DI_USB_CONNECTED		= {DI_USBC_CONNECTED_GPIO_Port,		DI_USBC_CONNECTED_Pin,		IO_HIGH};

GPIOs_t GPIOs;
oDebounce_t DB_USBConnected	= DEBOUNCE_INITIALIZER(100,100);
oDebounce_t DebounceError	= DEBOUNCE_INITIALIZER(1000,300);

oResult_t MiMain_GPIOControl()
{
	IO_WRITE(DO_LORA_ENABLE, GPIOs.DO.LoRaEnable);
	IO_WRITE(DO_ADC_REF_EANBLE, GPIOs.DO.ADCRefEnable);
	IO_WRITE(DO_NAND_ENABLE, GPIOs.DO.NANDEnable);
	IO_WRITE(DO_POWEROUT_ENALBE0, GPIOs.DO.PwrEnable_U0);
	IO_WRITE(DO_POWEROUT_ENALBE1, GPIOs.DO.PwrEnable_U1);

	IO_WRITE(DO_CAN_EANBLE, GPIOs.DO.CANEnable);
	IO_WRITE(DO_POWEROUT_SELECT, GPIOs.DO.PwrSupplySel);
	IO_WRITE(DO_LED_OPERATING, GPIOs.DO.OperatingLED);

	oDebounce_GPIO(DI_USB_CONNECTED, &DB_USBConnected);
	GPIOs.DI.UsbConnected = DB_USBConnected.Output || DB_USBConnected.Input;

	uint32_t IORun = 0;
	//IORun += GPIOs.DO.LoRaEnable;
	IORun += GPIOs.DO.ADCRefEnable;
	IORun += GPIOs.DO.NANDEnable;
	IORun += GPIOs.DO.PwrEnable_U0;
	IORun += GPIOs.DO.PwrEnable_U1;
	IORun += GPIOs.DO.CANEnable;
	IORun += GPIOs.DO.PwrSupplySel;
	IORun += GPIOs.DO.OperatingLED;
	IORun += GPIOs.DI.UsbConnected;

	GPIOs.DO.OperatingLED = MiIoT_LED;

	return IORun ? RESULT_RUN : RESULT_ERROR;
}

void MiMain_GPIODeInit(void)
{
	memset(&GPIOs.DO, 0, sizeof(GPIOs.DO));
	GPIOs.DO.LoRaEnable = 1;
	MiMain_GPIOControl();
	memset(&GPIOs.DI, 0, sizeof(GPIOs.DI));

	Native_DisableGPIOs(&DO_LORA_ENABLE, 3);

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = DI_USBC_CONNECTED_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(DI_USBC_CONNECTED_GPIO_Port, &GPIO_InitStruct);
}

IoT_DataPacket_t MiMain_GetStableData(IoT_DataPacket_t *pReference, IoT_DataPacket_t *pPast, IoT_DataPacket_t *pNew)
{
	double errorD1;
	double errorD2;
	IoT_DataPacket_t StableData;
	uint32_t i = 0;
	uint32_t cnt = 0;

	if(pReference->DLC == 0 || pReference->TypeOfData != pNew->TypeOfData){
		return *pNew;
	}

	StableData.TypeOfData = pReference->TypeOfData;
	StableData.DLC = pReference->DLC;

	do
	{
		if(pReference->TypeOfData == IoTDataType_ArrayDualTilt){
			IoTDataArrayDualTilt_t *pStable = (IoTDataArrayDualTilt_t *)&StableData.Frame;
			IoTDataArrayDualTilt_t *pRef = (IoTDataArrayDualTilt_t *)&pReference->Frame;
			IoTDataArrayDualTilt_t *pD1 = (IoTDataArrayDualTilt_t *)&pPast->Frame;
			IoTDataArrayDualTilt_t *pD2 = (IoTDataArrayDualTilt_t *)&pNew->Frame;

			errorD1 = MIIOT_DATA_DECODE_ANGLE(pRef->Sensor[i].AxisX) - MIIOT_DATA_DECODE_ANGLE(pD1->Sensor[i].AxisX);
			errorD2 = MIIOT_DATA_DECODE_ANGLE(pRef->Sensor[i].AxisX) - MIIOT_DATA_DECODE_ANGLE(pD2->Sensor[i].AxisX);

			pStable->Sensor[i].AxisX = (MATH_ABS(errorD1) <= MATH_ABS(errorD2)) ? pD1->Sensor[i].AxisX : pD2->Sensor[i].AxisX;

			errorD1 = MIIOT_DATA_DECODE_ANGLE(pRef->Sensor[i].AxisY) - MIIOT_DATA_DECODE_ANGLE(pD1->Sensor[i].AxisY);
			errorD2 = MIIOT_DATA_DECODE_ANGLE(pRef->Sensor[i].AxisY) - MIIOT_DATA_DECODE_ANGLE(pD2->Sensor[i].AxisY);

			pStable->Sensor[i].AxisY = (MATH_ABS(errorD1) <= MATH_ABS(errorD2)) ? pD1->Sensor[i].AxisY : pD2->Sensor[i].AxisY;

			if(cnt == 0){
				cnt = MIIOT_PAYLOAD_TO_DUALARRAY_COUNT(pReference->DLC);
			}
		}
		else if(pReference->TypeOfData == IoTDataType_ArraySingleTilt){
			IoTDataArraySingleTilt_t *pStable = (IoTDataArraySingleTilt_t *)&StableData.Frame;
			IoTDataArraySingleTilt_t *pRef = (IoTDataArraySingleTilt_t *)&pReference->Frame;
			IoTDataArraySingleTilt_t *pD1 = (IoTDataArraySingleTilt_t *)&pPast->Frame;
			IoTDataArraySingleTilt_t *pD2 = (IoTDataArraySingleTilt_t *)&pNew->Frame;

			errorD1 = MIIOT_DATA_DECODE_ANGLE(pRef->Sensor[i].Axis) - MIIOT_DATA_DECODE_ANGLE(pD1->Sensor[i].Axis);
			errorD2 = MIIOT_DATA_DECODE_ANGLE(pRef->Sensor[i].Axis) - MIIOT_DATA_DECODE_ANGLE(pD2->Sensor[i].Axis);

			pStable->Sensor[i].Axis = (MATH_ABS(errorD1) <= MATH_ABS(errorD2)) ? pD1->Sensor[i].Axis : pD2->Sensor[i].Axis;

			if(cnt == 0){
				cnt = MIIOT_PAYLOAD_TO_SINGLEARRAY_COUNT(pReference->DLC);
			}
		}
		else{
			break;
		}
	}
	while(++i < cnt);

	return StableData;
}

oRange_t MiMain_GetDataError(IoT_DataPacket_t *pReference, IoT_DataPacket_t *pCompare)
{
	oRange_t ragne = {3.402823466E+38, -3.402823466E+38};
	double error;
	uint32_t i = 0;
	uint32_t cnt = 0;

	if(!pReference || !pCompare || pReference->TypeOfData != pCompare->TypeOfData || pReference->DLC == 0){
		ragne.Min = 0;
		ragne.Max = 0;
		return ragne;
	}

	do
	{
		if(pReference->TypeOfData == IoTDataType_ArrayDualTilt){
			IoTDataArrayDualTilt_t *ref = (IoTDataArrayDualTilt_t *)&pReference->Frame;
			IoTDataArrayDualTilt_t *cmp = (IoTDataArrayDualTilt_t *)&pCompare->Frame;

			error = MIIOT_DATA_DECODE_ANGLE(ref->Sensor[i].AxisX) - MIIOT_DATA_DECODE_ANGLE(cmp->Sensor[i].AxisX);

			ragne.Min = MATH_MIN(error, ragne.Min);
			ragne.Max = MATH_MAX(error, ragne.Max);

			error = MIIOT_DATA_DECODE_ANGLE(ref->Sensor[i].AxisY) - MIIOT_DATA_DECODE_ANGLE(cmp->Sensor[i].AxisY);

			ragne.Min = MATH_MIN(error, ragne.Min);
			ragne.Max = MATH_MAX(error, ragne.Max);

			if(cnt == 0){
				cnt = MIIOT_PAYLOAD_TO_DUALARRAY_COUNT(pReference->DLC);
			}
		}
		else if(pReference->TypeOfData == IoTDataType_ArraySingleTilt){
			IoTDataArraySingleTilt_t *ref = (IoTDataArraySingleTilt_t *)&pReference->Frame;
			IoTDataArraySingleTilt_t *cmp = (IoTDataArraySingleTilt_t *)&pCompare->Frame;

			error = MIIOT_DATA_DECODE_ANGLE(ref->Sensor[i].Axis) - MIIOT_DATA_DECODE_ANGLE(cmp->Sensor[i].Axis);

			ragne.Min = MATH_MIN(error, ragne.Min);
			ragne.Max = MATH_MAX(error, ragne.Max);

			if(cnt == 0){
				cnt = MIIOT_PAYLOAD_TO_SINGLEARRAY_COUNT(pReference->DLC);
			}
		}
		else{
			break;
		}
	}
	while(++i < cnt);

	return ragne;
}

oResult_t MiMain_UpdateMeasure(IoT_DataPacket_t **ppPacket)
{
	static uint8_t UpdateMeasureStep = 0;
	static uint8_t ChannelNo = 0;
	static IoT_DataPacket_t SmaplingData;
	static IoT_DataPacket_t PastSampling[MEASUREMENT_CHANNEL_MAXCOUNT];
	static IoT_DataPacket_t StableDataBuffer[MEASUREMENT_CHANNEL_MAXCOUNT];
	static uint8_t TryCount[MEASUREMENT_CHANNEL_MAXCOUNT] = {0,};
	static uint8_t ChannelDone[MEASUREMENT_CHANNEL_MAXCOUNT] = {0,};
	static uint8_t ConsistentCount[MEASUREMENT_CHANNEL_MAXCOUNT] = {0,};
	static IoT_DataPacket_t LastSamplingData[MEASUREMENT_CHANNEL_MAXCOUNT];
	static uint32_t TryTimer[MEASUREMENT_CHANNEL_MAXCOUNT] = {0,};
	uint32_t i;
	oResult_t result = RESULT_RUN;
	IoTChannelConfig_t *pConfig;

	if(ChannelNo < MEASUREMENT_CHANNEL_MAXCOUNT){
		pConfig = &MiIoT_Parameter.ChannelConfig[ChannelNo];
	}

	switch(UpdateMeasureStep)
	{
		default:
			UpdateMeasureStep = 0; // @suppress("No break at end of case")
		case 0:
			oSerial_Log("UpdateMeasure", "start\r\n");
			memset(&TryCount, 0, sizeof(TryCount));
			memset(&TryTimer, 0, sizeof(TryTimer));
			memset(ChannelDone, 0, sizeof(ChannelDone));
			memset(ConsistentCount, 0, sizeof(ConsistentCount));
			ChannelNo = 0;
			UpdateMeasureStep++; // @suppress("No break at end of case")
		case 1:
			if(ChannelNo >= MEASUREMENT_CHANNEL_MAXCOUNT){
				ChannelNo = 0;
				result = RESULT_DONE;

				for(i=0; i<MEASUREMENT_CHANNEL_MAXCOUNT; i++){
					if(!ChannelDone[i]){
						result = RESULT_WAIT;
					}
				}
			}
			else if((pConfig->TypeOfSensor == IoTSensorType_ArrayDualTilt || pConfig->TypeOfSensor == IoTSensorType_ArraySingleTilt) && pConfig->Properties.Array.CountOfSensor > 0){
				SmaplingData.DLC = 0;
				SmaplingData.TypeOfData = IoTDataType_NULL;
				UpdateMeasureStep++;
			}
			else{
				ChannelDone[ChannelNo] = 1;
				ChannelNo++;
			}
			break;
		case 2:
			if(pConfig->RetryCount && (ChannelDone[ChannelNo] || !oTMR_Elapsed(&TryTimer[ChannelNo], SECOND_TO_MS(pConfig->RetryInterval), TICKBASE_SYSTICK))){
				UpdateMeasureStep = 1;
				ChannelNo++;
			}
			else{
				UpdateMeasureStep++;
				oSerial_Log("UpdateMeasure", "data sampling | %s\r\n", MiIoT_SensorTypeToString(pConfig->TypeOfSensor));
			}
			break;
		case 3:
			switch(Measurement_Sensor(ChannelNo+1, &SmaplingData))
			{
				case RESULT_OK:
					UpdateMeasureStep++;
					break;
				case RESULT_ERROR:
					UpdateMeasureStep = 5; // case 4(오차비교) 건너뜀
					oSerial_Log("UpdateMeasure", "sampling error | %s\r\n", MiIoT_SensorTypeToString(pConfig->TypeOfSensor));
					break;
				default:
					break;
			}
			break;
		case 4:
			oRange_t error;

			error = MiMain_GetDataError(&PastSampling[ChannelNo], &SmaplingData);

			if(PastSampling[ChannelNo].DLC == 0 || (MATH_ABS(error.Min) <= pConfig->ErrorTolerance && MATH_ABS(error.Max) <= pConfig->ErrorTolerance) || pConfig->ErrorTolerance == 0 || pConfig->RetryCount <= 0){
				//오차범위 내 데이터
				*ppPacket = &SmaplingData;
				PastSampling[ChannelNo] = SmaplingData;

				ChannelDone[ChannelNo] = 1;
			}
			else {
				//오차범위 초과 데이터
				if(TryCount[ChannelNo] > 0){
					StableDataBuffer[ChannelNo] = MiMain_GetStableData(&PastSampling[ChannelNo], &StableDataBuffer[ChannelNo], &SmaplingData);

					// 자기 일관성 검증: 새 측정값끼리 일치하는지 확인
					oRange_t newVsLast = MiMain_GetDataError(&LastSamplingData[ChannelNo], &SmaplingData);

					if(MATH_ABS(newVsLast.Min) <= pConfig->ErrorTolerance && MATH_ABS(newVsLast.Max) <= pConfig->ErrorTolerance){
						ConsistentCount[ChannelNo]++;
						oSerial_Log("UpdateMeasure", "CH%d Consistent=%d", ChannelNo+1, ConsistentCount[ChannelNo]);

						if(ConsistentCount[ChannelNo] >= 2){
							// 연속 2회 일치 - 센서 실제 변화 확정
							PastSampling[ChannelNo] = SmaplingData;
							*ppPacket = &PastSampling[ChannelNo];
							ChannelDone[ChannelNo] = 1;
							oSerial_Log("UpdateMeasure", "CH%d Done (sensor changed)", ChannelNo+1);
							LastSamplingData[ChannelNo] = SmaplingData;
							UpdateMeasureStep++;
							break;
						}
					}
					else{
						ConsistentCount[ChannelNo] = 0;
					}
				}
				else{
					StableDataBuffer[ChannelNo] = SmaplingData;
				}

				LastSamplingData[ChannelNo] = SmaplingData;
				TryTimer[ChannelNo] = oTMR_GetTick(TICKBASE_SYSTICK);
				TryCount[ChannelNo]++;

				error = MiMain_GetDataError(&PastSampling[ChannelNo], &StableDataBuffer[ChannelNo]);

				if(TryCount[ChannelNo] >= pConfig->RetryCount || (MATH_ABS(error.Min) <= pConfig->ErrorTolerance && MATH_ABS(error.Max) <= pConfig->ErrorTolerance)){
					PastSampling[ChannelNo] = StableDataBuffer[ChannelNo];
					*ppPacket = &PastSampling[ChannelNo];

					ChannelDone[ChannelNo] = 1;
				}
			}

			UpdateMeasureStep++;
			break;
		case 5:
			if(ChannelDone[ChannelNo]){
				if(ChannelNo >= (MEASUREMENT_CHANNEL_MAXCOUNT-1)){
					//재시도 횟수가 초과 했거나, 측정을 모두 완료 햇는지 확인
					for(i=0; i<MEASUREMENT_CHANNEL_MAXCOUNT; i++){
						if(!ChannelDone[i]){
							result = RESULT_WAIT;
						}
					}

					if(result != RESULT_WAIT){
						result = RESULT_DONE;
					}
				}
				else{
					result = RESULT_OK;
				}
			}

			UpdateMeasureStep = 1;
			ChannelNo = (ChannelNo+1) % MEASUREMENT_CHANNEL_MAXCOUNT;
			break;
	}

	if(result != RESULT_RUN){
		if(result == RESULT_DONE){
			UpdateMeasureStep = 0;
			oSerial_Log("UpdateMeasure", "finish\r\n");
		}
		else if(result == RESULT_WAIT){
			UpdateMeasureStep = 1;
		}
	}

	return result;
}

oResult_t MiMain_UpdateSampling(IoT_DataPacket_t **ppPacket)
{
	static uint8_t UpdateSamplingStep = 0;
	static uint8_t ChannelNo = 0;
	static IoT_DataPacket_t SmaplingData;
	oResult_t result = RESULT_RUN;
	IoTChannelConfig_t *pConfig;

	if(ChannelNo < MEASUREMENT_CHANNEL_MAXCOUNT){
		pConfig = &MiIoT_Parameter.ChannelConfig[ChannelNo];
	}

	switch(UpdateSamplingStep)
	{
		default:
			UpdateSamplingStep = 0; // @suppress("No break at end of case")
		case 0:
			oSerial_Log("UpdateSampling", "start\r\n");
			ChannelNo = 0;
			UpdateSamplingStep++; // @suppress("No break at end of case")
		case 1:
			if(ChannelNo >= MEASUREMENT_CHANNEL_MAXCOUNT){
				result = RESULT_DONE;
			}
			else if((pConfig->TypeOfSensor == IoTSensorType_ArrayDualTilt || pConfig->TypeOfSensor == IoTSensorType_ArraySingleTilt) && pConfig->Properties.Array.CountOfSensor > 0){
				SmaplingData.DLC = 0;
				SmaplingData.TypeOfData = IoTDataType_NULL;
				UpdateSamplingStep++;
				oSerial_Log("UpdateSampling", "data sampling | %s\r\n", MiIoT_SensorTypeToString(pConfig->TypeOfSensor));
			}
			else{
				ChannelNo++;
			}
			break;
		case 2:
			switch(Measurement_Sensor(ChannelNo+1, &SmaplingData))
			{
				case RESULT_OK:
					*ppPacket = &SmaplingData;
					result = RESULT_OK;
					UpdateSamplingStep++;
					break;
				case RESULT_ERROR:
					UpdateSamplingStep++;
					oSerial_Log("UpdateSampling", "sampling error | %s\r\n", MiIoT_SensorTypeToString(pConfig->TypeOfSensor));
					break;
				default:
					break;
			}
			break;
		case 3:
			UpdateSamplingStep = 1;
			ChannelNo++;
			break;
	}

	if(result != RESULT_RUN && result != RESULT_OK){
		ChannelNo = 0;
		UpdateSamplingStep = 0;
	}

	return result;
}

oResult_t MiMain_UpdateStatus()
{
	oResult_t result = RESULT_RUN;

	MiIoT_Status.SoftwareVersion = (uint16_t)(MI_SW_REVISION*100);

	if((result = Measurement_Supply(20)) == RESULT_OK){
		MiIoT_Status.UpdateTimestmap = oTMR_GetTick(TICKBASE_SYSTICK);
		MiIoT_Status.StatusBits.SystemSupplyVoltTooLow = GPIOs.ADC.SystemSupply < SYSTEM_SUPPLY_LOW_LIMIT;
		MiIoT_Status.StatusBits.ClockSupplyVoltTooLow = GPIOs.ADC.InternalBAT < RTC_SUPPLY_LOW_LIMIT;
		MiIoT_Status.StatusBits.Okay = (MiIoT_Status.StatusBits.Bits & 0xFFFFFFFE) == 0 ? 1 : 0;
		MiIoT_Status.SystemSupply = GPIOs.ADC.SystemSupply;

		oSerial_Log("IoTStatus", "SupplyVolt : %d | SWVersion : %d | StatusBit : %d\r\n", (int)MiIoT_Status.SystemSupply, (int)MiIoT_Status.SoftwareVersion, (int)MiIoT_Status.StatusBits.Bits);
	}

	return result;
}


void MiMain (void)
{
	static uint8_t MiMainStep = 0;

	Native_WatchDog(SECOND_TO_MS(11));

	switch(MiMainStep)
	{
		case 0: //Init
			MiIoT_SamplingCallback = MiMain_UpdateSampling;
			MiIoT_MeasurementCallback = MiMain_UpdateMeasure;
			MiIoT_StatusCallback = MiMain_UpdateStatus;
			MiIoT_IOControlCallback = MiMain_GPIOControl;
			MiIoT_GPIOInitCallback = main_GPIOInit;
			MiIoT_GPIODeInitCallback = MiMain_GPIODeInit;

			MiMainStep++;
		case 1:
			HAL_QSPI_DeInit(&hqspi);
			MiMainStep++;
			break;
		default:
			MiStorage();
			MiIoT(&huart3);
			MiSerial(&hlpuart1);
			break;
	}
}
