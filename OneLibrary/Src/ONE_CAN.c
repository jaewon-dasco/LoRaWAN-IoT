/*
 * Mi_CAN.c
 *
 *  Created on: Dec 3, 2024
 *      Author: JONE
 */
#include "ONE_Memory.h"
#include "ONE_Time.h"
#include "ONE_Math.h"
#include "ONE_CAN.h"

#ifdef CAN_ID_STD
#define CAN_MAXCHANNELS 	2

oCANVxD_t CANVxDs[CAN_MAXCHANNELS];

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	static CAN_RxHeaderTypeDef RxHeader;
	oCANVxD_t *pVxD = NULL;
	oCANMessageCallback_t *pCallback = NULL;
	oCANMessage_t RxMessage;
	uint8_t i;
	uint8_t matched = 0;

	for(i = 0; i < CAN_MAXCHANNELS; i++){
		if(CANVxDs[i].Handler.Instance == hcan->Instance){
			pVxD = &CANVxDs[i];
			break;
		}
	}

	if(pVxD == NULL){
		return;
	}

	pCallback = (oCANMessageCallback_t *)&pVxD->CallbackList;

	/* Get RX message */
	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxMessage.Data) == HAL_OK){
		if(oCAN_IsOpen(pVxD) == RESULT_OK){
			RxMessage.IsEextended = RxHeader.IDE & CAN_ID_EXT ? 1 : 0;
			RxMessage.ID = RxMessage.IsEextended ? RxHeader.ExtId : RxHeader.StdId;
			RxMessage.DLC = RxHeader.DLC;
			RxMessage.IsNew = 1;
			RxMessage.Timestamp = oTMR_GetTick(TICKBASE_SYSTICK); // 변경: oTMR_ResetNow → 직접 대입

			for(i=0; i<ArrayLen(pVxD->CallbackList); i++){
				matched = ((pCallback[i].ID ^ RxMessage.ID) & pCallback[i].Mask) == 0 ? 1 : 0;

				if(pCallback[i].Callback && matched){
					pCallback[i].Callback(&RxMessage, pCallback[i].Argument);
				}
			}
		}
	}
}

void oCAN_ResetReceiveCallback(oCANVxD_t *pVxD, oCANCallback_t Callback)
{
	if(pVxD == NULL) return;
	for(uint8_t i =0; i<ArrayLen(pVxD->CallbackList); i++){
		if(pVxD->CallbackList[i].Callback == Callback){
			pVxD->CallbackList[i].ID = 0;
			pVxD->CallbackList[i].Mask = 0;
			oCAN_ResetFilter(pVxD, i);
			pVxD->CallbackList[i].Callback = NULL;
		}
	}
}

void oCAN_SetReceiveCallback(oCANVxD_t *pVxD, oCANCallback_t Callback, uint32_t Argument)
{
	if(pVxD == NULL) return;
	for(uint8_t i =0; i<ArrayLen(pVxD->CallbackList); i++){
		if(pVxD->CallbackList[i].Callback == NULL){
			pVxD->CallbackList[i].Callback = Callback;
			pVxD->CallbackList[i].ID = 0;
			pVxD->CallbackList[i].Mask = 0;
			pVxD->CallbackList[i].Argument = Argument;
			break;
		}
	}
}

oResult_t oCAN_SetMaskReceiveCallback(oCANVxD_t *pVxD, uint32_t ID, uint32_t Mask, uint8_t Extended, oCANCallback_t Callback, uint32_t Argument)
{
	if(pVxD == NULL || pVxD->Handler.Instance == NULL){
		return RESULT_ERROR;
	}


	for(uint8_t i =0; i<ArrayLen(pVxD->CallbackList); i++){
		if(pVxD->CallbackList[i].Callback == NULL){
			pVxD->CallbackList[i].Callback = Callback;
			pVxD->CallbackList[i].ID = ID;
			pVxD->CallbackList[i].Mask = Mask;
			pVxD->CallbackList[i].Argument = Argument;
			oCAN_SetMaskFilter(pVxD, i, ID, Mask, Extended);
			break;
		}
	}

	return RESULT_OK;
}

oResult_t oCAN_Transmit(oCANVxD_t *pVxD, oCANMessage_t *Message)
{
	CAN_TxHeaderTypeDef TxHeader;

	uint32_t MailBox;
	uint8_t	TxData[8];

	if(pVxD == NULL || Message == NULL || pVxD->Handler.Instance == NULL){
		return RESULT_ERROR;
	}

	if(oCAN_IsOpen(pVxD) == RESULT_ERROR){
		return RESULT_ERROR;
	}

	MailBox = HAL_CAN_GetTxMailboxesFreeLevel(&pVxD->Handler);

	if(MailBox > 0){
		if(!Message->IsEextended) {
			TxHeader.StdId = Message->ID;
			TxHeader.IDE = CAN_ID_STD;
		}
		else {
			TxHeader.ExtId = Message->ID;
			TxHeader.IDE = CAN_ID_EXT;
		}

		TxHeader.DLC = Message->DLC;
		TxHeader.RTR = CAN_RTR_DATA;
		TxHeader.TransmitGlobalTime = DISABLE;

		memcpy(&TxData, &Message->Data, 8);

		if(HAL_CAN_AddTxMessage(&pVxD->Handler, &TxHeader, TxData, &MailBox) == HAL_OK){
			Message->IsNew = 0;
			Message->Checker = oMEM_CRC8((uint8_t *)&Message->Data, 8, 0);

			Message->Timestamp = oTMR_GetTick(TICKBASE_SYSTICK); // 변경: oTMR_ResetNow → 직접 대입
            return RESULT_OK;
		}
	}

	return RESULT_ERROR;
}

oResult_t oCAN_Transmitter(oCANVxD_t *pVxD, oCANMessage_t *Message, uint32_t Interval)
{
	uint8_t Checker;

	if(pVxD == NULL || Message == NULL || pVxD->Handler.Instance == NULL){
		return RESULT_ERROR;
	}

	if(oCAN_IsOpen(pVxD) == RESULT_ERROR){
		return RESULT_ERROR;
	}

	Checker = oMEM_CRC8((uint8_t *)&Message->Data, 8, 0);

	if(oTMR_Elapsed(&Message->Timestamp, MATH_MAX(Interval, 10), TICKBASE_SYSTICK) || (Checker != Message->Checker && oTMR_Elapsed(&Message->Timestamp, 5, TICKBASE_SYSTICK))){
		return oCAN_Transmit(pVxD, Message);
	}

	return RESULT_ERROR;
}

oResult_t oCAN_ResetFilter(oCANVxD_t *pVxD, uint8_t Bank)
{
	CAN_FilterTypeDef  FilterConfig;

	if(pVxD == NULL || pVxD->Handler.Instance == NULL){
		return RESULT_ERROR;
	}

	memset(&FilterConfig, 0, sizeof(CAN_FilterTypeDef));

	FilterConfig.FilterBank = Bank;
	FilterConfig.FilterMode = CAN_FILTERMODE_IDLIST;
	FilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
	FilterConfig.FilterActivation = DISABLE;

	if (HAL_CAN_ConfigFilter(&pVxD->Handler, &FilterConfig) != HAL_OK){
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

oResult_t oCAN_SetIdFilter(oCANVxD_t *pVxD, uint8_t Bank, uint32_t IDs[], uint8_t Count, uint8_t Extended)
{
	uint8_t i;
	uint32_t fId;
	CAN_FilterTypeDef  FilterConfig;

	if(pVxD == NULL || pVxD->Handler.Instance == NULL){
		return RESULT_ERROR;
	}

	memset(&FilterConfig, 0, sizeof(FilterConfig));
	FilterConfig.FilterBank = Bank;
	FilterConfig.FilterMode = CAN_FILTERMODE_IDLIST;
	FilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
	FilterConfig.FilterActivation = ENABLE;

	if(Extended){
		FilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;

		for(i=0; i<Count && i<2; i++)
		{
			fId = (IDs[i] << 3) | CAN_ID_EXT;

			switch(i)
			{
				case 0:
					FilterConfig.FilterIdHigh = (fId >> 16);
					FilterConfig.FilterIdLow = (fId & 0xFFFF);
					break;
				case 1:
					FilterConfig.FilterMaskIdHigh = (fId >> 16);
					FilterConfig.FilterMaskIdLow = (fId & 0xFFFF);
					break;
			}
		}
	}
	else{
		FilterConfig.FilterScale = CAN_FILTERSCALE_16BIT;

		for(i=0; i<Count && i<4; i++)
		{
			fId = (IDs[i] << 5) | CAN_ID_STD;

			switch(i)
			{
				case 0:
					FilterConfig.FilterIdHigh = fId;
					break;
				case 1:
					FilterConfig.FilterIdLow = fId;
					break;
				case 2:
					FilterConfig.FilterMaskIdHigh = fId;
					break;
				case 3:
					FilterConfig.FilterMaskIdLow = fId;
					break;
			}
		}
	}

	if (HAL_CAN_ConfigFilter(&pVxD->Handler, &FilterConfig) != HAL_OK){
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

oResult_t oCAN_SetMaskFilter(oCANVxD_t *pVxD, uint8_t Bank, uint32_t Id, uint32_t Mask, uint8_t Extended)
{
	uint32_t fId;
	CAN_FilterTypeDef  FilterConfig;

	if(pVxD == NULL || pVxD->Handler.Instance == NULL){
		return RESULT_ERROR;
	}

	FilterConfig.FilterBank = Bank; //max.14 | 0 to 13
	FilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	FilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
	FilterConfig.FilterActivation = ENABLE;

	if(Extended){
		FilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;

		fId = (Id << 3) | CAN_ID_EXT;
		FilterConfig.FilterIdHigh = (fId >> 16);
		FilterConfig.FilterIdLow = (fId & 0xFFFF);

		fId = (Mask << 3) | CAN_ID_EXT;
		FilterConfig.FilterMaskIdHigh = (fId >> 16);
		FilterConfig.FilterMaskIdLow = (fId & 0xFFFF);
	}
	else{
		FilterConfig.FilterScale = CAN_FILTERSCALE_16BIT;

		fId = (Id << 5) | CAN_ID_STD;
		FilterConfig.FilterIdHigh = (fId & 0xFFFF);
		FilterConfig.FilterIdLow = (fId & 0xFFFF);

		fId = (Mask << 5) | CAN_ID_STD;
		FilterConfig.FilterMaskIdHigh = (fId & 0xFFFF);
		FilterConfig.FilterMaskIdLow = (fId & 0xFFFF);
	}

	if (HAL_CAN_ConfigFilter(&pVxD->Handler, &FilterConfig) != HAL_OK){
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

oResult_t oCAN_Close(oCANVxD_t *pVxD)
{
	if(pVxD == NULL || pVxD->Handler.Instance == NULL){
		return RESULT_ERROR;
	}

	HAL_CAN_Stop(&pVxD->Handler);
	HAL_CAN_DeactivateNotification(&pVxD->Handler, CAN_IT_RX_FIFO0_MSG_PENDING);
	HAL_CAN_DeInit(&pVxD->Handler);

	memset(pVxD, 0, sizeof(oCANVxD_t));

	return RESULT_OK;
}

void* oCAN_GetHandler(oCANVxD_t *pVxD)
{
	if(pVxD == NULL || pVxD->Handler.Instance == NULL){
		return NULL;
	}

	return &pVxD->Handler;
}

oResult_t oCAN_IsMailBoxFull(oCANVxD_t *pVxD)
{
	if(pVxD == NULL || pVxD->Handler.Instance == NULL){
		return RESULT_ERROR;
	}

	if(oCAN_IsOpen(pVxD) == RESULT_ERROR){
		return RESULT_ERROR;
	}

	return HAL_CAN_GetTxMailboxesFreeLevel(&pVxD->Handler) <= 0;
}

oResult_t oCAN_IsError(oCANVxD_t *pVxD)
{
	if(pVxD == NULL || pVxD->Handler.Instance == NULL){
		return RESULT_NULL;
	}

	return (pVxD->Handler.ErrorCode & (HAL_CAN_ERROR_BOF|HAL_CAN_ERROR_EPV)) != 0 ? RESULT_ERROR : RESULT_NULL;
}

oResult_t oCAN_IsOpen(oCANVxD_t *pVxD)
{
	if(pVxD == NULL || pVxD->Handler.Instance == NULL){
		return RESULT_ERROR;
	}

	return (pVxD->Handler.State == HAL_CAN_STATE_RESET || pVxD->Handler.State == HAL_CAN_STATE_ERROR) ? RESULT_ERROR : RESULT_OK;
}

oResult_t oCAN_Open(oCANVxD_t **ppVxD, CAN_TypeDef *pCAN, uint16_t Bitrate)
{
	int Channel = 0;
	uint32_t CoreClock = HAL_RCC_GetPCLK1Freq();
	oCANVxD_t *pInitVxD = NULL;

	if(ppVxD == NULL || pCAN == NULL){
		return RESULT_NULL;
	}

	for(Channel=0; Channel < CAN_MAXCHANNELS; Channel++){
		if(CANVxDs[Channel].Handler.Instance == NULL){
			pInitVxD = &CANVxDs[Channel];
			CANVxDs[Channel].Handler.Instance = pCAN;
			break;
		}
		else if(CANVxDs[Channel].Handler.Instance == pCAN){
			pInitVxD = &CANVxDs[Channel];
			break;
		}
	}

	if(pInitVxD == NULL){
		return RESULT_NULL;
	}

	if(oCAN_IsOpen(pInitVxD) == RESULT_OK){
		if(pInitVxD->Bitrate == Bitrate){
			return RESULT_OK;
		}
		else{
			return RESULT_ERROR;
		}
	}

	HAL_CAN_DeInit(&pInitVxD->Handler);

	pInitVxD->Handler.Init.Mode = CAN_MODE_NORMAL;
	pInitVxD->Handler.Init.TimeTriggeredMode = DISABLE;
	pInitVxD->Handler.Init.AutoBusOff = ENABLE;
	pInitVxD->Handler.Init.AutoWakeUp = ENABLE;
	pInitVxD->Handler.Init.AutoRetransmission = ENABLE;
	pInitVxD->Handler.Init.ReceiveFifoLocked = DISABLE;
	pInitVxD->Handler.Init.TransmitFifoPriority = DISABLE;

	pInitVxD->Handler.Init.SyncJumpWidth = CAN_SJW_1TQ;

	if(CoreClock >= 16000000){
		pInitVxD->Handler.Init.TimeSeg1 = CAN_BS1_13TQ;
		pInitVxD->Handler.Init.TimeSeg2 = CAN_BS2_2TQ;

		switch(Bitrate)
		{
			case 1000:
				pInitVxD->Handler.Init.Prescaler = 1;
				break;
			case  500:
				pInitVxD->Handler.Init.Prescaler = 2;
				break;
			default:
			case  250:
				pInitVxD->Handler.Init.Prescaler = 4;
				break;
			case  125:
				pInitVxD->Handler.Init.Prescaler = 8;
				break;
			case  100:
				pInitVxD->Handler.Init.Prescaler = 10;
				break;
			case   50:
				pInitVxD->Handler.Init.Prescaler = 20;
				break;
			case   25:
				pInitVxD->Handler.Init.Prescaler = 40;
				break;
		}

		pInitVxD->Handler.Init.Prescaler *= CoreClock/16000000;
	}
	else{
		switch(CoreClock)
		{
			case 4000000:
				switch(Bitrate)
				{
					case 1000:
						return RESULT_NULL;
						break;
					case  500:
						pInitVxD->Handler.Init.Prescaler = 1;
						pInitVxD->Handler.Init.TimeSeg1 = CAN_BS1_6TQ;
						pInitVxD->Handler.Init.TimeSeg2 = CAN_BS2_1TQ;
						break;
					default:
					case  250:
						pInitVxD->Handler.Init.Prescaler = 1;
						pInitVxD->Handler.Init.TimeSeg1 = CAN_BS1_13TQ;
						pInitVxD->Handler.Init.TimeSeg2 = CAN_BS2_2TQ;
						break;
					case  125:
						pInitVxD->Handler.Init.Prescaler = 2;
						pInitVxD->Handler.Init.TimeSeg1 = CAN_BS1_13TQ;
						pInitVxD->Handler.Init.TimeSeg2 = CAN_BS2_2TQ;
						break;
					case  100:
						pInitVxD->Handler.Init.Prescaler = 10;
						pInitVxD->Handler.Init.TimeSeg1 = CAN_BS1_8TQ;
						pInitVxD->Handler.Init.TimeSeg2 = CAN_BS2_1TQ;
						break;
					case   50:
						pInitVxD->Handler.Init.Prescaler = 10;
						pInitVxD->Handler.Init.TimeSeg1 = CAN_BS1_6TQ;
						pInitVxD->Handler.Init.TimeSeg2 = CAN_BS2_1TQ;
						break;
					case   25:
						pInitVxD->Handler.Init.Prescaler = 10;
						pInitVxD->Handler.Init.TimeSeg1 = CAN_BS1_13TQ;
						pInitVxD->Handler.Init.TimeSeg2 = CAN_BS2_2TQ;
						break;
				}
				break;
			case 8000000:
				pInitVxD->Handler.Init.TimeSeg1 = CAN_BS1_6TQ;
				pInitVxD->Handler.Init.TimeSeg2 = CAN_BS2_1TQ;

				switch(Bitrate)
				{
					case 1000:
						pInitVxD->Handler.Init.Prescaler = 1;
						break;
					case  500:
						pInitVxD->Handler.Init.Prescaler = 2;
						break;
					default:
					case  250:
						pInitVxD->Handler.Init.Prescaler = 4;
						break;
					case  125:
						pInitVxD->Handler.Init.Prescaler = 8;
						break;
					case  100:
						pInitVxD->Handler.Init.Prescaler = 10;
						break;
					case   50:
						pInitVxD->Handler.Init.Prescaler = 20;
						break;
					case   25:
						pInitVxD->Handler.Init.Prescaler = 40;
						break;
				}
				break;
		}
	}

	if (HAL_CAN_Init(&pInitVxD->Handler) != HAL_OK){
		return RESULT_NULL;
	}

	if(HAL_CAN_ActivateNotification(&pInitVxD->Handler, CAN_IT_RX_FIFO0_MSG_PENDING|CAN_IT_ERROR|CAN_IT_BUSOFF) != HAL_OK){
		return RESULT_NULL;
	}

	if (HAL_CAN_Start(&pInitVxD->Handler) != HAL_OK){
		return RESULT_NULL;
	}

	pInitVxD->Bitrate = Bitrate;

	*ppVxD = pInitVxD;
	//oCAN_SetMaskFilter(pInitVxD, 0, 0, 0, 0);

	return RESULT_OK;
}
#endif
