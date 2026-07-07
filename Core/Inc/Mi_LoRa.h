/*
 * MiLoRa.h
 *
 *  Created on: Dec 2, 2024
 *      Author: JONE
 */

#ifndef INC_MI_LORA_H_
#define INC_MI_LORA_H_

#define MI_LORA_VERSION		0.11

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

extern uint32_t MiLoRa_Encode(IoT_MailboxItem_t *pMail, uint8_t SeqeunceCount, uint8_t *pOutData);
extern oResult_t MiLoRa_SendMailbox(IoT_MailboxItem_t *pMail);
extern void MiLoRa_Control();
extern void MiLoRa_MailBoxTransmitScheduler();
extern oResult_t MiLoRa_Open();
extern oResult_t MiLoRa_Close();
extern void MiLoRa(UART_HandleTypeDef *pUART);

#endif /* INC_MI_LORA_H_ */

/* History

2026-06-26 | v0.1
	- baseline (Mi_LoRa.h)
2026-07-03 | v0.11
	- MiLoRa(pUART) NULL 가드 추가: pUART==NULL 시 즉시 return (SIA100_VB 동기화)
	- 미사용 extern 선언 제거: MiLoRa_ReceiveCallback / MiLoRa_UpdateTime / MiLoRa_UpdateInformaiton
	- Mi_LoRa.c: 미사용 주석 라인 정리
*/
