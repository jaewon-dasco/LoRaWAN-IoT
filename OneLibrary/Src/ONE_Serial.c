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

void oSerial_ResetRegister(oSerialHandler_t *pSerial)
{
	if(!pSerial){
		return;
	}

	for(int i=0; i<SERIAL_REGISTRY_MAXCOUNT; i++){
		if(SerialRegister[i] == pSerial){
			SerialRegister[i]->IsRegistered = 0;
			SerialRegister[i]->pUART->RxCpltCallback = 0;
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
			SerialRegister[i]->pUART->RxCpltCallback = oSerial_RxCpltCallback;
			break;
		}
	}
}

oResult_t oSerial_Write(oSerialHandler_t *pSerial, char *pBuffer, uint32_t SizeOfBuffer)
{
	if(pSerial == NULL || pSerial->pUART == NULL){
		return RESULT_ERROR;
	}

	if(pSerial->pUART->gState != HAL_UART_STATE_READY){
		return RESULT_ERROR;
	}

	if(!pSerial->IsRegistered){
		oSerial_SetRegister(pSerial);
	}

	if(pSerial->pTxBuffer != NULL && pSerial->SizeOfTxBuffer > 0 ){
		SizeOfBuffer = MATH_MIN(SizeOfBuffer, pSerial->SizeOfTxBuffer);
		memcpy(pSerial->pTxBuffer, pBuffer, SizeOfBuffer);

		return HAL_UART_Transmit_DMA(pSerial->pUART, (uint8_t *)pSerial->pTxBuffer, SizeOfBuffer) == HAL_OK ? RESULT_OK : RESULT_ERROR;
	}
	else{
		return HAL_UART_Transmit(pSerial->pUART, (uint8_t *)pBuffer, SizeOfBuffer, 1000) == HAL_OK ? RESULT_OK : RESULT_ERROR;
	}
}

oResult_t oSerial_Read(oSerialHandler_t *pSerial, char *pBuffer, uint32_t SizeOfBuffer, uint32_t WaitDelay)
{
	int cnt = 0;

	if(pSerial == NULL || pBuffer == NULL || pSerial->pUART == NULL || pSerial->pRxBuffer == NULL || pSerial->SizeOfRxBuffer <= 0){
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
			if(pSerial->pUART->hdmarx->Init.Mode != DMA_CIRCULAR){
				pSerial->pUART->hdmarx->Init.Mode = DMA_CIRCULAR;
				HAL_DMA_Init(pSerial->pUART->hdmarx);
			}

			if(HAL_UART_Receive_DMA(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer) != HAL_OK){
				HAL_UART_AbortReceive(pSerial->pUART);
				return RESULT_ERROR;
			}
		}

		pSerial->IndexOfRxLast = pSerial->pUART->RxXferSize - pSerial->pUART->hdmarx->Instance->CNDTR;
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

	memset(pBuffer, 0, SizeOfBuffer);

	while(pSerial->IndexOfRxLast != pSerial->IndexOfRxFirst && cnt < SizeOfBuffer){
		*pBuffer = *(pSerial->pRxBuffer + pSerial->IndexOfRxFirst);
		*(pSerial->pRxBuffer + pSerial->IndexOfRxFirst) = 0;

		pSerial->IndexOfRxFirst = (pSerial->IndexOfRxFirst+1) % pSerial->SizeOfRxBuffer;
		pBuffer++;
		cnt++;
	}

	return RESULT_OK;
}

oResult_t oSerial_ReadSplit(oSerialHandler_t *pSerial, char *pBuffer, uint32_t SizeOfBuffer, char *pSplitString)
{
	char *pStr;
	int32_t Cnt = 0;
	int32_t SplitLength = 0;

	if(pSerial == NULL || pBuffer == NULL || pSerial->pUART == NULL || pSerial->pRxBuffer == NULL || pSerial->SizeOfRxBuffer <= 0){
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
			if(pSerial->pUART->hdmarx->Init.Mode != DMA_CIRCULAR){
				pSerial->pUART->hdmarx->Init.Mode = DMA_CIRCULAR;
				HAL_DMA_Init(pSerial->pUART->hdmarx);
			}

			if(HAL_UART_Receive_DMA(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer) != HAL_OK){
				HAL_UART_AbortReceive(pSerial->pUART);
				return RESULT_ERROR;
			}
		}

		pSerial->IndexOfRxLast = pSerial->pUART->RxXferSize - pSerial->pUART->hdmarx->Instance->CNDTR;
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
	memset(pBuffer, 0, SizeOfBuffer);

	while(pSerial->IndexOfRxFinder != pSerial->IndexOfRxFirst && Cnt < SizeOfBuffer){
		*(pBuffer+Cnt) = *(pSerial->pRxBuffer + pSerial->IndexOfRxFirst);
		*(pSerial->pRxBuffer + pSerial->IndexOfRxFirst) = 0;

		pSerial->IndexOfRxFirst = (pSerial->IndexOfRxFirst+1) % pSerial->SizeOfRxBuffer;
		Cnt++;
	}

	//remove split string
	if(pSplitString != NULL && (pStr=strstr(pBuffer, pSplitString)) != NULL){
		memset(pStr, 0, SplitLength);
	}

	pSerial->FindedCount = 0;

	return RESULT_OK;
}


oResult_t oSerial_ReadLine(oSerialHandler_t *pSerial, char *pBuffer, uint32_t SizeOfBuffer)
{
	return oSerial_ReadSplit(pSerial, pBuffer, SizeOfBuffer, "\r\n");
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
