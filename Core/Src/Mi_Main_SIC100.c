/*
 * Mi_Main_SIC100.c
 *
 *  Version: 0.2 (2026-06-29)
 */
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

/* SIC100 통합 패킷(DataArray_Type2)의 채널별 오차 검증.
 * 각 활성 채널을 자신의 ErrorTolerance에 대해 검사. 어느 한 채널이라도 초과하면 1, 모두 충족이면 0.
 * tolerance==0 또는 RetryCount==0 채널은 검사 대상 제외(재측정 불요). Analog Type 변경 시 즉시 error. */
uint8_t MiMain_IsError(IoT_DataPacket_t *pReference, IoT_DataPacket_t *pCompare)
{
	double error;
	uint32_t i;
	uint8_t ch;
	IoTChannelConfig_t *pConfig;

	if(!pReference || !pCompare ||
	   pReference->TypeOfData != pCompare->TypeOfData ||
	   pReference->DLC == 0 ||
	   pReference->TypeOfData != IoTDataType_DataArray_Type2){
		return 0;
	}

	IoTDataSIC100_2C_t *pRef = (IoTDataSIC100_2C_t *)&pReference->Frame;
	IoTDataSIC100_2C_t *pCmp = (IoTDataSIC100_2C_t *)&pCompare->Frame;

	for(ch = 0; ch < MEASUREMENT_CHANNEL_MAXCOUNT; ch++){
		pConfig = &MiIoT_Parameter.ChannelConfig[ch];

		if(pConfig->ErrorTolerance == 0 || pConfig->RetryCount == 0){
			continue;
		}

		switch(pConfig->TypeOfSensor){
			case IoTSensorType_ArrayDualTilt:
				if(pRef->TiltArray.Type != IoTSensorType_ArrayDualTilt){
					break;
				}
				for(i = 0; i < pRef->CountOfArraySensor; i++){
					error = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray.Dual[i].AxisX) - MIIOT_DATA_DECODE_ANGLE(pCmp->TiltArray.Dual[i].AxisX);
					if(MATH_ABS(error) > pConfig->ErrorTolerance) return 1;
					error = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray.Dual[i].AxisY) - MIIOT_DATA_DECODE_ANGLE(pCmp->TiltArray.Dual[i].AxisY);
					if(MATH_ABS(error) > pConfig->ErrorTolerance) return 1;
				}
				break;

			case IoTSensorType_ArraySingleTilt:
				if(pRef->TiltArray.Type != IoTSensorType_ArraySingleTilt){
					break;
				}
				for(i = 0; i < pRef->CountOfArraySensor; i++){
					error = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray.Single[i].Axis) - MIIOT_DATA_DECODE_ANGLE(pCmp->TiltArray.Single[i].Axis);
					if(MATH_ABS(error) > pConfig->ErrorTolerance) return 1;
				}
				break;

			case IoTSensorType_mV:
			case IoTSensorType_mA:
				if(pRef->Analog.Type != pConfig->TypeOfSensor){
					break;
				}
				if(pRef->Analog.Type != pCmp->Analog.Type){
					return 1;	/* Type 변경 → 강제 재측정 */
				}
				if(pRef->Analog.Type == IoTSensorType_mV){
					error = MIIOT_DATA_DECODE_mV(pRef->Analog.Data) - MIIOT_DATA_DECODE_mV(pCmp->Analog.Data);
				}
				else{
					error = MIIOT_DATA_DECODE_mA(pRef->Analog.Data) - MIIOT_DATA_DECODE_mA(pCmp->Analog.Data);
				}
				if(MATH_ABS(error) > pConfig->ErrorTolerance) return 1;
				break;

			default:
				break;
		}
	}

	return 0;
}

/* SIC100 통합 패킷(DataArray_Type2)의 안정값 합성 — Analog(CH2) + Tilt(CH1) 양쪽 처리.
 * 두 후보(past/new) 중 reference 와 더 가까운 쪽을 element별로 선택.
 * 메타데이터(Time/Type/Temp/Count/Channel)는 reference 그대로 복사. */
IoT_DataPacket_t MiMain_GetMeasureStableData(IoT_DataPacket_t *pReference, IoT_DataPacket_t *pPast, IoT_DataPacket_t *pNew)
{
	IoT_DataPacket_t StableData;
	double errorD1;
	double errorD2;
	uint32_t i;

	if(pReference->DLC == 0 ||
	   pReference->TypeOfData != pNew->TypeOfData ||
	   pReference->TypeOfData != IoTDataType_DataArray_Type2){
		return *pNew;
	}

	StableData.TypeOfData = pReference->TypeOfData;
	StableData.DLC = pReference->DLC;

	IoTDataSIC100_2C_t *pStable = (IoTDataSIC100_2C_t *)&StableData.Frame;
	IoTDataSIC100_2C_t *pRef    = (IoTDataSIC100_2C_t *)&pReference->Frame;
	IoTDataSIC100_2C_t *pD1     = (IoTDataSIC100_2C_t *)&pPast->Frame;
	IoTDataSIC100_2C_t *pD2     = (IoTDataSIC100_2C_t *)&pNew->Frame;

	pStable->Time                  = pRef->Time;
	pStable->Analog.Type           = pRef->Analog.Type;
	pStable->Analog.Channel        = pRef->Analog.Channel;
	pStable->TiltArray.Type        = pRef->TiltArray.Type;
	pStable->TiltArray.Temperature = pRef->TiltArray.Temperature;
	pStable->CountOfArraySensor    = pRef->CountOfArraySensor;

	/* Analog (CH2) — 단일값 best */
	if(pRef->Analog.Type == IoTSensorType_mV || pRef->Analog.Type == IoTSensorType_mA){
		double dRef, dD1, dD2;
		if(pRef->Analog.Type == IoTSensorType_mV){
			dRef = MIIOT_DATA_DECODE_mV(pRef->Analog.Data);
			dD1  = MIIOT_DATA_DECODE_mV(pD1->Analog.Data);
			dD2  = MIIOT_DATA_DECODE_mV(pD2->Analog.Data);
		}
		else{
			dRef = MIIOT_DATA_DECODE_mA(pRef->Analog.Data);
			dD1  = MIIOT_DATA_DECODE_mA(pD1->Analog.Data);
			dD2  = MIIOT_DATA_DECODE_mA(pD2->Analog.Data);
		}
		errorD1 = dRef - dD1;
		errorD2 = dRef - dD2;
		pStable->Analog.Data = (MATH_ABS(errorD1) <= MATH_ABS(errorD2)) ? pD1->Analog.Data : pD2->Analog.Data;
	}
	else{
		pStable->Analog.Data = pRef->Analog.Data;
	}

	/* Tilt (CH1) — 센서별 axis별 best */
	if(pRef->TiltArray.Type == IoTSensorType_ArrayDualTilt){
		for(i = 0; i < pRef->CountOfArraySensor; i++){
			errorD1 = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray.Dual[i].AxisX) - MIIOT_DATA_DECODE_ANGLE(pD1->TiltArray.Dual[i].AxisX);
			errorD2 = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray.Dual[i].AxisX) - MIIOT_DATA_DECODE_ANGLE(pD2->TiltArray.Dual[i].AxisX);
			pStable->TiltArray.Dual[i].AxisX = (MATH_ABS(errorD1) <= MATH_ABS(errorD2)) ? pD1->TiltArray.Dual[i].AxisX : pD2->TiltArray.Dual[i].AxisX;

			errorD1 = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray.Dual[i].AxisY) - MIIOT_DATA_DECODE_ANGLE(pD1->TiltArray.Dual[i].AxisY);
			errorD2 = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray.Dual[i].AxisY) - MIIOT_DATA_DECODE_ANGLE(pD2->TiltArray.Dual[i].AxisY);
			pStable->TiltArray.Dual[i].AxisY = (MATH_ABS(errorD1) <= MATH_ABS(errorD2)) ? pD1->TiltArray.Dual[i].AxisY : pD2->TiltArray.Dual[i].AxisY;
		}
	}
	else if(pRef->TiltArray.Type == IoTSensorType_ArraySingleTilt){
		for(i = 0; i < pRef->CountOfArraySensor; i++){
			errorD1 = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray.Single[i].Axis) - MIIOT_DATA_DECODE_ANGLE(pD1->TiltArray.Single[i].Axis);
			errorD2 = MIIOT_DATA_DECODE_ANGLE(pRef->TiltArray.Single[i].Axis) - MIIOT_DATA_DECODE_ANGLE(pD2->TiltArray.Single[i].Axis);
			pStable->TiltArray.Single[i].Axis = (MATH_ABS(errorD1) <= MATH_ABS(errorD2)) ? pD1->TiltArray.Single[i].Axis : pD2->TiltArray.Single[i].Axis;
		}
	}

	return StableData;
}

oResult_t MiMain_UpdateMeasure(IoT_DataPacket_t **ppPacket)
{
	static uint8_t UpdateMeasureStep = 0;
	static IoT_DataPacket_t SmaplingData;
	static IoT_DataPacket_t PastSampling;
	static IoT_DataPacket_t StableDataBuffer;
	static IoT_DataPacket_t LastSamplingData;
	static uint8_t TryCount = 0;
	static uint8_t ConsistentCount = 0;
	static uint32_t TryTimer = 0;
	static uint8_t MaxRetryCount = 0;	/* 활성 채널 중 가장 큰 RetryCount */
	static uint8_t MaxRetryInterval = 0;
	static uint8_t HasActiveChannel = 0;
	uint32_t i;
	oResult_t result = RESULT_RUN;

	switch(UpdateMeasureStep)
	{
		default:
			UpdateMeasureStep = 0; // @suppress("No break at end of case")
		case 0:
			oSerial_Log("UpdateMeasure", "start\r\n");
			TryCount = 0;
			TryTimer = 0;
			ConsistentCount = 0;
			memset(&SmaplingData, 0, sizeof(SmaplingData));
			MiSerial_SensorSamplingProgress = 0;

			/* 활성 채널 스캔 — 가장 큰 RetryCount/Interval 채택해 모든 채널의 재측정 요구 충족 */
			HasActiveChannel = 0;
			MaxRetryCount = 0;
			MaxRetryInterval = 0;
			for(i=0; i<MEASUREMENT_CHANNEL_MAXCOUNT; i++){
				IoTChannelConfig_t *pCfg = &MiIoT_Parameter.ChannelConfig[i];
				IoTSensorType_t t = pCfg->TypeOfSensor;
				uint8_t active = 0;

				if((t == IoTSensorType_ArrayDualTilt || t == IoTSensorType_ArraySingleTilt) && pCfg->Properties.Array.CountOfSensor > 0){
					active = 1;
				}
				else if(t == IoTSensorType_mV || t == IoTSensorType_mA){
					active = 1;
				}

				if(active){
					HasActiveChannel = 1;
					if(pCfg->RetryCount > MaxRetryCount) MaxRetryCount = pCfg->RetryCount;
					if(pCfg->RetryInterval > MaxRetryInterval) MaxRetryInterval = pCfg->RetryInterval;
				}
			}
			UpdateMeasureStep++; // @suppress("No break at end of case")
		case 1:
			if(!HasActiveChannel){
				MiSerial_SensorSamplingProgress = 100;
				result = RESULT_DONE;
			}
			else{
				SmaplingData.DLC = 0;
				SmaplingData.TypeOfData = IoTDataType_NULL;
				UpdateMeasureStep++;
			}
			break;
		case 2:
			if(TryCount > 0 && !oTMR_Elapsed(&TryTimer, SECOND_TO_MS(MaxRetryInterval), TICKBASE_SYSTICK)){
				/* retry interval 대기 */
			}
			else{
				UpdateMeasureStep++;
				oSerial_Log("UpdateMeasure", "data sampling\r\n");
			}
			break;
		case 3:
			switch(Measurement_Sensor(&SmaplingData))
			{
				case RESULT_OK:
					UpdateMeasureStep++;
					break;
				case RESULT_ERROR:
					UpdateMeasureStep = 5;	/* case 4 오차비교 건너뜀 */
					oSerial_Log("UpdateMeasure", "sampling error\r\n");
					break;
				default:
					break;
			}
			break;
		case 4: {
			uint8_t accept = 0;

			if(PastSampling.DLC == 0 || !MiMain_IsError(&PastSampling, &SmaplingData)){
				/* 오차범위 내 데이터 */
				PastSampling = SmaplingData;
				*ppPacket = &PastSampling;
				accept = 1;
			}
			else{
				/* 오차범위 초과 데이터 */
				if(TryCount > 0){
					StableDataBuffer = MiMain_GetMeasureStableData(&PastSampling, &StableDataBuffer, &SmaplingData);

					/* 자기 일관성 검증 — 측정값끼리 일치하는지 확인 */
					if(!MiMain_IsError(&LastSamplingData, &SmaplingData)){
						ConsistentCount++;
						oSerial_Log("UpdateMeasure", "Consistent=%d", ConsistentCount);

						if(ConsistentCount >= 2){
							/* 연속 2회 일치 — 센서 실제 변화로 확정 */
							PastSampling = SmaplingData;
							*ppPacket = &PastSampling;
							oSerial_Log("UpdateMeasure", "Done (sensor changed)");
							LastSamplingData = SmaplingData;
							accept = 1;
						}
					}
					else{
						ConsistentCount = 0;
					}
				}
				else{
					StableDataBuffer = SmaplingData;
				}

				if(!accept){
					LastSamplingData = SmaplingData;
					TryTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					TryCount++;

					if(TryCount >= MaxRetryCount || !MiMain_IsError(&PastSampling, &StableDataBuffer)){
						PastSampling = StableDataBuffer;
						*ppPacket = &PastSampling;
						accept = 1;
					}
				}
			}

			UpdateMeasureStep = accept ? 5 : 2;	/* accept면 5(완료 emit), 아니면 2(retry 대기) */
			break;
		}
		case 5:
			result = RESULT_OK;
			UpdateMeasureStep = 6;
			break;
		case 6:
			MiSerial_SensorSamplingProgress = 100;
			result = RESULT_DONE;
			break;
	}

	if(result != RESULT_RUN){
		if(result == RESULT_DONE){
			UpdateMeasureStep = 0;
			oSerial_Log("UpdateMeasure", "finish\r\n");
		}
	}

	return result;
}

oResult_t MiMain_UpdateSampling(IoT_DataPacket_t **ppPacket)
{
	static uint8_t UpdateSamplingStep = 0;
	static IoT_DataPacket_t SmaplingData;
	static uint8_t HasActiveChannel = 0;
	uint32_t i;
	oResult_t result = RESULT_RUN;

	switch(UpdateSamplingStep)
	{
		default:
			UpdateSamplingStep = 0; // @suppress("No break at end of case")
		case 0:
			oSerial_Log("UpdateSampling", "start\r\n");
			memset(&SmaplingData, 0, sizeof(SmaplingData));
			MiSerial_SensorSamplingProgress = 0;

			/* 활성 채널 스캔 — 하나라도 있으면 측정 진행 (재측정 없음) */
			HasActiveChannel = 0;
			for(i=0; i<MEASUREMENT_CHANNEL_MAXCOUNT; i++){
				IoTChannelConfig_t *pCfg = &MiIoT_Parameter.ChannelConfig[i];
				IoTSensorType_t t = pCfg->TypeOfSensor;

				if((t == IoTSensorType_ArrayDualTilt || t == IoTSensorType_ArraySingleTilt) && pCfg->Properties.Array.CountOfSensor > 0){
					HasActiveChannel = 1;
				}
				else if(t == IoTSensorType_mV || t == IoTSensorType_mA){
					HasActiveChannel = 1;
				}
			}
			UpdateSamplingStep++; // @suppress("No break at end of case")
		case 1:
			if(!HasActiveChannel){
				MiSerial_SensorSamplingProgress = 100;
				result = RESULT_DONE;
			}
			else{
				SmaplingData.DLC = 0;
				SmaplingData.TypeOfData = IoTDataType_NULL;
				UpdateSamplingStep++;
			}
			break;
		case 2:
			oSerial_Log("UpdateSampling", "data sampling\r\n");
			UpdateSamplingStep++; // @suppress("No break at end of case")
		case 3:
			switch(Measurement_Sensor(&SmaplingData))
			{
				case RESULT_OK:
					*ppPacket = &SmaplingData;
					UpdateSamplingStep++;
					break;
				case RESULT_ERROR:
					UpdateSamplingStep = 5;	/* 에러 시 emit 없이 완료로 진행 */
					oSerial_Log("UpdateSampling", "sampling error\r\n");
					break;
				default:
					break;
			}
			break;
		case 4:
			result = RESULT_OK;
			UpdateSamplingStep = 5;
			break;
		case 5:
			MiSerial_SensorSamplingProgress = 100;
			result = RESULT_DONE;
			break;
	}

	if(result != RESULT_RUN){
		if(result == RESULT_DONE){
			UpdateSamplingStep = 0;
			oSerial_Log("UpdateSampling", "finish\r\n");
		}
	}

	return result;
}

oResult_t MiMain_UpdateStatus(IoT_DataPacket_t **ppPacket)
{
	static IoT_DataPacket_t DataPacket;
	oResult_t result = RESULT_RUN;

	//최소 측정시간 60초로 설정
	if(MiIoT_Status.SystemSupply == 0 || oTMR_Elapsed(&MiIoT_Status.UpdateSupplyTimestamp, SECOND_TO_MS(60), TICKBASE_SYSTICK)){
		memset(&DataPacket, 0, sizeof(IoT_DataPacket_t));

		MiIoT_Status.SoftwareVersion = (uint16_t)(MI_SW_REVISION*100);

		if((result = Measurement_Supply(20)) == RESULT_OK){
			MiIoT_Status.UpdateSupplyTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);
			MiIoT_Status.StatusBits.SystemSupplyVoltTooLow = GPIOs.ADC.SystemSupply < SYSTEM_SUPPLY_LOW_LIMIT;
			MiIoT_Status.StatusBits.ClockSupplyVoltTooLow = GPIOs.ADC.InternalBAT < RTC_SUPPLY_LOW_LIMIT;
			MiIoT_Status.StatusBits.Okay = (MiIoT_Status.StatusBits.Bits & 0xFFFFFFFE) == 0 ? 1 : 0;
			MiIoT_Status.SystemSupply = GPIOs.ADC.SystemSupply;

			oSerial_Log("IoTStatus", "SupplyVolt : %d | SWVersion : %d | StatusBit : %d\r\n", (int)MiIoT_Status.SystemSupply, (int)MiIoT_Status.SoftwareVersion, (int)MiIoT_Status.StatusBits.Bits);
		}
	}


	DataPacket.TypeOfData  = IoTDataType_Status;
	DataPacket.DLC  = sizeof(IoTDataStatus_t);
	
	IoTDataStatus_t *pData = (IoTDataStatus_t *)&DataPacket.Frame;
	pData->SoftwareVersion = MiIoT_Status.SoftwareVersion;
	pData->StatusBits = MiIoT_Status.StatusBits;
	pData->SystemSupply = MiIoT_Status.SystemSupply;
	pData->TroubleCode = MiIoT_Status.TroubleCode;

	*ppPacket = &DataPacket;

	return result;
}

void MiMain(void)
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
