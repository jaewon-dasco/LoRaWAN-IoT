/*
 * Mi_Sensor.h
 *
 *  Created on: Dec 9, 2024
 *      Author: JONE
 */

#ifndef INC_MI_MEASUREMENT_H_
#define INC_MI_MEASUREMENT_H_

#define MI_MEASUREMENT_VERSION		0.1

#include "ONE_Common.h"
#include "Mi_LoRa.h"
#include "Mi_IoT.h"

#define MEASUREMENT_PORT_MAXCOUNT		3
#define MEASUREMENT_CHANNEL_MAXCOUNT 	9

/* Measurement_SamplingADCs 배열 인덱스 (총 4개) */
#define SAMPLINGADCS_INDEX_SUPPLY		0
#define SAMPLINGADCS_INDEX_MV			1
#define SAMPLINGADCS_INDEX_MA			2
#define SAMPLINGADCS_INDEX_DIFF			3
#define SAMPLINGADCS_COUNT				4

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

extern oResult_t Analog_InternalAdc(int8_t Channel, double *pAnalog, uint8_t ReadCount);
extern oResult_t Analog_ExternalAdc(int8_t Channel, IoTSensorType_t TypeOfSensor, double *pAnalog, uint8_t ReadCount);
extern void Measurement_PowerOn(int8_t Port);
extern oResult_t Measurement_Sensor(IoT_DataPacket_t *pPacket, IoTChannelConfig_t *pConfig);
extern oResult_t Measurement_Supply(uint32_t Count);
extern oResult_t Measurement_SamplingADCs(double *pValues);

#endif /* INC_MI_MEASUREMENT_H_ */

/* History

2026-06-26 | v0.1
	- baseline (Mi_Measurement.h)
*/
