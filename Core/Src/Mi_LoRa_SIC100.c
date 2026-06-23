/*
 * Mi_LoRa_SIC100.c
 *
 * SIC100_2C 디바이스 LoRa 페이로드 인코더.
 * MiLoRa_Encode 가 디바이스별 데이터 타입 분기를 책임지며, 다른 모델은
 * Mi_LoRa_<MODEL>.c 에서 동일한 시그니처로 별도 구현한다.
 */
#include "Mi_IoT.h"
#include "Mi_LoRa.h"
#include "ONE_Serial.h"

#define LORA_PAYLOAD_SIZE				51

/* SIC100_2C 통합 패킷용 분할 카운트
 * Seq 0      : SeqHdr(1)+Time(6)+Analog(5)         = 12 byte (Analog 있을 때만)
 * Tilt Seq 0 : SeqHdr(1)+Time(6)+TiltType(1)+Temp(2) = 10 byte → sensor 영역 41 byte
 *              → Dual(6B): 6노드, Single(3B): 13노드
 * Tilt Seq N : SeqHdr(1)+Time(6)+TiltType(1)       = 8 byte → sensor 영역 43 byte
 *              → Dual: 7노드, Single: 14노드
 * Analog 없으면 Seq 0 = Tilt Seq 0, Seq N = Tilt Seq N */
#define LORA_SIC100_DUAL_FIRST_COUNT	6
#define LORA_SIC100_DUAL_NEXT_COUNT		7
#define LORA_SIC100_SINGLE_FIRST_COUNT	13
#define LORA_SIC100_SINGLE_NEXT_COUNT	14

/* TiltArray 의 마지막 non-zero entry 의 index+1 — 실제 측정된 센서 개수 추론 */
static uint8_t SIC100_GetTiltCount(IoTDataSIC100_2C_t *pData)
{
	uint8_t i;
	uint8_t count = 0;

	if(pData->TiltArray.Type == IoTSensorType_ArrayDualTilt){
		for(i=0; i<MIIOT_ARRAYSENSOR_MAX_COUNT; i++){
			if(pData->TiltArray.Dual[i].AxisX != 0 || pData->TiltArray.Dual[i].AxisY != 0){
				count = i + 1;
			}
		}
	}
	else if(pData->TiltArray.Type == IoTSensorType_ArraySingleTilt){
		for(i=0; i<MIIOT_ARRAYSENSOR_MAX_COUNT; i++){
			if(pData->TiltArray.Single[i].Axis != 0){
				count = i + 1;
			}
		}
	}

	return count;
}

/* 메일박스 데이터를 LoRa 페이로드 크기(51B)에 맞게 시퀀스별로 분할 인코딩
 * SeqeunceCount: 현재 시퀀스 번호 (0부터 시작)
 * 반환: 인코딩된 바이트 수 (0이면 더 이상 전송할 데이터 없음)
 *
 * 분할 단위:
 *   Tilt/Status/Operating → 단일 시퀀스 (분할 없음)
 *   SIC100_2C (통합)      → Seq0=Analog (있을 때), Seq 1~=Tilt sub-sequence */
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
		default:	// 미지원 데이터 타입
			SizeofData = 0;
			break;

		case IoTDataType_Tilt:		// 단일 시퀀스 전송 (분할 불필요)
		case IoTDataType_Status:
		case IoTDataType_Operating:
			if(SeqeunceCount == 0){
				memcpy(pOutData, pFrame, pPayload->DLC);
				SizeofData += pPayload->DLC;
			}
			else{
				SizeofData = 0;	// 시퀀스 0만 존재, 이후 전송 완료 신호
			}
			break;
		case IoTDataType_DataArray_Type2:	// 통합 패킷: 패킷 Seq byte 는 0=Analog / 1+=Tilt 로 고정
		{
			IoTDataSIC100_2C_t *pSrc = (IoTDataSIC100_2C_t *)pFrame;

			/* 메일 1건 동안 불변 — SqC=0 에서만 갱신해 후속 seq 호출 시 재계산 회피.
			 * 특히 totalSensors 는 30 entry 순회라서 캐시 효과가 큼. */
			static uint8_t hasAnalog;
			static uint8_t isDual;
			static uint8_t hasTilt;
			static uint8_t sensorSize;
			static uint8_t firstCnt;
			static uint8_t nextCnt;
			static uint8_t totalSensors;

			int tiltSeq;		// Tilt sub-stream 내부 시퀀스 번호 (-1 = analog, 0~ = tilt)
			uint8_t emittedSeq;	// 패킷 [0] 에 쓰일 Seq byte (0=Analog / 1+=Tilt)
			uint8_t sentSensors;
			uint8_t chunkCnt;

			if(SeqeunceCount == 0){
				uint8_t isSingle;
				hasAnalog = (pSrc->Analog.Type == IoTSensorType_mV || pSrc->Analog.Type == IoTSensorType_mA) ? 1 : 0;
				isDual = (pSrc->TiltArray.Type == IoTSensorType_ArrayDualTilt) ? 1 : 0;
				isSingle = (pSrc->TiltArray.Type == IoTSensorType_ArraySingleTilt) ? 1 : 0;
				hasTilt = (isDual || isSingle) ? 1 : 0;
				sensorSize = isDual ? MIIOT_SENSORDATA_SIZE_TILT_DUAL : MIIOT_SENSORDATA_SIZE_TILT_SINGLE;
				firstCnt = isDual ? LORA_SIC100_DUAL_FIRST_COUNT : LORA_SIC100_SINGLE_FIRST_COUNT;
				nextCnt = isDual ? LORA_SIC100_DUAL_NEXT_COUNT : LORA_SIC100_SINGLE_NEXT_COUNT;
				totalSensors = hasTilt ? SIC100_GetTiltCount(pSrc) : 0;
			}

			// 외부 SeqeunceCount → 내부 tiltSeq / 패킷 emittedSeq 매핑
			//   hasAnalog 시: SqC 0=Analog(Seq=0), SqC 1=Tilt0(Seq=1), SqC 2=Tilt1(Seq=2), ...
			//   noAnalog  시: SqC 0=Tilt0(Seq=1),  SqC 1=Tilt1(Seq=2),  SqC 2=Tilt2(Seq=3), ...
			if(hasAnalog && SeqeunceCount == 0){
				// === Analog 시퀀스 (Seq byte = 0) ===
				*(pOutData++) = SeqeunceCount;	// 0
				SizeofData++;

				memcpy(pOutData, &pSrc->Time, MIIOT_IOTDATA_SIZE_TIME);
				pOutData += MIIOT_IOTDATA_SIZE_TIME;
				SizeofData += MIIOT_IOTDATA_SIZE_TIME;

				memcpy(pOutData, &pSrc->Analog, MIIOT_SENSORDATA_SIZE_DATAARRAY);
				SizeofData += MIIOT_SENSORDATA_SIZE_DATAARRAY;
				break;
			}

			// === Tilt 시퀀스 (Seq byte = 1, 2, 3, ...) ===
			if(!hasTilt || totalSensors == 0){
				SizeofData = 0;	// Tilt 없음 → 전송 종료
				break;
			}

			// SqC → tiltSeq 매핑: hasAnalog 면 SqC 1 부터 Tilt 시작, noAnalog 면 SqC 0 부터
			// 패킷 Seq byte 규칙: 0~49 = Analog 영역, 50~100 = Tilt 영역
			tiltSeq = hasAnalog ? (int)(SeqeunceCount - 1) : (int)SeqeunceCount;
			emittedSeq = (uint8_t)(50 + tiltSeq);	// Tilt sub-seq 0 → 50, sub-seq 1 → 51, ...

			sentSensors = (tiltSeq == 0) ? 0 : (firstCnt + (tiltSeq - 1) * nextCnt);

			if(sentSensors >= totalSensors){
				SizeofData = 0;	// 모든 센서 전송 완료
				break;
			}

			// [Seq 헤더] 항상 1 이상
			*(pOutData++) = emittedSeq;
			SizeofData++;

			// [Time] 6 byte
			memcpy(pOutData, &pSrc->Time, MIIOT_IOTDATA_SIZE_TIME);
			pOutData += MIIOT_IOTDATA_SIZE_TIME;
			SizeofData += MIIOT_IOTDATA_SIZE_TIME;

			// [Tilt 타입]
			*(pOutData++) = (uint8_t)pSrc->TiltArray.Type;
			SizeofData++;

			if(tiltSeq == 0){
				// [Temp] 첫 Tilt 시퀀스에만 온도 포함
				*((IoTSensorTemp_t *)pOutData) = pSrc->TiltArray.Temperature;
				pOutData += MIIOT_SENSORDATA_SIZE_TEMP;
				SizeofData += MIIOT_SENSORDATA_SIZE_TEMP;

				chunkCnt = MATH_MIN(firstCnt, totalSensors);
			}
			else{
				chunkCnt = MATH_MIN(nextCnt, totalSensors - sentSensors);
			}

			// [Sensors]
			if(chunkCnt > 0){
				uint8_t *pSensorSrc = isDual ? (uint8_t *)&pSrc->TiltArray.Dual[sentSensors] : (uint8_t *)&pSrc->TiltArray.Single[sentSensors];

				memcpy(pOutData, pSensorSrc, chunkCnt * sensorSize);
				SizeofData += chunkCnt * sensorSize;
			}
			break;
		}
	}

	return SizeofData;
}
