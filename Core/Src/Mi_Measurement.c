/*
 * Mi_Sensor.c
 *
 *  Created on: Dec 9, 2024
 *      Author: JONE
 */

/************************************************
 * ADC calibration index map (외부 ADC 측정타입별)
 ************************************************
 * [0] = Battery
 * [1] = mV  (전 채널 공통)
 * [2] = mA  (전 채널 공통)
 * [3] = Differential / FullBridge (전 채널 공통)
 *
 * 외부 ADC = NAU7802 단일 (gain=X1 고정)
 *  - V mux (IC1) → NAU CH1 : mV / mA
 *  - uV mux (IC2) → NAU CH2 : Differential / FullBridge
 *
 * 내부 ADC = STM32 ADC1 (thermistor 전용)
 *  - ch3 → ADC_CHANNEL_4 (PC3)
 *  - ch6 → ADC_CHANNEL_5 (PA0)
 *  - ch9 → ADC_CHANNEL_6 (PA1)
 ************************************************/

#include "Mi_Main.h"
#include "Mi_Measurement.h"
#include "Mi_IoT.h"
#include "Mi_Serial.h"
#include "ADC_NAU7802.h"
#include "ONE_Math.h"
#include "Mi_Native.h"

/* VREFINT_CAL_ADDR / VREFINT_CAL_VREF 은 HAL (stm32l4xx_ll_adc.h)에서 제공 — VDDA=3.0V(=3000mV) 기준 공장 교정값 */

#define ADC_VREF					(MiIoT_Parameter.SystemConfig.ActualVRef != 0 ? MiIoT_Parameter.SystemConfig.ActualVRef : g_VddaActual_mV)
#define ADC_MAXDIGIT				4095//(uint16_t)((uint32_t)(4095 * 64) >> 2)

#define ADC_TO_AI(x)				((double)(x)/(double)ADC_MAXDIGIT*(double)ADC_VREF)
#define ADC_TO_SUPPLY(x)			(ADC_TO_AI(x)/VOLT_DIV_RATIO(6980,20000))
#define ADC_TO_VBAT(x)				(ADC_TO_AI(x)*3)

#define AI_TO_MV(x)					(x*-5.066999494+11217.99778)
#define AI_TO_MA(x)					(ADC_TO_MV(x)/249)

#define MEASURE_AMP_I2C				&hi2c2
#define MEASURE_SAMPLING_MAXCOUNT	10

#define EXTADC_MUX_SETTLE_MS		300	/* ADG1407 + 케이블 RC 정착 (ms) */
#define EXTADC_DISCARD_SAMPLES		10	/* NAU 채널/게인 변경 후 transient 제거 샘플 수 */
#define EXTADC_CLEAR_MS				10	/* 채널 전환 전 S8(GND) 단락 잔류전압 클리어 (ms) */
#define MEASURE_READ_RETRY_MAXCOUNT	3	/* 측정 실패 시 채널당 재시도 횟수 */

static IoTDataArray_t *pDataArray = NULL;
static uint8_t CountOfItem = 0;	/* DataArray Items[] 채워진 개수 (dense) */
static int8_t PortChannels[MEASUREMENT_PORT_MAXCOUNT][MEASUREMENT_CHANNEL_MAXCOUNT] = {{0}};
static uint8_t PortChannelCount[MEASUREMENT_PORT_MAXCOUNT] = {0};
NAU7802_t NAU7802;

static uint32_t g_VddaActual_mV = 2800;	/* VREFINT 기반 동적 VDDA (mV), 갱신 전 기본 2.8V */

oResult_t Measurement_CalibrateVDD(void)
{
	static uint8_t CalStep = 0;
	uint16_t vrefint_raw;
	oResult_t result = RESULT_RUN;

	switch(CalStep)
	{
		case 0:
			HAL_ADC_Stop(&hadc1);
			if(HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) == HAL_OK){
				oSerial_Log("Mearment", "ADC offset calibrated");
			}
			CalStep++;
			break;
		case 1:
			result = Native_ADCRead(ADC_CHANNEL_VREFINT, ADC_SAMPLETIME_640CYCLES_5, &vrefint_raw, 20);
			if(result == RESULT_OK){
				if(vrefint_raw > 0){
					uint32_t vdda = ((uint32_t)VREFINT_CAL_VREF * (uint32_t)(*VREFINT_CAL_ADDR)) / vrefint_raw;
					if(vdda >= 2000 && vdda <= 3600){
						g_VddaActual_mV = vdda;
					}
				}
				oSerial_Log("Mearment", "VrefCal VDDA=%l mV (raw=%u cal=%u)", (unsigned long)g_VddaActual_mV, (unsigned int)vrefint_raw, (unsigned int)(*VREFINT_CAL_ADDR));
			}
			break;
	}

	if(result != RESULT_RUN){
		CalStep = 0;
	}

	return result;
}

/* V mux (IC1, mV/mA) — 채널→ADG1407 S 핀 주소 매핑.
 * S1=0, S2=1, ..., S8=7. 미사용 채널은 0xFF 반환. */
static uint8_t Analog_MuxAddress_V(int8_t Channel)
{
	switch(Channel)
	{
		case 1: return 0;  // S1
		case 4: return 1;  // S2
		case 7: return 2;  // S3
		case 2: return 3;  // S4
		case 5: return 4;  // S5
		case 8: return 5;  // S6
		default: return 0xFF;
	}
}

/* uV mux (IC2, Diff/FullBridge) — 채널 페어 시작 채널→ADG1407 S 핀 주소.
 * ch1=(1+2), ch4=(4+5), ch7=(7+8). 그 외 채널은 0xFF. */
static uint8_t Analog_MuxAddress_uV(int8_t Channel)
{
	switch(Channel)
	{
		case 1: return 0;  // S1 (pair 1+2)
		case 4: return 1;  // S2 (pair 4+5)
		case 7: return 2;  // S3 (pair 7+8)
		default: return 0xFF;
	}
}

void Analog_ChannelConfig(int8_t Channel, IoTSensorType_t TypeOfSensor)
{
	GPIOs.DO.uVEnable = 0;
	GPIOs.DO.uV_A0 = 0;
	GPIOs.DO.uV_A1 = 0;
	GPIOs.DO.uV_A2 = 0;

	GPIOs.DO.mVEnable = 0;
	GPIOs.DO.mV_A0 = 0;
	GPIOs.DO.mV_A1 = 0;
	GPIOs.DO.mV_A2 = 0;

	GPIOs.DO.MAModeEnable = 0;

	if(Channel < 0){
		return;
	}

	if(Channel == 0 || TypeOfSensor == IoTSensorType_NULL){
		GPIOs.DO.mV_A0 = 1;
		GPIOs.DO.mV_A1 = 1;
		GPIOs.DO.mV_A2 = 1;
		GPIOs.DO.mVEnable = 1;
	}
	else if(TypeOfSensor == IoTSensorType_mV || TypeOfSensor == IoTSensorType_mA){
		uint8_t addr = Analog_MuxAddress_V(Channel);
		if(addr == 0xFF) return;

		GPIOs.DO.mV_A0 = (addr >> 0) & 1;
		GPIOs.DO.mV_A1 = (addr >> 1) & 1;
		GPIOs.DO.mV_A2 = (addr >> 2) & 1;
		GPIOs.DO.mVEnable = 1;
	}
	else if(TypeOfSensor == IoTSensorType_Differential || TypeOfSensor == IoTSensorType_FullBridge){
		uint8_t addr = Analog_MuxAddress_uV(Channel);
		if(addr == 0xFF) return;

		GPIOs.DO.uV_A0 = (addr >> 0) & 1;
		GPIOs.DO.uV_A1 = (addr >> 1) & 1;
		GPIOs.DO.uV_A2 = (addr >> 2) & 1;
		GPIOs.DO.uVEnable = 1;

		GPIOs.DO.mV_A0 = 1;
		GPIOs.DO.mV_A1 = 1;
		GPIOs.DO.mV_A2 = 1;
		GPIOs.DO.mVEnable = 1;
	}

	GPIOs.DO.MAModeEnable = (TypeOfSensor == IoTSensorType_mA);
}

void Measurement_PowerOn(int8_t Port)
{
	GPIOs.DO.AMPEnable = Port >= 0;
	GPIOs.DO.ADCRefEnable = Port >= 0;
	GPIOs.DO.Enable12V = Port >= 0;

	GPIOs.DO.PwrSupplyCh1 = Port == 1 || Port == 127;
	GPIOs.DO.PwrSupplyCh2 = Port == 2 || Port == 127;
	GPIOs.DO.PwrSupplyCh3 = Port == 3 || Port == 127;
}

/* Internal ADC — thermistor 전용 (ch3/6/9 → ADC_CHANNEL_4/5/6).
 * Resistance 값을 반환. NTC→Celsius 변환은 호출자에서 처리. */
oResult_t Analog_InternalAdc(int8_t Channel, double *pAnalog, uint8_t ReadCount)
{
	static uint8_t VerifyStep = 0;
	static uint32_t AnalogReadTimer;
	double ReadAnalog;
	uint32_t ADChannel;
	uint16_t ADC;
	oResult_t result = RESULT_RUN;

	switch(Channel)
	{
		case 3: ADChannel = ADC_CHANNEL_4; break;
		case 6: ADChannel = ADC_CHANNEL_5; break;
		case 9: ADChannel = ADC_CHANNEL_6; break;
		default:
			VerifyStep = 0;
			return RESULT_ERROR;
	}

	if(!GPIOs.DO.ADCRefEnable){
		VerifyStep = 0;
		return result;
	}

	switch(VerifyStep)
	{
		default:
		case 0:
			if(!GPIOs.DO.ADCRefEnable || !GPIOs.DO.AMPEnable){
				Measurement_PowerOn(Channel);
				AnalogReadTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				VerifyStep++;
			}
			else{
				VerifyStep += 2;
			}
			break;
		case 1:
			if(oTMR_Elapsed(&AnalogReadTimer, 1000, TICKBASE_SYSTICK)){
				VerifyStep++;
			}
			break;
		case 2: /* Discard read: 3 samples thrown away to stabilize ADC */
			if((result = Native_ADCRead(ADChannel, ADC_SAMPLETIME_640CYCLES_5, &ADC, 3)) == RESULT_OK){
				VerifyStep++;
				result = RESULT_RUN;
			}
			break;
		case 3: /* Actual measurement — raw mV (저항·온도 환산은 호출자에서) */
			if((result = Native_ADCRead(ADChannel, ADC_SAMPLETIME_640CYCLES_5, &ADC, ReadCount)) == RESULT_OK){
				ReadAnalog = ADC_TO_AI(ADC);
				*pAnalog = ReadAnalog;
				oSerial_Log("Mearment", "InternalAdc CH%d ADC=%d mV=%.3f", (int)Channel, (int)ADC, ReadAnalog);
				result = RESULT_OK;
			}
			break;
	}

	if(result != RESULT_RUN){
		VerifyStep = 0;
	}

	return result;
}

/* External ADC = NAU7802 (gain=X1 고정, VLDO=4.5V).
 *  - mV/mA           → NAU CH1, V mux (IC1)
 *  - Differential/FB → NAU CH2, uV mux (IC2)
 * MUX 주소는 Analog_ChannelConfig()에서 GPIO에 이미 인가됨. 본 함수는 NAU만 제어. */
oResult_t Analog_ExternalAdc(int8_t Channel, IoTSensorType_t TypeOfSensor, double *pAnalog, uint8_t ReadCount)
{
	static uint8_t ExtAdcStep = 0;
	static uint8_t ExtErrorCount = 0;
	static uint8_t SumOfCount = 0;
	static uint32_t SettleTimer = 0;
	static double SumAnalog = 0;
	uint8_t NauChannel;
	uint8_t CalIndex;
	double ReadAnalog = 0;
	oResult_t result = RESULT_RUN;

	if(TypeOfSensor == IoTSensorType_mV || TypeOfSensor == IoTSensorType_mA){
		NauChannel = 1;	// V mux → NAU CH1
		CalIndex = (TypeOfSensor == IoTSensorType_mA) ? 2 : 1;
	}
	else if(TypeOfSensor == IoTSensorType_Differential || TypeOfSensor == IoTSensorType_FullBridge){
		NauChannel = 2;	// uV mux → NAU CH2
		CalIndex = 3;
	}
	else{
		ExtAdcStep = 0;
		return RESULT_ERROR;
	}

	switch(ExtAdcStep)
	{
		case 0:	// NAU7802 Init + Gain=BYPASS
			if(!NAU7802.IsOpen){
				switch(NAU7802_Init(&NAU7802, MEASURE_AMP_I2C, NAU7802_VLDO_4_5V))
				{
					case RESULT_RUN:
						break;
					case RESULT_OK:
						oSerial_Log("Mearment", "NAU7802 Init OK");
						ExtErrorCount = 0;
						break;
					default:
						ExtErrorCount++;
						oSerial_Log("Mearment", "NAU7802 Init FAIL (errCnt=%d)", (int)ExtErrorCount);
						break;
				}
			}
			else if(NAU7802.Registers.Gain != NAU7802_GAIN_BYPASS){
				switch(NAU7802_SetGain(&NAU7802, NAU7802_GAIN_BYPASS))
				{
					default:
						break;
					case RESULT_OK:
						oSerial_Log("Mearment", "NAU7802 Gain BYPASS SET OK");
						break;
					case RESULT_ERROR:
						ExtErrorCount++;
						oSerial_Log("Mearment", "NAU7802 Gain BYPASS SET ERROR");
						break;
				}
			}
			else{
				SettleTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				ExtErrorCount = 0;
				ExtAdcStep++;
			}
			break;
		case 1:	// MUX 안정화 대기 (transient 샘플 버림)
			if(NAU7802_AnalogRead(&NAU7802, NauChannel, NAU7802_SAMPLING_320SPS, &ReadAnalog) != RESULT_RUN)
			{
				if(oTMR_Elapsed(&NAU7802.OpenTimestamp, 1000, TICKBASE_SYSTICK) && oTMR_Elapsed(&SettleTimer, EXTADC_MUX_SETTLE_MS, TICKBASE_SYSTICK)){
					ExtAdcStep++;
				}
			}
			break;
		case 2:	// Read NAU7802 — 평균 누적
			switch(NAU7802_AnalogRead(&NAU7802, NauChannel, NAU7802_SAMPLING_320SPS, &ReadAnalog))
			{
				case RESULT_RUN:
					break;
				case RESULT_OK:
					ExtErrorCount = 0;

					if(NauChannel == 1){
						ReadAnalog = AI_TO_MV(ReadAnalog);
					}
					SumAnalog += ReadAnalog;
					SumOfCount++;
					oSerial_Log("Mearment", "Sample[%d/%d] val=%.4f sum=%.4f", (int)SumOfCount, (int)ReadCount, ReadAnalog, SumAnalog);

					if(SumOfCount >= ReadCount){
						double Avg = SumAnalog / SumOfCount;
						double Final = oLinear(&MiIoT_Parameter.SystemConfig.Calibration.ADC[CalIndex], Avg, 0);

						oSerial_Log("Mearment", "ExternalAdc CH%d Type=%d Nau=%d avg=%.4f Cal[%d]=%.4f",
							(int)Channel, (int)TypeOfSensor, (int)NauChannel, Avg, (int)CalIndex, Final);

						*pAnalog = Final;
						result = RESULT_OK;
					}
					break;
				default:
					ExtErrorCount++;
					oSerial_Log("Mearment", "Read FAIL (errCnt=%d, n=%d/%d)", (int)ExtErrorCount, (int)SumOfCount, (int)ReadCount);
					break;
			}
			break;
	}

	if(ExtErrorCount > 5){
		oSerial_Log("Mearment", "ERROR errCnt=%d > 5, abort", (int)ExtErrorCount);
		result = RESULT_ERROR;
		*pAnalog = 0;
	}

	if(result != RESULT_RUN){
		ExtErrorCount = 0;
		SumAnalog = 0;
		SumOfCount = 0;
		ExtAdcStep = 0;
	}

	return result;
}

/* 진단용 — 전원 인가부터 측정/전원 차단까지 한 함수에서 처리.
 * pValues[SAMPLINGADCS_COUNT] 배열에 결과 저장:
 *  [SAMPLINGADCS_INDEX_SUPPLY] : GPIOs.ADC.SystemSupply (mV)
 *  [SAMPLINGADCS_INDEX_MV]     : ch1     + V mux  → NAU CH1 (mV 단위)
 *  [SAMPLINGADCS_INDEX_MA]     : ch4     + V mux  → NAU CH1 (mV 단위, mA 환산은 호출자에서)
 *  [SAMPLINGADCS_INDEX_DIFF]   : ch7+8 페어 + uV mux → NAU CH2 (mV 단위) */
oResult_t Measurement_SamplingADCs(double *pValues)
{
	static uint8_t Step = 0;
	static uint32_t StepTimer = 0;
	double Analog = 0;
	oResult_t result = RESULT_RUN;

	if(pValues == NULL){
		Step = 0;
		return RESULT_ERROR;
	}

	switch(Step)
	{
		case 0:	// Supply 측정
			if((result = Measurement_Supply(15)) != RESULT_RUN){
				pValues[SAMPLINGADCS_INDEX_SUPPLY] = (result == RESULT_OK) ? (double)GPIOs.ADC.SystemSupply : 0;
				Step++;
				result = RESULT_RUN;
			}
			break;
		case 1:	// 전원 인가 (AMP/ADCRef/12V + PwrSupply CH1/2/3 모두 ON)
			Measurement_PowerOn(0);
			StepTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			Step++;
			break;
		case 2:	// power stable wait
			if(oTMR_Elapsed(&StepTimer, 500, TICKBASE_SYSTICK)){
				Step++;
			}
			break;
		case 3:	// mV: ch1 + V mux
			Analog_ChannelConfig(1, IoTSensorType_mV);
			if((result = Analog_ExternalAdc(1, IoTSensorType_mV, &Analog, MEASURE_SAMPLING_MAXCOUNT)) == RESULT_OK){
				pValues[SAMPLINGADCS_INDEX_MV] = Analog;
				oSerial_Log("Mearment", "SamplingADCs mV=%.3f", Analog);
				Step++;
				result = RESULT_RUN;
			}
			break;
		case 4:	// mA: ch4 + V mux
			Analog_ChannelConfig(4, IoTSensorType_mA);
			if((result = Analog_ExternalAdc(4, IoTSensorType_mA, &Analog, MEASURE_SAMPLING_MAXCOUNT)) == RESULT_OK){
				pValues[SAMPLINGADCS_INDEX_MA] = Analog;
				oSerial_Log("Mearment", "SamplingADCs mA=%.3f", Analog);
				Step++;
				result = RESULT_RUN;
			}
			break;
		case 5:	// Differential: ch7+8 페어 + uV mux
			Analog_ChannelConfig(7, IoTSensorType_Differential);
			if((result = Analog_ExternalAdc(7, IoTSensorType_Differential, &Analog, MEASURE_SAMPLING_MAXCOUNT)) == RESULT_OK){
				pValues[SAMPLINGADCS_INDEX_DIFF] = Analog;
				oSerial_Log("Mearment", "SamplingADCs Diff=%.3f", Analog);
				Step++;
				result = RESULT_RUN;
			}
			break;
		case 6:	// 전원 차단
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		Analog_ChannelConfig(-1, IoTSensorType_NULL);
		Measurement_PowerOn(-1);
		NAU7802_DeInit(&NAU7802);

		Step = 0;
	}

	return result;
}

/* 채널 배열로 측정 — 호출자가 port별로 정렬된 채널 목록을 전달.
 * Port는 PwrSupplyCh 전원 인가 및 warmup 결정에 사용. */
oResult_t Measurement_ReadAnalog(uint8_t Port, int8_t *Channels, uint8_t Count, IoTChannelConfig_t *pConfig)
{
	static uint8_t ReadAnalogStep = 0;
	static uint8_t ReadIndex = 0;
	static uint8_t RetryCount = 0;
	static uint32_t ReadTimer;
	static uint16_t ReadWarmupTime;
	oResult_t result = RESULT_RUN;
	IoTChannelConfig_t Config;
	double Analog = 0;

	if(Count == 0 || Channels == NULL){
		ReadAnalogStep = 0;
		return RESULT_NULL;
	}

	switch(ReadAnalogStep)
	{
		default:
			ReadAnalogStep = 0;
			break;
		case 0://Power on port + warmup 계산
			Measurement_PowerOn(Port);
			Analog_ChannelConfig(-1, IoTSensorType_NULL);

			ReadWarmupTime = 1000;
			for(uint8_t i=0; i<Count; i++){
				int8_t ch = Channels[i];
				if(ch > 0 && ch <= MEASUREMENT_CHANNEL_MAXCOUNT){
					ReadWarmupTime = MATH_MAX(ReadWarmupTime, pConfig[ch-1].WarmupTime);
				}
			}

			ReadIndex = 0;
			RetryCount = 0;
			ReadTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			ReadAnalogStep++;
			break;
		case 1://Wait power stable
			if(oTMR_Elapsed(&ReadTimer, ReadWarmupTime, TICKBASE_SYSTICK)){
				ReadTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				ReadAnalogStep++;
			}
			break;
		case 2://Pre-clear — S8(GND short)로 잔류전압 제거
			if(ReadIndex >= Count){
				result = RESULT_OK;
				break;
			}
			Analog_ChannelConfig(0, IoTSensorType_NULL);	// S8 GND short
			ReadTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			ReadAnalogStep++;
			break;
		case 3://Clear 정착 대기
			if(oTMR_Elapsed(&ReadTimer, EXTADC_CLEAR_MS, TICKBASE_SYSTICK)){
				ReadAnalogStep++;
			}
			break;
		case 4://Read each channel in array
			if(ReadIndex >= Count){
				result = RESULT_OK;
			}
			else{
				int8_t Ch = Channels[ReadIndex];
				if(Ch <= 0 || Ch > MEASUREMENT_CHANNEL_MAXCOUNT){
					ReadIndex++;
					ReadAnalogStep = 2;	/* 다음 채널 → clear부터 다시 */
					break;
				}
				Config = pConfig[Ch-1];

				/* MUX 채널 선택 — 외부 ADC만 의미 있음. Thermistor는 영향 없음. */
				Analog_ChannelConfig(Ch, Config.TypeOfSensor);

				switch(Config.TypeOfSensor)
				{
					case IoTSensorType_Thermistor:
						result = Analog_InternalAdc(Ch, &Analog, MEASURE_SAMPLING_MAXCOUNT);
						if(result == RESULT_OK){
							/* mV → 저항 (분압회로) → 섭씨 (NTC) */
							if(Analog < ADC_VREF){
								Analog = VOLT_TO_LOW_RESISTANCE(5000, Analog, 18000);
								Analog += Analog * 0.05;
							}
							Analog = NTC_3K_TO_CELSIUS(Analog);
						}
						break;
					case IoTSensorType_mV:
						result = Analog_ExternalAdc(Ch, Config.TypeOfSensor, &Analog, MEASURE_SAMPLING_MAXCOUNT);
						break;
					case IoTSensorType_mA:
						result = Analog_ExternalAdc(Ch, Config.TypeOfSensor, &Analog, MEASURE_SAMPLING_MAXCOUNT);
						if(result == RESULT_OK){
							Analog /= 249;						/* mV → mA (sense R=249Ω) */
						}
						break;
					case IoTSensorType_Differential:
						result = Analog_ExternalAdc(Ch, Config.TypeOfSensor, &Analog, MEASURE_SAMPLING_MAXCOUNT);
						/* mV 그대로 사용 */
						break;
					case IoTSensorType_FullBridge:
						result = Analog_ExternalAdc(Ch, Config.TypeOfSensor, &Analog, MEASURE_SAMPLING_MAXCOUNT);
						if(result == RESULT_OK){
							Analog = (double)CONVERT_mV_TO_uV(Analog);	/* mV → uV */
						}
						break;
					default:
						result = RESULT_ERROR;
						break;
				}

				if(result == RESULT_OK && pDataArray != NULL && CountOfItem < MIIOT_DATA_ARRAY_MAX_COUNT){
					uint32_t encoded = 0;
					switch(Config.TypeOfSensor)
					{
						case IoTSensorType_FullBridge:
							encoded = (uint32_t)MIIOT_DATA_ENCODE_uV(Analog);
							break;
						case IoTSensorType_Differential:
						case IoTSensorType_mV:
							encoded = (uint32_t)MIIOT_DATA_ENCODE_mV(Analog);
							break;
						case IoTSensorType_mA:
							encoded = (uint32_t)MIIOT_DATA_ENCODE_mA(Analog);
							break;
						case IoTSensorType_Resistance:
							encoded = (uint32_t)MIIOT_DATA_ENCODE_Ohm(Analog);
							break;
						case IoTSensorType_Thermistor:
							encoded = (uint32_t)MIIOT_DATA_ENCODE_TEMP(Analog);
							break;
						default:
							encoded = 0;
							break;
					}

					pDataArray->Items[CountOfItem].Channel = Ch;
					pDataArray->Items[CountOfItem].Type = Config.TypeOfSensor;
					pDataArray->Items[CountOfItem].Data = encoded;
					oSerial_Log("Mearment", "Port%d Item[%d] CH%d Type=%d Raw=%.3f Enc=%lu", (int)Port, (int)CountOfItem, (int)Ch, (int)Config.TypeOfSensor, Analog, (unsigned long)encoded);
					CountOfItem++;
				}

				if(result != RESULT_RUN){
					if(result == RESULT_OK){
						/* 측정 성공 → 다음 채널 (clear부터 다시) */
						ReadIndex++;
						RetryCount = 0;
						ReadAnalogStep = 2;
					}
					else{
						/* 오류 → 재시도 (clear부터 다시) */
						RetryCount++;
						if(RetryCount >= MEASURE_READ_RETRY_MAXCOUNT){
							oSerial_Log("Mearment", "CH%d Type=%d ABORT after %d retries (result=%d Analog=%.3f)", (int)Ch, (int)Config.TypeOfSensor, (int)MEASURE_READ_RETRY_MAXCOUNT, (int)result, Analog);
							ReadIndex++;
							RetryCount = 0;
						}
						else{
							oSerial_Log("Mearment", "CH%d Type=%d RETRY %d/%d (result=%d Analog=%.3f)", (int)Ch, (int)Config.TypeOfSensor, (int)RetryCount, (int)MEASURE_READ_RETRY_MAXCOUNT, (int)result, Analog);
						}
						ReadAnalogStep = 2;
					}
				}

				result = RESULT_RUN;
			}
			break;
	}


	if(result != RESULT_RUN){
		ReadAnalogStep = 0;
		RetryCount = 0;
	}

	return result;
}

oResult_t Measurement_Sensor(IoT_DataPacket_t *pPacket, IoTChannelConfig_t *pConfig)
{
	static uint32_t MeasureTimer = 0;
	static uint8_t MeasureStep = 0;
	static uint8_t ReadPort = 0;
	static uint8_t AdcInitCount = 0;
	static uint8_t TotalActiveChannels = 0;	/* 진행률 계산용 — 활성 채널 총 개수 */
	oResult_t result = RESULT_RUN;

	switch(MeasureStep)
	{
		case 0:
			memset(pPacket, 0, sizeof(IoT_DataPacket_t));
			MiSerial_SensorSamplingProgress = 0;

			HAL_ADC_DeInit(&hadc1);
			MiIoT_Status.StatusBits.DisconnectedSensor = False;
			MiIoT_Status.TroubleCode = 0;

			if(MiIoT_IsValidParameter(&MiIoT_Parameter)){
				pDataArray = (IoTDataArray_t *)&pPacket->Frame;
				CountOfItem = 0;
				MeasureTimer = oTMR_GetTick(TICKBASE_SYSTICK);

				/* 포트별 채널 그룹화 (1회) */
				memset(PortChannels, 0, sizeof(PortChannels));
				memset(PortChannelCount, 0, sizeof(PortChannelCount));
				for(uint8_t i=0; i<MEASUREMENT_CHANNEL_MAXCOUNT; i++){
					if(pConfig[i].TypeOfSensor == IoTSensorType_NULL) continue;
					uint8_t p = pConfig[i].SupplySource;
					if(p < 1 || p > MEASUREMENT_PORT_MAXCOUNT) continue;
					PortChannels[p-1][PortChannelCount[p-1]++] = i+1;
				}

				TotalActiveChannels = PortChannelCount[0] + PortChannelCount[1] + PortChannelCount[2];

				ReadPort = 1;
				MeasureStep++;
				oSerial_Log("Mearment", "START VREF=%d Port1=%d Port2=%d Port3=%d", (int)ADC_VREF, (int)PortChannelCount[0], (int)PortChannelCount[1], (int)PortChannelCount[2]);
			}
			else{
				result = RESULT_NULL;
			}
			break;
		case 1:
			Measurement_PowerOn(-1);

			if(oTMR_Trigger(&MeasureTimer, 100, 1, TICKBASE_SYSTICK)){
				MeasureStep++;
			}
			break;
		case 2:
			Measurement_PowerOn(0); //DCDC On

			if(oTMR_Trigger(&MeasureTimer, 500, 1, TICKBASE_SYSTICK)){
				MeasureStep++;
			}
			break;
		case 3:
			if(AdcInitCount >= 10){
				oSerial_Log("Mearment", "ADC Init ABORT (try=%d)", (int)AdcInitCount);
				result = RESULT_NULL;
			}
			else if(HAL_ADC_Init(&hadc1) == HAL_OK){
				oSerial_Log("Mearment", "ADC Init OK (try=%d)", (int)AdcInitCount);
				MeasureStep++;
			}
			else{
				AdcInitCount++;
				oSerial_Log("Mearment", "ADC Init FAIL (try=%d)", (int)AdcInitCount);
				MeasureStep = 0;
			}
			break;
		case 4:
			if(Measurement_CalibrateVDD() != RESULT_RUN){
				MeasureStep++;
			}
			break;
		case 5:
			/* 측정된 채널 수 기준 진행률 (0~99%, 100%는 case 7 완료 시) */
			MiSerial_SensorSamplingProgress = (TotalActiveChannels > 0)
				? MATH_LIMIT((uint8_t)((float)CountOfItem/(float)TotalActiveChannels*99), 0, 99)
				: 99;

			if(ReadPort > MEASUREMENT_PORT_MAXCOUNT){
				MeasureStep++;
			}
			else if(PortChannelCount[ReadPort-1] == 0){
				/* 포트에 채널 없음 → 건너뛰기 */
				ReadPort++;
			}
			else if(Measurement_ReadAnalog(ReadPort, PortChannels[ReadPort-1], PortChannelCount[ReadPort-1], pConfig) != RESULT_RUN){
				oSerial_Log("Mearment", "Port%d DONE, next=%d", (int)ReadPort, (int)(ReadPort+1));

				//시리얼 정지 명령어 실행
				if(MiSerial_UpdateSensorCmd && MiSerial_StopSensorCmd){
					MeasureStep++;    /* 현재 포트 완료 후 Stop 처리 */
				}
				else{
					ReadPort++;
				}
			}
			break;
		case 6:
			Measurement_PowerOn(0);

			if(NAU7802_DeInit(&NAU7802) != RESULT_RUN){
				MeasureStep++;
				MeasureTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			}
			break;
		case 7:
			MiSerial_SensorSamplingProgress = 100;
			oSerial_Log("Mearment", "COMPLETE OK");
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		if(result == RESULT_OK){
			pPacket->TypeOfData = IoTDataType_DataArray_Type1;
			pPacket->DLC = MIIOT_IOTDATA_SIZE_DATAARRAY(CountOfItem);
			oSerial_Log("Mearment", "ALL PORTS DONE Items=%d DLC=%d", (int)CountOfItem, (int)pPacket->DLC);

			MIIOT_DT_TO_IOTTIME(&MiIoT_DT, &pPacket->Frame);
		}

		Analog_ChannelConfig(-1, IoTSensorType_NULL);
		Measurement_PowerOn(-1);
		HAL_ADC_Stop(&hadc1);
		AdcInitCount = 0;
		MeasureStep = 0;
		pDataArray = NULL;
	}

	return result;
}

oResult_t Measurement_Supply(uint32_t Count)
{
	static uint8_t ReadSupplyStep = 0;
	static uint32_t ReadSupplyTimer = 0;
	double Analog;
	uint16_t ADC;
	oResult_t result = RESULT_RUN;

	switch(ReadSupplyStep)
	{
		case 0:
			ReadSupplyStep++;
			break;
		case 1:
			ReadSupplyTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			Measurement_PowerOn(0); //DCDC On
			ReadSupplyStep++;
			break;
		case 2:
			if(oTMR_Elapsed(&ReadSupplyTimer, 1000, TICKBASE_SYSTICK)){
				ReadSupplyStep++;
			}
			break;
		case 3:
			if(Measurement_CalibrateVDD() != RESULT_RUN){
				ReadSupplyStep++;
			}
			break;
		case 4:
			if(Native_ADCRead(ADC_CHANNEL_3, ADC_SAMPLETIME_640CYCLES_5, &ADC, Count) == RESULT_OK){
				Analog = (double)ADC_TO_SUPPLY(ADC);
				GPIOs.ADC.SystemSupply = (uint16_t)oLinear(&MiIoT_Parameter.SystemConfig.Calibration.ADC[0], Analog, 0); //Offset, Gain 보정
				ReadSupplyStep++;
			}
			break;
		case 5:
			if(Native_ADCRead(ADC_CHANNEL_VBAT, ADC_SAMPLETIME_640CYCLES_5, &ADC, Count) == RESULT_OK){
				GPIOs.ADC.InternalBAT = (uint16_t)ADC_TO_VBAT(ADC);
				ReadSupplyStep++;
			}
			break;
		case 6:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		ReadSupplyStep = 1;
		Measurement_PowerOn(-1); //DCDC On

		HAL_ADC_Stop(&hadc1);
	}

	return result;
}

/* History

2026-06-26 | v0.1
	- baseline (Mi_Measurement.c)
*/
