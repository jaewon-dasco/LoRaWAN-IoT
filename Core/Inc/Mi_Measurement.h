/*
 * Mi_Measurement.h
 *
 *  Created on: Dec 9, 2024
 *      Author: JONE
 *
 *  통합 측정 (가속도 → 각도 보상 → 3축 주파수 → 3축 PPV)
 */

#ifndef INC_MI_MEASUREMENT_H_
#define INC_MI_MEASUREMENT_H_

#define MI_MEASUREMENT_VERSION		0.1

#include "Mi_LoRa.h"
#include "Mi_IoT.h"
#include "ONE_Common.h"
#include "ONE_RingBuffer.h"
#include "MEMS_ADXL355BEZRL7.h"
#include "TEMP_TMP1075.h"

#define MEASUREMENT_CHANNEL_MAXCOUNT 1

#define MEASURE_HISTORY_SIZE  60       /* 1 sample/sec × 60s = 1분 링버퍼 (22 B × 60 = 1,320 B) */

#pragma pack(1)

// Vibration PPV/Freq Result (axis-wise + composite PVS)
typedef struct {
	float   PPV_X;         // mm/s
	float   PPV_Y;         // mm/s
	float   PPV_Z;         // mm/s
	float   PVS;           // mm/s (Peak Vector Sum = max(sqrt(vx²+vy²+vz²)))
	float 	Freq_X;        // Hz
	float 	Freq_Y;        // Hz
	float	Freq_Z;        // Hz
} Measure_Vibrating_t;

#pragma pack()

extern ADXL355_t           ADXL355;
extern TMP1075_t           TMP1075;
extern oRPY_t              Measure_RawAngle;
extern oRPY_t              Measure_CalibratedAngle;
extern double              Measure_Temperature;
extern Measure_Vibrating_t  Measure_Vibrating;       /* 현재 1초 윈도우 누적 중 (live) */
extern Measure_Vibrating_t  Measure_Vibrating_Last;  /* 마지막 완료된 1초 윈도우 결과 (안정 snapshot) */
extern oRingBuffer_t        Measure_VibratingHistoryRB;    /* 1분 링버퍼 — Count는 byte 단위 (sizeof(Measure_Vibrating_t)로 나눠 record 수 산출) */

extern oResult_t Measurement_Init(void);
extern oResult_t Measurement(void);
extern oResult_t Measurement_Sensor(IoT_DataPacket_t *pPacket);
extern oResult_t Measurement_Supply(uint8_t Count);

#endif /* INC_MI_MEASUREMENT_H_ */

/* History

2026-06-26 | v0.1
	- baseline (Mi_Measurement.h)
*/
