/*
 * Mi_Sensor.c
 *
 *  Created on: Dec 9, 2024
 *      Author: JONE
 */
#include "ONE_SlidingHistogram.h"
#include "Mi_Main.h"
#include "Mi_Measurement.h"
#include "Mi_IoT.h"
#include "ONE_Math.h"
#include "ONE_Math.h"
#include "Mi_Native.h"

#define ADC_VREF					(MiIoT_Parameter.SystemConfig.ActualVRef == 0 ? 2492 : MiIoT_Parameter.SystemConfig.ActualVRef)
#define ADC_MAXDIGIT				4095//(uint16_t)((uint32_t)(4095 * 64) >> 2)

#define ADC_TO_AI(x)				((double)(x)/(double)ADC_MAXDIGIT*(double)ADC_VREF)
#define ADC_TO_SUPPLY(x)			(ADC_TO_AI(x)/VOLT_DIV_RATIO(200000,200000))
#define ADC_TO_VBAT(x)				(ADC_TO_AI(x)*3)
#define SAMPLING_MAX_FREQ			6000
#define SWEEPFREQ_DIV				250

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

					*pPrev = 0;
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

            GPIOs.DO.MAX489_DE = 1;
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

        GPIOs.DO.MAX489_DE = 0;
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
	static double AI_PLUS, AI_MINUS;
	uint16_t AD;
	oResult_t result = RESULT_RUN;

	switch(SamplingTempStep)
	{
		case 0:
			if((result=Native_ADCRead(ADC_CHANNEL_1, ADC_SAMPLETIME_247CYCLES_5, &AD, 15)) == RESULT_OK){
				if(AD > 0){
					AI_PLUS = ADC_TO_AI(AD);
				}
				else{
					AI_PLUS = 0;
				}

				result = RESULT_RUN;
				SamplingTempStep++;
			}
			break;
		case 1:
			if((result=Native_ADCRead(ADC_CHANNEL_2, ADC_SAMPLETIME_247CYCLES_5, &AD, 15)) == RESULT_OK){
				if(AD > 0){
					AI_MINUS = ADC_TO_AI(AD) + 50; //U6앰프 1IN+에 50mV Offset 생김
				}
				else{
					AI_MINUS = 0;
				}

				result = RESULT_RUN;
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

void Measurement_ChannelSelect(int32_t Channel)
{
	GPIOs.DO.Dc12VEnable = Channel >= 0;
	GPIOs.DO.MUX_SEL1 = Channel == 2 || Channel == 4;
	GPIOs.DO.MUX_SEL2 = Channel == 6 || Channel == 8 || Channel == 10;
	GPIOs.DO.VWRelay1 = Channel == 1;
	GPIOs.DO.VWRelay2 = Channel == 3;
	GPIOs.DO.VWRelay3 = Channel == 5;
	GPIOs.DO.VWRelay4 = Channel == 7;
	GPIOs.DO.VWRelay5 = Channel == 9;

	switch(Channel)
	{
		case 2:
		case 6:
		case 10:
			GPIOs.DO.MUX_A = 1;
			break;
		default:
			GPIOs.DO.MUX_A = 0;
			break;
	}

	switch(Channel)
	{
		case 2:
		case 4:
		case 6:
		case 8:
			GPIOs.DO.MUX_B = 1;
			break;
		default:
			GPIOs.DO.MUX_B = 0;
			break;
	}
}

//Resolution 0.01 hz
oResult_t Measurement_Sensor(IoT_DataPacket_t *pPacket)
{
	static uint8_t MeasureStep = 0;
	static uint32_t MeasureTimer = 0;
	static uint8_t ChannelIndex = 0;
	static uint8_t LastChannel = 0;
	static uint8_t MeasureCount = 0;
	static uint8_t TryCount = 0;
	static IoTChannelConfig_t *pConf = NULL;
	oResult_t result = RESULT_RUN;
	IoTDataAnalog_t *pDataFrame = (IoTDataAnalog_t*)&pPacket->Frame;
	float SamplingData = 0;

	switch(MeasureStep)
	{
		default:
			MeasureStep = 0;
		case 0:
			memset(pPacket, 0, sizeof(*pPacket));
			LastChannel = 0;
			ChannelIndex = 0;
			MeasureCount = 0;
		case 1:
			Measurement_ChannelSelect(0); //DCDC On
			if(oTMR_Elapsed(&MeasureTimer, 300, TICKBASE_SYSTICK)){
				MeasureStep++;
			}
			break;
		case 2:
			Measurement_ChannelSelect(0); //Channel off all
			if(ChannelIndex >= MEASUREMENT_CHANNEL_MAXCOUNT){
				if(MeasureCount > 0){
					result = RESULT_OK;
				}
				else{
					result = RESULT_NULL;
				}
			}
			else{
				pConf = &MiIoT_Parameter.ChannelConfig[ChannelIndex];
				MeasureTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				TryCount++;
				MeasureStep++;
			}
			break;
		case 3:
			if(oTMR_Elapsed(&MeasureTimer, 50, TICKBASE_SYSTICK)){
				MeasureTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				MeasureStep++;
			}
			break;
		case 4:
			//채널선택
			Measurement_ChannelSelect(ChannelIndex+1);

			switch(pConf->TypeOfSensor)
			{
				case IoTSensorType_VibratingWire:
					if(oTMR_Elapsed(&MeasureTimer, 150, TICKBASE_SYSTICK)){
						LastChannel = ChannelIndex+1;
						MeasureStep = 5;
						MeasureCount++;
					}
					break;
				case IoTSensorType_Resistance:
				case IoTSensorType_Thermistor:
					LastChannel = ChannelIndex+1;
					MeasureStep = 6;
					MeasureCount++;
					break;
				default:
					ChannelIndex++;
					MeasureStep = 2;
					break;
			}
			break;
		case 5:
			if(Measurement_SamplingFrequency(&pConf->Properties.VibrationWrie, &SamplingData, 1) != RESULT_RUN){
				if(SamplingData != 0 || TryCount >= 3){
					pDataFrame->Channel[ChannelIndex].Type = IoTSensorType_VibratingWire;
					pDataFrame->Channel[ChannelIndex].Analog = MIIOT_DATA_ENCODE_FREQUENCY(SamplingData);

					TryCount = 0;
					ChannelIndex++;
				}

				MeasureTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				MeasureStep = 2;
			}
			break;
		case 6:
			if(Measurement_SamplingResistance(&SamplingData) != RESULT_RUN){
				pDataFrame->Channel[ChannelIndex].Type = pConf->TypeOfSensor;

				if(pConf->TypeOfSensor == IoTSensorType_Thermistor){
					pDataFrame->Channel[ChannelIndex].Analog = MIIOT_DATA_ENCODE_TEMP(NTC_3K_TO_CELSIUS(SamplingData));
				}
				else{
					pDataFrame->Channel[ChannelIndex].Analog = MIIOT_DATA_ENCODE_Ohm(SamplingData);
				}

				MeasureTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				ChannelIndex++;
				TryCount = 0;
				MeasureStep = 2;
			}
			break;
	}

	if(result != RESULT_RUN){
		if(result == RESULT_OK){
			pPacket->TypeOfData = IoTDataType_Analog;
			pPacket->DLC = MIIOT_IOTDATA_SIZE_ANALOG(LastChannel);
		}

		GPIOs.DO.MAX489_DE = 0;
		Measurement_ChannelSelect(-1);
		ChannelIndex = 0;
		MeasureStep = 0;
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
			ReadSupplyStep++;
			break;
		case 2:
			if(oTMR_Elapsed(&ReadSupplyTimer, 100, TICKBASE_SYSTICK)){
				ReadSupplyStep++;
			}
			break;
		case 3:
			if(Native_ADCRead(ADC_CHANNEL_3, ADC_SAMPLETIME_640CYCLES_5, &ADC, Count) == RESULT_OK){
				Analog = (double)ADC_TO_SUPPLY(ADC);
				GPIOs.ADC.SystemSupply = (uint16_t)oLinear(&MiIoT_Parameter.SystemConfig.Calibration.ADC[0], Analog, 0); //Offset, Gain 보정
				ReadSupplyStep++;
			}
			break;
		case 4:
			if(Native_ADCRead(ADC_CHANNEL_VBAT, ADC_SAMPLETIME_640CYCLES_5, &ADC, Count) == RESULT_OK){
				GPIOs.ADC.InternalBAT = (uint16_t)ADC_TO_VBAT(ADC);
				ReadSupplyStep++;
			}
			break;
		case 5:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		ReadSupplyStep = 1;
		GPIOs.DO.ADCRefEnable = 0;
		GPIOs.DO.Dc12VEnable = 0;

		HAL_ADC_Stop(&hadc1);
	}
	else{
		GPIOs.DO.ADCRefEnable = 1;
		GPIOs.DO.Dc12VEnable = 1;
	}

	return result;
}
