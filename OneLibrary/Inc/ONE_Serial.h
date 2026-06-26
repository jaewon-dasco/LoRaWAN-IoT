#ifndef INC_ONE_SERIAL_H_
#define INC_ONE_SERIAL_H_

#define ONE_SERIAL_VERSION		0.1

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
	uint32_t IndexOfTxFirst;	/* DMA/IT consumer 포인터 (송신 시작 위치) */
	uint32_t IndexOfTxLast;		/* producer 포인터 (다음 쓰기 위치) */
	uint32_t CountOfTxBuffer;	/* 링버퍼 점유 바이트 (write +, callback -) */
	uint32_t TxChunkSize;		/* 진행 중 DMA/IT chunk 크기 — 완료 시 First 전진용 */
	uint32_t TxCount;
	uint32_t TxErrorCount;
	uint8_t IsTxBusy;

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

/* 링버퍼 점유 바이트 = (Last - First) with wrap */
#define SERIAL_TRANSMIT_BUFFER_COUNT(pSerial)	((pSerial)->IndexOfTxLast < (pSerial)->IndexOfTxFirst ? \
													((pSerial)->SizeOfTxBuffer - (pSerial)->IndexOfTxFirst + (pSerial)->IndexOfTxLast) : \
													((pSerial)->IndexOfTxLast - (pSerial)->IndexOfTxFirst))

#define SERIAL_RECEIVE_BUFFER_COUNT(pSerial)	((pSerial)->IndexOfRxLast < (pSerial)->IndexOfRxFirst ? \
													((pSerial)->SizeOfRxBuffer - (pSerial)->IndexOfRxFirst + (pSerial)->IndexOfRxLast) : \
													((pSerial)->IndexOfRxLast - (pSerial)->IndexOfRxFirst))

#if defined(__HAL_UART_ENABLE)
extern void oSerial_RxCpltCallback(struct __UART_HandleTypeDef *huart);
extern void oSerial_TxCpltCallback(struct __UART_HandleTypeDef *huart);
extern void oSerial_ErrorCallback(struct __UART_HandleTypeDef *huart);
#endif

extern oResult_t oSerial_Write(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData);
extern oResult_t oSerial_Read(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData, uint32_t WaitDelay);
extern oResult_t oSerial_ReadSplit(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData, char* pSplitString);
extern oResult_t oSerial_ReadLine(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData);

extern void oSerial_PutChar(oSerialHandler_t *pSerial, char* pChar);
extern void oSerial_vPrint(oSerialHandler_t *pSerial, const char* format, va_list args);
extern void oSerial_Printf(oSerialHandler_t *pSerial, const char* format, ...);

extern void oSerial_LogEnagle(oSerialHandler_t *pSerial, uint8_t Enable);
extern void oSerial_Log(char* Title, const char* format, ...);

#endif /* INC_ONE_TIMER_H_ */

/* History

2026-06-26 | v0.1
	- baseline (ONE_Serial.h)
*/
