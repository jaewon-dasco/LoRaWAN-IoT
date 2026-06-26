/*
 * Mi_LoRa_SIM100.c
 *
 * SIM100_6C 디바이스 LoRa 페이로드 인코더.
 * 사용 데이터 타입: DataArray_Type1 / Status / Operating.
 */
#include "Mi_IoT.h"
#include "Mi_LoRa.h"
#include "ONE_Serial.h"

#define LORA_PAYLOAD_SIZE			51
#define LORA_DATAARRAY_SPLIT_COUNT	7

/* 메일박스 데이터를 LoRa 페이로드 크기(51B)에 맞게 시퀀스별로 분할 인코딩
 * SeqeunceCount: 현재 시퀀스 번호 (0부터 시작)
 * 반환: 인코딩된 바이트 수 (0이면 더 이상 전송할 데이터 없음)
 *
 * 분할 단위:
 *   Tilt/Status/Operating → 단일 시퀀스 (분할 없음)
 *   DataArray_Type1       → 7 item씩 분할 */
uint32_t MiLoRa_Encode(IoT_MailboxItem_t *pMail, uint8_t SeqeunceCount, uint8_t *pOutData)
{
	uint32_t i;
	uint32_t SizeofSended = 0;
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

		case IoTDataType_DataArray_Type1:
			SizeofSended = MIIOT_IOTDATA_SIZE_TIME + (SeqeunceCount * MIIOT_SENSORDATA_SIZE_DATAARRAY * LORA_DATAARRAY_SPLIT_COUNT);

			if(SizeofSended >= pPayload->DLC){
				SizeofData = 0;
			}
			else{
				*(pOutData++) = SeqeunceCount;
				SizeofData++;

				memcpy(pOutData, pFrame, MIIOT_IOTDATA_SIZE_TIME);
				pOutData += MIIOT_IOTDATA_SIZE_TIME;
				pFrame += MIIOT_IOTDATA_SIZE_TIME;
				SizeofData += MIIOT_IOTDATA_SIZE_TIME;

				pFrame += SeqeunceCount * MIIOT_SENSORDATA_SIZE_DATAARRAY * LORA_DATAARRAY_SPLIT_COUNT;

				for(i=0; i < LORA_DATAARRAY_SPLIT_COUNT && (i+(SeqeunceCount*LORA_DATAARRAY_SPLIT_COUNT)) < MIIOT_PAYLOAD_TO_DATAARRAY_COUNT(pPayload->DLC); i++){
					*((IoTDataArrayItem_t *)pOutData) = *((IoTDataArrayItem_t *)pFrame);
					pOutData += MIIOT_SENSORDATA_SIZE_DATAARRAY;
					pFrame += MIIOT_SENSORDATA_SIZE_DATAARRAY;
					SizeofData += MIIOT_SENSORDATA_SIZE_DATAARRAY;
				}
			}
			break;
	}

	return SizeofData;
}

/* History

2026-06-26 | v0.1
	- baseline (Mi_LoRa_SIM100.c)
*/
