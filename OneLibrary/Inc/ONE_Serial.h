#ifndef INC_ONE_SERIAL_H_
#define INC_ONE_SERIAL_H_

#include "ONE_Common.h"

typedef struct{
	uint32_t (*Write)(uint8_t *pData, uint32_t SizeOfData);
	uint32_t (*Read)(uint8_t *pData, uint32_t SizeOfData);
	void (*Reset)(void);
}oSerialStack_t;

typedef struct{
#if defined(__HAL_UART_ENABLE)
	UART_HandleTypeDef *pUART;
#else
	void *pUART;
#endif

	uint8_t *pTxBuffer;
	uint32_t SizeOfTxBuffer;

	uint8_t *pRxBuffer;
	uint32_t SizeOfRxBuffer;

	uint32_t IndexOfRxFirst;
	uint32_t IndexOfRxLast;
	uint32_t IndexOfRxFinder;
	uint32_t CountOfRxBuffer;

	uint32_t FindedCount;

	uint32_t ReceiveTimer;
	uint32_t IdleTimer;

	uint8_t IsRegistered;
} oSerialHandler_t;


extern oResult_t oSerial_Write(oSerialHandler_t *pSerial, char *pBuffer, uint32_t SizeOfBuffer);
extern oResult_t oSerial_Read(oSerialHandler_t *pSerial, char *pBuffer, uint32_t SizeOfBuffer, uint32_t WaitDelay);
extern oResult_t oSerial_ReadSplit(oSerialHandler_t *pSerial, char *pBuffer, uint32_t SizeOfBuffer, char* pSplitString);
extern oResult_t oSerial_ReadLine(oSerialHandler_t *pSerial, char *pBuffer, uint32_t SizeOfBuffer);

extern void oSerial_PutChar(oSerialHandler_t *pSerial, char* pChar);
extern void oSerial_vPrint(oSerialHandler_t *pSerial, const char* format, va_list args);
extern void oSerial_Printf(oSerialHandler_t *pSerial, const char* format, ...);

extern void oSerial_LogEnagle(oSerialHandler_t *pSerial, uint8_t Enable);
extern void oSerial_Log(char* Title, const char* format, ...);

#endif /* INC_ONE_TIMER_H_ */
