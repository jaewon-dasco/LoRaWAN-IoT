/*
 * MiLoRa.h
 *
 *  Created on: Dec 2, 2024
 *      Author: JONE
 */

#ifndef INC_MI_LORA_H_
#define INC_MI_LORA_H_

#define MI_LORA_VERSION		0.13

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
2026-07-08 | v0.12
	- MiLoRa_SendMailbox inter-sequence duty cycle 보장 (case 2, line 189):
	  · 이전: SendTryCount==0 시 즉시 진행 → 연속 시퀀스 back-to-back 송신으로 EU868 duty cycle 위반 위험
	  · 수정: (SendSqcCount==0 && SendTryCount==0) 첫 시퀀스만 즉시, 이후 5s * MAX(SendTryCount,1) 대기
2026-07-08 | v0.13
	- MiLoRa_SendMailbox 무한 재전송 루프 수정 (설계 의도: seq당 QoS회 시도 → skip,
	  누적 10회 실패마다 재부팅, 마지막 seq까지 소진 시 mail 삭제):
	  · 결함: 전송 영구 실패 시 RadioFailCount(>5) 재부팅이 SendErrorCount(10) 소진보다 항상
	    선행 → PAUSE에서 Step=0 리셋 → ErrorCount 소실 → seq skip 불가 → seq 0 무한 반복
	  · PAUSE 시 SendMailboxStep 유지 (Step=0 삭제) — 재개 시 중단 지점부터, ErrorCount 축적 유지
	  · 메일 신원 감지 신설 (pLastMail 포인터 + Timestamp 쌍) — 새 메일이면 Step=0 초기화
	    (Sort 물리 재배열·mailbox overflow 슬롯 재사용 시 이전 메일 상태 오염 방지)
	  · FAULT(무응답 10회) 시 현재 seq 포기 + SqcCount 전진 후 RESULT_FAULT 반환,
	    EXIT의 FAULT 경로 Step=0 삭제 — 재부팅 후 다음 seq부터 재개 (같은 seq 무한 FAULT 방지)
	  · MiLoRa_Control 재부팅 임계 >5 → >=10 (QoS 5 × 2 seq) — ErrorCount(5)가 임계(10)보다
	    먼저 소진되어 seq skip 정상 동작, 시퀀스 2개 실패 주기로 재부팅
	  · seq당 QoS 재시도 상한 10 → 30 (QoS_MAX 메일의 seq당 최대 시도 확대,
	    Step 유지로 재부팅 사이에도 ErrorCount 연속 축적되므로 유한 종결 유지)
	  · NULL 메일 가드 즉시 return으로 변경 — 기존 goto EXIT는 EXIT 블록의
	    pMail->Result 대입에서 NULL 역참조 (잠복 크래시)
*/
