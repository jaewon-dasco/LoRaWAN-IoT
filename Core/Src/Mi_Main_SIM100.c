#include "ONE_Signal.h"
#include "ONE_Time.h"
#include "ONE_Memory.h"
#include "ADC_NAU7802.h"
#include "Mi_LoRa.h"
#include "Mi_IoT.h"
#include "Mi_SoftwareRevision.h"
#include "Mi_Measurement.h"
#include "Mi_Storage.h"
#include "Mi_Serial.h"
#include "Mi_Native.h"

//#define SYSTEM_SUPPLY_LOW_LIMIT		3250
#define SYSTEM_SUPPLY_LOW_LIMIT		3200
#define RTC_SUPPLY_LOW_LIMIT		2100

GPIOs_t GPIOs;
oDebounce_t DB_USBConnected	= DEBOUNCE_INITIALIZER(30,30);
oDebounce_t DebounceError	= DEBOUNCE_INITIALIZER(1000,300);

oIO_t DO_NAND_ENABLE		= {DO_NAND_ENABLE_GPIO_Port,		 DO_NAND_ENABLE_Pin,		 IO_LOW};
oIO_t DO_ADC_REF_EANBLE		= {DO_ADC_REF_ENABLE_GPIO_Port,		 DO_ADC_REF_ENABLE_Pin,		 IO_LOW};
oIO_t DO_12V_ENABLE			= {DO_12V_ENABLE_GPIO_Port,			 DO_12V_ENABLE_Pin,			 IO_LOW};
oIO_t DO_AMP_ENABLE			= {DO_AMP_ENABLE_GPIO_Port,			 DO_AMP_ENABLE_Pin,			 IO_HIGH};
oIO_t DO_MA_MODE_ENABLE		= {DO_MA_MODE_ENABLE_GPIO_Port,		 DO_MA_MODE_ENABLE_Pin,		 IO_HIGH};
oIO_t DO_uV_ENABLE			= {DO_uV_ENABLE_GPIO_Port,			 DO_uV_ENABLE_Pin,			 IO_HIGH};
oIO_t DO_uV_A0				= {DO_uV_A0_GPIO_Port,				 DO_uV_A0_Pin,				 IO_HIGH};
oIO_t DO_uV_A1				= {DO_uV_A1_GPIO_Port,				 DO_uV_A1_Pin,				 IO_HIGH};
oIO_t DO_uV_A2				= {DO_uV_A2_GPIO_Port,				 DO_uV_A2_Pin,				 IO_HIGH};
oIO_t DO_mV_ENABLE			= {DO_mV_ENABLE_GPIO_Port,			 DO_mV_ENABLE_Pin,			 IO_HIGH};
oIO_t DO_mV_A0				= {DO_mV_A0_GPIO_Port,				 DO_mV_A0_Pin,				 IO_HIGH};
oIO_t DO_mV_A1				= {DO_mV_A1_GPIO_Port,				 DO_mV_A1_Pin,				 IO_HIGH};
oIO_t DO_mV_A2				= {DO_mV_A2_GPIO_Port,				 DO_mV_A2_Pin,				 IO_HIGH};
oIO_t DO_PWSUPPLY_CH1		= {DO_PWSUPPLY_CH1_GPIO_Port,		 DO_PWSUPPLY_CH1_Pin,		 IO_HIGH};
oIO_t DO_PWSUPPLY_CH2		= {DO_PWSUPPLY_CH2_GPIO_Port,		 DO_PWSUPPLY_CH2_Pin,		 IO_HIGH};
oIO_t DO_PWSUPPLY_CH3		= {DO_PWSUPPLY_CH3_GPIO_Port,		 DO_PWSUPPLY_CH3_Pin,		 IO_HIGH};

oIO_t DO_LORA_ENABLE		= {DO_LORA_ENABLE_GPIO_Port,		 DO_LORA_ENABLE_Pin,		 IO_LOW};
oIO_t DI_USB_CONNECTED		= {DI_USBC_CONNECTED_GPIO_Port,		DI_USBC_CONNECTED_Pin,		 IO_HIGH};
oIO_t DO_LED_OPERATING		= {DO_LED_OPERATING_GPIO_Port,		DO_LED_OPERATING_Pin,		 IO_HIGH};

oResult_t MiMain_GPIOControl()
{
	GPIOs.DO.OperatingLED = MiIoT_LED;

	IO_WRITE(DO_LORA_ENABLE, GPIOs.DO.LoRaEnable);
	IO_WRITE(DO_NAND_ENABLE, GPIOs.DO.NANDEnable);

	IO_WRITE(DO_ADC_REF_EANBLE, GPIOs.DO.ADCRefEnable);
	IO_WRITE(DO_AMP_ENABLE, GPIOs.DO.AMPEnable);
	IO_WRITE(DO_12V_ENABLE, GPIOs.DO.Enable12V);
	IO_WRITE(DO_MA_MODE_ENABLE, GPIOs.DO.MAModeEnable);

	IO_WRITE(DO_uV_ENABLE, GPIOs.DO.uVEnable);
	IO_WRITE(DO_uV_A0, GPIOs.DO.uV_A0);
	IO_WRITE(DO_uV_A1, GPIOs.DO.uV_A1);
	IO_WRITE(DO_uV_A2, GPIOs.DO.uV_A2);

	IO_WRITE(DO_mV_ENABLE, GPIOs.DO.mVEnable);
	IO_WRITE(DO_mV_A0, GPIOs.DO.mV_A0);
	IO_WRITE(DO_mV_A1, GPIOs.DO.mV_A1);
	IO_WRITE(DO_mV_A2, GPIOs.DO.mV_A2);

	IO_WRITE(DO_PWSUPPLY_CH1, GPIOs.DO.PwrSupplyCh1);
	IO_WRITE(DO_PWSUPPLY_CH2, GPIOs.DO.PwrSupplyCh2);
	IO_WRITE(DO_PWSUPPLY_CH3, GPIOs.DO.PwrSupplyCh3);

	IO_WRITE(DO_LED_OPERATING, GPIOs.DO.OperatingLED);

	oDebounce_GPIO(DI_USB_CONNECTED, &DB_USBConnected);
	GPIOs.DI.UsbConnected = DB_USBConnected.Output || DB_USBConnected.Input;

	uint32_t IORun = 0;
	IORun += GPIOs.DO.LoRaEnable && !MiLoRa_IsSleep;
	IORun += GPIOs.DO.ADCRefEnable;
	IORun += GPIOs.DO.NANDEnable;
	IORun += GPIOs.DO.AMPEnable;
	IORun += GPIOs.DO.Enable12V;
	IORun += GPIOs.DO.MAModeEnable;
	IORun += GPIOs.DO.uVEnable;
	IORun += GPIOs.DO.mVEnable;
	IORun += GPIOs.DO.PwrSupplyCh1;
	IORun += GPIOs.DO.PwrSupplyCh2;
	IORun += GPIOs.DO.PwrSupplyCh3;
	IORun += GPIOs.DO.OperatingLED;
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
	cfg.Pin = DO_MA_MODE_ENABLE.Pin;
	HAL_GPIO_Init(DO_MA_MODE_ENABLE.Port, &cfg);
	cfg.Pin = DO_12V_ENABLE.Pin;
	HAL_GPIO_Init(DO_12V_ENABLE.Port, &cfg);
	cfg.Pin = DO_uV_ENABLE.Pin;
	HAL_GPIO_Init(DO_uV_ENABLE.Port, &cfg);
	cfg.Pin = DO_uV_A0.Pin;
	HAL_GPIO_Init(DO_uV_A0.Port, &cfg);
	cfg.Pin = DO_uV_A1.Pin;
	HAL_GPIO_Init(DO_uV_A1.Port, &cfg);
	cfg.Pin = DO_uV_A2.Pin;
	HAL_GPIO_Init(DO_uV_A2.Port, &cfg);
	cfg.Pin = DO_mV_ENABLE.Pin;
	HAL_GPIO_Init(DO_mV_ENABLE.Port, &cfg);
	cfg.Pin = DO_mV_A0.Pin;
	HAL_GPIO_Init(DO_mV_A0.Port, &cfg);
	cfg.Pin = DO_mV_A1.Pin;
	HAL_GPIO_Init(DO_mV_A1.Port, &cfg);
	cfg.Pin = DO_mV_A2.Pin;
	HAL_GPIO_Init(DO_mV_A2.Port, &cfg);
	cfg.Pin = DO_ADC_REF_EANBLE.Pin;
	HAL_GPIO_Init(DO_ADC_REF_EANBLE.Port, &cfg);
	cfg.Pin = DO_PWSUPPLY_CH1.Pin;
	HAL_GPIO_Init(DO_PWSUPPLY_CH1.Port, &cfg);
	cfg.Pin = DO_PWSUPPLY_CH2.Pin;
	HAL_GPIO_Init(DO_PWSUPPLY_CH2.Port, &cfg);
	cfg.Pin = DO_PWSUPPLY_CH3.Pin;
	HAL_GPIO_Init(DO_PWSUPPLY_CH3.Port, &cfg);
	cfg.Pin = DO_LED_OPERATING.Pin;
	HAL_GPIO_Init(DO_LED_OPERATING.Port, &cfg);
	cfg.Pin = DO_AMP_ENABLE.Pin;
	HAL_GPIO_Init(DO_AMP_ENABLE.Port, &cfg);

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

	//OD/NoPull | LORA, NAND, 12V, ADC_REF
	cfg.Mode = GPIO_MODE_OUTPUT_OD;
	cfg.Pull = GPIO_NOPULL;

	if(!MiLoRa_IsSleep){
		cfg.Pin = DO_LORA_ENABLE.Pin;
		HAL_GPIO_Init(DO_LORA_ENABLE.Port, &cfg);
	}

	cfg.Pin = DO_NAND_ENABLE.Pin;
	HAL_GPIO_Init(DO_NAND_ENABLE.Port, &cfg);
	cfg.Pin = DO_12V_ENABLE.Pin;
	HAL_GPIO_Init(DO_12V_ENABLE.Port, &cfg);
	cfg.Pin = DO_ADC_REF_EANBLE.Pin;
	HAL_GPIO_Init(DO_ADC_REF_EANBLE.Port, &cfg);

	//PP/NoPull | MA_MODE, uV EN+A0/A1/A2, mV EN+A0/A1/A2, AMP, LED
	cfg.Mode = GPIO_MODE_OUTPUT_PP;
	cfg.Pull = GPIO_NOPULL;

	cfg.Pin = DO_MA_MODE_ENABLE.Pin;
	HAL_GPIO_Init(DO_MA_MODE_ENABLE.Port, &cfg);
	cfg.Pin = DO_uV_ENABLE.Pin;
	HAL_GPIO_Init(DO_uV_ENABLE.Port, &cfg);
	cfg.Pin = DO_uV_A0.Pin;
	HAL_GPIO_Init(DO_uV_A0.Port, &cfg);
	cfg.Pin = DO_uV_A1.Pin;
	HAL_GPIO_Init(DO_uV_A1.Port, &cfg);
	cfg.Pin = DO_uV_A2.Pin;
	HAL_GPIO_Init(DO_uV_A2.Port, &cfg);
	cfg.Pin = DO_mV_ENABLE.Pin;
	HAL_GPIO_Init(DO_mV_ENABLE.Port, &cfg);
	cfg.Pin = DO_mV_A0.Pin;
	HAL_GPIO_Init(DO_mV_A0.Port, &cfg);
	cfg.Pin = DO_mV_A1.Pin;
	HAL_GPIO_Init(DO_mV_A1.Port, &cfg);
	cfg.Pin = DO_mV_A2.Pin;
	HAL_GPIO_Init(DO_mV_A2.Port, &cfg);
	cfg.Pin = DO_AMP_ENABLE.Pin;
	HAL_GPIO_Init(DO_AMP_ENABLE.Port, &cfg);
	cfg.Pin = DO_LED_OPERATING.Pin;
	HAL_GPIO_Init(DO_LED_OPERATING.Port, &cfg);

	//PP/PullUp | PWSUPPLY CH1/2/3
	cfg.Mode = GPIO_MODE_OUTPUT_PP;
	cfg.Pull = GPIO_PULLUP;

	cfg.Pin = DO_PWSUPPLY_CH1.Pin;
	HAL_GPIO_Init(DO_PWSUPPLY_CH1.Port, &cfg);
	cfg.Pin = DO_PWSUPPLY_CH2.Pin;
	HAL_GPIO_Init(DO_PWSUPPLY_CH2.Port, &cfg);
	cfg.Pin = DO_PWSUPPLY_CH3.Pin;
	HAL_GPIO_Init(DO_PWSUPPLY_CH3.Port, &cfg);

	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

	HAL_ADC_DeInit(&hadc1);
	HAL_ADC_Init(&hadc1);
}

/* DataArray Items[]에서 채널번호로 항목 찾기. 없으면 NULL. */
static IoTDataArrayItem_t* DataArray_FindItem(IoTDataArray_t *pData, int count, uint8_t channel)
{
	if(pData == NULL) return NULL;
	for(int k=0; k<count && k<MIIOT_DATA_ARRAY_MAX_COUNT; k++){
		if(pData->Items[k].Channel == channel){
			return &pData->Items[k];
		}
	}
	return NULL;
}

/* DataArray Items[]에서 채널번호 항목을 set. 없으면 끝에 append. count 자동 증가. */
static void DataArray_SetItem(IoTDataArray_t *pData, int *pCount, uint8_t channel, IoTSensorType_t type, uint32_t data)
{
	if(pData == NULL || pCount == NULL) return;
	IoTDataArrayItem_t *pItem = DataArray_FindItem(pData, *pCount, channel);
	if(pItem == NULL){
		if(*pCount >= MIIOT_DATA_ARRAY_MAX_COUNT) return;
		pItem = &pData->Items[*pCount];
		(*pCount)++;
	}
	pItem->Channel = channel;
	pItem->Type = type;
	pItem->Data = data;
}

double MiMain_GetDataError(uint8_t Channel, IoT_DataPacket_t *pReference, IoT_DataPacket_t *pCompare)
{
	double error;

	if(!pReference || !pCompare){
		return 0;
	}

	if(pReference->TypeOfData != pCompare->TypeOfData){
		return 0;
	}

	IoTDataArray_t *ref = (IoTDataArray_t *)&pReference->Frame;
	IoTDataArray_t *cmp = (IoTDataArray_t *)&pCompare->Frame;
	int refCount = MIIOT_PAYLOAD_TO_DATAARRAY_COUNT(pReference->DLC);
	int cmpCount = MIIOT_PAYLOAD_TO_DATAARRAY_COUNT(pCompare->DLC);

	IoTDataArrayItem_t *pRefItem = DataArray_FindItem(ref, refCount, Channel);
	IoTDataArrayItem_t *pCmpItem = DataArray_FindItem(cmp, cmpCount, Channel);
	if(pRefItem == NULL || pCmpItem == NULL){
		return 0;
	}

	switch(pRefItem->Type)
	{
		case IoTSensorType_mV:
		case IoTSensorType_Differential:
			error = MIIOT_DATA_DECODE_mV(pRefItem->Data) - MIIOT_DATA_DECODE_mV(pCmpItem->Data);
			break;
		case IoTSensorType_FullBridge:
			error = MIIOT_DATA_DECODE_uV(pRefItem->Data) - MIIOT_DATA_DECODE_uV(pCmpItem->Data);
			break;
		case IoTSensorType_mA:
			error = MIIOT_DATA_DECODE_mA(pRefItem->Data) - MIIOT_DATA_DECODE_mA(pCmpItem->Data);
			break;
		case IoTSensorType_Resistance:
			error = MIIOT_DATA_DECODE_Ohm(pRefItem->Data) - MIIOT_DATA_DECODE_Ohm(pCmpItem->Data);
			break;
		case IoTSensorType_Thermistor:
			error = MIIOT_DATA_DECODE_TEMP(pRefItem->Data) - MIIOT_DATA_DECODE_TEMP(pCmpItem->Data);
			break;
		case IoTSensorType_VibratingWire:
			error = MIIOT_DATA_DECODE_FREQUENCY(pRefItem->Data) - MIIOT_DATA_DECODE_FREQUENCY(pCmpItem->Data);
			break;
		default:
			error = 0;
			break;
	}

	return error;
}

oResult_t MiMain_UpdateMeasure(IoT_DataPacket_t **ppPacket)
{
	static uint8_t UpdateMeasureStep = 0;
	static IoT_DataPacket_t SamplingData;
	static IoT_DataPacket_t PastSampling;
	static IoT_DataPacket_t BestDataBuffer;
	static double BestError[MEASUREMENT_CHANNEL_MAXCOUNT];
	static uint8_t TryCount[MEASUREMENT_CHANNEL_MAXCOUNT];
	static uint8_t ChannelDone[MEASUREMENT_CHANNEL_MAXCOUNT];
	static IoTChannelConfig_t ChannelConfig[MEASUREMENT_CHANNEL_MAXCOUNT];
	static IoT_DataPacket_t LastSamplingData;
	static uint8_t ConsistentCount[MEASUREMENT_CHANNEL_MAXCOUNT];
	static uint32_t RetryTimer;
	static uint32_t RetryInterval;
	static uint8_t allChannelsDone;

	oResult_t result = RESULT_RUN;
	int i;
	double dError;
	uint8_t maxRetryInterval;
	IoTDataArray_t *pBestData;
	IoTDataArray_t *pSamplingData;
	int bestCount, samplingCount;

	switch(UpdateMeasureStep)
	{
		default:
			UpdateMeasureStep = 0;
			break;
		case 0:
			allChannelsDone = 0;

			if((result = Measurement_Supply(20)) == RESULT_OK)
			{
				oSerial_Log("UpdateMeasure", "SupplyVolt %d(mV)", (int)GPIOs.ADC.SystemSupply);

				if(GPIOs.ADC.SystemSupply < SYSTEM_SUPPLY_LOW_LIMIT){
					oSerial_Log("UpdateMeasure", "Fail low battery");
					result = RESULT_FAULT;
				}
				else{
					memcpy(&ChannelConfig, MiIoT_Parameter.ChannelConfig, sizeof(ChannelConfig));
					memset(&BestDataBuffer, 0, sizeof(BestDataBuffer));
					memset(TryCount, 0, sizeof(TryCount));
					memset(ChannelDone, 0, sizeof(ChannelDone));
					memset(ConsistentCount, 0, sizeof(ConsistentCount));
					for(i = 0; i < MEASUREMENT_CHANNEL_MAXCOUNT; i++){
						BestError[i] = 1e10; // 큰 값으로 초기화
					}

					oSerial_Log("UpdateMeasure", "Start");
					result = RESULT_RUN;
					UpdateMeasureStep++;
				}
			}
			break;
		case 1:
			// 측정 수행
			switch(Measurement_Sensor(&SamplingData, ChannelConfig))
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
		case 2:
			// 첫 번째 측정 (비교할 이전 데이터 없음)
			if(PastSampling.DLC == 0){
				// 현재 샘플링을 최적 데이터로 사용하고 종료
				BestDataBuffer = SamplingData;
				PastSampling = SamplingData;
				*ppPacket = &BestDataBuffer;
				result = RESULT_OK;
				allChannelsDone = 1;
				UpdateMeasureStep++;
			}
			else{
				// 이전 측정값과 비교하여 최적값 업데이트
				pBestData = (IoTDataArray_t *)&BestDataBuffer.Frame;
				pSamplingData = (IoTDataArray_t *)&SamplingData.Frame;
				bestCount = MIIOT_PAYLOAD_TO_DATAARRAY_COUNT(BestDataBuffer.DLC);
				samplingCount = MIIOT_PAYLOAD_TO_DATAARRAY_COUNT(SamplingData.DLC);

				allChannelsDone = 1;

				for(i = 0; i < MEASUREMENT_CHANNEL_MAXCOUNT; i++){
					uint8_t Ch = i + 1;

					// 이미 완료된 채널은 건너뜀
					// 센서 타입이 없으면 건너뜀 (이미 비활성화됨)
					if(ChannelDone[i] || ChannelConfig[i].TypeOfSensor == IoTSensorType_NULL){
						ChannelDone[i] = 1;
						continue;
					}

					IoTDataArrayItem_t *pSampItem = DataArray_FindItem(pSamplingData, samplingCount, Ch);

					// 측정 실패 감지: 설정된 채널인데 측정값이 없는 경우
					if(pSampItem == NULL || pSampItem->Type == IoTSensorType_NULL){
						oSerial_Log("UpdateMeasure", "CH%d skipped (no data)", (int)Ch);
						allChannelsDone = 0;
						continue;
					}

					// 센서 타입 변경 감지: PastSampling과 타입이 다르면 비교 없이 수락
					{
						IoTDataArray_t *pPastData = (IoTDataArray_t *)&PastSampling.Frame;
						int pastCount = MIIOT_PAYLOAD_TO_DATAARRAY_COUNT(PastSampling.DLC);
						IoTDataArrayItem_t *pPastItem = DataArray_FindItem(pPastData, pastCount, Ch);
						if(pPastItem == NULL || pPastItem->Type != pSampItem->Type){
							DataArray_SetItem(pBestData, &bestCount, Ch, pSampItem->Type, pSampItem->Data);
							ChannelDone[i] = 1;
							oSerial_Log("UpdateMeasure", "CH%d type changed (%d->%d)", (int)Ch,
								pPastItem ? (int)pPastItem->Type : 0, (int)pSampItem->Type);
							continue;
						}
					}

					// 오차 허용값이 0이면 검사 불필요, 건너뜀
					if(ChannelConfig[i].ErrorTolerance <= 0){
						DataArray_SetItem(pBestData, &bestCount, Ch, pSampItem->Type, pSampItem->Data);
						ChannelDone[i] = 1;
						continue;
					}

					// 이전 샘플링과의 오차 계산
					dError = MiMain_GetDataError(Ch, &PastSampling, &SamplingData);
					dError = MATH_ABS(dError);

					TryCount[i]++;
					oSerial_Log("UpdateMeasure", "CH%d Error:%.3f Tol:%.3f Try:%d/%d",
						(int)Ch, dError, ChannelConfig[i].ErrorTolerance, TryCount[i], ChannelConfig[i].RetryCount);

					// 오차가 허용 범위 이내인지 확인
					if(dError <= (double)ChannelConfig[i].ErrorTolerance){
						// 허용 범위 이내 - 이 값을 사용하고 채널 완료 표시
						DataArray_SetItem(pBestData, &bestCount, Ch, pSampItem->Type, pSampItem->Data);
						ChannelDone[i] = 1;
						oSerial_Log("UpdateMeasure", "CH%d Done (within tolerance)", (int)Ch);
					}
					else{
						// 허용 범위 초과 - 이전 값과 가장 유사한 값 유지
						if(dError < BestError[i]){
							BestError[i] = dError;
							DataArray_SetItem(pBestData, &bestCount, Ch, pSampItem->Type, pSampItem->Data);
							oSerial_Log("UpdateMeasure", "CH%d Best updated (err:%.3f)", (int)Ch, dError);
						}

						// 자기 일관성 검증: 새 측정값끼리 일치하는지 확인
						if(TryCount[i] > 0){
							double dNewVsLast = MiMain_GetDataError(Ch, &LastSamplingData, &SamplingData);
							dNewVsLast = MATH_ABS(dNewVsLast);

							if(dNewVsLast <= (double)ChannelConfig[i].ErrorTolerance){
								ConsistentCount[i]++;
								oSerial_Log("UpdateMeasure", "CH%d Consistent=%d (newErr:%.3f)",
									(int)Ch, ConsistentCount[i], dNewVsLast);

								if(ConsistentCount[i] >= 2){
									// 연속 2회 일치 - 센서 실제 변화 확정
									DataArray_SetItem(pBestData, &bestCount, Ch, pSampItem->Type, pSampItem->Data);
									ChannelDone[i] = 1;
									oSerial_Log("UpdateMeasure", "CH%d Done (sensor changed)", (int)Ch);
									continue;
								}
							}
							else{
								ConsistentCount[i] = 0;
							}
						}

						// 최대 재시도 횟수 초과 여부 확인
						if(TryCount[i] >= ChannelConfig[i].RetryCount){
							ChannelDone[i] = 1;
							oSerial_Log("UpdateMeasure", "CH%d Done (max retries)", (int)Ch);
						}
						else{
							allChannelsDone = 0;
						}
					}
				}

				// 포트 완료 확인 - 같은 SupplySource의 모든 채널이 완료되면 해당 포트 비활성화
				for(uint8_t port = 1; port <= 3; port++){
					uint8_t portAllDone = 1;
					uint8_t portHasChannel = 0;

					// 이 포트의 모든 채널이 완료되었는지 확인
					for(i = 0; i < MEASUREMENT_CHANNEL_MAXCOUNT; i++){
						if(ChannelConfig[i].SupplySource == port && ChannelConfig[i].TypeOfSensor != IoTSensorType_NULL){
							portHasChannel = 1;
							if(!ChannelDone[i]){
								portAllDone = 0;
								break;
							}
						}
					}

					// 포트에 채널이 있고 모두 완료되면 TypeOfSensor를 0으로 설정하여 포트 비활성화
					if(portHasChannel && portAllDone){
						for(i = 0; i < MEASUREMENT_CHANNEL_MAXCOUNT; i++){
							if(ChannelConfig[i].SupplySource == port){
								ChannelConfig[i].TypeOfSensor = IoTSensorType_NULL;
							}
						}
						oSerial_Log("UpdateMeasure", "Port%d completed, disabled", port);
					}
				}

				// 종료 조건 확인
				// 모든 채널 완료 (허용 범위 이내 또는 최대 재시도 횟수 초과)
				if(allChannelsDone){
					// 샘플링 데이터에서 시간 복사
					BestDataBuffer.TypeOfData = SamplingData.TypeOfData;
					BestDataBuffer.DLC = MIIOT_IOTDATA_SIZE_DATAARRAY(bestCount);
					pBestData->Time = pSamplingData->Time;

					PastSampling = BestDataBuffer;
					*ppPacket = &BestDataBuffer;

					result = RESULT_OK;
					UpdateMeasureStep++;
					oSerial_Log("UpdateMeasure", "All channels done");
				}
				else{
					// 재측정 필요 - 포트별 최대 RetryInterval 계산
					maxRetryInterval = 0;
					for(i = 0; i < MEASUREMENT_CHANNEL_MAXCOUNT; i++){
						// 아직 완료되지 않은 활성 채널에서 최대 RetryInterval 찾기
						if(!ChannelDone[i] && ChannelConfig[i].TypeOfSensor != IoTSensorType_NULL){
							if(ChannelConfig[i].RetryInterval > maxRetryInterval){
								maxRetryInterval = ChannelConfig[i].RetryInterval;
							}
						}
					}

					// 재측정 대기 시간 설정 (RetryInterval은 초 단위)
					LastSamplingData = SamplingData;
					RetryInterval = SECOND_TO_MS(maxRetryInterval);
					RetryTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					UpdateMeasureStep = 3;
					oSerial_Log("UpdateMeasure", "Waiting %dms before re-measure", (int)RetryInterval);
				}
			}
			break;
		case 3:
			if(allChannelsDone){
				result = RESULT_DONE;
			}
			// 재측정 대기 시간 경과 확인
			else if(oTMR_Elapsed(&RetryTimer, RetryInterval, TICKBASE_SYSTICK)){
				// 대기 완료 - 측정 단계로 이동
				UpdateMeasureStep = 1;
				oSerial_Log("UpdateMeasure", "Re-measuring...");
			}
			else{
				result = RESULT_WAIT;
			}
			break;
	}

	if(result != RESULT_RUN && result != RESULT_WAIT){
		if(result == RESULT_OK){
			//DLC는 BestDataBuffer 구성 시 이미 계산됨 (DataArray dense)
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
	static uint8_t UpdateMeasureStep = 0;
	static IoT_DataPacket_t SmaplingData;
	oResult_t result = RESULT_RUN;

	switch(UpdateMeasureStep)
	{
		default:
			UpdateMeasureStep = 0;
			break;
		case 0:
			if((result = Measurement_Supply(20)) == RESULT_OK)
			{
				oSerial_Log("UpdateMeasure", "SupplyVolt %d(mV)", (int)GPIOs.ADC.SystemSupply);

				if(GPIOs.ADC.SystemSupply < SYSTEM_SUPPLY_LOW_LIMIT){
					oSerial_Log("UpdateMeasure", "Fail low battery");
					result = RESULT_FAULT;
				}
				else{
					UpdateMeasureStep++;
					oSerial_Log("UpdateMeasure", "Start");
					result = RESULT_RUN;
				}
			}
			break;
		case 1:
			switch(Measurement_Sensor(&SmaplingData, MiIoT_Parameter.ChannelConfig))
			{
				case RESULT_RUN:
					break;
				case RESULT_OK:
					*ppPacket = &SmaplingData;
					result = RESULT_OK;
					UpdateMeasureStep++;
					break;
				default:
					oSerial_Log("UpdateMeasure", "sampling Error\r\n");
					result = RESULT_ERROR;
					break;
			}
			break;
		case 2:
			result = RESULT_DONE;
			break;
	}

	if(result != RESULT_RUN && result != RESULT_OK){
		UpdateMeasureStep = 0;
		oSerial_Log("UpdateMeasure", "finish");
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
			break;
		case 1:
			HAL_I2C_DeInit(&hi2c2);
			HAL_QSPI_DeInit(&hqspi);
			MiMainStep++;
			break;
		default:
			MiStorage();
			MiIoT(&huart3);
			MiSerial(&huart1);
			break;
	}
}

/* History

2026-06-26 | v0.1
	- baseline (Mi_Main_SIM100.c)
*/
