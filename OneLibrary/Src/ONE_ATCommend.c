#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ONE_Math.h"
#include "ONE_Common.h"
#include "ONE_Memory.h"
#include "ONE_Time.h"
#include <ONE_ATCommend.h>
#include <ONE_Serial.h>

#if defined(__HAL_UART_ENABLE)
#define AT_UART_TRANSMIT_TIMEOUT_TIME	10000

oATResult_t oAT_UartRecovery(oATCommend_t *pAT)
{
	static uint32_t DMAResetTimer;

	/* DMA RX 핸들 검증 - DMA 미설정 시 ERROR 리턴 */
	if(pAT->pUART->hdmarx == NULL){
		return AT_RESULT_ERROR;
	}

	/* DMA Circular 모드 확인 - 링버퍼 미설정 시 Circular로 재설정 */
	if(pAT->pUART->hdmarx->Init.Mode != DMA_CIRCULAR){
		pAT->pUART->hdmarx->Init.Mode = DMA_CIRCULAR;
		if(HAL_DMA_Init(pAT->pUART->hdmarx) != HAL_OK){
			return AT_RESULT_ERROR;
		}
	}

	if (pAT->pUART->ErrorCode & (HAL_UART_ERROR_ORE|HAL_UART_ERROR_FE|HAL_UART_ERROR_NE|HAL_UART_ERROR_PE|HAL_UART_ERROR_DMA)){
		HAL_UART_AbortReceive(pAT->pUART);
	}
	else if(pAT->pUART->RxState == HAL_UART_STATE_READY && pAT->State.IsOpen){
		if(oTMR_Trigger(&DMAResetTimer, 500, 1, TICKBASE_SYSTICK)){
			if(HAL_UART_Receive_DMA(pAT->pUART, (uint8_t *)pAT->pBufferRx, pAT->LengthOfBufferRx) == HAL_OK){
				pAT->IndexOfRxFirst = 0;
			}
		}
	}
	else{
		DMAResetTimer = oTMR_GetTick(TICKBASE_SYSTICK);
	}

	return AT_RESULT_OK;
}

uint32_t oAT_ReceiveCopyTo(oATCommend_t *pAT, uint8_t *pDest, uint32_t DestSize)
{
	uint32_t i = 0;
	uint32_t copysize = 0;

	if(DestSize <= 0 || pAT->State.IsRxBusy){
		return 0;
	}

	copysize = AT_RECEIVE_BUFFER_COUNT(pAT);

	if(copysize <= 0){
		return 0;
	}
	else if(DestSize < copysize){
		copysize = DestSize;
	}

	for(i=0; i<copysize; i++){
		*(pDest+i) = *(AT_RECEIVE_BUFFER_ITEM(pAT, i));
	}

	return copysize;
}

uint8_t oAT_ReceiveResponse(oATCommend_t *pAT, char *Response)
{
	uint32_t Length = 0;
	char *pStr;

	if(pAT == NULL || Response == NULL || strlen(Response) <= 0 || !pAT->State.IsWaitResponse){
		return 0;
	}

	if(strlen(pAT->ResponseFilter) <= 0){
		if(pAT->State.IsWaitResponse){
			pAT->State.IsReceivedResponse = 1;
			pAT->State.IsWaitResponse = 0;
		}

		return 0;
	}
	else if((pStr = strstr(Response, pAT->ResponseFilter)) == NULL){
		return 0;
	}

	pStr += strlen(pAT->ResponseFilter);

	memset(pAT->pResponseData, 0, pAT->LengthOfResponse);
	Length = strlen(pStr);

	if(Length > 0){
		strncpy(pAT->pResponseData, pStr, pAT->LengthOfResponse - 1);

		if(pAT->ResponseCallback){
			(*pAT->ResponseCallback)(pAT->pBufferTx, pAT->pResponseData, pAT->ResponseFilter);
		}
	}

	pAT->State.IsReceivedResponse = 1;
	pAT->State.IsWaitResponse = 0;

	return 1;
}

uint8_t oAT_ReadLine(oATCommend_t *pAT)
{
	uint8_t result = 0;
	uint32_t length = 0;
	char *pBuffer = NULL;
	char *pStr;

	if(pAT == NULL){
		return 0;
	}

	length = AT_RECEIVE_BUFFER_COUNT(pAT);

	if(length <= 0){
		return 0;
	}

	pBuffer = calloc(length+1, sizeof(char));

	if(pBuffer == NULL){
		return 0;
	}

	if(oAT_ReceiveCopyTo(pAT, (uint8_t *)pBuffer, length) == 0){
		goto EXIT;
	}

	//Invalid
	pStr = strtok(pBuffer, "\r\n");

	if(pStr != NULL && AT_RECEIVE_BUFFER_COUNT(pAT) > 0){
		uint32_t leadingLen = (uint32_t)(pStr - pBuffer);
		uint32_t tokenLen = strlen(pStr);
		uint32_t removeLen = leadingLen + tokenLen;

		// Count trailing \r \n delimiters from ring buffer
		while(removeLen < AT_RECEIVE_BUFFER_COUNT(pAT)){
			char c = *AT_RECEIVE_BUFFER_ITEM(pAT, removeLen);
			if(c == '\r' || c == '\n') removeLen++;
			else break;
		}

		oAT_ReceiveResponse(pAT, pStr);

		if(pAT->ReceiveCallback){
			(*pAT->ReceiveCallback)(pStr, strlen(pStr));
		}

		AT_RECEIVE_BUFFER_REMOVE(pAT, removeLen);
		result = 1;
	}

EXIT:
	free(pBuffer);

	return result;
}

uint8_t oAT_Read(oATCommend_t *pAT)
{
	uint8_t result = 0;
	uint32_t length;
	char *pBuffer;
	char *pStr;

	if(pAT == NULL){
		return 0;
	}

	length = AT_RECEIVE_BUFFER_COUNT(pAT);

	if(length <= 0){
		return 0;
	}

	pBuffer = calloc(length+1, sizeof(char));

	if(pBuffer == NULL){
		return 0;
	}

	if(oAT_ReceiveCopyTo(pAT, (uint8_t *)pBuffer, length) == 0){
		goto EXIT;
	}

	if(pAT->ReceiveCallback){
		(*pAT->ReceiveCallback)(pBuffer, length);
	}

	//Invalid
	pStr = strtok(pBuffer, "\r\n");

	while(pStr != NULL){
		if(oAT_ReceiveResponse(pAT, pStr)){
			break;
		}
		else{
			pStr = strtok(NULL, "\r\n");
		}
	}


	AT_RECEIVE_BUFFER_REMOVE(pAT, length);
	result = 1;

EXIT:
	free(pBuffer);

	return result;
}

void oAT_ReceiveFromDMA(oATCommend_t *pAT)
{
	if(pAT == NULL){
		return;
	}
	else if(pAT->pUART == NULL || pAT->LengthOfBufferRx <= 0){
		return;
	}
	else{
		if(oAT_UartRecovery(pAT) != AT_RESULT_OK){
			return;
		}
	}

	if(pAT->IndexOfRxLast != AT_DMA_INDEX(pAT->pUART)){
		pAT->IndexOfRxLast = AT_DMA_INDEX(pAT->pUART);
		pAT->RxTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);
	}
	else if(oTMR_Elapsed(&pAT->RxTimestamp, 250, TICKBASE_SYSTICK)){
		pAT->IndexOfRxLast = AT_DMA_INDEX(pAT->pUART);

		if(AT_RECEIVE_BUFFER_COUNT(pAT) > 0)
		{
			if(pAT->State.IsReceiveLine){
				oAT_ReadLine(pAT);
			}
			else{
				oAT_Read(pAT);
			}
		}
	}
}

oATResult_t oAT_TransmitSerialize(oATCommend_t *pAT, uint8_t *pData, uint32_t SizeOfData)
{
	uint32_t i = 0;

	if(pData == NULL || SizeOfData <= 0 || AT_TRANSMIT_BUFFER_COUNT(pAT) + SizeOfData > pAT->LengthOfBufferTx){
		return AT_RESULT_ERROR;
	}

	for(i=0; i<SizeOfData; i++){
		*(pAT->pBufferTx+pAT->IndexOfTxLast) = *pData;
		pData++;
		pAT->IndexOfTxLast = (pAT->IndexOfTxLast+1) % pAT->LengthOfBufferTx;
	}

	return AT_RESULT_OK;
}

oATResult_t oAT_TransmitBuffer(oATCommend_t *pAT)
{
	static uint8_t ATTransmitBufferStep = 0;

	uint8_t *pData;
	uint32_t Count = 0;

	switch(ATTransmitBufferStep)
	{
		case 0:
			if(AT_TRANSMIT_BUFFER_COUNT(pAT) <= 0){
				return AT_RESULT_NULL;
			}
			else if(pAT->pUART->gState != HAL_UART_STATE_READY || (pAT->pUART->hdmatx != NULL && pAT->pUART->hdmatx->Instance->CNDTR != 0)){
				return AT_RESULT_BUSY;
			}
			ATTransmitBufferStep++;
		case 1:
			if(pAT->IndexOfTxFirst+AT_TRANSMIT_BUFFER_COUNT(pAT) > pAT->LengthOfBufferTx){
				Count = pAT->LengthOfBufferTx - pAT->IndexOfTxFirst;
			}
			else{
				Count = AT_TRANSMIT_BUFFER_COUNT(pAT);
			}

			pData = (uint8_t *)pAT->pBufferTx + pAT->IndexOfTxFirst;

			if(HAL_UART_Transmit_DMA(pAT->pUART, pData, Count) == HAL_OK){
				pAT->IndexOfTxFirst = (pAT->IndexOfTxFirst+Count) % pAT->LengthOfBufferTx;
				pAT->TxTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);

				ATTransmitBufferStep++;
			}
			else{
				break;
			}
		case 2:
			if(AT_TRANSMIT_BUFFER_COUNT(pAT) <= 0){
				ATTransmitBufferStep = 0;
				return AT_RESULT_OK;
			}
			else if(pAT->pUART->gState == HAL_UART_STATE_READY){
				ATTransmitBufferStep = 1;
			}
			break;
	}

	return AT_RESULT_RUN;
}

oATResult_t oAT_WaitForResponse(oATCommend_t *pAT, char* Filter, uint32_t TimeoutDelay)
{
	static uint8_t ATResponseStep = 0;
	oATResult_t result = AT_RESULT_RUN;

	switch(ATResponseStep)
	{
		case 0:
			if(Filter == NULL || strlen(Filter) <= 0){
				result = AT_RESULT_ERROR;
				break;
			}
			else{
				ATResponseStep++;
			}
		case 1:
			memset(&pAT->ResponseFilter, 0, sizeof(pAT->ResponseFilter));
			strncpy(pAT->ResponseFilter, Filter, sizeof(pAT->ResponseFilter) - 1);
			pAT->State.IsWaitResponse = 1;
			pAT->State.IsReceivedResponse = 0;
			ATResponseStep++;
		case 2:
			if(pAT->State.IsReceivedResponse){
				result = AT_RESULT_OK;
			}
			else if(oTMR_Elapsed(&pAT->TxTimestamp, TimeoutDelay, TICKBASE_SYSTICK)){
				result = AT_RESULT_TIMEOUT;
			}
			break;
	}

	if(result != AT_RESULT_RUN){
		pAT->State.IsWaitResponse = 0;
		pAT->State.IsReceivedResponse = 0;
		ATResponseStep = 0;
	}

	return result;
}

oATResult_t oAT_TransmitData(oATCommend_t *pAT, uint8_t *Data, uint16_t Size)
{
	oATResult_t result = AT_RESULT_RUN;

	if(pAT->pBufferTx == NULL || pAT->LengthOfBufferTx <= 0){
		pAT->ProcessStep = 0;
		return AT_RESULT_ERROR;
	}

	switch (pAT->ProcessStep)
	{
		case 0:
			if(pAT->pUART->gState == HAL_UART_STATE_READY && Size > 0){
				oAT_TransmitSerialize(pAT, Data, Size);
				pAT->ProcessStep++;
			}
			else{
				if(oTMR_CountDown(&pAT->TxTimestamp, 300, TICKBASE_SYSTICK) == 0){
					result = AT_RESULT_ERROR;
				}
				break;
			}
		case 1:
			if(oAT_TransmitBuffer(pAT) == AT_RESULT_OK) {
				result = AT_RESULT_OK;
				pAT->TxTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);
			}
			else if(oTMR_Elapsed(&pAT->TxTimestamp, 1000, TICKBASE_SYSTICK)){
				result = AT_RESULT_ERROR; //HAL library error
			}
			break;
	}

	if(result != AT_RESULT_RUN){
		pAT->ProcessStep = 0;
	}

	return result;
}

oATResult_t oAT_TransmitCommend(oATCommend_t *pAT, char *Commend, char *ResponseFilter, uint32_t TimeoutDelay, uint8_t TryCount, uint8_t CRLF)
{
	oATResult_t result = AT_RESULT_RUN;

	oAT_ReceiveFromDMA(pAT);

	if(pAT->pBufferTx == NULL || pAT->LengthOfBufferTx <= 0){
		pAT->ProcessStep = 0;
		return AT_RESULT_ERROR;
	}

	switch (pAT->ProcessStep)
	{
		case 0:
			if(pAT->pUART->gState == HAL_UART_STATE_READY && strlen(Commend)){
				pAT->State.IsWaitResponse = 1;
				pAT->State.IsReceivedResponse = 0;
				pAT->CountOfTry = 0;
				pAT->ProcessStep++;
			}
			else {
				if(oTMR_CountDown(&pAT->TxTimestamp, 300, TICKBASE_SYSTICK) == 0){
					result = AT_RESULT_ERROR;
				}
				break;
			}
		case 1:
			oAT_TransmitSerialize(pAT,  (uint8_t *)Commend, strlen(Commend));

			if(strstr(Commend, "\n") == NULL && CRLF){
				oAT_TransmitSerialize(pAT,  (uint8_t *)AT_STRING_CRLF, strlen(AT_STRING_CRLF));
			}

			memset(&pAT->ResponseFilter, 0, sizeof(pAT->ResponseFilter));

			if(ResponseFilter != NULL && strlen(ResponseFilter) > 0){
				strncpy(pAT->ResponseFilter, ResponseFilter, sizeof(pAT->ResponseFilter) - 1);
			}

			pAT->TxTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);
			pAT->CountOfTry = MATH_MIN(pAT->CountOfTry+1, 254);
			pAT->ProcessStep++;
			break;
		case 2:
			if(oAT_TransmitBuffer(pAT) == AT_RESULT_OK) {
				pAT->ProcessStep++;
				pAT->TxTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);

				if(pAT->TransmitCallback != NULL){
					pAT->TransmitCallback(pAT->pBufferTx);
				}
			}
			else if(oTMR_Elapsed(&pAT->TxTimestamp, 1000, TICKBASE_SYSTICK)){
				result = AT_RESULT_ERROR; //HAL library error
			}
			break;
		case 3:
			if(pAT->State.IsReceivedResponse || strlen(pAT->ResponseFilter) <= 0){
				result = AT_RESULT_OK;

				if(pAT->State.IsReceivedResponse){
					pAT->CountOfTimeout = 0;
				}
			}
			else if(oTMR_Elapsed(&pAT->TxTimestamp, TimeoutDelay, TICKBASE_SYSTICK)){
				pAT->CountOfTimeout = MATH_MIN(pAT->CountOfTimeout+1, 254);

				if(pAT->CountOfTry >= TryCount){
					result = AT_RESULT_TIMEOUT;
				}
				else{
					pAT->ProcessStep = 1;
				}
			}
			break;
	}

	if(result == AT_RESULT_ERROR || result == AT_RESULT_TIMEOUT || result == AT_RESULT_OK){
		pAT->ProcessStep = 0;
		pAT->State.IsWaitResponse = 0;
		pAT->State.IsReceivedResponse = 0;
	}

	return result;
}

oATResult_t oAT_TransmitCommendWithData(oATCommend_t *pAT, char *Commend, char *ResponseFilter, uint32_t TimeoutDelay, uint8_t *pData, uint32_t SizeOfData, uint8_t CRLF)
{
	oATResult_t result = AT_RESULT_RUN;

	oAT_ReceiveFromDMA(pAT);

	if(pAT->pBufferTx == NULL || pAT->LengthOfBufferTx <= 0){
		pAT->ProcessStep = 0;
		return AT_RESULT_ERROR;
	}

	switch (pAT->ProcessStep)
	{
		case 0:
			if(pAT->pUART->gState == HAL_UART_STATE_READY && strlen(Commend)){
				oAT_TransmitSerialize(pAT,  (uint8_t *)Commend, strlen(Commend));

				if(strstr(Commend, "\n") == NULL && CRLF){
					oAT_TransmitSerialize(pAT,  (uint8_t *)AT_STRING_CRLF, strlen(AT_STRING_CRLF));
				}

				oAT_TransmitSerialize(pAT, pData, SizeOfData);

				pAT->TxTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);
				memset(&pAT->ResponseFilter, 0, sizeof(pAT->ResponseFilter));
				if(ResponseFilter != NULL && strlen(ResponseFilter) > 0){
					strncpy(pAT->ResponseFilter, ResponseFilter, sizeof(pAT->ResponseFilter) - 1);
				}

				pAT->State.IsWaitResponse = 1;
				pAT->State.IsReceivedResponse = 0;
				pAT->CountOfTry = 0;
				pAT->ProcessStep++;
			}
			else{
				if(oTMR_CountDown(&pAT->TxTimestamp, 300, TICKBASE_SYSTICK) == 0){
					result = AT_RESULT_ERROR;
				}

				break;
			}
		case 1:
			if(oTMR_Elapsed(&pAT->TxTimestamp, 500, TICKBASE_SYSTICK)){
				pAT->ProcessStep++;
			}
			break;
		case 2:
			if(oAT_TransmitBuffer(pAT) == AT_RESULT_OK) {
				pAT->ProcessStep++;
				pAT->TxTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);
			}
			else if(oTMR_Elapsed(&pAT->TxTimestamp, 1000, TICKBASE_SYSTICK)){
				result = AT_RESULT_ERROR; //HAL library error
			}
			break;
		case 3:
			if(pAT->State.IsReceivedResponse || strlen(pAT->ResponseFilter) <= 0){
				result = AT_RESULT_OK;

				if(pAT->State.IsReceivedResponse){
					pAT->CountOfTimeout = 0;
				}
			}
			else if(oTMR_Elapsed(&pAT->TxTimestamp, TimeoutDelay, TICKBASE_SYSTICK)){
				result = AT_RESULT_TIMEOUT;
			}
			break;
	}

	if(result == AT_RESULT_ERROR || result == AT_RESULT_TIMEOUT || result == AT_RESULT_OK){
		pAT->ProcessStep = 0;
		pAT->State.IsWaitResponse = 0;
		pAT->State.IsReceivedResponse = 0;
	}

	return result;
}

oATResult_t oAT_Close(oATCommend_t *pAT)
{
	if(pAT == NULL || pAT->pUART == NULL){
		return AT_RESULT_NULL;
	}

	if(HAL_UART_DeInit(pAT->pUART) != HAL_OK){
		return AT_RESULT_ERROR;
	}

	memset(&pAT->State, 0, sizeof(pAT->State));

	return AT_RESULT_OK;
}

oATResult_t oAT_Open(oATCommend_t *pAT)
{
	if(pAT == NULL || pAT->pUART == NULL){
		return AT_RESULT_NULL;
	}

	if(HAL_UART_DeInit(pAT->pUART) != HAL_OK){
		return AT_RESULT_ERROR;
	}

	if(HAL_UART_Init(pAT->pUART) != HAL_OK){
		return AT_RESULT_ERROR;
	}

	if(pAT->pBufferRx && pAT->LengthOfBufferRx){
		memset(pAT->pBufferRx, 0, pAT->LengthOfBufferRx);
	}

	/* DMA RX 핸들 검증 */
	if(pAT->pUART->hdmarx == NULL){
		return AT_RESULT_ERROR;
	}

	/* DMA Circular 모드 확인 - 링버퍼 미설정 시 Circular로 설정 */
	if(pAT->pUART->hdmarx->Init.Mode != DMA_CIRCULAR){
		pAT->pUART->hdmarx->Init.Mode = DMA_CIRCULAR;
		if(HAL_DMA_Init(pAT->pUART->hdmarx) != HAL_OK){
			return AT_RESULT_ERROR;
		}
	}

	if(HAL_UART_Receive_DMA(pAT->pUART, (uint8_t *)pAT->pBufferRx, pAT->LengthOfBufferRx) != HAL_OK){
		return AT_RESULT_ERROR;
	}
	pAT->IndexOfRxFirst = 0;
	pAT->IndexOfRxLast = 0;

	if(pAT->pBufferTx && pAT->LengthOfBufferTx){
		memset(pAT->pBufferTx, 0, pAT->LengthOfBufferTx);
	}

	pAT->IndexOfTxFirst = 0;
	pAT->IndexOfTxLast = 0;

	if(pAT->pResponseData && pAT->LengthOfResponse){
		memset(pAT->pResponseData, 0, pAT->LengthOfResponse);
	}

	pAT->ProcessStep = 0;
	memset(&pAT->State, 0, sizeof(pAT->State));

	pAT->State.IsOpen = 1;

	return AT_RESULT_OK;
}

void oAT_Proc(oATCommend_t *pAT)
{
	if(pAT == NULL || pAT->pUART == NULL){
		return;
	}

	pAT->State.IsReceiveTimeout = pAT->RxTimestamp == 0 || pAT->CountOfTimeout > 15;

	oAT_ReceiveFromDMA(pAT);
}
#endif

/* History

2026-06-26 | v0.1
	- baseline (ONE_ATCommend.c)
*/
