/*
 * LoRa_RAK3172.h
 *
 *  Created on: Apr 29, 2025
 *      Author: JONE
 */

#ifndef INC_LORA_RAK3172_H_
#define INC_LORA_RAK3172_H_

#define LORA_RAK3172_VERSION		0.1

#include "ONE_ATCommend.h"
#include "ONE_Common.h"

/*
  --------------------------------------------------------------------------------------------------------------
    DR     SF      BW(kHz)     Bitrate     Range      TimeOnAir(ms)    Sensitivity(dBm)    Max.Payload(bytes)
  --------------------------------------------------------------------------------------------------------------
    DR0    SF12    125          290        14km       1125              -137                51
    DR1    SF11    125          440        11km        617              -135                51
    DR2    SF10    125          980         8km        329              -133                51
    DR3    SF9     125         1760         6km        185              -129               115
    DR4    SF8     125         3125         4km        103              -126               242
    DR5    SF7     125         5470         2km         56              -123               242
    DR6    SF7     250        11000        <1km         28              -120               RFU
  --------------------------------------------------------------------------------------------------------------

 */

#define RAK3172_TX_BUFF_MAXSIZE 400
#define RAK3172_RX_BUFF_MAXSIZE 800

typedef void (*DownlinkHandler_t)(uint8_t fPort, uint8_t *Payload, uint32_t SizeOfPayload);

typedef struct{
	oATCommend_t AT;

	struct{
		uint8_t TxBuffer[RAK3172_TX_BUFF_MAXSIZE];
		uint8_t RxBuffer[RAK3172_RX_BUFF_MAXSIZE];
		uint8_t ResponseBuffer[RAK3172_RX_BUFF_MAXSIZE];
	}Buffer;

	DownlinkHandler_t	DownlinkCallback;

	struct{
		uint8_t DataRate;
		char	DevEUI[17];
		char	AppEUI[17];
		char	AppKEY[33];
		char	Version[32];
	}Information;

	struct{
		uint8_t NJM;
		char CLASS;
		uint8_t BAND;
		uint8_t ADR;
		uint8_t LINKCHECK;
		uint8_t NWM;
		uint8_t LBT;
		uint8_t RX1DL;
		uint8_t RX2DL;
		uint8_t RETY;
		uint8_t LPMLVL;

		uint8_t JoinInterval;
		uint8_t JoinAttempts;
		uint8_t JoinMaxRetry;
	}Config;

	int16_t RSSI;
	int16_t SNR;

	struct{
		uint8_t IsOpening : 1;
		uint8_t IsOpened : 1;
		uint8_t IsJoined : 1;
		uint8_t IsJoinFail : 1;
		uint8_t IsBusy : 1;

		uint8_t IsSending : 1;
		uint8_t IsSendComformed : 1;	//+EVT:TX_DONE | SEND_CONFIRMED_OK
		uint8_t IsSendFailed : 1;
		uint8_t IsTxDone : 1;
		uint8_t IsTimeSyncEnabled : 1;
		uint8_t IsTimeSynced : 1; 		//+EVT:TIMEREQ_OK
		uint8_t IsSleeping : 1;
	}Status;
	
	uint8_t IsInitailzed : 1;
	uint8_t IsNotworking : 1;
	uint8_t IsReachable : 1;	// 이전 Join 성공 이력 (NJS 확인 여부 결정용, Close 시 유지)
}RAK3172_t;

extern RAK3172_t RAK3172Dev;

extern oResult_t RAK3172_JoinState();
extern oResult_t RAK3172_Join();
extern oResult_t RAK3172_GetLocalTime(oDateAndTime_t *pDT, oUtcOffsetMinute_t UtcOffset);
extern oResult_t RAK3172_Transmit(uint8_t Port, uint8_t *pData, uint32_t SizeofData, uint8_t Confirmed);
extern oResult_t RAK3172_Open(UART_HandleTypeDef *pUART, DownlinkHandler_t DownlinkCallback);
extern oResult_t RAK3172_Sleep();
extern oResult_t RAK3172_Wakeup();
extern oResult_t RAK3172_Close();
extern void RAK3172();

#endif /* INC_LORA_RAK3172_H_ */

/* History

2026-06-26 | v0.1
	- baseline (LoRa_RAK3172.h)
*/
