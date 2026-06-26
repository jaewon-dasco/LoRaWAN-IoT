/*
 * MiCAN.h
 *
 *  Created on: Dec 3, 2024
 *      Author: JONE
 */

#ifndef INC_ONE_CAN_H_
#define INC_ONE_CAN_H_

#define ONE_CAN_VERSION		0.1

#include "ONE_Common.h"
#include "main.h"

#define CAN_MESSAGE_INITIALIZER(Id,Dlc,Extd)		{Id,Dlc,{0,0,0,0,0,0,0,0},0,Extd,0}
#define CAN_CALLBACK_MAXCOUNT						5

typedef struct CANMessageTypeDef{
	uint32_t ID;
	uint8_t DLC;
	uint8_t Data[8];
	uint32_t Timestamp;

	uint8_t IsEextended;
	uint8_t IsNew;
	uint8_t Checker;
} oCANMessage_t;

typedef void (*oCANCallback_t) (oCANMessage_t *Message, uint32_t Argument);

typedef struct{
	uint32_t ID;
	uint32_t Mask;
	uint32_t Argument;
	oCANCallback_t Callback;
} oCANMessageCallback_t;

typedef struct {
#ifdef CAN_ID_STD
	CAN_HandleTypeDef 		Handler;
#else
	uint32_t				Handler;
#endif
	uint16_t 				Bitrate;
	oCANMessageCallback_t 	CallbackList[CAN_CALLBACK_MAXCOUNT];
} oCANVxD_t;



extern void oCAN_ResetReceiveCallback(oCANVxD_t *pVxD, oCANCallback_t Callback);
extern void oCAN_SetReceiveCallback(oCANVxD_t *pVxD, oCANCallback_t Callback, uint32_t Argument);
extern oResult_t oCAN_SetMaskReceiveCallback(oCANVxD_t *pVxD, uint32_t ID, uint32_t Mask, uint8_t Extended, oCANCallback_t Callback, uint32_t Argument);
extern oResult_t oCAN_Transmit(oCANVxD_t *pVxD, oCANMessage_t *Message);
extern oResult_t oCAN_Transmitter(oCANVxD_t *pVxD, oCANMessage_t *Message, uint32_t Interval);
extern oResult_t oCAN_ResetFilter(oCANVxD_t *pVxD, uint8_t Bank);
extern oResult_t oCAN_SetIdFilter(oCANVxD_t *pVxD, uint8_t Bank, uint32_t IDs[], uint8_t Count, uint8_t Extended);
extern oResult_t oCAN_SetMaskFilter(oCANVxD_t *pVxD, uint8_t Bank, uint32_t Id, uint32_t Mask, uint8_t Extended);
extern oResult_t oCAN_Close(oCANVxD_t *pVxD);
extern void* oCAN_GetHandler(oCANVxD_t *pVxD);
extern oResult_t oCAN_IsMailBoxFull(oCANVxD_t *pVxD);
extern oResult_t oCAN_IsOpen(oCANVxD_t *pVxD);
extern oResult_t oCAN_IsError(oCANVxD_t *pVxD);
extern oResult_t oCAN_Open(oCANVxD_t **ppVxD, CAN_TypeDef *pCAN, uint16_t Bitrate);

#endif /* INC_ONE_CAN_H_ */

/* History

2026-06-26 | v0.1
	- baseline (ONE_CAN.h)
*/
