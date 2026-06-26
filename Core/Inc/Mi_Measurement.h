/*
 * Mi_Sensor.h
 *
 *  Created on: Dec 9, 2024
 *      Author: JONE
 */

#ifndef INC_MI_MEASUREMENT_H_
#define INC_MI_MEASUREMENT_H_

#define MI_MEASUREMENT_VERSION		0.1

#include "Mi_LoRa.h"
#include "Mi_IoT.h"
#include "ONE_Common.h"
#include "MEMS_ADXL355BEZRL7.h"
#include "TEMP_TMP1075.h"

#define MEASUREMENT_CHANNEL_MAXCOUNT 1

#pragma pack(1)

#pragma pack()

extern ADXL355_t ADXL355;
extern TMP1075_t TMP1075;
extern oRPY_t Measure_RawAngle;
extern oRPY_t Measure_CalibratedAngle;

extern oResult_t Measurement_ReadAngle(oRPY_t *pRPY);
extern oResult_t Measurement_Sensor(IoT_DataPacket_t *pPacket);
extern oResult_t Measurement_Supply(uint8_t Count);

#endif /* INC_MI_MEASUREMENT_H_ */

/* History

2026-06-26 | v0.1
	- baseline (Mi_Measurement.h)
*/
