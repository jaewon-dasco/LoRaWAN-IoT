/*
 * Mi_LoRa_SIA100SD.c
 *
 * SIA100_SD 디바이스 LoRa 페이로드 인코더.
 * 사용 데이터 타입: Tilt / Status / Operating (단일 시퀀스 전송).
 */
#include "Mi_IoT.h"
#include "Mi_LoRa.h"
#include "ONE_Serial.h"

#define LORA_PAYLOAD_SIZE			51

/* 메일박스 데이터를 LoRa 페이로드 크기(51B)에 맞게 시퀀스별로 분할 인코딩
 * SeqeunceCount: 현재 시퀀스 번호 (0부터 시작)
 * 반환: 인코딩된 바이트 수 (0이면 더 이상 전송할 데이터 없음)
 *
 * 분할 단위:
 *   Tilt/Status/Operating → 단일 시퀀스 (분할 없음) */
uint32_t MiLoRa_Encode(IoT_MailboxItem_t *pMail, uint8_t SeqeunceCount, uint8_t *pOutData)
{
	uint32_t SizeofData = 0;

	if(pMail == NULL){
		return 0;
	}

	IoT_DataPacket_t *pPayload = &pMail->Payload;
	uint8_t *pFrame = (uint8_t *)&pPayload->Frame;

	switch(pPayload->TypeOfData)
	{
		default:
			SizeofData = 0;
			break;

		case IoTDataType_Tilt:
		case IoTDataType_Status:
		case IoTDataType_Operating:
			if(SeqeunceCount == 0){
				memcpy(pOutData, pFrame, pPayload->DLC);
				SizeofData += pPayload->DLC;
			}
			else{
				SizeofData = 0;
			}
			break;
	}

	return SizeofData;
}

/* History

2026-06-26 | v0.1
	- baseline (Mi_LoRa_SIA100SD.c)
*/
