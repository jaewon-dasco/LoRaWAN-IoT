/*
 * MiLoRa.h
 *
 *  Created on: Dec 2, 2024
 *      Author: JONE
 */

#ifndef INC_MI_LORA_H_
#define INC_MI_LORA_H_

#include "Mi_IoT.h"
#include "Mi_Main.h"

#include "LoRa_RAK3172.h"

#define MILORA_QOS_MAX					0xFF
/* Join 실패 시 backoff: 5s, 10s, 20s, 40s, 80s, 160s, 300s(cap) — 5*(2^(n-1)), max 5min */
#define MILORA_OPEN_BACKOFF_MS			SECOND_TO_MS(MiLoRa_OpenFailCount >= 7 ? 300 : (5 << (MiLoRa_OpenFailCount - 1)))
#define MILORA_OPEN_REMAINING_TIME		((MiLoRa_OpenFailTimestamp == 0 || MiLoRa_OpenFailCount == 0) ? 0 : oTMR_CountDown(&MiLoRa_OpenFailTimestamp, MILORA_OPEN_BACKOFF_MS, TICKBASE_SYSTICK))

extern RAK3172_t *pLoRaDevice;

extern uint8_t MiLoRa_IsPowerOn;
extern uint8_t MiLoRa_IsBusy;
extern uint8_t MiLoRa_IsOpen;
extern uint8_t MiLoRa_IsReachable;
extern uint32_t MiLoRa_OpenFailCount;
extern uint8_t MiLoRa_IsSleep;


extern uint32_t MiLoRa_OpenFailTimestamp;
extern uint32_t MiLoRa_RadioFailCount;
extern uint32_t MiLoRa_TransmitTimestamp;
extern uint32_t MiLoRa_ReceiveTimestamp;
extern uint32_t MiLoRa_OpenTimestamp;
extern uint32_t MiLoRa_CloseTimestamp;

extern void MiLoRa_ReceiveCallback(uint32_t Address, uint16_t DLC, uint8_t *Payload);
extern oResult_t MiLoRa_SendMailbox(IoT_MailboxItem_t *pMail);
extern void MiLoRa_Control();
extern void MiLoRa_MailBoxTransmitScheduler();
extern oResult_t MiLoRa_UpdateTime();
extern oResult_t MiLoRa_UpdateInformaiton();
extern oResult_t MiLoRa_Open();
extern oResult_t MiLoRa_Close();
extern void MiLoRa(UART_HandleTypeDef *pUART);

#endif /* INC_MI_LORA_H_ */
