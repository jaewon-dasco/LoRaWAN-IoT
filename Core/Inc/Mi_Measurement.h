/*
 * Mi_Sensor.h
 *
 *  Created on: Dec 9, 2024
 *      Author: JONE
 */

#ifndef INC_MI_MEASUREMENT_H_
#define INC_MI_MEASUREMENT_H_

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
