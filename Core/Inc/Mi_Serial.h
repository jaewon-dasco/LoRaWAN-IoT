/*
 * Mi_Serial.h
 *
 *  Created on: Feb 28, 2025
 *      Author: JONE
 */

#ifndef INC_MI_SERIAL_H_
#define INC_MI_SERIAL_H_

#define MI_SERIAL_VERSION		0.11

#include "Mi_IoT.h"
#include "Mi_Main.h"
#include "ONE_Serial.h"

#define MISERIAL_TX_BUFFER_SIZE		1000
#define MISERIAL_RX_BUFFER_SIZE		1000

extern uint8_t MiSerial_TxBuffer[MISERIAL_TX_BUFFER_SIZE];
extern uint8_t MiSerial_RxBuffer[MISERIAL_RX_BUFFER_SIZE];
extern oSerialHandler_t MiSerial_Handler;

extern uint8_t MiSerial_SensorSamplingProgress;
extern uint8_t MiSerial_StopSensorCmd;
extern uint8_t MiSerial_SamplingSensorCmd;
extern uint8_t MiSerial_SamplingADCsTrig;

extern void MiSerial_Process();
extern oResult_t MiSerial_Specific(char *Message);
extern void MiSerial_PrintChannelConfig(int32_t Channel, IoTParameter_t *pParameter);
extern void MiSerial_PrintSensorData(IoT_DataPacket_t *pPacket);
extern void MiSerial_PrintResponse(char *Type, char *Message);
extern oResult_t MiSerial_SetChannelConfig(char *Message, IoTParameter_t *pParameter, uint8_t Channel);

extern void MiSerial(UART_HandleTypeDef *pUART);

#endif /* INC_MI_SERIAL_H_ */

/* History

2026-06-26 | v0.1
	- baseline (Mi_Serial.h)
2026-07-07 | v0.11
	- MiSerial_UpdateSensorCmd → MiSerial_SamplingSensorCmd 명명 정합화
	  · MiIoT_SamplingSensor 시퀀스와 명칭 일치 (Get/SensorData 트리거 플래그)
	  · Mi_Serial.h extern 선언 + Mi_Serial.c 정의/사용처 2곳 갱신
*/
