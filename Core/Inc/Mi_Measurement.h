/*
 * Mi_Sensor.h
 *
 *  Created on: Dec 9, 2024
 *      Author: JONE
 */

#ifndef INC_MI_MEASUREMENT_H_
#define INC_MI_MEASUREMENT_H_

#define MI_MEASUREMENT_VERSION		0.2

#include "Mi_LoRa.h"
#include "Mi_IoT.h"
#include "ONE_Common.h"

#define MEASUREMENT_CHANNEL_MAXCOUNT 	2
#define MEASUREMENT_NODE_MAXCOUNT 		250

#pragma pack(1)
#pragma pack()

//static const uint8_t IPIRequestData[8] = {0x1D,0x00,0x01,0x00,0x00,0x00,0x00,0x00}; //X axis
static const uint8_t IPIRequestData[8] = {0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00}; //X axis

extern uint32_t Measure_ScanIdList[MEASUREMENT_NODE_MAXCOUNT];

extern oResult_t Measurement_Scan(uint8_t Channel, uint32_t StartId, uint32_t EndId, uint8_t *pRequestData, uint8_t DLC, uint8_t Try);
extern oResult_t Measurement_Sensor(IoT_DataPacket_t *pPacket);
extern oResult_t Measurement_Supply(uint8_t Count);

#endif /* INC_MI_MEASUREMENT_H_ */

/* History

2026-06-26 | v0.1
	- baseline (Mi_Measurement.h)
2026-07-07 | v0.2
	- 실패 마커 규약 통일 (SIM/SIV와 일치):
	  · Measurement_ArraySensorDual/Single: TiltArray.Type / CountOfArraySensor 설정을
	    RESULT_OK 분기 → EXIT 직전(result != RESULT_RUN)으로 이동 (v0.98 반영)
	  · Measurement_Analog: Analog.Type 설정을 RESULT_OK 조건 밖으로 이동 — 실패 시에도
	    Type = pConfig->TypeOfSensor 유지, Data는 초기값 0 (v1.11 반영)
	- 효과: 측정 실패 채널도 서버 디코딩 시 슬롯 0(-180) 표시, 빈 채널로 인한 encode 실패 해소
	- Measurement_Scan 인덱스 정합화:
	  · 조회 인덱스 [SensorStartId+IdIndex] → [IdIndex]로 교정
	    (저장은 [Message->ID - SensorStartId]로 0-based이므로 조회도 동일 index 사용)
	  · Measurement_RecieveCallback_Scan 경계 off-by-one 수정:
	    Message->ID > (SensorStartId+MEASUREMENT_NODE_MAXCOUNT) → >= 로 변경
	    (배열 크기(250)와 동일한 오프셋을 통과시켜 [250] 쓰기 발생하던 1칸 밖 접근 차단)
	  · Measurement_Scan case 4 조회 루프 OOB read 차단:
	    종료 조건에 || IdIndex >= MEASUREMENT_NODE_MAXCOUNT 추가
	    (실호출 Measurement_Scan(ch, 1, 255, ...) 시 IdIndex 250~254에서
	     Measure_ScanIdList[IdIndex] 읽기가 배열 밖으로 넘어가던 문제 해소)
	- Measurement_RecieveCallback_ArrayDual/ArraySingle 경계 off-by-one 수정:
	  · (ReceivedId - SensorStartId) <= MIIOT_ARRAYSENSOR_MAX_COUNT → < 로 변경
	    (MAX_COUNT=30, 배열 [30] 유효 0~29 — <= 는 인덱스 30 통과 시
	     TiltArray.Dual[30].AxisX/Y 또는 Single[30].Axis 쓰기로 인접 필드
	     CountOfArraySensor·Analog 영역 오염 유발)
*/
