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

#define SYSTEM_SUPPLY_LOW_LIMIT		3200
#define RTC_SUPPLY_LOW_LIMIT		2100

oIO_t DO_ADC_REF_EANBLE			= {DO_ADC_REF_ENABLE_GPIO_Port,			DO_ADC_REF_ENABLE_Pin,		IO_LOW};
oIO_t DO_NAND_ENABLE			= {DO_NAND_ENABLE_GPIO_Port,			DO_NAND_ENABLE_Pin,			IO_LOW};
oIO_t DO_POWEROUT_ENALBE0		= {DO_POWEROUT_ENABLE_0_GPIO_Port, 		DO_POWEROUT_ENABLE_0_Pin,	IO_LOW};
oIO_t DO_POWEROUT_ENALBE1		= {DO_POWEROUT_ENABLE_1_GPIO_Port, 		DO_POWEROUT_ENABLE_1_Pin,	IO_LOW};
oIO_t DO_CAN_EANBLE				= {DO_CAN_ENABLE_GPIO_Port,				DO_CAN_ENABLE_Pin,		 	IO_HIGH};
oIO_t DO_POWEROUT_SELECT		= {DO_POWEROUT_CH_SELECT_GPIO_Port,		DO_POWEROUT_CH_SELECT_Pin,	IO_HIGH};
oIO_t DO_AMP_ENABLE				= {DO_AMP_ENALBE_GPIO_Port,				DO_AMP_ENALBE_Pin,			IO_HIGH};
oIO_t DO_CURRENT_MODE_ENABLE	= {DO_CURRENT_MODE_ENABLE_GPIO_Port,	DO_CURRENT_MODE_ENABLE_Pin,	IO_HIGH};
oIO_t DO_POWEROUT_5V_ENABLE		= {DO_POWEROUT_5V_ENABLE_GPIO_Port,		DO_POWEROUT_5V_ENABLE_Pin,	IO_HIGH};

oIO_t DO_LORA_ENABLE			= {DO_LORA_ENABLE_GPIO_Port,		DO_LORA_ENABLE_Pin,			IO_LOW};
oIO_t DO_LED_OPERATING			= {DO_LED_OPERATING_GPIO_Port,		DO_LED_OPERATING_Pin,		IO_HIGH};
oIO_t DI_USB_CONNECTED			= {DI_USBC_CONNECTED_GPIO_Port,		DI_USBC_CONNECTED_Pin,		IO_HIGH};

GPIOs_t GPIOs;
oDebounce_t DB_USBConnected	= DEBOUNCE_INITIALIZER(100,100);
oDebounce_t DebounceError	= DEBOUNCE_INITIALIZER(1000,300);

oResult_t MiMain_GPIOControl()
{
	GPIOs.DO.OperatingLED = MiIoT_LED;

	IO_WRITE(DO_LORA_ENABLE, GPIOs.DO.LoRaEnable);
	IO_WRITE(DO_ADC_REF_EANBLE, GPIOs.DO.ADCRefEnable);
	IO_WRITE(DO_NAND_ENABLE, GPIOs.DO.NANDEnable);
	IO_WRITE(DO_POWEROUT_ENALBE0, GPIOs.DO.PwrEnable_U0);
	IO_WRITE(DO_POWEROUT_ENALBE1, GPIOs.DO.PwrEnable_U1);

	IO_WRITE(DO_CAN_EANBLE, GPIOs.DO.CANEnable);
	IO_WRITE(DO_POWEROUT_SELECT, GPIOs.DO.PwrSupplySel);
	IO_WRITE(DO_AMP_ENABLE, GPIOs.DO.AmpEnable);
	IO_WRITE(DO_CURRENT_MODE_ENABLE, GPIOs.DO.CurrentModeEnable);
	IO_WRITE(DO_POWEROUT_5V_ENABLE, GPIOs.DO.PwrSupply5VEnable);
	IO_WRITE(DO_LED_OPERATING, GPIOs.DO.OperatingLED);

	oDebounce_GPIO(DI_USB_CONNECTED, &DB_USBConnected);
	GPIOs.DI.UsbConnected = DB_USBConnected.Output || DB_USBConnected.Input;

	uint32_t IORun = 0;
	IORun += GPIOs.DO.LoRaEnable && !MiLoRa_IsSleep;
	IORun += GPIOs.DO.ADCRefEnable;
	IORun += GPIOs.DO.NANDEnable;
	IORun += GPIOs.DO.PwrEnable_U0;
	IORun += GPIOs.DO.PwrEnable_U1;
	IORun += GPIOs.DO.CANEnable;
	IORun += GPIOs.DO.PwrSupplySel;
	IORun += GPIOs.DO.OperatingLED;

	IORun += GPIOs.DO.AmpEnable;
	IORun += GPIOs.DO.CurrentModeEnable;
	IORun += GPIOs.DO.PwrSupply5VEnable;

	IORun += GPIOs.DI.UsbConnected;

	return IORun ? RESULT_RUN : RESULT_ERROR;
}

void MiMain_GPIODeInit(void)
{
	GPIO_InitTypeDef cfg = {.Mode = GPIO_MODE_ANALOG, .Pull = GPIO_NOPULL};

	HAL_I2C_DeInit(&hi2c2);
	HAL_QSPI_DeInit(&hqspi);

	if(!MiLoRa_IsSleep){
		cfg.Pin = DO_LORA_ENABLE.Pin;
		HAL_GPIO_Init(DO_LORA_ENABLE.Port, &cfg);
	}

	cfg.Pin = DO_NAND_ENABLE.Pin;
	HAL_GPIO_Init(DO_NAND_ENABLE.Port, &cfg);
	cfg.Pin = DO_ADC_REF_EANBLE.Pin;
	HAL_GPIO_Init(DO_ADC_REF_EANBLE.Port, &cfg);
	cfg.Pin = DO_POWEROUT_ENALBE0.Pin;
	HAL_GPIO_Init(DO_POWEROUT_ENALBE0.Port, &cfg);
	cfg.Pin = DO_POWEROUT_ENALBE1.Pin;
	HAL_GPIO_Init(DO_POWEROUT_ENALBE1.Port, &cfg);
	cfg.Pin = DO_PW_EXTCOM_Pin;
	HAL_GPIO_Init(DO_PW_EXTCOM_GPIO_Port, &cfg);
	cfg.Pin = DO_CAN_EANBLE.Pin;
	HAL_GPIO_Init(DO_CAN_EANBLE.Port, &cfg);
	cfg.Pin = DO_POWEROUT_SELECT.Pin;
	HAL_GPIO_Init(DO_POWEROUT_SELECT.Port, &cfg);
	cfg.Pin = DO_AMP_ENABLE.Pin;
	HAL_GPIO_Init(DO_AMP_ENABLE.Port, &cfg);
	cfg.Pin = DO_CURRENT_MODE_ENABLE.Pin;
	HAL_GPIO_Init(DO_CURRENT_MODE_ENABLE.Port, &cfg);
	cfg.Pin = DO_POWEROUT_5V_ENABLE.Pin;
	HAL_GPIO_Init(DO_POWEROUT_5V_ENABLE.Port, &cfg);
	cfg.Pin = DO_LED_OPERATING.Pin;
	HAL_GPIO_Init(DO_LED_OPERATING.Port, &cfg);

	cfg.Mode = GPIO_MODE_IT_RISING;
	cfg.Pull = GPIO_PULLDOWN;
	cfg.Pin = DI_USBC_CONNECTED_Pin;
	HAL_GPIO_Init(DI_USBC_CONNECTED_GPIO_Port, &cfg);
}

void MiMain_GPIOInit(void)
{
	GPIO_InitTypeDef cfg = {.Speed = GPIO_SPEED_FREQ_LOW};

	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	MiMain_GPIOControl();

	//OD outputs, NOPULL, LOW speed | LORA, NAND, ADC_REF, PWR_EN0, PWR_EN1, PW_EXTCOM
	cfg.Mode  = GPIO_MODE_OUTPUT_OD;
	cfg.Pull  = GPIO_NOPULL;
	cfg.Speed = GPIO_SPEED_FREQ_LOW;

	if(!MiLoRa_IsSleep){
		cfg.Pin   = DO_LORA_ENABLE.Pin;
		HAL_GPIO_Init(DO_LORA_ENABLE.Port, &cfg);
	}

	cfg.Pin   = DO_NAND_ENABLE.Pin;
	HAL_GPIO_Init(DO_NAND_ENABLE.Port, &cfg);
	cfg.Pin   = DO_ADC_REF_EANBLE.Pin;
	HAL_GPIO_Init(DO_ADC_REF_EANBLE.Port, &cfg);
	cfg.Pin   = DO_POWEROUT_ENALBE0.Pin;
	HAL_GPIO_Init(DO_POWEROUT_ENALBE0.Port, &cfg);
	cfg.Pin   = DO_POWEROUT_ENALBE1.Pin;
	HAL_GPIO_Init(DO_POWEROUT_ENALBE1.Port, &cfg);
	cfg.Pin   = DO_PW_EXTCOM_Pin;
	HAL_GPIO_Init(DO_PW_EXTCOM_GPIO_Port, &cfg);

	//PP outputs, PULLUP, LOW speed | CAN_ENABLE, POWEROUT_SELECT
	cfg.Mode  = GPIO_MODE_OUTPUT_PP;
	cfg.Pull  = GPIO_PULLUP;
	cfg.Speed = GPIO_SPEED_FREQ_LOW;

	cfg.Pin   = DO_CAN_EANBLE.Pin;
	HAL_GPIO_Init(DO_CAN_EANBLE.Port, &cfg);
	cfg.Pin   = DO_POWEROUT_SELECT.Pin;
	HAL_GPIO_Init(DO_POWEROUT_SELECT.Port, &cfg);

	//PP outputs, NOPULL, LOW speed | AMP_ENABLE, CURRENT_MODE_ENABLE, POWEROUT_5V_ENABLE
	cfg.Mode  = GPIO_MODE_OUTPUT_PP;
	cfg.Pull  = GPIO_NOPULL;
	cfg.Speed = GPIO_SPEED_FREQ_LOW;

	cfg.Pin   = DO_AMP_ENABLE.Pin;
	HAL_GPIO_Init(DO_AMP_ENABLE.Port, &cfg);
	cfg.Pin   = DO_CURRENT_MODE_ENABLE.Pin;
	HAL_GPIO_Init(DO_CURRENT_MODE_ENABLE.Port, &cfg);
	cfg.Pin   = DO_POWEROUT_5V_ENABLE.Pin;
	HAL_GPIO_Init(DO_POWEROUT_5V_ENABLE.Port, &cfg);

	//PP output, NOPULL, VERY_HIGH speed | OPERATING LED
	cfg.Mode  = GPIO_MODE_OUTPUT_PP;
	cfg.Pull  = GPIO_NOPULL;
	cfg.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

	cfg.Pin   = DO_LED_OPERATING.Pin;
	HAL_GPIO_Init(DO_LED_OPERATING.Port, &cfg);

	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

	HAL_ADC_DeInit(&hadc1);
	HAL_ADC_Init(&hadc1);
}

IoT_DataPacket_t MiMain_GetArrayStableData(IoT_DataPacket_t *pReference, IoT_DataPacket_t *pPast, IoT_DataPacket_t *pNew)
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

			errorD1 = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray[i].AxisX) - MIIOT_DATA_DECODE_ANGLE(pD1->TiltArray[i].AxisX);
			errorD2 = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray[i].AxisX) - MIIOT_DATA_DECODE_ANGLE(pD2->TiltArray[i].AxisX);

			pStable->TiltArray[i].AxisX = (MATH_ABS(errorD1) <= MATH_ABS(errorD2)) ? pD1->TiltArray[i].AxisX : pD2->TiltArray[i].AxisX;

			errorD1 = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray[i].AxisY) - MIIOT_DATA_DECODE_ANGLE(pD1->TiltArray[i].AxisY);
			errorD2 = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray[i].AxisY) - MIIOT_DATA_DECODE_ANGLE(pD2->TiltArray[i].AxisY);

			pStable->TiltArray[i].AxisY = (MATH_ABS(errorD1) <= MATH_ABS(errorD2)) ? pD1->TiltArray[i].AxisY : pD2->TiltArray[i].AxisY;

			if(cnt == 0){
				cnt = MIIOT_PAYLOAD_TO_DUALARRAY_COUNT(pReference->DLC);
			}
		}
		else if(pReference->TypeOfData == IoTDataType_ArraySingleTilt){
			IoTDataArraySingleTilt_t *pStable = (IoTDataArraySingleTilt_t *)&StableData.Frame;
			IoTDataArraySingleTilt_t *pRef = (IoTDataArraySingleTilt_t *)&pReference->Frame;
			IoTDataArraySingleTilt_t *pD1 = (IoTDataArraySingleTilt_t *)&pPast->Frame;
			IoTDataArraySingleTilt_t *pD2 = (IoTDataArraySingleTilt_t *)&pNew->Frame;

			errorD1 = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray[i].Axis) - MIIOT_DATA_DECODE_ANGLE(pD1->TiltArray[i].Axis);
			errorD2 = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray[i].Axis) - MIIOT_DATA_DECODE_ANGLE(pD2->TiltArray[i].Axis);

			pStable->TiltArray[i].Axis = (MATH_ABS(errorD1) <= MATH_ABS(errorD2)) ? pD1->TiltArray[i].Axis : pD2->TiltArray[i].Axis;

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

oRange_t MiMain_GetArrayDataError(IoT_DataPacket_t *pReference, IoT_DataPacket_t *pCompare)
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

			error = MIIOT_DATA_DECODE_ANGLE(ref->TiltArray[i].AxisX) - MIIOT_DATA_DECODE_ANGLE(cmp->TiltArray[i].AxisX);

			ragne.Min = MATH_MIN(error, ragne.Min);
			ragne.Max = MATH_MAX(error, ragne.Max);

			error = MIIOT_DATA_DECODE_ANGLE(ref->TiltArray[i].AxisY) - MIIOT_DATA_DECODE_ANGLE(cmp->TiltArray[i].AxisY);

			ragne.Min = MATH_MIN(error, ragne.Min);
			ragne.Max = MATH_MAX(error, ragne.Max);

			if(cnt == 0){
				cnt = MIIOT_PAYLOAD_TO_DUALARRAY_COUNT(pReference->DLC);
			}
		}
		else if(pReference->TypeOfData == IoTDataType_ArraySingleTilt){
			IoTDataArraySingleTilt_t *ref = (IoTDataArraySingleTilt_t *)&pReference->Frame;
			IoTDataArraySingleTilt_t *cmp = (IoTDataArraySingleTilt_t *)&pCompare->Frame;

			error = MIIOT_DATA_DECODE_ANGLE(ref->TiltArray[i].Axis) - MIIOT_DATA_DECODE_ANGLE(cmp->TiltArray[i].Axis);

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

/* TypeOfData 기반 오차 디스패처 — Array는 Min/Max 범위, Analog는 단일값(Min=Max) */
oRange_t MiMain_GetMeasureError(IoT_DataPacket_t *pReference, IoT_DataPacket_t *pCompare);

/* TypeOfData 기반 Stable 데이터 디스패처 */
IoT_DataPacket_t MiMain_GetMeasureStableData(IoT_DataPacket_t *pReference, IoT_DataPacket_t *pPast, IoT_DataPacket_t *pNew);

/* Analog 단일 채널(Channel[1]) 측정값 오차 */
double MiMain_GetAnalogDataError(IoT_DataPacket_t *pReference, IoT_DataPacket_t *pCompare)
{
	if(!pReference || !pCompare ||
	   pReference->TypeOfData != IoTDataType_Analog ||
	   pCompare->TypeOfData != IoTDataType_Analog ||
	   pReference->DLC == 0){
		return 0;
	}

	IoTDataAnalog_t *ref = (IoTDataAnalog_t *)&pReference->Frame;
	IoTDataAnalog_t *cmp = (IoTDataAnalog_t *)&pCompare->Frame;

	/* Type 변경은 무조건 큰 오차로 판단 — 최대값 반환하여 재측정 유도 */
	if(ref->Channel[1].Type != cmp->Channel[1].Type){
		return 3.402823466E+38;
	}

	return (double)ref->Channel[1].Analog - (double)cmp->Channel[1].Analog;
}

/* Analog Best (오차 작은 쪽) 데이터 선택 */
IoT_DataPacket_t MiMain_GetAnalogStableData(IoT_DataPacket_t *pReference, IoT_DataPacket_t *pPast, IoT_DataPacket_t *pNew)
{
	IoT_DataPacket_t StableData;
	double errPast;
	double errNew;

	if(pReference->DLC == 0 || pReference->TypeOfData != pNew->TypeOfData){
		return *pNew;
	}

	StableData.TypeOfData = pReference->TypeOfData;
	StableData.DLC = pReference->DLC;

	errPast = MATH_ABS(MiMain_GetAnalogDataError(pReference, pPast));
	errNew  = MATH_ABS(MiMain_GetAnalogDataError(pReference, pNew));

	IoTDataAnalog_t *stable = (IoTDataAnalog_t *)&StableData.Frame;
	IoTDataAnalog_t *src    = (errNew < errPast) ? (IoTDataAnalog_t *)&pNew->Frame
	                                             : (IoTDataAnalog_t *)&pPast->Frame;
	stable->Channel[1].Type   = src->Channel[1].Type;
	stable->Channel[1].Analog = src->Channel[1].Analog;
	stable->Time = src->Time;

	return StableData;
}

/* TypeOfData 기반 디스패처 — case 4 본문 변경 없이 Array/Analog 양쪽 지원 */
oRange_t MiMain_GetMeasureError(IoT_DataPacket_t *pReference, IoT_DataPacket_t *pCompare)
{
	if(pReference && pReference->TypeOfData == IoTDataType_Analog){
		double err = MiMain_GetAnalogDataError(pReference, pCompare);
		oRange_t r;
		r.Min = err;
		r.Max = err;
		return r;
	}
	return MiMain_GetArrayDataError(pReference, pCompare);
}

IoT_DataPacket_t MiMain_GetMeasureStableData(IoT_DataPacket_t *pReference, IoT_DataPacket_t *pPast, IoT_DataPacket_t *pNew)
{
	if(pReference && pReference->TypeOfData == IoTDataType_Analog){
		return MiMain_GetAnalogStableData(pReference, pPast, pNew);
	}
	return MiMain_GetArrayStableData(pReference, pPast, pNew);
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
			memset(&SmaplingData, 0, sizeof(SmaplingData));
			ChannelNo = 0;
			UpdateMeasureStep++; // @suppress("No break at end of case")
		case 1:
			MiSerial_SensorSamplingProgress = (uint8_t)((float)ChannelNo/(float)MEASUREMENT_CHANNEL_MAXCOUNT*100);

			if(MiSerial_UpdateSensorCmd && MiSerial_StopSensorCmd){
				ChannelNo = 0;
				result = RESULT_DONE;
			}
			else if(ChannelNo >= MEASUREMENT_CHANNEL_MAXCOUNT){
				ChannelNo = 0;
				result = RESULT_DONE;

				for(i=0; i<MEASUREMENT_CHANNEL_MAXCOUNT; i++){
					if(!ChannelDone[i]){
						result = RESULT_WAIT;
					}
				}
			}
			else if(((pConfig->TypeOfSensor == IoTSensorType_ArrayDualTilt || pConfig->TypeOfSensor == IoTSensorType_ArraySingleTilt) && pConfig->Properties.Array.CountOfSensor > 0) ||
			        pConfig->TypeOfSensor == IoTSensorType_mV ||
			        pConfig->TypeOfSensor == IoTSensorType_mA){
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
			// Measurement_Sensor가 각 채널의 통합 측정값 단일 패킷 반환 — ChannelNo==0 시점만 호출, 이후 채널은 동일 SamplingData 사용
			if(ChannelNo > 0){
				UpdateMeasureStep++;
				break;
			}

			switch(Measurement_Sensor(&SmaplingData))
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

			error = MiMain_GetMeasureError(&PastSampling[ChannelNo], &SmaplingData);

			if(PastSampling[ChannelNo].DLC == 0 || (MATH_ABS(error.Min) <= pConfig->ErrorTolerance && MATH_ABS(error.Max) <= pConfig->ErrorTolerance) || pConfig->ErrorTolerance == 0 || pConfig->RetryCount <= 0){
				//?ㅼ감踰붿쐞 ???곗씠??
				*ppPacket = &SmaplingData;
				PastSampling[ChannelNo] = SmaplingData;

				ChannelDone[ChannelNo] = 1;
			}
			else {
				//?ㅼ감踰붿쐞 珥덇낵 ?곗씠??
				if(TryCount[ChannelNo] > 0){
					StableDataBuffer[ChannelNo] = MiMain_GetMeasureStableData(&PastSampling[ChannelNo], &StableDataBuffer[ChannelNo], &SmaplingData);

					// 자기 일관성 검증 — 측정값끼리 일치하는지 확인
					oRange_t newVsLast = MiMain_GetMeasureError(&LastSamplingData[ChannelNo], &SmaplingData);

					if(MATH_ABS(newVsLast.Min) <= pConfig->ErrorTolerance && MATH_ABS(newVsLast.Max) <= pConfig->ErrorTolerance){
						ConsistentCount[ChannelNo]++;
						oSerial_Log("UpdateMeasure", "CH%d Consistent=%d", ChannelNo+1, ConsistentCount[ChannelNo]);

						if(ConsistentCount[ChannelNo] >= 2){
							// 연속 2회 일치 — 센서 실제 변화로 확정
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

				error = MiMain_GetMeasureError(&PastSampling[ChannelNo], &StableDataBuffer[ChannelNo]);

				if(TryCount[ChannelNo] >= pConfig->RetryCount || (MATH_ABS(error.Min) <= pConfig->ErrorTolerance && MATH_ABS(error.Max) <= pConfig->ErrorTolerance)){
					PastSampling[ChannelNo] = StableDataBuffer[ChannelNo];
					*ppPacket = &PastSampling[ChannelNo];

					ChannelDone[ChannelNo] = 1;
				}
			}

			UpdateMeasureStep++;
			break;
		case 5:
				// 채널 완료 직후 상위 RESULT_OK 반환(호출자 Mi_IoT)이 즉시 Mailbox 큐잉
				// 전체 완료(RESULT_DONE)는 case 1에서 모든 ChannelDone 확인 후 처리
			if(ChannelDone[ChannelNo]){
				result = RESULT_OK;
			}

			UpdateMeasureStep = 1;
			ChannelNo++;
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
	static uint8_t SampliingIndex = 0;
	static IoT_DataPacket_t SmaplingData[MEASUREMENT_CHANNEL_MAXCOUNT];
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
			memset(SmaplingData, 0, sizeof(SmaplingData));

			ChannelNo = 0;
			SampliingIndex = 0;
			UpdateSamplingStep++; // @suppress("No break at end of case")
			oSerial_Log("UpdateSampling", "start\r\n");
		case 1:
			MiSerial_SensorSamplingProgress = (uint8_t)((float)ChannelNo/(float)MEASUREMENT_CHANNEL_MAXCOUNT*100);

			if(MiSerial_UpdateSensorCmd && MiSerial_StopSensorCmd){
				result = RESULT_DONE;
			}
			else if(ChannelNo >= MEASUREMENT_CHANNEL_MAXCOUNT){
				result = RESULT_DONE;
			}
			else if(((pConfig->TypeOfSensor == IoTSensorType_ArrayDualTilt || pConfig->TypeOfSensor == IoTSensorType_ArraySingleTilt) && pConfig->Properties.Array.CountOfSensor > 0) ||
			        pConfig->TypeOfSensor == IoTSensorType_mV || pConfig->TypeOfSensor == IoTSensorType_mA){

				SmaplingData[ChannelNo].DLC = 0;
				SmaplingData[ChannelNo].TypeOfData = IoTDataType_NULL;
				UpdateSamplingStep++;
				oSerial_Log("UpdateSampling", "data sampling | %s\r\n", MiIoT_SensorTypeToString(pConfig->TypeOfSensor));
			}
			else{
				UpdateSamplingStep = 3;
			}
			break;
		case 2:
			// Measurement_Sensor가 각 채널의 통합 측정값 단일 패킷 반환 — ChannelNo==0 시점만 호출
			if(ChannelNo > 0){
				UpdateSamplingStep = 3;
				break;
			}

			switch(Measurement_Sensor(&SmaplingData[SampliingIndex]))
			{
				case RESULT_OK:
					SampliingIndex++;
					UpdateSamplingStep = 3;
					break;
				case RESULT_ERROR:
					UpdateSamplingStep = 3;
					oSerial_Log("UpdateSampling", "sampling error | %s\r\n", MiIoT_SensorTypeToString(pConfig->TypeOfSensor));
					break;
				default:
					break;
			}
			break;
		case 3:
			if(ChannelNo+1 == MEASUREMENT_CHANNEL_MAXCOUNT){
				*ppPacket = &SmaplingData[0];
				result = RESULT_OK;
			}

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

	Native_WatchDog(MIIOT_SLEEP_TIME*2);

	switch(MiMainStep)
	{
		case 0: //Init
			MiIoT_SamplingCallback = MiMain_UpdateSampling;
			MiIoT_MeasurementCallback = MiMain_UpdateMeasure;
			MiIoT_StatusCallback = MiMain_UpdateStatus;
			MiIoT_IOControlCallback = MiMain_GPIOControl;
			MiIoT_GPIOInitCallback = MiMain_GPIOInit;
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
