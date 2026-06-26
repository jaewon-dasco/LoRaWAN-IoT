#ifndef INC_NATIVE_MEMORY_H_
#define INC_NATIVE_MEMORY_H_

#define MI_NATIVE_VERSION		0.1

#include "ONE_Common.h"
#include "Mi_Main.h"

#ifdef INC_ONE_SERIAL_H_
#include "Mi_Serial.h"
#endif

extern oResult_t Native_FlashReadAddress(uint32_t Address, uint8_t *pData, uint32_t SizeOfData, uint8_t CrcCheck);
extern oResult_t Native_FlashRead(uint8_t *pData, uint32_t SizeOfData);
extern oResult_t Native_FlashWriteAddress(uint32_t Address, uint8_t *pData, uint32_t SizeOfData, uint8_t CrcCheck);
extern oResult_t Native_FlashWrite(uint8_t *pData, uint32_t SizeOfData);
extern oResult_t Native_FlashCompare(uint8_t *pData, uint32_t SizeOfData);

extern oResult_t Native_SRAMRead(uint32_t Index, uint8_t *pData, uint32_t SizeOfData);
extern oResult_t Native_SRAMWrite(uint32_t Index, uint8_t *pData, uint32_t SizeOfData);

extern oResult_t Native_ADCRead(uint32_t Channel, uint32_t SamplingTime, uint16_t *pData, uint32_t Count);

extern void Native_DisableGPIOs(oIO_t *Filter, uint32_t CountOfFilter);
#define NATIVE_WATCHDOG_MAX_MS  32760
extern void Native_WatchDog(uint32_t Millisecond);
extern oResult_t Native_SleepMode(uint32_t Millisecond);

#ifdef __HAL_TIM_ENABLE
extern uint32_t Native_GetTIMxCLK(TIM_HandleTypeDef *pTIM);
extern oResult_t Native_SetFreqeuncyCapture(TIM_HandleTypeDef *pTIM, uint32_t Channel, uint32_t Frequency);
extern oResult_t Native_SetPWMFreqeuncy(TIM_HandleTypeDef *pTIM, uint32_t Frequency);
extern oResult_t Native_SetPWMDuty(TIM_HandleTypeDef *pTIM, uint32_t Channel, float Duty);
extern oResult_t Native_SetPWM(TIM_HandleTypeDef *pTIM, uint32_t Frequency, uint32_t Channel, float Duty);
#endif

#endif

/* History

2026-06-26 | v0.1
	- baseline (Mi_Native.h)
*/
