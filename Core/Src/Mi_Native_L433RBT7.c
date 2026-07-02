/*
 * Mi_Native_L433RBT7.c
 *
 *  Version: 0.1 (2026-06-29)
 */

#include "main.h"
#include "ONE_Common.h"
#include "ONE_Memory.h"
#include "Mi_Main.h"
#include "Mi_Native.h"

#define NATIVE_FLASH_ADDRESS_START   ((uint32_t)0x08000000)
#define NATIVE_FLASH_ADDRESS_END	 ((uint32_t)0x0803FFFF)
#define NATIVE_FLASH_SIZE			 (NATIVE_FLASH_ADDRESS_END-NATIVE_FLASH_ADDRESS_START)
#define NATIVE_FLASH_PAGE_SIZE       FLASH_PAGE_SIZE  // 2KB

// RBT6 기준: SRAM1 32KB = 0x8000
#define NATIVE_SRAM_ADDRESS_FIRST     0x20000000
#define NATIVE_SRAM_ADDRESS_LAST      0x20007FFF
#define NATIVE_SRAM_SIZE              (NATIVE_SRAM_ADDRESS_LAST - NATIVE_SRAM_ADDRESS_FIRST + 1)

#if defined(IWDG_PRESCALER_4)
extern IWDG_HandleTypeDef hiwdg;
#endif

extern void SystemClock_Config();

oResult_t Native_FlashReadAddress(uint32_t Address, uint8_t *pData, uint32_t SizeOfData, uint8_t CrcCheck)
{
	uint32_t ReadOfSize = 0;
	uint8_t *ReadOfPointer = pData;
	uint32_t Size;

	if (pData == NULL || SizeOfData <= 0 || SizeOfData > NATIVE_FLASH_SIZE) {
		return RESULT_ERROR;
	}

	if(CrcCheck){
		if(Address < NATIVE_FLASH_ADDRESS_START + FLASH_PAGE_SIZE){
			return RESULT_ERROR;
		}
		Address -= 8;
	}

	// 주소를 페이지 경계로 정렬
	if((Address%FLASH_PAGE_SIZE) != 0){
		Address -= (Address%FLASH_PAGE_SIZE);
	}

	if (OutOfRange(Address+SizeOfData, NATIVE_FLASH_ADDRESS_START, NATIVE_FLASH_ADDRESS_END)) {
		return RESULT_ERROR;
	}


	while(ReadOfSize < SizeOfData && InOfRange(Address, NATIVE_FLASH_ADDRESS_START, NATIVE_FLASH_ADDRESS_END)){
		Size = MATH_MIN(SizeOfData - ReadOfSize, 8);  // 이 줄 수정됨: 읽을 크기 보정

		memcpy(ReadOfPointer, (void *)Address, Size);

		ReadOfSize += Size;
		ReadOfPointer += Size;

		Address += 8;
	}

	if(CrcCheck)
	{
	    // Step 2: CRC32 읽기 (데이터 다음 4바이트)

	    uint64_t StoredCRC = 0;

	    memcpy(&StoredCRC, (void *)Address, 8);

	    // Step 3: CRC 계산 및 비교
	    uint32_t CalcCRC = oMEM_CRC32(pData, SizeOfData, 0);
	    if (CalcCRC != (uint32_t)StoredCRC) {
	    	return RESULT_ERROR;
	    }
	}

	return RESULT_OK;
}

oResult_t Native_FlashRead(uint8_t *pData, uint32_t SizeOfData)
{
    if (pData == NULL || SizeOfData == 0 || SizeOfData > (NATIVE_FLASH_SIZE - 8)) {
        return RESULT_ERROR;
    }

    uint32_t Address = NATIVE_FLASH_ADDRESS_END - (SizeOfData - 1);

	if((Address%8) != 0){
		Address -= (Address%8);
	}

    // Step 1: 임시 버퍼로 데이터 읽기
    if (Native_FlashReadAddress(Address, pData, SizeOfData, 1) != RESULT_OK) {
    	memset(pData, 0, SizeOfData);
    	return RESULT_ERROR;
    }

    return RESULT_OK;
}


oResult_t Native_FlashWriteAddress(uint32_t Address, uint8_t *pData, uint32_t SizeOfData, uint8_t CrcCheck)
{
	// 해당 페이지를 지우기 위한 설정
	FLASH_EraseInitTypeDef EraseInit;
	uint32_t WriteOfSize = 0;
	uint8_t *WriteOfPointer = pData;
	uint32_t PageError;
    oResult_t result = RESULT_RUN;
    uint64_t Data;
    uint32_t Size;
    uint32_t PageNo;

    if (WriteOfPointer == NULL || SizeOfData <= 0 || SizeOfData > NATIVE_FLASH_SIZE || OutOfRange(Address, NATIVE_FLASH_ADDRESS_START, NATIVE_FLASH_ADDRESS_END)) {
        return RESULT_ERROR;
    }

    // Flash Unlock
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return RESULT_ERROR;
    }

	if(CrcCheck){
		if(Address < NATIVE_FLASH_ADDRESS_START + FLASH_PAGE_SIZE){
			HAL_FLASH_Lock();
			return RESULT_ERROR;
		}
		Address -= 8;
	}

    // 주소를 페이지 경계로 정렬
	if((Address%FLASH_PAGE_SIZE) != 0){
		Address -= (Address%FLASH_PAGE_SIZE);
	}

	while(WriteOfSize < SizeOfData){
		// 페이지 번호 계산
		PageNo = ((Address+WriteOfSize) - NATIVE_FLASH_ADDRESS_START) / FLASH_PAGE_SIZE;

		EraseInit.Banks = FLASH_BANK_1;
		EraseInit.Page = PageNo;
		EraseInit.NbPages = 1;  // 하나의 페이지만 지우기
		EraseInit.TypeErase = FLASH_TYPEERASE_PAGES;

		// 페이지 지우기
		if (HAL_FLASHEx_Erase(&EraseInit, &PageError) != HAL_OK) {
			result = RESULT_ERROR;
			goto EXIT;
		}

		WriteOfSize += FLASH_PAGE_SIZE;
	}

	WriteOfSize = 0;

    // 8바이트 단위로 플래시 쓰기
    while (WriteOfSize < SizeOfData && result == RESULT_RUN) {

    	Size = MATH_MIN(SizeOfData-WriteOfSize, 8);
        Data = 0;
        memcpy(&Data, WriteOfPointer, Size);  // 8바이트 데이터 복사

        // 플래시에 기록
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, Address, Data) == HAL_OK) {
            Address += 8;
            WriteOfPointer += Size;
            WriteOfSize += Size;
        } else {
            result = RESULT_ERROR;
            goto EXIT;
        }
    }

	if(CrcCheck)
	{
	    // Step 3: CRC 계산 및 비교
	    uint64_t CalcCRC = (uint64_t)oMEM_CRC32(pData, SizeOfData, 0);

	    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, Address, CalcCRC) != HAL_OK) {
            result = RESULT_ERROR;
            goto EXIT;
		}
	}

    result = RESULT_OK;  // 성공적으로 데이터 기록 완료

EXIT:
    HAL_FLASH_Lock();  // 플래시 잠금

    return result;
}



oResult_t Native_FlashWrite(uint8_t *pData, uint32_t SizeOfData)
{
    if (pData == NULL || SizeOfData == 0 || SizeOfData > (NATIVE_FLASH_SIZE - 4)) {
        return RESULT_ERROR;
    }

    // 마지막 위치에서 TotalSize만큼 거슬러 올라감
    uint32_t Address = NATIVE_FLASH_ADDRESS_END - (SizeOfData - 1);

    // 주소가 8바이트 정렬되어 있는지 확인
	if((Address%8) != 0){
		Address -= (Address%8);
	}

	if (Native_FlashWriteAddress(Address, pData, SizeOfData, 1) != RESULT_OK) {
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

oResult_t Native_FlashCompare(uint8_t *pData, uint32_t SizeOfData)
{
    if (pData == NULL || SizeOfData == 0 || SizeOfData > (NATIVE_FLASH_SIZE - 4)) {
        return RESULT_ERROR;
    }

    // 마지막 위치에서 TotalSize만큼 거슬러 올라감
    uint32_t Address = NATIVE_FLASH_ADDRESS_END - (SizeOfData - 1);

    Address -= 8;

	// 주소가 8바이트 정렬되어 있는지 확인
	if((Address%FLASH_PAGE_SIZE) != 0){
		Address -= (Address%FLASH_PAGE_SIZE);
	}

	// 주소가 8바이트 정렬되어 있는지 확인
	if (pData == NULL || SizeOfData <= 0 || SizeOfData > NATIVE_FLASH_SIZE || OutOfRange(Address+SizeOfData, NATIVE_FLASH_ADDRESS_START, NATIVE_FLASH_ADDRESS_END)) {
		return RESULT_ERROR;
	}

	return memcmp(pData, (void *)Address, SizeOfData) == 0 ? RESULT_OK : RESULT_ERROR;
}

oResult_t Native_SRAMRead(uint32_t Index, uint8_t *pData, uint32_t SizeOfData)
{
	uint32_t Data;
	uint32_t Address;
	uint8_t Size;

	Address = NATIVE_SRAM_ADDRESS_FIRST+Index;

	if((Address%4) != 0){
		Address -= (Address%4);
	}

	// 주소가 8바이트 정렬되어 있는지 확인
	if (pData == NULL || SizeOfData <= 0 || SizeOfData > NATIVE_SRAM_SIZE || OutOfRange(Address, NATIVE_SRAM_ADDRESS_FIRST, NATIVE_SRAM_ADDRESS_LAST)) {
		return RESULT_ERROR;
	}

	while(SizeOfData > 0){
		Size = MATH_MIN(SizeOfData, 4);

		Data = *(volatile uint32_t*)Address;  // SRAM에 32비트 단위로 읽기

		memcpy(pData, &Data, Size);

		Address += 4;
		pData += Size;
		SizeOfData -= Size;
	}

	return RESULT_OK;
}


oResult_t Native_SRAMWrite(uint32_t Index, uint8_t *pData, uint32_t SizeOfData)
{
	uint32_t Data;
	uint32_t Address;
	uint8_t Size;

	Address = NATIVE_SRAM_ADDRESS_FIRST+Index;

	if((Address%4) != 0){
		Address -= (Address%4);
	}

	// 주소가 8바이트 정렬되어 있는지 확인
	if (pData == NULL || SizeOfData <= 0 || SizeOfData > NATIVE_SRAM_SIZE || OutOfRange(Address, NATIVE_SRAM_ADDRESS_FIRST, NATIVE_SRAM_ADDRESS_LAST)) {
		return RESULT_ERROR;
	}

	while(SizeOfData > 0){
		Size = MATH_MIN(SizeOfData, 4);
		Data = 0;
		memcpy(&Data, pData, Size);

		*(volatile uint32_t*)Address = Data;  // SRAM에 32비트 단위로 쓰기

		Address += 4;
		pData += Size;
		SizeOfData -= Size;
	}

	return RESULT_OK;
}

oResult_t Native_ADCRead(uint32_t Channel, uint32_t SamplingTime, uint16_t *pData, uint32_t Count)
{
	oResult_t result = RESULT_RUN;
#ifdef ADC_GET_RESOLUTION
	static uint8_t ReadAdcStep = 0;
	static uint32_t ErrorCount;
	static uint32_t ReadCount;
	static int64_t SumAdc;
	static uint16_t MinAdc;
	static uint16_t MaxAdc;
	static uint8_t DummyRead;
	static uint8_t RecoveryAttempted;
	ADC_ChannelConfTypeDef sConfig;

	if(pData == NULL){
		ReadAdcStep = 0;
		return RESULT_ERROR;
	}

	switch(ReadAdcStep)
	{
		default:
			ReadAdcStep = 0;
			break;
		case 0:
			HAL_ADC_Stop(&hadc1);
	    	ReadAdcStep++;
			ErrorCount = 0;
			RecoveryAttempted = 0;
			break;
		case 1:
			HAL_ADC_Stop(&hadc1);

			sConfig.Channel = Channel;
			sConfig.Rank = 1;
			sConfig.SamplingTime = SamplingTime;
			sConfig.Rank = ADC_REGULAR_RANK_1;
			sConfig.SingleDiff = ADC_SINGLE_ENDED;
			sConfig.OffsetNumber = ADC_OFFSET_NONE;
			sConfig.Offset = 0;
			if(HAL_ADC_ConfigChannel(&hadc1, &sConfig) == HAL_OK){
				ReadCount = 0;
				SumAdc = 0;
				MinAdc = 0xFFFF;
				MaxAdc = 0;
				DummyRead = 1;
				ReadAdcStep++;
			}
			else{
				oSerial_Log("NativeADCRead", "ConfigChannel FAIL CH=0x%X State=0x%X", (unsigned int)Channel, (unsigned int)hadc1.State);
				result = RESULT_ERROR;
			}
			break;
		case 2:
		    if(HAL_ADC_Start(&hadc1) == HAL_OK){
		    	ReadAdcStep++;
		    }
		    else{
		    	ErrorCount++;
		    	if(ErrorCount == 1){
		    		oSerial_Log("NativeADCRead", "Start FAIL CH=0x%X State=0x%X ErrCode=0x%X", (unsigned int)Channel, (unsigned int)hadc1.State, (unsigned int)hadc1.ErrorCode);
		    	}
		    	ReadAdcStep = 1;
		    }
		    break;
		case 3:
		    if(HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK){
		    	ReadAdcStep++;
		    }
		    else{
		    	ErrorCount++;
		    	if(ErrorCount == 1){
		    		oSerial_Log("NativeADCRead", "PollTimeout CH=0x%X State=0x%X", (unsigned int)Channel, (unsigned int)hadc1.State);
		    	}
		    	ReadAdcStep = 1;
		    }
			break;
		case 4:
			uint16_t adcVal = (uint16_t)HAL_ADC_GetValue(&hadc1);

			ErrorCount = 0;
			HAL_ADC_Stop(&hadc1);

			/* 채널 전환 후 첫 번째 변환값 버림 (잔류 전하 오염 제거) */
			if(DummyRead){
				DummyRead = 0;
				ReadAdcStep = 2;
				break;
			}

			SumAdc += (int64_t)adcVal;
			if(adcVal < MinAdc) MinAdc = adcVal;
			if(adcVal > MaxAdc) MaxAdc = adcVal;

			if(++ReadCount >= Count){
				/* Trimmed Mean - 최소/최대 각 1개 제거 후 평균 (이상치 방지) */
				if(Count > 2){
					*pData = (uint16_t)((SumAdc - (int64_t)MinAdc - (int64_t)MaxAdc) / (int64_t)(ReadCount - 2));
				}
				else{
					*pData = (uint16_t)(SumAdc / (int64_t)ReadCount);
				}
				result = RESULT_OK;
			}
			else{
				ReadAdcStep = 2;
			}
			break;
	}

	if(ErrorCount >= 10){
		oSerial_Log("NativeADCRead", "ERROR CH=0x%X ErrorCount=%d Recovery=%d", (unsigned int)Channel, (int)ErrorCount, (int)RecoveryAttempted);

		/* ADC 복구 시도: DeInit → Init */
		if(!RecoveryAttempted){
			RecoveryAttempted = 1;
			oSerial_Log("NativeADCRead", "Attempting ADC recovery...");
			HAL_ADC_Stop(&hadc1);
			HAL_ADC_DeInit(&hadc1);
			HAL_ADC_Init(&hadc1);
			ErrorCount = 0;
			ReadAdcStep = 1; /* 채널 설정부터 재시도 */
		}
		else{
			result = RESULT_ERROR;
		}
	}

	if(result != RESULT_RUN){
		HAL_ADC_Stop(&hadc1);
		ReadAdcStep = 0;
	}

#endif

	return result;
}

#ifdef INC_ONE_SERIAL_H_
void Native_HardFaultCallback(uint32_t *stacked_regs) {
    uint32_t r0  = stacked_regs[0];
    uint32_t r1  = stacked_regs[1];
    uint32_t r2  = stacked_regs[2];
    uint32_t r3  = stacked_regs[3];
    uint32_t r12 = stacked_regs[4];
    uint32_t lr  = stacked_regs[5];  // Link Register
    uint32_t pc  = stacked_regs[6];  // Program Counter
    uint32_t psr = stacked_regs[7];  // Program Status Register

    // 오류 지점 주소 출력
    oSerial_Log("HardFault", "Detected!");
    oSerial_Log("HardFault", "PC  = 0x%08lX", pc);
    oSerial_Log("HardFault", "LR  = 0x%08lX", lr);
    oSerial_Log("HardFault", "PSR = 0x%08lX", psr);
    oSerial_Log("HardFault", "R0  = 0x%08lX", r0);
    oSerial_Log("HardFault", "R1  = 0x%08lX", r1);
    oSerial_Log("HardFault", "R2  = 0x%08lX", r2);
    oSerial_Log("HardFault", "R3  = 0x%08lX", r3);
    oSerial_Log("HardFault", "R12 = 0x%08lX", r12);

    // 무한 루프 대기 (디버깅 중단용)
    while (1);
}
#endif

/*
  프리스케일러      LSI 주파수(Hz)     최대 Timeout (초)
 --------------|----------------|-----------------
  4            | 32,000         | 0.512
  8            | 32,000         | 1.024
  16           | 32,000         | 2.048
  32           | 32,000         | 4.096
  64           | 32,000         | 8.192
  128          | 32,000         | 16.384
  256          | 32,000         | 32.768
 */

void Native_WatchDog(uint32_t Millisecond)
{
#if defined(IWDG_PRESCALER_4)
	static uint8_t WatchDogInit = 0;

	//watchdog clock = 32khz
	float timeunit = 0;
	float prescaler = 1;
	float result = 1;

	if(!WatchDogInit){
		if(Millisecond > 0){
			Millisecond = MATH_MAX(1000, Millisecond);
			hiwdg.Instance = IWDG;
			hiwdg.Init.Prescaler = IWDG_PRESCALER_256;

			switch(hiwdg.Init.Prescaler)
			{
				case IWDG_PRESCALER_4:
					prescaler = 4;
					break;
				case IWDG_PRESCALER_8:
					prescaler = 8;
					break;
				case IWDG_PRESCALER_16:
					prescaler = 16;
					break;
				case IWDG_PRESCALER_32:
					prescaler = 32;
					break;
				case IWDG_PRESCALER_64:
					prescaler = 64;
					break;
				case IWDG_PRESCALER_128:
					prescaler = 128;
					break;
				default:
				case IWDG_PRESCALER_256:
					prescaler = 256;
					break;
			}

			timeunit = 1000.0/32000.0*1000.0;
			result = (float)Millisecond / (prescaler*timeunit) * 1000;

			hiwdg.Init.Window = MATH_LIMIT((uint32_t)result, 0, 0x0FFF);
			hiwdg.Init.Reload = MATH_LIMIT((uint32_t)result, 0, 0x0FFF);

			if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
			{
				Error_Handler();
			}

			WatchDogInit = 1;
		}
	}
	else{
		HAL_IWDG_Refresh(&hiwdg);
	}
#endif
}


void Native_DisableGPIOs(oIO_t *Filter, uint32_t CountOfFilter)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint16_t ExcludePinsA = GPIO_PIN_13 | GPIO_PIN_14;  // SWDIO, SWCLK
    uint16_t ExcludePinsB = 0;
    uint16_t ExcludePinsC = GPIO_PIN_14 | GPIO_PIN_15;  // LSE
    uint16_t ExcludePinsD = 0;
    uint16_t ExcludePinsH = GPIO_PIN_0 | GPIO_PIN_1;    // HSE

    // Filter에서 포트별 제외 핀 수집 (1회 순회)
    if(Filter != NULL && CountOfFilter > 0){
        for(uint32_t i = 0; i < CountOfFilter; i++){
            switch((uint32_t)Filter[i].Port)
			{
                case (uint32_t)GPIOA: ExcludePinsA |= Filter[i].Pin; break;
                case (uint32_t)GPIOB: ExcludePinsB |= Filter[i].Pin; break;
                case (uint32_t)GPIOC: ExcludePinsC |= Filter[i].Pin; break;
                case (uint32_t)GPIOD: ExcludePinsD |= Filter[i].Pin; break;
                case (uint32_t)GPIOH: ExcludePinsH |= Filter[i].Pin; break;
                default: break;
            }
        }
    }

    // 공통 설정
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    // GPIOA
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_All & ~ExcludePinsA;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // GPIOB
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_All & ~ExcludePinsB;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // GPIOC
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_All & ~ExcludePinsC;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

#if defined(GPIOD)
    // GPIOD
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_All & ~ExcludePinsD;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
#endif

    // GPIOH
    __HAL_RCC_GPIOH_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_All & ~ExcludePinsH;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);
}


/* Sleep 모드 진입 함수

 분주기 설정                        주파수(Hz)   최대 카운트    최대 시간(초)   비고
 -------------------------------|----------|-----------|------------|---------------------
 RTC_WAKEUPCLOCK_DIV2           | 16384    | 65535     | 4.0        | 16비트
 RTC_WAKEUPCLOCK_DIV4           | 8192     | 65535     | 8.0        | 16비트
 RTC_WAKEUPCLOCK_DIV8           | 4096     | 65535     | 16.0       | 16비트
 RTC_WAKEUPCLOCK_DIV16          | 2048     | 65535     | 32.0       | 16비트
 RTC_WAKEUPCLOCK_CK_SPRE_16BITS | 1        | 65535     | 65535.0    | 약 18.2시간
 RTC_WAKEUPCLOCK_CK_SPRE_17BITS | 1        | 131071    | 131071.0   | 약 36.4시간 (STM32L4)
 */
oResult_t Native_SleepMode(uint32_t Millisecond)
{
#ifdef BKP_REG_NUMBER
	uint32_t wakeup_counter = (Millisecond * 2048) / 1000;  // ms → 2048Hz 변환

	HAL_RTCEx_DeactivateWakeUpTimer(&hrtc); //Wake-up timer off

	//RTC Wake-up 설정
	if(Millisecond > 0 && Millisecond < 0xFFFFFFFF){
		if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, wakeup_counter, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK){
			return RESULT_ERROR;
		}
	}

    Native_WatchDog(0);

	//Start sleep
    HAL_SuspendTick();
    //HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI); // Sleep모드
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI); // Stop 1 모드

    //Tick count update
    oTMR_SetTick(TICKBASE_SYSTICK, oTMR_GetTick(TICKBASE_SYSTICK) + Millisecond);

    SystemClock_Config();

    //Wake-up
    HAL_ResumeTick();

    Native_WatchDog(0);

#endif

    return RESULT_OK;
}
#ifdef __HAL_TIM_ENABLE
uint32_t Native_GetTIMxCLK(TIM_HandleTypeDef *pTIM)
{
    uint32_t pclk;
    uint32_t prescaler = 1;

    if (pTIM->Instance == TIM2 || pTIM->Instance == TIM6 || pTIM->Instance == TIM7){
        pclk = HAL_RCC_GetPCLK1Freq();

        // APB1 프리스케일러 값 확인
        if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1){
            prescaler = 2;   // APB1 prescaler > 1 → Timer Clock x2
        }
    }
    else{ // APB2에 연결된 타이머 (TIM1, TIM8, TIM9, TIM10, TIM11 등)
        pclk = HAL_RCC_GetPCLK2Freq();

        // APB2 프리스케일러 값 확인
        if ((RCC->CFGR & RCC_CFGR_PPRE2) != RCC_CFGR_PPRE2_DIV1){
            prescaler = 2;
        }
    }

    return pclk * prescaler;
}
oResult_t Native_SetFreqeuncyCapture(TIM_HandleTypeDef *pTIM, uint32_t Channel, uint32_t Frequency)
{
	uint32_t TIMCLK = Native_GetTIMxCLK(pTIM);
	Frequency = MATH_LIMIT(Frequency, 1000000, TIMCLK);

    // 16-bit 타이머 한계 내로 PSC/ARR 분해
    uint32_t psc = MATH_LIMIT(TIMCLK/Frequency, 1, 65536) - 1;  // 약 1 MHz 목표

    pTIM->Init.Prescaler = psc;

	__HAL_TIM_SET_PRESCALER(pTIM, psc);
	__HAL_TIM_SET_AUTORELOAD(pTIM, 65535);

    HAL_TIM_IC_Start_IT(pTIM, Channel);

    __HAL_TIM_SET_COUNTER(pTIM, 0);

	return RESULT_OK;
}
oResult_t Native_SetPWMFreqeuncy(TIM_HandleTypeDef *pTIM, uint32_t Frequency)
{
	uint32_t TIMCLK = Native_GetTIMxCLK(pTIM);
	Frequency = MATH_MIN(Frequency, TIMCLK);

	uint32_t TIMFreq = 1000000;

	if(Frequency > 1000000){
		TIMFreq += (Frequency/1000000) * 1000000;
	}

    // 16-bit 타이머 한계 내로 PSC/ARR 분해
    uint32_t psc = MATH_LIMIT(TIMCLK/TIMFreq, 1, 65536) - 1;  // 약 1 MHz 목표
    uint32_t arr = MATH_MAX((TIMCLK / (psc + 1) + (Frequency/2)) / Frequency, 2);

    if (arr > 65536) {
    	return RESULT_ERROR;
    }

    pTIM->Init.Prescaler = psc;

	__HAL_TIM_SET_PRESCALER(pTIM, psc);
	__HAL_TIM_SET_AUTORELOAD(pTIM, arr);

	return RESULT_OK;
}
oResult_t Native_SetPWMDuty(TIM_HandleTypeDef *pTIM, uint32_t Channel, float Duty)
{
	uint32_t pulse = 0;
	uint32_t arr = __HAL_TIM_GET_AUTORELOAD(pTIM);
	Duty = MATH_LIMIT(Duty, 0, 100);

	if(Duty != 0){
		pulse = (uint32_t)((arr + 1) * (Duty/100)) - 1;
	}

	if(pulse > arr){
		pulse = arr;
	}

    __HAL_TIM_SET_COMPARE(pTIM, Channel, pulse);

    return RESULT_OK;
}
oResult_t Native_SetPWM(TIM_HandleTypeDef *pTIM, uint32_t Frequency, uint32_t Channel, float Duty)
{
	HAL_TIM_PWM_Stop(pTIM, Channel);

	if(Native_SetPWMFreqeuncy(pTIM, Frequency) != RESULT_OK){
		return RESULT_ERROR;
	}

	if(Native_SetPWMDuty(pTIM, Channel, Duty) != RESULT_OK){
		return RESULT_ERROR;
	}

	if(HAL_TIM_PWM_Start(pTIM, Channel) != HAL_OK){
		return RESULT_ERROR;
	}

	//__HAL_TIM_SET_COUNTER(pTIM, 0);

    return RESULT_OK;
}
#endif
