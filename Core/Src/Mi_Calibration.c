/*
 * Mi_Calibration.c
 *
 *  Created on: Aug 6, 2025
 *      Author: JONE
 */
#include "ONE_CAN.h"
#include "Mi_IoT.h"
#include "Mi_Serial.h"
#include "Mi_Main.h"
#include "Mi_Storage.h"
#include "Mi_Measurement.h"
#include "Mi_Calibration.h"

void MiCalibration_CANCallback(oCANMessage_t *Message);

uint8_t CalibrationMode;
uint8_t IsCalibrationRun;
uint8_t IsCanOpen;
uint8_t CanChannel;
uint32_t CalibrationTimer;

void CanToRS232_Deserialize()
{
	static uint8_t PacketBuffer[20] = {0,};
	static uint8_t CanToRS232_DecodeStep = 0;
	static uint8_t CanToRS232_DLC = 0;
	static uint8_t CanToRS232_PID = 0;
	static uint8_t CanToRS232_DataIndex = 0;
	static uint32_t CanToRS232_CheckSum = 0;
	static uint32_t CanToRS232_BufferIndex = 0;

	oCANMessage_t Message = CAN_MESSAGE_INITIALIZER(0,0,0);
	uint8_t Data;

	if(MiSerial_Handler.SizeOfRxBuffer <= 0){
		return;
	}

	uint8_t cnt=0;
	uint32_t IndexOfRxLast = MiSerial_Handler.pUART->RxXferSize - MiSerial_Handler.pUART->hdmarx->Instance->CNDTR;

	while(IndexOfRxLast != CanToRS232_BufferIndex && ++cnt < 68)
	{
		Data = MiSerial_Handler.pRxBuffer[CanToRS232_BufferIndex];
		CanToRS232_BufferIndex = (CanToRS232_BufferIndex+1) % MiSerial_Handler.SizeOfRxBuffer;

		switch(CanToRS232_DecodeStep)
		{
			case 0:
				if(Data == 0xF0){
					CanToRS232_DecodeStep++;
				}
				break;
			case 1:
				if (Data == 0x84 || Data == 0x86){
					CanToRS232_PID = Data;
					CanToRS232_CheckSum = Data;
					CanToRS232_DecodeStep++;
				}
				else{
					CanToRS232_DecodeStep = 0;
				}
				break;
			case 2:
				if (Data >= 5 && Data <= sizeof(PacketBuffer)){
					CanToRS232_DLC = Data;
					CanToRS232_DataIndex = 0;
					memset(&PacketBuffer, 0, sizeof(PacketBuffer));

					CanToRS232_CheckSum += Data;
					CanToRS232_DecodeStep++;
				}
				else{
					CanToRS232_DecodeStep = 0;
				}
				break;
			case 3:
				PacketBuffer[CanToRS232_DataIndex++] = Data;
				CanToRS232_CheckSum += Data;

				if (CanToRS232_DataIndex >= CanToRS232_DLC){
					CanToRS232_DecodeStep++;
				}
				break;
			case 4:
				if ((uint8_t)CanToRS232_CheckSum == Data){
					CanToRS232_DecodeStep++;
				}
				else{
					CanToRS232_DecodeStep = 0;
				}
				break;
			case 5:
				if (Data == 0xE0){
					memcpy(&Message.Data, &PacketBuffer[4], 8);

					Message.ID = *((uint32_t *)&PacketBuffer[0]);
					Message.DLC = CanToRS232_DLC-4;
					Message.IsEextended = CanToRS232_PID == 0x86;
					Message.Timestamp = oTMR_GetTick(TICKBASE_SYSTICK);

					MiCalibration_CANCallback(&Message);
				}

				CanToRS232_DecodeStep = 0;
				break;
		}
	}
}

void CanToRS232_Serialize(oCANMessage_t *Message)
{
	static uint8_t PacketBuffer[20] = {0,};

	memset(&PacketBuffer, 0, sizeof(PacketBuffer));

	PacketBuffer[0] = 0xF0;
	PacketBuffer[1] = 0x85;
	PacketBuffer[2] = Message->DLC + 4;

	memcpy(&PacketBuffer[3], &Message->ID, 4);
	memcpy(&PacketBuffer[7], &Message->Data, Message->DLC);

	for(int i=1; i<Message->DLC + 7; i++){
		PacketBuffer[Message->DLC + 7] += PacketBuffer[i];
	}

	PacketBuffer[Message->DLC + 8] = 0xE0;

	oSerial_Write(&MiSerial_Handler, (char *)&PacketBuffer, Message->DLC + 9);
}

void MiCalibration_CANCallback(oCANMessage_t *Message)
{
	uint8_t Mathced = 1;
	oCANMessage_t ReturnMessage;

	ReturnMessage = *Message;
	ReturnMessage.Data[1] = 0x80;

	if(Message->ID == 0x01)
	{
		memset(&ReturnMessage.Data[4], 0, 4);

		switch(Message->Data[0])
		{
			case 0x1C:
				if(Message->Data[2] == 0x02){
					switch(Message->Data[3])
					{
						case 0x14://parameter save
							MiStorage_ParameterSaveCmd = 1;
							break;
						case 0x01://final angle mode
						case 0x02://can mode = ?
							break;
						case 0x1E://analog calibration mode
						case 0x1F://plyfit calibration mode
							CalibrationMode = Message->Data[3];
							break;
						default:
							ReturnMessage.DLC = 0;
							break;
					}
				}
				else{
					ReturnMessage.DLC = 0;
				}
				break;
			case 0x1D:
				if(Message->Data[2] == 0x01){
					switch(Message->Data[3])
					{
						case 0x00:
							*((int32_t *)(&ReturnMessage.Data[4])) = (int32_t)(Measure_CalibratedAngle.Pitch * 1000);
							break;
						case 0x01:
							*((int32_t *)(&ReturnMessage.Data[4])) = (int32_t)(Measure_CalibratedAngle.Roll * 1000);
							break;
						default:
							ReturnMessage.DLC = 0;
							break;
					}
				}
				else{
					ReturnMessage.DLC = 0;
				}
				break;
			default:
				ReturnMessage.DLC = 0;
				break;
		}
	}
	else if(Message->ID == 0x1FEF0001 && (Message->Data[2] == 0x01 || Message->Data[2] == 0x02))
	{
		if(Message->Data[2] == 0x01){ //read
			switch(Message->Data[0])
			{
				case 0x01: //Raw X angle
					*((int32_t *)(&ReturnMessage.Data[3])) = (int32_t)(Measure_RawAngle.Pitch * 1000);
					break;
				case 0x02: //Raw Y angle
					*((int32_t *)(&ReturnMessage.Data[3])) = (int32_t)(Measure_RawAngle.Roll * 1000);
					break;
				case 0x03: //Polynomial X
					if(Message->Data[3] <= 5){
						*((float *)(&ReturnMessage.Data[4])) = (float)MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialX[Message->Data[3]];
					}
					else{
						ReturnMessage.DLC = 0;
					}
					break;
				case 0x04: //Polynomial Y
					if(Message->Data[3] <= 5){
						*((float *)(&ReturnMessage.Data[4])) = (float)MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialY[Message->Data[3]];
					}
					else{
						ReturnMessage.DLC = 0;
					}
					break;
				case 0x05: //Range X
					switch(Message->Data[3])
					{
						case 1:
							*((uint32_t *)(&ReturnMessage.Data[4])) = (uint32_t)(MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeX.Max * 1000);
							break;
						case 2:
							*((uint32_t *)(&ReturnMessage.Data[4])) = (uint32_t)(MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeX.Min * 1000);
							break;
						default:
							ReturnMessage.DLC = 0;
							break;
					}
					break;
				case 0x06: //Range Y
					switch(Message->Data[3])
					{
						case 1:
							*((uint32_t *)(&ReturnMessage.Data[4])) = (uint32_t)(MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeY.Max * 1000);
							break;
						case 2:
							*((uint32_t *)(&ReturnMessage.Data[4])) = (uint32_t)(MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeY.Min * 1000);
							break;
						default:
							ReturnMessage.DLC = 0;
							break;
					}
					break;
				default:
					Mathced = 0;
					break;
			}
		}
		else if(Message->Data[2] == 0x02){ //write
			switch(Message->Data[0])
			{
				case 0x03: //Polynomial X
					if(Message->Data[3] <= 5){
						MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialX[Message->Data[3]] = (double)(*((float *)&Message->Data[4]));
					}
					break;
				case 0x04: //Polynomial Y
					if(Message->Data[3] <= 5){
						MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialY[Message->Data[3]] = (double)(*((float *)&Message->Data[4]));
					}
					break;
				case 0x05: //Range X
					switch(Message->Data[3])
					{
						case 1:
							MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeX.Max = (double)(*((int32_t *)&Message->Data[4])) / 1000;
							break;
						case 2:
							MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeX.Min = (double)(*((int32_t *)&Message->Data[4])) / 1000;
							break;
					}
					break;
				case 0x06: //Range Y
					switch(Message->Data[3])
					{
						case 1:
							MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeY.Max = (double)(*((int32_t *)&Message->Data[4])) / 1000;
							break;
						case 2:
							MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeY.Min = (double)(*((int32_t *)&Message->Data[4])) / 1000;
							break;
					}
					break;
				case 0x00:
					if(Message->Data[3] == 0){ //factory reset
						MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeX.Max = 180;
						MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeX.Min = -180;
						MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeY = MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeX;

						memset(&MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialX, 0, sizeof(MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialX));
						memcpy(&MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialY, &MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialX, sizeof(MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialY));

					}
					break;
				case 0x0A:
					if(Message->Data[3] == 1){ //reboot
						HAL_NVIC_SystemReset();
					}
					break;
			}
		}
	}
	else{
		Mathced = 0;
	}

	if(Mathced && ReturnMessage.DLC){
		CanToRS232_Serialize(&ReturnMessage);
	}

	if(Mathced){
		CalibrationTimer = oTMR_GetTick(TICKBASE_SYSTICK);
		IsCalibrationRun = 1;
	}
}

void MiCalibration()
{
	if(IsCalibrationRun){
		MiIoT_IsPause = 1;

		if(Measurement_Sensor(NULL) != RESULT_RUN && !GPIOs.DI.UsbConnected){
			ADXL355_Close(&ADXL355); // adxl close
			GPIOs.DO.MEMSEnable = 0;

			IsCalibrationRun = 0;
			MiIoT_IsPause = 0;
		}
	}

	if(GPIOs.DI.UsbConnected){
		CanToRS232_Deserialize();
	}
}

/* History

2026-06-26 | v0.1
	- baseline (Mi_Calibration.c)
*/
