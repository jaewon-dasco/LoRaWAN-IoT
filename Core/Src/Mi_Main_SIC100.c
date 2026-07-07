/*
 * Mi_Main_SIC100.c
 *
 *  Version: 0.31 (2026-07-07)
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
#define MEASURE_UPDATE_RETRY_MAX	10	/* UpdateMeasure 전체 재시도 상한 (채널별 RetryCount와 무관하게 강제 상한) */

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

/* SIC100 통합 패킷(DataArray_Type2)의 채널별 오차 검증 — CH1=TiltArray, CH2=Analog.
 * ErrorTolerance==0 또는 RetryCount==0 채널은 검사 제외. Type 불일치(기준값 없음/설정 변경)는 비교 불가 → 수락.
 * 반환 1=오차 초과(재측정 필요), 0=허용. */
uint8_t MiMain_DataIsError(IoTDataSIC100_2C_t *pReference, IoTDataSIC100_2C_t *pCompare, uint8_t ChannelNo)
{
	IoTChannelConfig_t *pConfig;
	double error;
	uint32_t i;

	if(!pReference || !pCompare || ChannelNo == 0 || ChannelNo > MEASUREMENT_CHANNEL_MAXCOUNT){
		return 0;
	}

	pConfig = &MiIoT_Parameter.ChannelConfig[ChannelNo-1];

	if(pConfig->ErrorTolerance <= 0 || pConfig->RetryCount <= 0){
		return 0;
	}

	switch(pConfig->TypeOfSensor)
	{
		case IoTSensorType_ArrayDualTilt:
			if(pReference->TiltArray.Type != IoTSensorType_ArrayDualTilt || pCompare->TiltArray.Type != IoTSensorType_ArrayDualTilt){
				break;
			}
			for(i = 0; i < pReference->CountOfArraySensor; i++){
				error = MIIOT_DATA_DECODE_ANGLE(pReference->TiltArray.Dual[i].AxisX) - MIIOT_DATA_DECODE_ANGLE(pCompare->TiltArray.Dual[i].AxisX);
				if(MATH_ABS(error) > pConfig->ErrorTolerance) return 1;
				error = MIIOT_DATA_DECODE_ANGLE(pReference->TiltArray.Dual[i].AxisY) - MIIOT_DATA_DECODE_ANGLE(pCompare->TiltArray.Dual[i].AxisY);
				if(MATH_ABS(error) > pConfig->ErrorTolerance) return 1;
			}
			break;

		case IoTSensorType_ArraySingleTilt:
			if(pReference->TiltArray.Type != IoTSensorType_ArraySingleTilt || pCompare->TiltArray.Type != IoTSensorType_ArraySingleTilt){
				break;
			}
			for(i = 0; i < pReference->CountOfArraySensor; i++){
				error = MIIOT_DATA_DECODE_ANGLE(pReference->TiltArray.Single[i].Axis) - MIIOT_DATA_DECODE_ANGLE(pCompare->TiltArray.Single[i].Axis);
				if(MATH_ABS(error) > pConfig->ErrorTolerance) return 1;
			}
			break;

		case IoTSensorType_mV:
		case IoTSensorType_mA:
			if(pReference->Analog.Type != pConfig->TypeOfSensor || pCompare->Analog.Type != pConfig->TypeOfSensor){
				break;
			}
			/* 실패 마커(Data=0)는 비교 배제 — 정상값을 -34000mV로 오인해 불필요한 재측정 반복 방지 */
			if(pReference->Analog.Data == 0 || pCompare->Analog.Data == 0){
				break;
			}
			if(pConfig->TypeOfSensor == IoTSensorType_mV){
				error = MIIOT_DATA_DECODE_mV(pReference->Analog.Data) - MIIOT_DATA_DECODE_mV(pCompare->Analog.Data);
			}
			else{
				error = MIIOT_DATA_DECODE_mA(pReference->Analog.Data) - MIIOT_DATA_DECODE_mA(pCompare->Analog.Data);
			}
			if(MATH_ABS(error) > pConfig->ErrorTolerance) return 1;
			break;

		default:
			break;
	}

	return 0;
}

oResult_t MiMain_UpdateMeasure(IoT_DataPacket_t **ppPacket)
{
	static uint8_t UpdateMeasureStep = 0;
	static IoT_DataPacket_t SamplingData;
	static IoT_DataPacket_t PastSampling;
	static uint16_t ChDoneBits;	/* 채널별 확정 비트맵 — CH1=bit0, CH2=bit1 */
	static uint8_t TotalRetryCount;
	static uint32_t RetryTimer;
	static uint32_t RetryInterval;

	oResult_t result = RESULT_RUN;
	int k;

	switch(UpdateMeasureStep)
	{
		default:
			UpdateMeasureStep = 0;
			break;
		case 0:
			ChDoneBits = 0xFFFF;
			TotalRetryCount = 0;

			if((result = Measurement_Supply(20)) == RESULT_OK)
			{
				oSerial_Log("UpdateMeasure", "SupplyVolt %d(mV)", (int)GPIOs.ADC.SystemSupply);

				if(GPIOs.ADC.SystemSupply < SYSTEM_SUPPLY_LOW_LIMIT){
					oSerial_Log("UpdateMeasure", "Fail low battery");
					result = RESULT_FAULT;
				}
				else{
					memset(&SamplingData, 0, sizeof(SamplingData));

					/* 활성 채널 수 집계 (진단 로그용) */
					uint8_t activeChannels = 0;
					for(k = 0; k < MEASUREMENT_CHANNEL_MAXCOUNT; k++){
						if(MiIoT_Parameter.ChannelConfig[k].TypeOfSensor != IoTSensorType_NULL){
							activeChannels++;
						}
					}

					oSerial_Log("UpdateMeasure", "Start (active=%d ch)", (int)activeChannels);
					result = RESULT_RUN;
					UpdateMeasureStep++;
				}
			}
			break;
		case 1:
			// 측정 수행 — CH1(Tilt Array) + CH2(Analog) 통합 패킷 단일 emit
			switch(Measurement_Sensor(&SamplingData))
			{
				case RESULT_RUN:
					break;
				case RESULT_OK:
					UpdateMeasureStep++;
					break;
				default:
					oSerial_Log("UpdateMeasure", "sampling Error\r\n");
					result = RESULT_ERROR;
					break;
			}
			break;
		case 2: {
			IoTDataSIC100_2C_t *pSampling = (IoTDataSIC100_2C_t *)&SamplingData.Frame;
			IoTDataSIC100_2C_t *pPast     = (IoTDataSIC100_2C_t *)&PastSampling.Frame;

			ChDoneBits = 0xFFFF;	/* 매 평가마다 리셋 — 실패 채널만 비트 클리어 */

			if(TotalRetryCount < MEASURE_UPDATE_RETRY_MAX){
				for(k = 0; k < MEASUREMENT_CHANNEL_MAXCOUNT; k++){
					IoTChannelConfig_t *pConfig = &MiIoT_Parameter.ChannelConfig[k];

					if(pConfig->TypeOfSensor == IoTSensorType_NULL){
						continue;
					}

					/* 채널별 RetryCount 도달 → 현재 값 그대로 수락 (스킵) */
					if(TotalRetryCount >= pConfig->RetryCount){
						continue;
					}

					/* CH2 Analog 측정 실패 마커 (Data=0) → 재측정 요청
					 * Type은 Measurement_Analog가 성공/실패 모두 pConfig->TypeOfSensor로 설정 (실패 마커 규약).
					 * 실패 판별은 Data==0(초기값)으로 확인. */
					if((pConfig->TypeOfSensor == IoTSensorType_mV || pConfig->TypeOfSensor == IoTSensorType_mA) &&
					   pSampling->Analog.Data == 0){
						oMEM_SetBit(&ChDoneBits, k, 0);
						oSerial_Log("UpdateMeasure", "CH%d measurement failed → retry", (int)(k+1));
						continue;
					}

					/* 이전 측정값과 오차 비교 (PastSampling 없으면 첫 사이클 → 무조건 수락) */
					if(PastSampling.DLC == 0){
						continue;
					}

					if(MiMain_DataIsError(pPast, pSampling, (uint8_t)(k+1))){
						oMEM_SetBit(&ChDoneBits, k, 0);
						oSerial_Log("UpdateMeasure", "CH%d error → retry", (int)(k+1));
					}
				}
			}

			if(ChDoneBits == 0xFFFF){
				PastSampling = SamplingData;
				*ppPacket = &SamplingData;
				result = RESULT_OK;
			}
			else{
				/* 실패 채널들 중 최대 RetryInterval 계산 */
				uint32_t maxInterval = 0;
				for(k = 0; k < MEASUREMENT_CHANNEL_MAXCOUNT; k++){
					if(!(ChDoneBits & (1 << k)) && MiIoT_Parameter.ChannelConfig[k].RetryInterval > maxInterval){
						maxInterval = MiIoT_Parameter.ChannelConfig[k].RetryInterval;
					}
				}
				RetryInterval = SECOND_TO_MS(maxInterval);
				RetryTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				oSerial_Log("UpdateMeasure", "Waiting %dms before re-measure", (int)RetryInterval);
			}

			UpdateMeasureStep++;
			break;
		}
		case 3:
			if(ChDoneBits == 0xFFFF){
				result = RESULT_DONE;
			}
			// 재측정 대기 시간 경과 확인
			else if(oTMR_Elapsed(&RetryTimer, RetryInterval, TICKBASE_SYSTICK)){
				TotalRetryCount++;

				/* 아직 확정 안 된 채널 수 집계 */
				uint8_t pendingChannels = 0;
				for(k = 0; k < MEASUREMENT_CHANNEL_MAXCOUNT; k++){
					if(!(ChDoneBits & (1 << k))){
						pendingChannels++;
					}
				}

				UpdateMeasureStep = 1;
				oSerial_Log("UpdateMeasure", "Re-measuring (retry #%d/%d, pending=%d ch)", (int)TotalRetryCount, (int)MEASURE_UPDATE_RETRY_MAX, (int)pendingChannels);
			}
			else{
				result = RESULT_WAIT;
			}
			break;
	}

	if(result != RESULT_RUN && result != RESULT_WAIT){
		if(result == RESULT_OK){
			oSerial_Log("UpdateMeasure", "data ready DLC=%d\r\n", (int)(*ppPacket)->DLC);
		}
		else{
			UpdateMeasureStep = 0;
			oSerial_Log("UpdateMeasure", "finish\r\n");
		}
	}

	return result;
}

oResult_t MiMain_UpdateSampling(IoT_DataPacket_t **ppPacket)
{
	static uint8_t UpdateSamplingStep = 0;
	static IoT_DataPacket_t SamplingData;
	oResult_t result = RESULT_RUN;

	switch(UpdateSamplingStep)
	{
		default:
			UpdateSamplingStep = 0;
			break;
		case 0:
			if((result = Measurement_Supply(20)) == RESULT_OK)
			{
				oSerial_Log("UpdateSampling", "SupplyVolt %d(mV)", (int)GPIOs.ADC.SystemSupply);

				if(GPIOs.ADC.SystemSupply < SYSTEM_SUPPLY_LOW_LIMIT){
					oSerial_Log("UpdateSampling", "Fail low battery");
					result = RESULT_FAULT;
				}
				else{
					UpdateSamplingStep++;
					memset(&SamplingData, 0, sizeof(SamplingData));
					MiSerial_SensorSamplingProgress = 0;
					oSerial_Log("UpdateSampling", "Start");
					result = RESULT_RUN;
				}
			}
			break;
		case 1:
			switch(Measurement_Sensor(&SamplingData))
			{
				case RESULT_RUN:
					break;
				case RESULT_OK:
					*ppPacket = &SamplingData;
					result = RESULT_OK;
					UpdateSamplingStep++;
					break;
				default:
					oSerial_Log("UpdateSampling", "sampling Error\r\n");
					result = RESULT_ERROR;
					break;
			}
			break;
		case 2:
			MiSerial_SensorSamplingProgress = 100;
			result = RESULT_DONE;
			break;
	}

	if(result != RESULT_RUN && result != RESULT_OK){
		UpdateSamplingStep = 0;
		oSerial_Log("UpdateSampling", "finish");
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
	else{
		result = RESULT_OK;
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
			break;
		case 1:
			HAL_I2C_DeInit(&hi2c2);
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
