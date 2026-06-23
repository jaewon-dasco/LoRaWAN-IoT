/*
 * ONE_Debug.c
 *
 *  Created on: Sep 5, 2024
 *      Author: JONE
 */

#include "stdarg.h"
#include "stdio.h"
#include "stdlib.h"
#include "ctype.h"
#include "ONE_Math.h"
#include "ONE_Time.h"
#include "ONE_Common.h"
#include "ONE_Serial.h"

#if defined(__HAL_UART_ENABLE)
#define SERIAL_REGISTRY_LOGINDEX 	0
#define SERIAL_REGISTRY_MAXCOUNT 	5

oSerialHandler_t *SerialRegister[SERIAL_REGISTRY_MAXCOUNT];
oSerialHandler_t *LogSerial;

void oSerial_RxCpltCallback(struct __UART_HandleTypeDef *huart)
{
	for(int i=0; i<SERIAL_REGISTRY_MAXCOUNT; i++){
		if(SerialRegister[i] && SerialRegister[i]->pUART == huart && !huart->hdmarx){
			HAL_UART_Receive_IT(huart, (uint8_t *)SerialRegister[i]->pRxBuffer, SerialRegister[i]->SizeOfRxBuffer);
		}
	}
}

/* 링버퍼에서 wrap 안 넘는 연속 구간만 한 번에 송신.
 *   - First + Count <= Size : Count 바이트 한 번에 (연속)
 *   - First + Count >  Size : Size - First 바이트만 (wrap — 다음 콜백에서 나머지 송신)
 * 호출 전제: IsTxBusy=1 이 외부에서 이미 설정됨. 실패 시 함수가 IsTxBusy=0 클리어. */
static oResult_t oSerial_TransmitBuffer(oSerialHandler_t *pSerial)
{
	uint32_t size;
	HAL_StatusTypeDef status;

	if(pSerial->CountOfTxBuffer == 0){
		pSerial->IsTxBusy = 0;
		pSerial->TxChunkSize = 0;
		return RESULT_DONE;
	}

	if(pSerial->IndexOfTxFirst + pSerial->CountOfTxBuffer > pSerial->SizeOfTxBuffer){
		size = pSerial->SizeOfTxBuffer - pSerial->IndexOfTxFirst;   /* wrap: First~end 만 */
	}
	else{
		size = pSerial->CountOfTxBuffer;                            /* 연속 */
	}

	pSerial->TxChunkSize = size;

	if(pSerial->pUART->hdmatx){
		status = HAL_UART_Transmit_DMA(pSerial->pUART, &pSerial->pTxBuffer[pSerial->IndexOfTxFirst], size);
	}
	else{
		status = HAL_UART_Transmit_IT(pSerial->pUART, &pSerial->pTxBuffer[pSerial->IndexOfTxFirst], size);
	}

	if(status != HAL_OK){
		pSerial->IsTxBusy = 0;
		pSerial->TxChunkSize = 0;
		pSerial->TxErrorCount++;
		return RESULT_ERROR;
	}
	return RESULT_OK;
}

void oSerial_TxCpltCallback(struct __UART_HandleTypeDef *huart)
{
	oSerialHandler_t *pHandle;

	for(int i=0; i<SERIAL_REGISTRY_MAXCOUNT; i++){
		pHandle = SerialRegister[i];

		if(!pHandle || pHandle->pUART != huart){
			continue;
		}

		/* 완료된 chunk 만큼 consumer 포인터 전진 + 카운트 감소 */
		pHandle->IndexOfTxFirst = (pHandle->IndexOfTxFirst + pHandle->TxChunkSize) % pHandle->SizeOfTxBuffer;
		pHandle->CountOfTxBuffer -= pHandle->TxChunkSize;
		pHandle->TxChunkSize = 0;
		pHandle->TxCount++;

		/* 잔여 데이터 있으면 다음 chunk 자동 시작, 없으면 IsTxBusy 클리어 */
		if(pHandle->CountOfTxBuffer > 0){
			oSerial_TransmitBuffer(pHandle);
		}
		else{
			pHandle->IsTxBusy = 0;
		}
		break;
	}
}

void oSerial_ErrorCallback(struct __UART_HandleTypeDef *huart)
{
	for(int i=0; i<SERIAL_REGISTRY_MAXCOUNT; i++){
		if(SerialRegister[i] && SerialRegister[i]->pUART == huart){
			if(SerialRegister[i]->IsTxBusy){
				SerialRegister[i]->TxErrorCount++;
			}
			SerialRegister[i]->IsTxBusy = 0;
			break;
		}
	}
}

void oSerial_ResetRegister(oSerialHandler_t *pSerial)
{
	if(!pSerial){
		return;
	}

	for(int i=0; i<SERIAL_REGISTRY_MAXCOUNT; i++){
		if(SerialRegister[i] == pSerial){
			SerialRegister[i]->IsRegistered = 0;
			SerialRegister[i]->IsTxBusy = 0;
			SerialRegister[i]->IndexOfTxFirst = 0;
			SerialRegister[i]->IndexOfTxLast = 0;
			SerialRegister[i]->CountOfTxBuffer = 0;
			SerialRegister[i]->TxChunkSize = 0;
			SerialRegister[i]->pUART->RxCpltCallback = 0;
			SerialRegister[i]->pUART->TxCpltCallback = 0;
			SerialRegister[i]->pUART->ErrorCallback = 0;
			SerialRegister[i] = 0;
			break;
		}
	}
}

void oSerial_SetRegister(oSerialHandler_t *pSerial)
{
	if(!pSerial || !pSerial->pUART || !pSerial->pRxBuffer){
		return;
	}

	for(int i=0; i<SERIAL_REGISTRY_MAXCOUNT; i++){
		if(!SerialRegister[i] || SerialRegister[i] == pSerial){
			SerialRegister[i] = pSerial;
			SerialRegister[i]->IsRegistered = 1;
			SerialRegister[i]->IsTxBusy = 0;
			SerialRegister[i]->IndexOfTxFirst = 0;
			SerialRegister[i]->IndexOfTxLast = 0;
			SerialRegister[i]->CountOfTxBuffer = 0;
			SerialRegister[i]->TxChunkSize = 0;
			SerialRegister[i]->pUART->RxCpltCallback = oSerial_RxCpltCallback;
			SerialRegister[i]->pUART->TxCpltCallback = oSerial_TxCpltCallback;
			SerialRegister[i]->pUART->ErrorCallback  = oSerial_ErrorCallback;
			break;
		}
	}
}

oResult_t oSerial_Write(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData)
{
	uint32_t freeSpace;
	uint8_t needStart;
	HAL_StatusTypeDef status;

	if(pSerial == NULL || pSerial->pUART == NULL || pData == NULL || SizeOfData == 0){
		return RESULT_ERROR;
	}

	if(!pSerial->IsRegistered){
		oSerial_SetRegister(pSerial);
	}

	/* TxBuffer 미설정 시 블로킹 송신 fallback */
	if(pSerial->pTxBuffer == NULL || pSerial->SizeOfTxBuffer == 0){
		status = HAL_UART_Transmit(pSerial->pUART, (uint8_t *)pData, SizeOfData, 1000);
		if(status == HAL_OK){
			pSerial->TxCount++;
			return RESULT_OK;
		}
		pSerial->TxErrorCount++;
		return RESULT_ERROR;
	}

	/* === Critical: 링버퍼 상태 변경 (ISR과 공유) === */
	__disable_irq();

	freeSpace = pSerial->SizeOfTxBuffer - pSerial->CountOfTxBuffer;
	if(SizeOfData > freeSpace){
		__enable_irq();
		return RESULT_BUSY;   /* 공간 부족 — 데이터 일부 적재 금지 (all-or-nothing) */
	}

	/* 링버퍼 적재 — byte 단위 wrap 처리 */
	for(uint32_t i = 0; i < SizeOfData; i++){
		pSerial->pTxBuffer[pSerial->IndexOfTxLast] = (uint8_t)pData[i];
		pSerial->IndexOfTxLast = (pSerial->IndexOfTxLast + 1) % pSerial->SizeOfTxBuffer;
	}
	pSerial->CountOfTxBuffer += SizeOfData;

	/* DMA idle 이면 본 호출이 시작 책임. 진행 중이면 콜백이 자동으로 이어 송신. */
	needStart = !pSerial->IsTxBusy;
	if(needStart){
		pSerial->IsTxBusy = 1;
	}

	__enable_irq();
	/* === End critical === */

	if(needStart){
		return oSerial_TransmitBuffer(pSerial);
	}

	return RESULT_OK;
}

oResult_t oSerial_Read(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData, uint32_t WaitDelay)
{
	int cnt = 0;

	if(pSerial == NULL || pData == NULL || pSerial->pUART == NULL || pSerial->pRxBuffer == NULL || pSerial->SizeOfRxBuffer <= 0){
		return RESULT_NULL;
	}
	else if(pSerial->pUART->gState == HAL_UART_STATE_RESET){
		return RESULT_NULL;
	}

	if(!pSerial->IsRegistered){
		oSerial_SetRegister(pSerial);
	}

	if(pSerial->pUART->hdmarx){
		/* DMA 모드 */
		if(pSerial->pUART->ErrorCode & (HAL_UART_ERROR_ORE|HAL_UART_ERROR_FE|HAL_UART_ERROR_NE|HAL_UART_ERROR_PE|HAL_UART_ERROR_DMA)){
			HAL_UART_AbortReceive(pSerial->pUART);
		}

		if(pSerial->pUART->RxState == HAL_UART_STATE_READY){
			if(!ONE_DMA_IS_CIRCULAR(pSerial->pUART->hdmarx)){
				ONE_DMA_SET_CIRCULAR(pSerial->pUART->hdmarx);
				HAL_DMA_Init(pSerial->pUART->hdmarx);
			}

			if(HAL_UART_Receive_DMA(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer) != HAL_OK){
				HAL_UART_AbortReceive(pSerial->pUART);
				return RESULT_ERROR;
			}
		}

		pSerial->IndexOfRxLast = pSerial->pUART->RxXferSize - ONE_DMA_GET_COUNTER(pSerial->pUART->hdmarx);
	}
	else{
		/* 인터럽트 모드 */
		if(pSerial->pUART->RxState == HAL_UART_STATE_READY){
			if(HAL_UART_Receive_IT(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer) != HAL_OK){
				HAL_UART_AbortReceive(pSerial->pUART);
				return RESULT_ERROR;
			}
		}
		else if(pSerial->pUART->RxXferSize && pSerial->IdleTimer && oTMR_Elapsed(&pSerial->IdleTimer, MATH_MAX(WaitDelay, 100), TICKBASE_SYSTICK)){
			/* IT 모드 idle 타임아웃: 수신 없으면 재시작 */
			HAL_UART_AbortReceive(pSerial->pUART);
			pSerial->IndexOfRxFirst = 0;
			pSerial->IndexOfRxLast = 0;
			pSerial->IdleTimer = 0;
			HAL_UART_Receive_IT(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer);
		}

		pSerial->IndexOfRxLast = pSerial->pUART->RxXferSize - pSerial->pUART->RxXferCount;
	}

	if(pSerial->IndexOfRxLast == pSerial->IndexOfRxFirst){
		pSerial->ReceiveTimer = oTMR_GetTick(TICKBASE_SYSTICK);
		return RESULT_NULL;
	}
	else{
		pSerial->IdleTimer = oTMR_GetTick(TICKBASE_SYSTICK);

		if(!oTMR_Elapsed(&pSerial->ReceiveTimer, MATH_MAX(WaitDelay, 100), TICKBASE_SYSTICK)){
			return RESULT_BUSY;
		}
	}

	memset(pData, 0, SizeOfData);

	while(pSerial->IndexOfRxLast != pSerial->IndexOfRxFirst && cnt < SizeOfData){
		*pData = *(pSerial->pRxBuffer + pSerial->IndexOfRxFirst);
		*(pSerial->pRxBuffer + pSerial->IndexOfRxFirst) = 0;

		pSerial->IndexOfRxFirst = (pSerial->IndexOfRxFirst+1) % pSerial->SizeOfRxBuffer;
		pData++;
		cnt++;
	}

	return RESULT_OK;
}

oResult_t oSerial_ReadSplit(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData, char *pSplitString)
{
	char *pStr;
	int32_t Cnt = 0;
	int32_t SplitLength = 0;

	if(pSerial == NULL || pData == NULL || pSerial->pUART == NULL || pSerial->pRxBuffer == NULL || pSerial->SizeOfRxBuffer <= 0){
		return RESULT_NULL;
	}
	else if(pSerial->pUART->gState == HAL_UART_STATE_RESET){
		return RESULT_NULL;
	}

	if(!pSerial->IsRegistered){
		oSerial_SetRegister(pSerial);
	}

	if(pSerial->pUART->hdmarx){
		/* DMA 모드 */
		if(pSerial->pUART->ErrorCode & (HAL_UART_ERROR_ORE|HAL_UART_ERROR_FE|HAL_UART_ERROR_NE|HAL_UART_ERROR_PE|HAL_UART_ERROR_DMA)){
			HAL_UART_AbortReceive(pSerial->pUART);
		}

		if(pSerial->pUART->RxState == HAL_UART_STATE_READY){
			if(!ONE_DMA_IS_CIRCULAR(pSerial->pUART->hdmarx)){
				ONE_DMA_SET_CIRCULAR(pSerial->pUART->hdmarx);
				HAL_DMA_Init(pSerial->pUART->hdmarx);
			}

			if(HAL_UART_Receive_DMA(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer) != HAL_OK){
				HAL_UART_AbortReceive(pSerial->pUART);
				return RESULT_ERROR;
			}
		}

		pSerial->IndexOfRxLast = pSerial->pUART->RxXferSize - ONE_DMA_GET_COUNTER(pSerial->pUART->hdmarx);
	}
	else{
		/* 인터럽트 모드 */
		if(pSerial->pUART->RxState == HAL_UART_STATE_READY){
			if(HAL_UART_Receive_IT(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer) != HAL_OK){
				HAL_UART_AbortReceive(pSerial->pUART);
				return RESULT_ERROR;
			}
		}
		else if(pSerial->pUART->RxXferSize && pSerial->IdleTimer && oTMR_Elapsed(&pSerial->IdleTimer, 1000, TICKBASE_SYSTICK)){
			/* IT 모드 idle 타임아웃: 1초 이상 수신 없으면 재시작 */
			HAL_UART_AbortReceive(pSerial->pUART);
			pSerial->IndexOfRxFirst = 0;
			pSerial->IndexOfRxLast = 0;
			pSerial->IndexOfRxFinder = 0;
			pSerial->FindedCount = 0;
			pSerial->IdleTimer = 0;
			HAL_UART_Receive_IT(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer);
		}

		pSerial->IndexOfRxLast = pSerial->pUART->RxXferSize - pSerial->pUART->RxXferCount;
	}

	//split string check
	if(pSplitString != NULL){
		SplitLength = strlen(pSplitString);
	}
	else{
		pSerial->FindedCount = 0;
	}

	if(pSerial->IndexOfRxLast == pSerial->IndexOfRxFirst){
		pSerial->ReceiveTimer = oTMR_GetTick(TICKBASE_SYSTICK);
		return RESULT_NULL;
	}
	else{
		pSerial->IdleTimer = oTMR_GetTick(TICKBASE_SYSTICK);

		if(!oTMR_Elapsed(&pSerial->ReceiveTimer, 100, TICKBASE_SYSTICK)){
			return RESULT_BUSY;
		}
	}

	while(pSerial->IndexOfRxLast != pSerial->IndexOfRxFinder && pSerial->FindedCount < SplitLength){
		if(*(pSplitString + pSerial->FindedCount) == *(pSerial->pRxBuffer + pSerial->IndexOfRxFinder)){
			pSerial->FindedCount++;
		}
		else{
			pSerial->FindedCount = 0;
		}

		pSerial->IndexOfRxFinder = (pSerial->IndexOfRxFinder+1) % pSerial->SizeOfRxBuffer;
	}

	if(pSerial->FindedCount != SplitLength){
		return RESULT_NULL;
	}

	//copy to
	memset(pData, 0, SizeOfData);

	while(pSerial->IndexOfRxFinder != pSerial->IndexOfRxFirst && Cnt < SizeOfData){
		*(pData+Cnt) = *(pSerial->pRxBuffer + pSerial->IndexOfRxFirst);
		*(pSerial->pRxBuffer + pSerial->IndexOfRxFirst) = 0;

		pSerial->IndexOfRxFirst = (pSerial->IndexOfRxFirst+1) % pSerial->SizeOfRxBuffer;
		Cnt++;
	}

	//remove split string
	if(pSplitString != NULL && (pStr=strstr(pData, pSplitString)) != NULL){
		memset(pStr, 0, SplitLength);
	}

	pSerial->FindedCount = 0;

	return RESULT_OK;
}


oResult_t oSerial_ReadLine(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData)
{
	return oSerial_ReadSplit(pSerial, pData, SizeOfData, "\r\n");
}

void oSerial_PutChar(oSerialHandler_t *pSerial, char* pChar)
{
	if(pSerial == NULL || pChar == NULL || pSerial->pUART == NULL || pSerial->pUART->gState != HAL_UART_STATE_READY){
		return;
	}

	if(!pSerial->IsRegistered){
		oSerial_SetRegister(pSerial);
	}

	HAL_UART_Transmit(pSerial->pUART, (uint8_t *)pChar, strlen(pChar), 10);  // 정확한 전송 크기 설정
}

void oSerial_vPrint(oSerialHandler_t *pSerial, const char* format, va_list args)
{
    char Buffer[100];

    if (pSerial == NULL || pSerial->pUART == NULL || pSerial->pUART->gState != HAL_UART_STATE_READY)
        return;

    memset(Buffer, 0, sizeof(Buffer));

    while (*format)
    {
        memset(Buffer, 0, sizeof(Buffer));

        if (*format == '%' && *(format + 1))
        {
            format++;

            int left_align = 0;
            int zero_padding = 0;
            int width = 0;
            int precision = -1;

            if (*format == '-') { left_align = 1; format++; }
            else if (*format == '0') { zero_padding = 1; format++; }

            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (*format - '0');
                format++;
            }

            if(*format == '.') {
                format++;
                precision = 0;
                while (*format >= '0' && *format <= '9') {
                    precision = precision * 10 + (*format - '0');
                    format++;
                }
            }

            char fmt[24] = {0};

            switch (*format)
            {
                case 'd':
                    snprintf(fmt, sizeof(fmt), "%%%s%s%dd", left_align ? "-" : "", zero_padding && !left_align ? "0" : "", width);
                    snprintf(Buffer, sizeof(Buffer), fmt, va_arg(args, int));
                    break;

                case 'u':
                    snprintf(fmt, sizeof(fmt), "%%%s%s%du", left_align ? "-" : "", zero_padding && !left_align ? "0" : "", width);
                    snprintf(Buffer, sizeof(Buffer), fmt, va_arg(args, unsigned int));
                    break;

                case 'x':
                    snprintf(fmt, sizeof(fmt), "%%%s%s%dx", left_align ? "-" : "", zero_padding && !left_align ? "0" : "", width);
                    snprintf(Buffer, sizeof(Buffer), fmt, va_arg(args, unsigned int));
                    break;

                case 'X':
                    snprintf(fmt, sizeof(fmt), "%%%s%s%dX", left_align ? "-" : "", zero_padding && !left_align ? "0" : "", width);
                    snprintf(Buffer, sizeof(Buffer), fmt, va_arg(args, unsigned int));
                    break;

                case 'c':
                    snprintf(fmt, sizeof(fmt), "%%%s%s%dc", left_align ? "-" : "", zero_padding && !left_align ? "0" : "", width);
                    snprintf(Buffer, sizeof(Buffer), fmt, (char)va_arg(args, int));
                    break;

                case 's':
                    snprintf(fmt, sizeof(fmt), "%%%s%s%ds", left_align ? "-" : "", zero_padding && !left_align ? "0" : "", width);
                    snprintf(Buffer, sizeof(Buffer), fmt, va_arg(args, char*));
                    break;

                case 'f':
                    double val = va_arg(args, double);
                    double frac = val - (int)val;

                    if (precision < 0){
                    	if ((int)(val * 10) % 10 == 0) {
							precision = 1;  // 3.0 같은 값이면 소숫점 1자리만
						}
                    	else{
							precision = 6;
							for (int i = 6; i > 1; i--) {
								if ((int)(frac * pow(10, i)) % 10 != 0) {
									break;  // 유효숫자 존재 → 이 precision 유지
								}
								precision--;
							}
                    	}
                    }

                    snprintf(fmt, sizeof(fmt), "%%%s%s%d.%df", left_align ? "-" : "", zero_padding && !left_align ? "0" : "", width, precision);
                    snprintf(Buffer, sizeof(Buffer), fmt, val);
                    break;

                default:
                    Buffer[0] = *format;
                    Buffer[1] = '\0';
                    break;
            }

            oSerial_PutChar(pSerial, Buffer);
        }
        else
        {
            Buffer[0] = *format;
            Buffer[1] = '\0';
            oSerial_PutChar(pSerial, Buffer);
        }

        format++;
    }
}


void oSerial_Printf(oSerialHandler_t *pSerial, const char* format, ...)
{
    if(pSerial == NULL || pSerial->pUART == NULL || pSerial->pUART->gState != HAL_UART_STATE_READY){
        return;
    }

    va_list args;
    va_start(args, format);

    oSerial_vPrint(pSerial, format, args);

    va_end(args);
}

void oSerial_PrintLine(oSerialHandler_t *pSerial, const char* format, ...)
{
    if(pSerial == NULL || pSerial->pUART == NULL || pSerial->pUART->gState != HAL_UART_STATE_READY){
        return;
    }

    va_list args;
    va_start(args, format);

    oSerial_vPrint(pSerial, format, args);

    va_end(args);

    if(strstr(format, "\r\n") == NULL){
    	oSerial_PutChar(pSerial, "\r\n");
    }
}

void oSerial_LogEnagle(oSerialHandler_t *pSerial, uint8_t Enable)
{
	if(!Enable || !pSerial || !pSerial->pUART){
		LogSerial = NULL;
	}
	else{
		LogSerial = pSerial;
	}
}

void oSerial_Log(char* Title, const char* format, ...)
{
    va_list args;

	if(LogSerial && strlen(Title) && strlen(format)){
		va_start(args, format);

		oSerial_PutChar(LogSerial, "Log | ");
		oSerial_PutChar(LogSerial, Title);
		oSerial_PutChar(LogSerial, " | ");

		oSerial_vPrint(LogSerial, format, args);

		if(strstr(format, "\r\n") == NULL){
			oSerial_PutChar(LogSerial, "\r\n");
		}

		va_end(args);
	}
}
#endif
