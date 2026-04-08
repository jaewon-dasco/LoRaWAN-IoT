/*
 * Mi_Sensor.h
 *
 *  Created on: Dec 9, 2024
 *      Author: JONE
 */

#ifndef INC_MI_MEASUREMENT_H_
#define INC_MI_MEASUREMENT_H_

#include "ONE_Common.h"
#include "Mi_LoRa.h"
#include "Mi_IoT.h"

#define MEASUREMENT_CHANNEL_MAXCOUNT 10

typedef enum{
	MeasureError_Null = 0,
	MeasureError_OutOfRange,
	MeasureError_Timeout,
	MeasureError_Disconnected,

	MeasureError_Okay = 0xFF,
}Measure_ErrorBit_t;

typedef struct{
	uint32_t 	Timestamp;
	uint8_t 	TryCount;
	uint8_t 	ErrorBit;
}Measure_Diagnostics_t;

extern oResult_t Measurement_Sensor(IoT_DataPacket_t *pPacket);
extern oResult_t Measurement_Supply(uint32_t Count);

#endif /* INC_MI_MEASUREMENT_H_ */
