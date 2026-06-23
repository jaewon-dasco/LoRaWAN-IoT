/*
 * Mi_Serial.h
 *
 *  Created on: Feb 28, 2025
 *      Author: JONE
 */

#ifndef INC_MI_SERIAL_H_
#define INC_MI_SERIAL_H_

#include "Mi_IoT.h"
#include "Mi_Main.h"
#include "ONE_Serial.h"

#define MISERIAL_TX_BUFFER_SIZE		1
#define MISERIAL_RX_BUFFER_SIZE		1000

extern uint8_t MiSerial_TxBuffer[MISERIAL_TX_BUFFER_SIZE];
extern uint8_t MiSerial_RxBuffer[MISERIAL_RX_BUFFER_SIZE];
extern oSerialHandler_t MiSerial_Handler;

extern uint8_t MiSerial_SensorSamplingProgress;
extern uint8_t MiSerial_StopSensorCmd;
extern uint8_t MiSerial_UpdateSensorCmd;
extern uint8_t MiSerial_SamplingADCsTrig;

extern void MiSerial_Process();
extern oResult_t MiSerial_Specific(char *Message);
extern void MiSerial_PrintChannelConfig(int32_t Channel, IoTParameter_t *pParameter);
extern void MiSerial_PrintSensorData(IoT_DataPacket_t *pPacket);
extern void MiSerial_PrintResponse(char *Type, char *Message);
extern oResult_t MiSerial_SetChannelConfig(char *Message, IoTParameter_t *pParameter, uint8_t Channel);

extern void MiSerial(UART_HandleTypeDef *pUART);

#endif /* INC_MI_SERIAL_H_ */
