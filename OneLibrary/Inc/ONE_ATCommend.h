#ifndef INC_ONE_ATCOMMEND_H_
#define INC_ONE_ATCOMMEND_H_

#define ONE_ATCOMMEND_VERSION		0.1

#include "main.h"
#include "ONE_Time.h"

#if defined(__HAL_UART_ENABLE)
/*************************************************************************************
 Define
 *************************************************************************************/
#define AT_STRING_CRLF						"\r\n"
#define AT_STRING_CR						"\r"
#define AT_STRING_LF						"\n"

#define AT_DMA_INDEX(pUART)					(((uint32_t)((pUART)->RxXferSize) - (pUART)->hdmarx->Instance->CNDTR))
#define AT_TRANSMIT_BUFFER_COUNT(pAT)		((pAT)->IndexOfTxLast<(pAT)->IndexOfTxFirst ? ((pAT)->LengthOfBufferTx-(pAT)->IndexOfTxFirst+(pAT)->IndexOfTxLast) : ((pAT)->IndexOfTxLast-(pAT)->IndexOfTxFirst))
#define AT_RECEIVE_BUFFER_COUNT(pAT)		((pAT)->IndexOfRxLast<(pAT)->IndexOfRxFirst ? ((pAT)->LengthOfBufferRx-(pAT)->IndexOfRxFirst+(pAT)->IndexOfRxLast) : ((pAT)->IndexOfRxLast-(pAT)->IndexOfRxFirst))
#define AT_RECEIVE_BUFFER_ITEM(pAT, Index)	(char *)((pAT)->pBufferRx + (((pAT)->IndexOfRxFirst + (uint32_t)Index) % (pAT)->LengthOfBufferRx))
#define AT_RECEIVE_BUFFER_REMOVE(pAT,Cnt)	((pAT)->IndexOfRxFirst = ((pAT)->IndexOfRxFirst+Cnt)%(pAT)->LengthOfBufferRx)

/*************************************************************************************
 Typedef
 *************************************************************************************/
typedef enum {
	AT_RESULT_BUSY		= -4,
	AT_RESULT_ERROR		= -3,
	AT_RESULT_FAULT		= -2,
	AT_RESULT_TIMEOUT	= -1,
	AT_RESULT_NULL		= 0,
	AT_RESULT_INIT		= 1,
	AT_RESULT_RUN		= 2,
	AT_RESULT_OK		= 3,
	AT_RESULT_DONE		= 4,
} oATResult_t;

typedef void (*oATTransmitCallback_t)(char *Message);
typedef void (*oATReceiveCallback_t)(char *Message, uint32_t SizeOfMessage);
typedef void (*oATResponseCallback_t)(char *Commend, char *Response, char *Filter);

#pragma pack(1)
typedef struct ATCommendTypeDef{
	UART_HandleTypeDef *pUART;

	char				*pBufferRx;
	uint32_t			LengthOfBufferRx;
	uint32_t			IndexOfRxFirst;
	uint32_t			IndexOfRxLast;

	char				*pBufferTx;
	uint32_t			IndexOfTxFirst;
	uint32_t			IndexOfTxLast;
	uint32_t			LengthOfBufferTx;

	char				*pResponseData;
	uint32_t			LengthOfResponse;

	char				ResponseFilter[50];

	uint32_t 			TxTimestamp;
	uint32_t 			RxTimestamp;
	uint8_t 			CountOfTry;
	uint8_t 			CountOfTimeout;
	uint8_t 			ProcessStep;

	struct{
		uint8_t				IsRxBusy;
		uint8_t				IsWaitResponse;
		uint8_t				IsReceivedResponse;
		uint8_t				IsReceiveTimeout;
		uint8_t				IsReceiveLine;
		uint8_t				IsOpen;
	}State;

	oATTransmitCallback_t	TransmitCallback;
	oATReceiveCallback_t	ReceiveCallback;
	oATResponseCallback_t	ResponseCallback;
} oATCommend_t;
#pragma pack()

extern oATResult_t oAT_TransmitSerialize(oATCommend_t *pAT, uint8_t *pData, uint32_t SizeOfData);
extern oATResult_t oAT_TransmitBuffer(oATCommend_t *pAT);
extern oATResult_t oAT_WaitForResponse(oATCommend_t *pAT, char* Filter, uint32_t TimeoutDelay);
extern uint32_t oAT_ReceiveCopyTo(oATCommend_t *pAT, uint8_t *pDest, uint32_t DestSize);
extern oATResult_t oAT_TransmitData(oATCommend_t *pAT, uint8_t *Data, uint16_t Size);
extern oATResult_t oAT_TransmitCommend(oATCommend_t *pAT, char *Commend, char *ResponseFilter, uint32_t TimeoutDelay, uint8_t TryCount, uint8_t CRLF);
extern oATResult_t oAT_TransmitCommendWithData(oATCommend_t *pAT, char *Commend, char *ResponseFilter, uint32_t TimeoutDelay, uint8_t *pData, uint32_t SizeOfData, uint8_t CRLF);
extern oATResult_t oAT_Close(oATCommend_t *pAT);
extern oATResult_t oAT_Open(oATCommend_t *pAT);
extern void oAT_Proc(oATCommend_t *pAT);

#endif
#endif

/* History

2026-06-26 | v0.1
	- baseline (ONE_ATCommend.h)
*/
