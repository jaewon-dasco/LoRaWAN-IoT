/*
 * Mi_Sensor.c
 *
 *  Created on: Dec 9, 2024
 *      Author: JONE
 */

/************************************************
 * ADC Calibration channel map
 ************************************************
 * [0] = Battery
 * [1] = AI1 mV (0545P)
 * [2] = AI2 mV (0545M)
 * [3] = AI1 mA (0545P)
 * [4] = AI2 mA (0545M)
 ************************************************/

#include "ONE_SlidingHistogram.h"
#include "Mi_Main.h"
#include "Mi_Measurement.h"
#include "Mi_IoT.h"
#include "ONE_Math.h"
#include "ONE_Math.h"
#include "Mi_Native.h"

#define ADC_CHANNEL_MAXINDEX		5
#define ADC_VREF					(MiIoT_Parameter.SystemConfig.ActualVRef == 0 ? 3300 : MiIoT_Parameter.SystemConfig.ActualVRef)
#define ADC_MAXDIGIT				(uint16_t)((uint32_t)(4095 * 64) >> 2)

#define ADC_AMP_VOTDIV				VOLT_DIV_RATIO(3000,750)
#define ADC_TO_AI(x)				((double)(x)/(double)ADC_MAXDIGIT*(double)ADC_VREF)
#define ADC_TO_SUPPLY(x)			(ADC_TO_AI(x)/ADC_AMP_VOTDIV/VOLT_DIV_RATIO(200000,200000))
#define ADC_TO_EXTSUPPLY(x)			(ADC_TO_AI(x)/ADC_AMP_VOTDIV/VOLT_DIV_RATIO(1000000,100000))
#define ADC_TO_VBAT(x)				(ADC_TO_AI(x)*3)
#define SAMPLING_MAX_FREQ			6000
#define SWEEPFREQ_DIV				250

/************************************************
 * DMA Channel
 ************************************************
 * CH1[0] = TempSupply
 * CH2[1] = TempSignal
 * CH3[2] = AI1(0545P)
 * CH4[3] = AI2(0545M)
 * CH5[4] = Vbat
 */
uint16_t DMABuffer[ADC_CHANNEL_MAXINDEX];
TIM_HandleTypeDef *pTIMCapture_Timer;
uint32_t TIMCapture_Channel;
TIM_HandleTypeDef *pTIMPWM_Timer;
uint32_t TIMCapture_CLK;
uint16_t HistorgramBuffer[8000];
uint32_t CaptureDMABuffer[300];
oSlidingHistogram_t SlidingHistorgam;

oResult_t Measurement_FreqCapture(TIM_HandleTypeDef *pTIM, uint32_t Channel, uint32_t Start, uint32_t End, uint32_t SamplingTime)
{
	static uint8_t SamplingStep = 0;
	static uint32_t SamplingTimer = 0;
	static uint16_t DMABufferSize;
	static DMA_HandleTypeDef *hdma = NULL;
	static uint32_t LastReadIndex = 0;        // CPU가 마지막으로 읽은 버퍼 위치
	oResult_t result = RESULT_RUN;

	switch(SamplingStep)
	{
		case 0:
			oSlidingHistogram_Init(&SlidingHistorgam, Start, End, 1, &HistorgramBuffer[0], ArrayLen(HistorgramBuffer), 0, 0);

			pTIMCapture_Timer = pTIM;
			TIMCapture_Channel = Channel;

			__HAL_TIM_SET_PRESCALER(pTIM, 0);
			__HAL_TIM_SET_AUTORELOAD(pTIM, 0xFFFFFFFF);
			__HAL_TIM_SET_COUNTER(pTIM, 0);
			memset(&CaptureDMABuffer, 0, sizeof(CaptureDMABuffer));

			LastReadIndex = 0;
			TIMCapture_CLK = Native_GetTIMxCLK(pTIM);

			switch(Channel)
			{
				case TIM_CHANNEL_1: hdma = pTIM->hdma[1]; break;
				case TIM_CHANNEL_2: hdma = pTIM->hdma[2]; break;
				case TIM_CHANNEL_3: hdma = pTIM->hdma[3]; break;
				case TIM_CHANNEL_4: hdma = pTIM->hdma[4]; break;
				case TIM_CHANNEL_5: hdma = pTIM->hdma[5]; break;
				default: 			hdma = pTIM->hdma[6]; break;
			}

			if(hdma->State != HAL_DMA_STATE_BUSY){
				DMABufferSize = ArrayLen(CaptureDMABuffer);
				HAL_TIM_GenerateEvent(pTIM, TIM_EVENTSOURCE_UPDATE);
				HAL_TIM_IC_Start_DMA(pTIM, Channel, (uint32_t *)&CaptureDMABuffer, DMABufferSize);
			}

			SamplingTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			SamplingStep++;
			 // @suppress("No break at end of case")
		case 1:
			if(oTMR_Elapsed(&SamplingTimer, SamplingTime, TICKBASE_SYSTICK)){
				SamplingStep++;
			}
			else if(oTMR_Elapsed(&SamplingTimer, 100, TICKBASE_SYSTICK) && SlidingHistorgam.Result == 0){
				result = RESULT_ERROR;
			}
			else if(hdma){
				uint32_t dma_write_index = DMABufferSize - hdma->Instance->CNDTR;

				while (LastReadIndex != dma_write_index) {
					uint32_t now = CaptureDMABuffer[LastReadIndex];
					uint32_t *pPrev = LastReadIndex != 0 ? &CaptureDMABuffer[LastReadIndex-1] : &CaptureDMABuffer[DMABufferSize-1];

					if(*pPrev != 0 && now != 0){
						float period_count = (float)((now >= *pPrev) ? (now - *pPrev) : (pTIM->Instance->ARR + 1u - *pPrev + now));
						float freq = (float)TIMCapture_CLK /period_count;
						oSlidingHistogram_Update(&SlidingHistorgam, freq);
					}

					//*pPrev = 0;
					LastReadIndex = (LastReadIndex+1) % DMABufferSize;
				}
			}
			break;
		case 2:
			if(OutOfRange(SlidingHistorgam.Result, Start, End)){
				result = RESULT_ERROR;
			}
			else{
				result = RESULT_OK;
			}
			break;
	}

	if(result != RESULT_RUN){
		HAL_TIM_IC_Stop_DMA(pTIM, Channel);
		SamplingStep = 0;
	}

	return result;
}

uint32_t SweepElapsedTime = 0;

oResult_t Measurement_PWMSweep(TIM_HandleTypeDef *pTIM, uint32_t Channel, float Start, float End)
{
    static uint32_t SweepTimer = 0;
    static uint8_t SweepStep = 0;
    static uint16_t DMABuffer[SWEEPFREQ_DIV * 3];

    oResult_t result = RESULT_RUN;

    if(Start == End){
        SweepStep = 0;
        return RESULT_ERROR;
    }

    switch(SweepStep)
    {
        case 0:
            // 1. 주파수 스텝 계산 (End - Start 순서 주의)
            float freq_step = (End - Start) / (float)(SWEEPFREQ_DIV - 1);

            // TIM1은 보통 APB2 버스에 있습니다. 정확한 클럭 확인 필요.
            // (일반적으로 SystemCoreClock와 같거나 PCLK2의 2배입니다)
            uint32_t F_CLK = HAL_RCC_GetPCLK2Freq();
            if(pTIM->Init.Prescaler > 0) F_CLK /= (pTIM->Init.Prescaler + 1);

            float current_freq;
            uint16_t arr_val;
            uint32_t data_index = 0;

            // 2. 버퍼 채우기 (TIM1 전용 구조: ARR -> RCR -> CCR1)
            for (uint32_t i = 0; i < SWEEPFREQ_DIV; i++) {
                current_freq = Start + (freq_step * i);

                if (current_freq > 0) {
                    arr_val = (uint16_t)roundf((float)F_CLK / current_freq) - 1;
                } else {
                    arr_val = 0;
                }

                // [데이터 1] ARR: 주파수 설정
                DMABuffer[data_index++] = arr_val;

                // [데이터 2] RCR: 반복 카운터 (TIM1 필수, 0으로 설정)
                DMABuffer[data_index++] = 0;

                // [데이터 3] CCR1: 듀티 50% 설정
                // 주의: Channel 1을 사용할 때만 이 순서가 맞습니다.
                DMABuffer[data_index++] = arr_val / 2;
            }

            GPIOs.DO.VwDe = 1;
            SweepTimer = oTMR_GetTick(TICKBASE_SYSTICK);
            pTIMPWM_Timer = pTIM; // 콜백용 핸들 저장
            SweepStep++;
            break;

        case 1:
            // Base: ARR, Length: 3 (ARR, RCR, CCR1)
            pTIM->Instance->DCR = TIM_DMABASE_ARR | TIM_DMABURSTLENGTH_3TRANSFERS;
            pTIM->Instance->ARR = DMABuffer[0];          // 첫 주파수
			pTIM->Instance->RCR = DMABuffer[1];          // RCR (0)
			pTIM->Instance->CCR1 = DMABuffer[2];
			pTIM->Instance->CNT = 0;

			// 1. DMA 먼저 Start (대기 상태로 만듦)
            HAL_DMA_Start_IT(pTIM->hdma[TIM_DMA_ID_UPDATE], (uint32_t)DMABuffer, (uint32_t)&pTIM->Instance->DMAR, SWEEPFREQ_DIV * 3);

            // 2. 타이머 카운터 초기화 (중요: 업데이트 이벤트 발생 방지)
			__HAL_TIM_SET_COUNTER(pTIM, 0);

			// 3. 타이머의 DMA 요청 허용 (문 열기)
			__HAL_TIM_ENABLE_DMA(pTIM, TIM_DMA_UPDATE);

			// 4. 마지막으로 PWM 시작 (카운트 시작)
			HAL_TIM_PWM_Start(pTIM, Channel);

            __HAL_TIM_MOE_ENABLE(pTIM);

            SweepStep++;
            break;

        case 2:
            // 4. 완료 대기 (Callback에 의해 PWM_dma_finished가 1이 됨)
        	if (HAL_DMA_GetState(pTIM->hdma[TIM_DMA_ID_UPDATE]) == HAL_DMA_STATE_READY){
                result = RESULT_OK;
            }
        	else if(oTMR_Elapsed(&SweepTimer, 600, TICKBASE_SYSTICK)){
        		result = RESULT_ERROR;
        	}
            break;
    }

    if(result != RESULT_RUN){
        // 종료 처리
    	HAL_DMA_Abort_IT(pTIM->hdma[TIM_DMA_ID_UPDATE]);
        HAL_TIM_PWM_Stop(pTIM, Channel);

        pTIMPWM_Timer = 0;
        SweepStep = 0;

        GPIOs.DO.VwDe = 0;
    }

    return result;
}

oResult_t Measurement_SamplingFrequency(IoTChannelPropertiesVW_t *pProperties, float *pFreq, uint8_t Count)
{
	static uint8_t SamplingFreqStep = 0;
	static uint8_t SamplingCount = 0;
	static float SamplingFreq;
	static uint32_t SamplingFreqTimer = 0;
	oResult_t result = RESULT_RUN;

	switch(SamplingFreqStep)
	{
		case 0:
			if((pProperties->StartFreq == 0 || pProperties->EndFreq == 0) || (pProperties->StartFreq >= pProperties->EndFreq)){
				pProperties->StartFreq = 400;
				pProperties->EndFreq = 6000;
			}

			pProperties->EndFreq = MATH_LIMIT(pProperties->EndFreq, 500, 6000);
			pProperties->StartFreq = MATH_LIMIT(pProperties->StartFreq, 400, pProperties->EndFreq-100);

			if(pFreq){
				*pFreq = 0;
			}

			SamplingCount = 0;
			SamplingFreq = 0;
			SamplingFreqTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			SamplingFreqStep++;
			break;
		case 1:
			if((result=Measurement_PWMSweep(&htim1, TIM_CHANNEL_1, pProperties->StartFreq, pProperties->EndFreq)) == RESULT_OK){
				SamplingFreqTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				SamplingFreqStep++;
				result = RESULT_RUN;
			}
			break;
		case 2:
			if(oTMR_Elapsed(&SamplingFreqTimer, 100, TICKBASE_SYSTICK)){
				SamplingFreqStep++;
			}
			break;
		case 3:
			switch(Measurement_FreqCapture(&htim2, TIM_CHANNEL_1, pProperties->StartFreq, pProperties->EndFreq, 700))
			{
				default:
				case RESULT_RUN:
					break;
				case RESULT_OK:
					SamplingFreqStep++;
					break;
				case RESULT_ERROR:
					result = RESULT_ERROR;
					break;
			}
			break;
		case 4:
			if(SlidingHistorgam.BinCache > 0){
				SamplingFreq += SlidingHistorgam.Result;

				if(++SamplingCount >= Count){
					if(pFreq && SamplingCount > 0){
						*pFreq = SamplingFreq / (float)SamplingCount;
					}
					result = RESULT_OK;
				}
				else{
					SamplingFreqStep = 1;
				}

			}
			else{
				result = RESULT_ERROR;
			}
			break;
	}

	if(result != RESULT_RUN){
		SamplingFreqStep = 0;
	}

	return result;
}

oResult_t Measurement_SamplingResistance(float *pOhm)
{
	static uint8_t SamplingTempStep = 0;
	static uint8_t SamplingCount = 0;
	static uint32_t SamplingSumPLUS = 0;
	static uint32_t SamplingSumMINUS = 0;
	static uint32_t SamplingTimer = 0;
	static double AI_PLUS, AI_MINUS;
	oResult_t result = RESULT_RUN;

	switch(SamplingTempStep)
	{
		case 0:
			SamplingCount = 0;
			SamplingSumPLUS = 0;
			SamplingSumMINUS = 0;
			SamplingTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			SamplingTempStep++;
			break;
		case 1:
			if(oTMR_Trigger(&SamplingTimer, 50, 1, TICKBASE_SYSTICK)){
				SamplingSumPLUS += (uint32_t)DMABuffer[0];
				SamplingSumMINUS += (uint32_t)DMABuffer[1];
				SamplingCount++;
			}

			if(SamplingCount >= 15){
				AI_PLUS = ADC_TO_AI((double)SamplingSumPLUS/(double)SamplingCount);
				AI_MINUS = ADC_TO_AI((double)SamplingSumMINUS/(double)SamplingCount);

				SamplingTempStep++;
			}
			break;
		case 2:
			if(AI_MINUS <= 0){
				*pOhm = 0;
			}
			else{
				*pOhm = VOLT_TO_HIGH_RESISTANCE(AI_PLUS, AI_MINUS, 500) - 1000;
			}

		    result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		SamplingTempStep = 0;
	}

	return result;
}

oResult_t Measurement_SamplingAnalog(uint8_t Channel, IoTSensorType_t SensorType, float *pAnalog)
{
	static uint8_t SamplingTempStep = 0;
	static uint8_t SamplingCount = 0;
	static uint32_t SamplingTimer = 0;
	static double SamplingSumAdc = 0;
	oResult_t result = RESULT_RUN;

	if(Channel > 2){
		SamplingTempStep = 0;
		return RESULT_ERROR;
	}

	switch(SamplingTempStep)
	{
		case 0:
			SamplingCount = 0;
			SamplingSumAdc = 0;
			SamplingTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			SamplingTempStep++;
			break;
		case 1:
			if(oTMR_Trigger(&SamplingTimer, 50,1, TICKBASE_SYSTICK)){
				SamplingSumAdc += DMABuffer[Channel+1];
				SamplingCount++;
			}

			if(SamplingCount >= 15){
				double FinalAnalog = ADC_TO_AI((SamplingSumAdc/(double)SamplingCount)) / ADC_AMP_VOTDIV;

				switch(SensorType)
				{
					case IoTSensorType_mA:
						FinalAnalog /= 249;
						FinalAnalog = oLinear(&MiIoT_Parameter.SystemConfig.Calibration.ADC[Channel+2], FinalAnalog, 0);
						break;
					case IoTSensorType_mV:
						FinalAnalog = oLinear(&MiIoT_Parameter.SystemConfig.Calibration.ADC[Channel], FinalAnalog, 0);
						break;
					default:
						break;
				}

				*pAnalog = (float)FinalAnalog;

				SamplingCount = 0;
				SamplingSumAdc = 0;
				SamplingTempStep++;

				result = RESULT_OK;
			}
			break;
	}

	if(result != RESULT_RUN){
		SamplingTempStep = 0;
	}

	return result;
}

oResult_t Measurement_SamplingDifferantial(float *pVolgate)
{
	static uint8_t SamplingTempStep = 0;
	static float AI_PLUS, AI_MINUS;
	oResult_t result = RESULT_RUN;

	switch(SamplingTempStep)
	{
		case 0:
			AI_PLUS = 0;
			AI_MINUS = 0;
			SamplingTempStep++;
			break;
		case 1:
			if(Measurement_SamplingAnalog(1, IoTSensorType_mV, &AI_PLUS) != RESULT_RUN){
				SamplingTempStep++;
			}
			break;
		case 2:
			if(Measurement_SamplingAnalog(2, IoTSensorType_mV, &AI_MINUS) != RESULT_RUN){
				SamplingTempStep++;
			}
			break;
		case 3:
			*pVolgate = AI_PLUS-AI_MINUS;
		    result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		SamplingTempStep = 0;
	}

	return result;
}

oResult_t Measurement_ChannelSelect(int32_t Channel)
{
	static uint8_t ChannelSelectStep = 0;
	static uint32_t ChannelSelectTimer = 0;
	static IoTChannelConfig_t *pConfig;
	static uint32_t SelectedPort = 0;
	static uint32_t SelectedChannel = 0;
	oResult_t result = RESULT_RUN;

	if(Channel > MIIOT_CHANNEL_MAXCOUNT){
		ChannelSelectStep = 0;
		return RESULT_ERROR;
	}

	switch(ChannelSelectStep)
	{
		case 0://5v on
			if(Channel == 0 || Channel < 0){
				GPIOs.DO.Dc5vEnable = Channel == 0;

				GPIOs.DO.AnalogPCurrentEnable = 0;
				GPIOs.DO.AnalogMCurrentEnable = 0;
				GPIOs.DO.AnalogAmpEnable = 0;
				GPIOs.DO.AnalogModeEnable = 0;
				GPIOs.DO.TempModeEnable = 0;
				GPIOs.DO.VwModeEnable = 0;
				GPIOs.DO.VwEnable = 0;

				memset(&GPIOs.DO.PortArray, 0, sizeof(GPIOs.DO.PortArray));
				GPIOs.DO.Ch1Enable = 0;
				GPIOs.DO.Ch2Enable = 0;

				SelectedPort = 0;
				SelectedChannel = 0;

				result = RESULT_OK;
			}
			else{
				if(GPIOs.DO.Dc5vEnable){
					ChannelSelectStep += 2;
				}
				else{
					ChannelSelectStep += 1;
				}

				pConfig = &MiIoT_Parameter.ChannelConfig[Channel-1];
				ChannelSelectTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			}
			break;
		case 1://wait relay
			if(oTMR_Elapsed(&ChannelSelectTimer, 500, TICKBASE_SYSTICK)){
				ChannelSelectStep++;
			}
			break;
		case 2://set property
			GPIOs.DO.AnalogPCurrentEnable = 0;
			GPIOs.DO.AnalogMCurrentEnable = 0;
			GPIOs.DO.AnalogAmpEnable = 0;
			GPIOs.DO.AnalogModeEnable = 0;
			GPIOs.DO.TempModeEnable = 0;
			GPIOs.DO.VwModeEnable = 0;
			GPIOs.DO.VwEnable = 0;

			switch(pConfig->TypeOfSensor)
			{
				case IoTSensorType_mA:
					if(((Channel - 1) % 2) == 0){ //odd ch=Plus, even ch=Minus
						GPIOs.DO.AnalogPCurrentEnable = 1;
					}
					else{
						GPIOs.DO.AnalogMCurrentEnable = 1;
					}

					GPIOs.DO.AnalogAmpEnable = 1;
					GPIOs.DO.AnalogModeEnable = 1;
					break;
				case IoTSensorType_Differential:
				case IoTSensorType_mV:
					GPIOs.DO.AnalogAmpEnable = 1;
					GPIOs.DO.AnalogModeEnable = 1;
					break;
				case IoTSensorType_Thermistor:
					GPIOs.DO.TempModeEnable = 1;
					GPIOs.DO.VwEnable = 1;
					break;
				case IoTSensorType_VibratingWire:
					GPIOs.DO.VwModeEnable = 1;
					GPIOs.DO.VwEnable = 1;
					break;
				default:
					result = RESULT_ERROR;
					break;
			}

			if(result == RESULT_RUN){
				ChannelSelectStep++;
			}
			break;
		case 3://open channel
			memset(&GPIOs.DO.PortArray, 0, sizeof(GPIOs.DO.PortArray));

			uint32_t CurrentPort = (Channel - 1)/4; //Channel is 1-based, 4ch per port
			GPIOs.DO.PortArray[CurrentPort] = 1;

			uint32_t CurrentChannel = (Channel-1)%4; //0=Ch1, 1=Ch2
			GPIOs.DO.Ch1Enable = CurrentChannel == 0 || CurrentChannel == 1;
			GPIOs.DO.Ch2Enable = CurrentChannel == 2 || CurrentChannel == 3;

			if((SelectedPort != CurrentPort) || (SelectedChannel != CurrentChannel)){
				SelectedPort = CurrentPort;
				SelectedChannel = CurrentChannel;

				ChannelSelectStep += 1;
				ChannelSelectTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			}
			else{
				ChannelSelectStep += 2;
			}
			break;
		case 4://wait relay
			if(oTMR_Elapsed(&ChannelSelectTimer, 500, TICKBASE_SYSTICK)){
				ChannelSelectStep++;
			}
			break;
		case 5:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		ChannelSelectStep = 0;
	}

	return result;
}

void Measurement_ValidateConfig()
{
	uint8_t i;

	//1. sensor type validation: only mV, mA, Differential, Thermistor, VW allowed
	for(i = 0; i < MEASUREMENT_CHANNEL_MAXCOUNT; i++){
		switch(MiIoT_Parameter.ChannelConfig[i].TypeOfSensor){
			case IoTSensorType_NULL:
			case IoTSensorType_mV:
			case IoTSensorType_mA:
			case IoTSensorType_Differential:
			case IoTSensorType_Thermistor:
			case IoTSensorType_VibratingWire:
				break;
			default:
				MiIoT_Parameter.ChannelConfig[i].TypeOfSensor = IoTSensorType_NULL;
				break;
		}
	}

	//2. pair validation: Differential/Thermistor/VW → even channel is pair master, odd = NULL
	for(i = 0; i < MEASUREMENT_CHANNEL_MAXCOUNT; i++){
		switch(MiIoT_Parameter.ChannelConfig[i].TypeOfSensor)
		{
			case IoTSensorType_Differential:
			case IoTSensorType_Thermistor:
			case IoTSensorType_VibratingWire:
				if(i%2 == 0){
					MiIoT_Parameter.ChannelConfig[i+1].TypeOfSensor = IoTSensorType_NULL;
				}
				else{
					if(MiIoT_Parameter.ChannelConfig[i-1].TypeOfSensor == IoTSensorType_NULL){
						MiIoT_Parameter.ChannelConfig[i-1].TypeOfSensor = MiIoT_Parameter.ChannelConfig[i].TypeOfSensor;
					}
					
					MiIoT_Parameter.ChannelConfig[i].TypeOfSensor = IoTSensorType_NULL;
				}
				break;			
			default:
				break;
		}
		
		//3. VW frequency range validation
		if(MiIoT_Parameter.ChannelConfig[i].TypeOfSensor == IoTSensorType_VibratingWire){
			IoTChannelPropertiesVW_t *pVW = &MiIoT_Parameter.ChannelConfig[i].Properties.VibrationWrie;
			pVW->EndFreq = MATH_LIMIT(pVW->EndFreq, 500, 6000);
			pVW->StartFreq = MATH_LIMIT(pVW->StartFreq, 400, pVW->EndFreq - 100);
		}
	}
}

//Resolution 0.01 hz
oResult_t Measurement_Sensor(IoT_DataPacket_t *pPacket)
{
	static uint32_t MeasureTimer = 0;
	static uint8_t MeasureStep = 0;
	static uint8_t IndexOfChannel = 0;
	static uint8_t CountOfChannel = 0;
	static uint8_t TryCount = 0;
	static IoTChannelConfig_t *pConf = NULL;
	oResult_t result = RESULT_RUN;
	IoTDataArray_t *pDataFrame = (IoTDataArray_t*)&pPacket->Frame;
	float SamplingData = 0;

	switch(MeasureStep)
	{
		default:
			MeasureStep = 0;
		case 0:
			//adc/dma start (DMA가 멈춰있을 때만)
			if(hadc1.DMA_Handle->State == HAL_DMA_STATE_READY){
				if(HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&DMABuffer, ArrayLen(DMABuffer)) != HAL_OK){
					return RESULT_ERROR;
				}
			}

			memset(pPacket, 0, sizeof(*pPacket));
			Measurement_ValidateConfig();
			CountOfChannel = 0;
			IndexOfChannel = 0;
		case 1:
			if(Measurement_ChannelSelect(0) == RESULT_OK){
				GPIOs.DO.Vo5vEnable = 1;
				GPIOs.DO.Vo12vEnable = 1;
				MeasureTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				MeasureStep++;
			}
			break;
		case 2:
			if(oTMR_Elapsed(&MeasureTimer, 1500, TICKBASE_SYSTICK)){
				MeasureStep++;
			}
			break;
		case 3:
			if(IndexOfChannel >= MEASUREMENT_CHANNEL_MAXCOUNT){
				result = RESULT_OK;
			}
			else{
				//채널선택
				switch(Measurement_ChannelSelect(IndexOfChannel+1))
				{
					case RESULT_RUN:
						break;
					case RESULT_OK:
						pConf = &MiIoT_Parameter.ChannelConfig[IndexOfChannel];
						MeasureStep++;
						break;
					default:
						IndexOfChannel++;
						break;
				}
			}
			break;
		case 4:
			switch(pConf->TypeOfSensor)
			{
				case IoTSensorType_VibratingWire:
					if(Measurement_SamplingFrequency(&pConf->Properties.VibrationWrie, &SamplingData, 1) != RESULT_RUN){
						if(SamplingData != 0 || ++TryCount >= 3){
							pDataFrame->Items[CountOfChannel].Channel = IndexOfChannel+1;
							pDataFrame->Items[CountOfChannel].Type = pConf->TypeOfSensor;
							pDataFrame->Items[CountOfChannel].Data = MIIOT_DATA_ENCODE_FREQUENCY(SamplingData);

							TryCount = 0;
							CountOfChannel++;
							IndexOfChannel++;
						}

						MeasureStep = 3;
					}
					break;
				case IoTSensorType_Thermistor:
					if(Measurement_SamplingResistance(&SamplingData) != RESULT_RUN){
						pDataFrame->Items[CountOfChannel].Channel = IndexOfChannel+1;
						pDataFrame->Items[CountOfChannel].Type = pConf->TypeOfSensor;
						pDataFrame->Items[CountOfChannel].Data = MIIOT_DATA_ENCODE_TEMP(NTC_3K_TO_CELSIUS(SamplingData));

						TryCount = 0;
						CountOfChannel++;
						IndexOfChannel++;
						MeasureStep = 3;
					}
					break;
				case IoTSensorType_Differential:
					if(Measurement_SamplingDifferantial(&SamplingData) != RESULT_RUN){
						pDataFrame->Items[CountOfChannel].Channel = IndexOfChannel+1;
						pDataFrame->Items[CountOfChannel].Type = pConf->TypeOfSensor;
						pDataFrame->Items[CountOfChannel].Data = MIIOT_DATA_ENCODE_mV(SamplingData);

						TryCount = 0;
						CountOfChannel++;
						IndexOfChannel++;
						MeasureStep = 3;
					}
					break;
				case IoTSensorType_mV:
				case IoTSensorType_mA:
					uint8_t adc_ch = (IndexOfChannel%2)+1; //1 or 2

					if(Measurement_SamplingAnalog(adc_ch, pConf->TypeOfSensor, &SamplingData) != RESULT_RUN){
						pDataFrame->Items[CountOfChannel].Channel = IndexOfChannel+1;
						pDataFrame->Items[CountOfChannel].Type = pConf->TypeOfSensor;

						if(pConf->TypeOfSensor == IoTSensorType_mV){
							pDataFrame->Items[CountOfChannel].Data = MIIOT_DATA_ENCODE_mV(SamplingData);
						}
						else{
							pDataFrame->Items[CountOfChannel].Data = MIIOT_DATA_ENCODE_mA(SamplingData);
						}

						TryCount = 0;
						CountOfChannel++;
						IndexOfChannel++;
						MeasureStep = 3;
					}
					break;
				default:
					TryCount = 0;
					IndexOfChannel++;
					MeasureStep = 3;
					break;
			}
			break;
	}

	if(result != RESULT_RUN){
		if(result == RESULT_OK){
			pPacket->TypeOfData = IoTDataType_DataArray;
			pPacket->DLC = MIIOT_IOTDATA_SIZE_DATAARRAY(CountOfChannel);
		}

		if(pTIMCapture_Timer != NULL){
			HAL_TIM_IC_Stop_DMA(pTIMCapture_Timer, TIMCapture_Channel);
		}

		GPIOs.DO.Vo5vEnable = 0;
		GPIOs.DO.Vo12vEnable = 0;
		GPIOs.DO.VwDe = 0;

		Measurement_ChannelSelect(-1);
		IndexOfChannel = 0;
		MeasureStep = 0;
	}

	return result;
}

oResult_t Measurement_Supply(uint32_t Count)
{
	static uint8_t ReadSupplyStep = 0;
	static uint32_t ReadSupplyTimer = 0;
	static uint8_t ReadCount = 0;
	static double SumAnalog = 0;
	double Analog;
	oResult_t result = RESULT_RUN;

	switch(ReadSupplyStep)
	{
		case 0:
			//adc/dma start (DMA가 멈춰있을 때만)
			if(hadc1.DMA_Handle->State == HAL_DMA_STATE_READY){
				if(HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&DMABuffer, ArrayLen(DMABuffer)) != HAL_OK){
					return RESULT_ERROR;
				}
			}

			memset(&GPIOs.DO.PortArray, 0, sizeof(GPIOs.DO.PortArray));
			GPIOs.DO.BatCheck = 0;
			GPIOs.DO.AnalogModeEnable = 0;
			GPIOs.DO.AnalogPCurrentEnable = 0;
			GPIOs.DO.AnalogMCurrentEnable = 0;

			ReadSupplyTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			ReadSupplyStep++;
			break;
		case 1:
			GPIOs.DO.Dc5vEnable = 1;
			GPIOs.DO.AnalogAmpEnable = 1;

			if(oTMR_Elapsed(&ReadSupplyTimer, 100, TICKBASE_SYSTICK)){
				ReadSupplyTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				ReadSupplyStep++;
			}
			break;
		case 2:
			GPIOs.DO.BatCheck = 1;

			if(oTMR_Elapsed(&ReadSupplyTimer, 100, TICKBASE_SYSTICK)){
				SumAnalog = 0;
				ReadCount = 0;

				ReadSupplyStep++;
			}
			break;
		case 3://Read external supply (12V)
			if(oTMR_Trigger(&ReadSupplyTimer, 10, 1, TICKBASE_SYSTICK)){
				SumAnalog += (double)ADC_TO_EXTSUPPLY(DMABuffer[2]);

				if(++ReadCount > Count){
					Analog = SumAnalog / (double)ReadCount;
					GPIOs.ADC.ExternalSupply = (uint16_t)Analog;

					SumAnalog = 0;
					ReadCount = 0;
					ReadSupplyStep++;
				}
			}
			break;
		case 4://Read system supply (3.3V)
			if(oTMR_Trigger(&ReadSupplyTimer, 10, 1, TICKBASE_SYSTICK)){
				SumAnalog += (double)ADC_TO_SUPPLY(DMABuffer[3]);

				if(++ReadCount > Count){
					Analog = SumAnalog / (double)ReadCount;
					GPIOs.ADC.SystemSupply = (uint16_t)oLinear(&MiIoT_Parameter.SystemConfig.Calibration.ADC[0], Analog, 0); //Offset, Gain 보정

					SumAnalog = 0;
					ReadCount = 0;
					ReadSupplyTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					ReadSupplyStep++;
				}
			}
			break;
		case 5:
			GPIOs.DO.BatCheck = 0;
			GPIOs.DO.AnalogAmpEnable = 0;
			if(oTMR_Elapsed(&ReadSupplyTimer, 100, TICKBASE_SYSTICK)){
				ReadSupplyTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				ReadSupplyStep++;
			}
			break;
		case 6://Read Vbat
			if(oTMR_Trigger(&ReadSupplyTimer, 10, 1, TICKBASE_SYSTICK)){
				SumAnalog += (double)ADC_TO_VBAT(DMABuffer[4]);

				if(++ReadCount > Count){
					Analog = SumAnalog / (double)ReadCount;
					GPIOs.ADC.InternalBAT = (uint16_t)Analog;
					ReadSupplyStep++;
				}
			}
			break;
		case 7:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		ReadSupplyStep = 0;

		GPIOs.DO.Dc5vEnable = 0;
		GPIOs.DO.BatCheck = 0;
		GPIOs.DO.AnalogAmpEnable = 0;
	}

	return result;
}
