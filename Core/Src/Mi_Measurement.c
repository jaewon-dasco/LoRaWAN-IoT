/*
 * Mi_Sensor.c
 *
 *  Created on: Dec 9, 2024
 *      Author: JONE
 */

#include "Mi_Native.h"
#include "Mi_IoT.h"
#include "Mi_Main.h"
#include "Mi_Measurement.h"
#include "ONE_CAN.h"
#include "ONE_Common.h"
#include "ONE_Math.h"
#include "ONE_Memory.h"

/* VREFINT_CAL_ADDR / VREFINT_CAL_VREF 은 HAL (stm32l4xx_ll_adc.h)에서 제공 — VDDA=3.0V(=3000mV) 기준 공장 교정값 */

#define ADC_VREF						(MiIoT_Parameter.SystemConfig.ActualVRef != 0 ? MiIoT_Parameter.SystemConfig.ActualVRef : g_VddaActual_mV)
#define ADC_MAXDIGIT					4095.0f
#define ADC_TO_AI(x)					((double)(x)/(double)ADC_MAXDIGIT*(double)ADC_VREF)
#define ADC_TO_SUPPLY(x)				(ADC_TO_AI(x)/VOLT_DIV_RATIO(6980,20000))
#define ADC_TO_VBAT(x)					(ADC_TO_AI(x)*3)

#define MEASURE_SAMPLING_INTERVAL(try)	(5+(try*2))
#define MEASURE_RETRY_MAXCOUNT			5
#define MEASURE_AVERAGE_SIZE			15 //16bit x 50개
#define MEASURE_CAN_ERROR_MAXCOUNT		3

GPIOs_t Measure_SupplyVolt;

static uint32_t g_VddaActual_mV = 2800;	/* VREFINT 기반 동적 VDDA (mV), 갱신 전 기본 2.8V */

oResult_t Measurement_CalibrateVDD(void)
{
	static uint8_t CalStep = 0;
	uint16_t vrefint_raw;
	oResult_t result = RESULT_RUN;

	switch(CalStep)
	{
		case 0:
			HAL_ADC_Stop(&hadc1);
			if(HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) == HAL_OK){
				oSerial_Log("Mearment", "ADC offset calibrated");
			}
			CalStep++;
			break;
		case 1:
			result = Native_ADCRead(ADC_CHANNEL_VREFINT, ADC_SAMPLETIME_640CYCLES_5, &vrefint_raw, 20);
			if(result == RESULT_OK){
				if(vrefint_raw > 0){
					uint32_t vdda = ((uint32_t)VREFINT_CAL_VREF * (uint32_t)(*VREFINT_CAL_ADDR)) / vrefint_raw;
					if(vdda >= 2000 && vdda <= 3600){
						g_VddaActual_mV = vdda;
					}
				}
				oSerial_Log("Mearment", "VrefCal VDDA=%lu mV (raw=%u cal=%u)", (unsigned long)g_VddaActual_mV, (unsigned int)vrefint_raw, (unsigned int)(*VREFINT_CAL_ADDR));
			}
			break;
	}

	if(result != RESULT_RUN){
		CalStep = 0;
	}

	return result;
}

uint32_t Measure_ScanIdList[MEASUREMENT_NODE_MAXCOUNT];
uint8_t Measure_Buffer[MIIOT_PAYLOAD_MAXSIZE];
IoTDataArrayDualTilt_t *pArrayDualTilt = (IoTDataArrayDualTilt_t *)&Measure_Buffer;
IoTDataArraySingleTilt_t *pArraySingleTilt = (IoTDataArraySingleTilt_t *)&Measure_Buffer;
uint8_t Measure_SequenceStep = 0;
uint8_t Measure_CanErrorCount = 0;

oCANVxD_t *pVxD;
uint32_t ReceivedId;
uint32_t SensorStartId;
uint32_t Measure_Timer;
oCANMessage_t ReceiveMessage;
oCANMessage_t TransmitMessage = CAN_MESSAGE_INITIALIZER(0,8,False);

void Measurement_RecieveCallback_Scan(oCANMessage_t* Message, uint32_t Arguemnt)
{
	if(Message->ID < SensorStartId || Message->ID > (SensorStartId+MEASUREMENT_NODE_MAXCOUNT) || Message->DLC <= 0){
		return;
	}

	Measure_ScanIdList[Message->ID-SensorStartId] = Message->ID;
}

void Measurement_RecieveCallback_ArrayDual(oCANMessage_t* Message, uint32_t Arguemnt)
{
	double dData = 0;
	int32_t s32 = 0;
	int16_t s16 = 0;

	if(pArrayDualTilt == NULL || Message->ID == 0 || Message->DLC != 8 || Message->Data[1] != 0x80 || SensorStartId <= 0){
		return;
	}

	ReceivedId = Message->ID;

	if(ReceivedId >= SensorStartId && (ReceivedId-SensorStartId) <= MIIOT_ARRAYSENSOR_MAX_COUNT)
	{
		ReceiveMessage = *Message;

		if(Message->Data[0] == 0x09){//temp
			memcpy(&s16, &Message->Data[2], 2);

			if(s16 != 0){
				pArrayDualTilt->Temperature = MIIOT_DATA_ENCODE_TEMP((double)s16 * 0.01);
			}
		}
		else if(Message->Data[0] == 0x1D && Message->Data[2] == 0x01){ //angle
			memcpy(&s32, &Message->Data[4], 4);
			dData = MATH_MAX(-179.9999, (double)s32 * 0.001);

			if(s32 != 0){
				if(Message->Data[3] == 0x00){
					pArrayDualTilt->Sensor[ReceivedId-SensorStartId].AxisX = MIIOT_DATA_ENCODE_ANGLE(dData);
				}
				else if(Message->Data[3] == 0x01){
					pArrayDualTilt->Sensor[ReceivedId-SensorStartId].AxisY = MIIOT_DATA_ENCODE_ANGLE(dData);
				}
			}
		}
	}
}

void Measurement_RecieveCallback_ArraySingle(oCANMessage_t* Message, uint32_t Arguemnt)
{
	double dData = 0;
	int32_t s32 = 0;
	int16_t s16 = 0;

	if(pArraySingleTilt == NULL || Message->ID == 0 || Message->DLC != 8 || Message->Data[1] != 0x80 || SensorStartId <= 0){
		return;
	}

	ReceivedId = Message->ID;

	if(ReceivedId >= SensorStartId && (ReceivedId-SensorStartId) <= MIIOT_ARRAYSENSOR_MAX_COUNT)
	{
		ReceiveMessage = *Message;

		if(Message->Data[0] == 0x09){//temp
			memcpy(&s16, &Message->Data[2], 2);

			if(s16 != 0){
				pArraySingleTilt->Temperature = MIIOT_DATA_ENCODE_TEMP((double)s16 * 0.01);
			}
		}
		else if(Message->Data[0] == 0x1D && Message->Data[2] == 0x01){ //angle
			memcpy(&s32, &Message->Data[4], 4);
			dData = MATH_MAX(-179.9999, (double)s32 * 0.001);

			if(s32 != 0){
				if(Message->Data[3] == 0x00){
					pArraySingleTilt->Sensor[ReceivedId-SensorStartId].Axis = MIIOT_DATA_ENCODE_ANGLE(dData);
				}
			}
		}
	}
}

oResult_t Measurement_Power(uint8_t Channel, uint8_t OnOff)
{
	static uint8_t MeasurementPowerStep = 0;
	static uint32_t MeasurementPowerTimer = 0;
	oResult_t result = RESULT_RUN;

	if(OutOfRange(Channel, 1, 2)){
		result = RESULT_ERROR;
		OnOff = 0;
	}

	if(OnOff){
		switch(MeasurementPowerStep)
		{
			default:
				MeasurementPowerStep = 0;
			case 0:
				MeasurementPowerTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				MeasurementPowerStep++;
			case 1:
				if(GPIOs.DO.PwrEnable_U0){
					MeasurementPowerStep++;
				}
				else if(oTMR_Trigger(&MeasurementPowerTimer, 10, 1, TICKBASE_SYSTICK)){
					GPIOs.DO.PwrEnable_U0 = 1;
				}
				break;
			case 2:
				if(GPIOs.DO.PwrEnable_U1){
					MeasurementPowerStep++;
				}
				else if(oTMR_Trigger(&MeasurementPowerTimer, 50, 1, TICKBASE_SYSTICK)){
					GPIOs.DO.PwrEnable_U1 = 1;
				}
				break;
			case 3:
				if(oTMR_Trigger(&MeasurementPowerTimer, 50, 1, TICKBASE_SYSTICK)){
					if(Channel == 1){
						GPIOs.DO.PwrSupplySel = 1;
					}
					else if(Channel == 2){
						GPIOs.DO.PwrSupplySel = 0;
					}

					MeasurementPowerStep++;
				}
				break;
			case 4:
				if(GPIOs.DO.CANEnable){
					MeasurementPowerStep++;
				}
				else if(oTMR_Trigger(&MeasurementPowerTimer, 10, 1, TICKBASE_SYSTICK)){
					GPIOs.DO.CANEnable = 1;
				}
				break;
			case 5:
				result = RESULT_OK;
				break;
		}
	}
	else{
		GPIOs.DO.CANEnable = 0;
		GPIOs.DO.PwrEnable_U0 = 0;
		GPIOs.DO.PwrEnable_U1 = 0;
		GPIOs.DO.PwrSupplySel = 0;
		result = RESULT_OK;
	}

	if(result != RESULT_RUN){
		MeasurementPowerStep = 0;
	}

	return result;
}

oResult_t Measurement_Scan(uint8_t Channel, uint32_t StartId, uint32_t EndId, uint8_t *pRequestData, uint8_t DLC, uint8_t Try)
{
	static uint8_t MeasurementScanStep = 0;
	static uint32_t MeasurementScanTimer = 0;
	static uint32_t IdIndex = 0;
	static uint8_t MeasurementScanRetry = 0;
	oResult_t result = RESULT_RUN;

	if(pRequestData == NULL || DLC <= 0 || DLC > 8){
		MeasurementScanStep = 0;
		return RESULT_ERROR;
	}

	switch(MeasurementScanStep)
	{
		case 0:
			if(oCAN_IsOpen(0) != RESULT_OK){
				IdIndex = 0;
				MeasurementScanRetry = 0;
				SensorStartId = StartId;
				memcpy(&TransmitMessage.Data, pRequestData, DLC);
				MeasurementScanStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 1:
			if((result=Measurement_Power(Channel, 1)) == RESULT_OK){
				MeasurementScanStep++;
				result = RESULT_RUN;
			}
			break;
		case 2:
			if(oCAN_Open(&pVxD, CAN1, 125) == RESULT_OK){
				memset(&Measure_ScanIdList, 0, sizeof(Measure_ScanIdList));

				oCAN_SetMaskReceiveCallback(pVxD, 0x7FE, 0, 0, Measurement_RecieveCallback_Scan, 0);
				MeasurementScanTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				MeasurementScanStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 3:
			if(oTMR_Elapsed(&MeasurementScanTimer, 300, TICKBASE_SYSTICK)){
				MeasurementScanStep++;
			}
			break;
		case 4:
			if(SensorStartId + IdIndex > EndId){
				MeasurementScanStep++;
				IdIndex = 0;
				MeasurementScanTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			}
			else if(Measure_ScanIdList[SensorStartId+IdIndex] != 0){
				IdIndex++;
			}
			else if(oTMR_Elapsed(&TransmitMessage.Timestamp, 5, TICKBASE_SYSTICK)){
				TransmitMessage.ID = SensorStartId + IdIndex;

				if(oCAN_Transmit(pVxD, &TransmitMessage) != RESULT_OK){
					Measure_CanErrorCount++;
					TransmitMessage.Timestamp = oTMR_GetTick(TICKBASE_SYSTICK);
				}
				else{
					Measure_CanErrorCount = 0;
					IdIndex++;
				}
			}

			if( Measure_CanErrorCount >= MEASURE_CAN_ERROR_MAXCOUNT){
				result = RESULT_ERROR;
			}
			break;
		case 5:
			if(oTMR_Elapsed(&MeasurementScanTimer, 100, TICKBASE_SYSTICK)){
				if(++MeasurementScanRetry >= Try){
					result = RESULT_OK;
				}
				else{
					MeasurementScanStep--;
				}
			}
			break;
	}

	if(result != RESULT_RUN){
		MeasurementScanStep = 0;
		Measure_CanErrorCount = 0;
		oCAN_ResetReceiveCallback(0, Measurement_RecieveCallback_Scan);
		Measurement_Power(Channel, 0);
		oCAN_Close(pVxD);
	}

	return result;
}

oResult_t Measurement_ArraySensorDual(uint8_t ChannelNo, IoT_DataPacket_t *pPacket)
{
	static uint32_t Index = 0;
	static uint16_t Trycount = 0;
	IoTChannelConfig_t *pConfig = &MiIoT_Parameter.ChannelConfig[ChannelNo-1];
	uint8_t MaxIndex = 0;
	oResult_t result = RESULT_RUN;
	uint8_t Empty = 0;

	MaxIndex = MATH_MIN((uint8_t)MIIOT_ARRAYSENSOR_MAX_COUNT, pConfig->Properties.Array.CountOfSensor);
	SensorStartId = pConfig->Properties.Array.SensorId;

	if(oCAN_IsError(pVxD) == RESULT_ERROR || (Measure_SequenceStep > 0 && Measure_CanErrorCount >= MEASURE_CAN_ERROR_MAXCOUNT)){
		memset(&Measure_Buffer, 0, sizeof(Measure_Buffer));
		result = RESULT_ERROR;
		goto EXIT;
	}

	switch(Measure_SequenceStep)
	{
		default:
			Measure_SequenceStep = 0;
		case 0:
			if(oCAN_IsOpen(pVxD) != RESULT_OK){
				Measure_SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 1:
			if(oCAN_Open(&pVxD, CAN1, 125) == RESULT_OK){
				Measure_Timer = oTMR_GetTick(TICKBASE_SYSTICK);
				Measure_CanErrorCount = 0;
				Measure_SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 2:
			if(oTMR_Elapsed(&Measure_Timer, MATH_MAX(pConfig->WarmupTime, 500), TICKBASE_SYSTICK)){
				oCAN_SetMaskReceiveCallback(pVxD, 0x7FE, 0, 0, Measurement_RecieveCallback_ArrayDual, 0);

				memset(&Measure_Buffer, 0, sizeof(Measure_Buffer));

				Measure_Timer = oTMR_GetTick(TICKBASE_SYSTICK);
				Trycount = 0;
				Measure_SequenceStep++;
				Index = 0;
			}
			break;
		case 3://read temp
			if(Index >= MaxIndex){
				if(oTMR_Elapsed(&Measure_Timer, 100, TICKBASE_SYSTICK)){
					Measure_Timer = oTMR_GetTick(TICKBASE_SYSTICK);
					Measure_SequenceStep++;
					Index = 0;
				}
			}
			else if(pArrayDualTilt->Temperature != 0){
				Measure_SequenceStep++;
				Index = 0;
			}
			else if(oTMR_Elapsed(&TransmitMessage.Timestamp, MEASURE_SAMPLING_INTERVAL(Trycount), TICKBASE_SYSTICK)){
				TransmitMessage.ID = SensorStartId + (MaxIndex-Index) - 1;

				memset(&TransmitMessage.Data, 0, 8);
				TransmitMessage.Data[0] = 0x09;
				TransmitMessage.Data[1] = 0x00;
				TransmitMessage.Data[2] = 0x00;
				TransmitMessage.Data[3] = 0x00;

				if(oCAN_Transmit(pVxD, &TransmitMessage) != RESULT_OK){
					Measure_CanErrorCount++;
				}
				else{
					Index++;
				}
			}
			break;
		case 4://read x
			if(Index >= MaxIndex){
				if(oTMR_Elapsed(&Measure_Timer, 100, TICKBASE_SYSTICK)){
					Measure_Timer = oTMR_GetTick(TICKBASE_SYSTICK);
					Measure_SequenceStep++;
					Index = 0;
				}
			}
			else if(pArrayDualTilt->Sensor[Index].AxisX != 0 || oMEM_GetBit(&pConfig->Properties.Array.DisableSensorBit, Index)){
				Index++;
			}
			else if(oTMR_Elapsed(&TransmitMessage.Timestamp, MEASURE_SAMPLING_INTERVAL(Trycount), TICKBASE_SYSTICK)){
				TransmitMessage.ID = SensorStartId + Index;

				memset(&TransmitMessage.Data, 0, 8);
				TransmitMessage.Data[0] = 0x1D;
				TransmitMessage.Data[1] = 0x00;
				TransmitMessage.Data[2] = 0x01;
				TransmitMessage.Data[3] = 0x00;

				if(oCAN_Transmit(pVxD, &TransmitMessage) != RESULT_OK){
					Measure_CanErrorCount++;
				}
				else{
					Index++;
				}
			}
			break;
		case 5://read y
			if(Index >= MaxIndex){
				if(oTMR_Elapsed(&Measure_Timer, 100, TICKBASE_SYSTICK)){
					Measure_Timer = oTMR_GetTick(TICKBASE_SYSTICK);
					Measure_SequenceStep++;
					Index = 0;
				}
			}
			else if(pArrayDualTilt->Sensor[Index].AxisY != 0 || oMEM_GetBit(&pConfig->Properties.Array.DisableSensorBit, Index)){
				Index++;
			}
			else if(oTMR_Elapsed(&TransmitMessage.Timestamp, MEASURE_SAMPLING_INTERVAL(Trycount), TICKBASE_SYSTICK)){
				TransmitMessage.ID = SensorStartId + Index;

				memset(&TransmitMessage.Data, 0, 8);
				TransmitMessage.Data[0] = 0x1D;
				TransmitMessage.Data[1] = 0x00;
				TransmitMessage.Data[2] = 0x01;
				TransmitMessage.Data[3] = 0x01;

				if(oCAN_Transmit(pVxD, &TransmitMessage) != RESULT_OK){
					Measure_CanErrorCount++;
				}
				else{
					Index++;
				}
			}
			break;
		case 6:
			if(oTMR_Elapsed(&Measure_Timer, 100, TICKBASE_SYSTICK)){
				for(Index=0; Index<MaxIndex && Empty == 0; Index++){
					if(!oMEM_GetBit(&pConfig->Properties.Array.DisableSensorBit, Index) && (pArrayDualTilt->Sensor[Index].AxisX == 0 || pArrayDualTilt->Sensor[Index].AxisY == 0)){
						Empty = 1;
					}

					if(Empty && MiIoT_Status.StatusBits.DisconnectedSensor == False){
						MiIoT_Status.TroubleCode = SensorStartId + Index;
						MiIoT_Status.StatusBits.DisconnectedSensor = True;
					}
				}

				if(++Trycount >= MEASURE_RETRY_MAXCOUNT || !Empty){
					pArrayDualTilt->Channel = ChannelNo;

					pPacket->DLC = MIIOT_IOTDATA_SIZE_ARRAYDUALTILT(pConfig->Properties.Array.CountOfSensor);
					pPacket->TypeOfData = IoTDataType_ArrayDualTilt;
					memcpy(&pPacket->Frame, &Measure_Buffer, pPacket->DLC);

					result = RESULT_OK;
				}
				else{
					Measure_SequenceStep = 3;
				}
			}
			break;
	}

EXIT:
	if(result != RESULT_RUN){
		Measure_SequenceStep = 0;
		Measure_CanErrorCount = 0;
		oCAN_ResetReceiveCallback(pVxD, Measurement_RecieveCallback_ArrayDual);
		oCAN_Close(pVxD);
	}

	return result;
}

oResult_t Measurement_ArraySensorSingle(uint8_t ChannelNo, IoT_DataPacket_t *pPacket)
{
	static uint32_t Index = 0;
	static uint16_t Trycount = 0;
	IoTChannelConfig_t *pConfig = &MiIoT_Parameter.ChannelConfig[ChannelNo-1];
	uint8_t MaxIndex = 0;
	oResult_t result = RESULT_RUN;
	uint8_t Empty = 0;

	MaxIndex = MATH_MIN((uint8_t)MIIOT_ARRAYSENSOR_MAX_COUNT, pConfig->Properties.Array.CountOfSensor);
	SensorStartId = pConfig->Properties.Array.SensorId;

	if(oCAN_IsError(pVxD) == RESULT_ERROR || (Measure_SequenceStep > 0 && Measure_CanErrorCount >= MEASURE_CAN_ERROR_MAXCOUNT)){
		memset(&Measure_Buffer, 0, sizeof(Measure_Buffer));
		result = RESULT_ERROR;
		goto EXIT;
	}

	switch(Measure_SequenceStep)
	{
		default:
			Measure_SequenceStep = 0;
		case 0:
			if(oCAN_IsOpen(pVxD) != RESULT_OK){
				Measure_SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 1:
			if(oCAN_Open(&pVxD, CAN1, 125) == RESULT_OK){
				Measure_Timer = oTMR_GetTick(TICKBASE_SYSTICK);
				Measure_CanErrorCount = 0;
				Measure_SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 2:
			if(oTMR_Elapsed(&Measure_Timer, MATH_MAX(pConfig->WarmupTime, 1500), TICKBASE_SYSTICK)){
				oCAN_SetMaskReceiveCallback(pVxD, 0x7FE, 0, 0, Measurement_RecieveCallback_ArraySingle, 0);

				memset(&Measure_Buffer, 0, sizeof(Measure_Buffer));

				Measure_Timer = oTMR_GetTick(TICKBASE_SYSTICK);
				Trycount = 0;
				Measure_SequenceStep++;
				Index = 0;
			}
			break;
		case 3://read temp
			if(Index >= MaxIndex){
				if(oTMR_Elapsed(&Measure_Timer, 100, TICKBASE_SYSTICK)){
					Measure_Timer = oTMR_GetTick(TICKBASE_SYSTICK);
					Measure_SequenceStep++;
					Index = 0;
				}
			}
			else if(pArraySingleTilt->Temperature != 0){
				Measure_SequenceStep++;
				Index = 0;
			}
			else if(oTMR_Elapsed(&TransmitMessage.Timestamp, MEASURE_SAMPLING_INTERVAL(Trycount), TICKBASE_SYSTICK)){
				TransmitMessage.ID = SensorStartId + (MaxIndex-Index) - 1;

				memset(&TransmitMessage.Data, 0, 8);
				TransmitMessage.Data[0] = 0x09;
				TransmitMessage.Data[1] = 0x00;
				TransmitMessage.Data[2] = 0x00;
				TransmitMessage.Data[3] = 0x00;

				if(oCAN_Transmit(pVxD, &TransmitMessage) != RESULT_OK){
					Measure_CanErrorCount++;
				}
				else{
					Index++;
				}
			}
			break;
		case 4://read x
			if(Index >= MaxIndex){
				if(oTMR_Elapsed(&Measure_Timer, 100, TICKBASE_SYSTICK)){
					Measure_Timer = oTMR_GetTick(TICKBASE_SYSTICK);
					Measure_SequenceStep++;
					Index = 0;
				}
			}
			else if(pArraySingleTilt->Sensor[Index].Axis != 0 || oMEM_GetBit(&pConfig->Properties.Array.DisableSensorBit, Index)){
				Index++;
			}
			else if(oTMR_Elapsed(&TransmitMessage.Timestamp, MEASURE_SAMPLING_INTERVAL(Trycount), TICKBASE_SYSTICK)){
				TransmitMessage.ID = SensorStartId + Index;

				memset(&TransmitMessage.Data, 0, 8);
				TransmitMessage.Data[0] = 0x1D;
				TransmitMessage.Data[1] = 0x00;
				TransmitMessage.Data[2] = 0x01;
				TransmitMessage.Data[3] = 0x00;

				if(oCAN_Transmit(pVxD, &TransmitMessage) != RESULT_OK){
					Measure_CanErrorCount++;
				}
				else{
					Index++;
				}
			}
			break;
		case 5:
			for(Index=0; Index<MaxIndex && Empty == 0; Index++){

				if(!oMEM_GetBit(&pConfig->Properties.Array.DisableSensorBit, Index) && pArraySingleTilt->Sensor[Index].Axis == 0){
					Empty = 1;
				}

				if(Empty && MiIoT_Status.StatusBits.DisconnectedSensor == False){
					MiIoT_Status.TroubleCode = SensorStartId + Index;
					MiIoT_Status.StatusBits.DisconnectedSensor = True;
				}
			}

			if(++Trycount >= MEASURE_RETRY_MAXCOUNT || !Empty){
				pArraySingleTilt->Channel = ChannelNo;

				pPacket->DLC = MIIOT_IOTDATA_SIZE_ARRAYSINGLETILT(pConfig->Properties.Array.CountOfSensor);
				pPacket->TypeOfData = IoTDataType_ArraySingleTilt;
				memcpy(&pPacket->Frame, &Measure_Buffer, pPacket->DLC);
				result = RESULT_OK;
			}
			else{
				Measure_SequenceStep = 3;
			}
			break;
	}

EXIT:
	if(result != RESULT_RUN){
		Measure_SequenceStep = 0;
		Measure_CanErrorCount = 0;
		oCAN_ResetReceiveCallback(0, Measurement_RecieveCallback_ArraySingle);
		oCAN_Close(pVxD);
	}

	return result;
}

oResult_t Measurement_Sensor(uint8_t ChannelNo, IoT_DataPacket_t *pPacket)
{
	static uint8_t MeasurementSensorStep = 0;
	static uint32_t MeasurementSensorTimer = 0;
	oResult_t result = RESULT_RUN;
	IoTChannelConfig_t *pConfig = NULL;

	MiIoT_Status.StatusBits.DisconnectedSensor = False;
	MiIoT_Status.TroubleCode = 0;

	if(OutOfRange(ChannelNo, 1, 2)){
		result = RESULT_ERROR;
		goto EXIT;
	}

	pConfig = &MiIoT_Parameter.ChannelConfig[ChannelNo-1];

	if((pConfig->TypeOfSensor != IoTSensorType_ArrayDualTilt && pConfig->TypeOfSensor != IoTSensorType_ArraySingleTilt) || OutOfRange(pConfig->SupplySource, 1, 2)){
		result = RESULT_ERROR;
		goto EXIT;
	}

	switch(MeasurementSensorStep)
	{
		default:
			MeasurementSensorStep = 0;
		case 0:
			pPacket->TypeOfData = IoTDataType_NULL;
			pPacket->DLC = 0;
			Measure_Timer = oTMR_GetTick(TICKBASE_SYSTICK);
			MeasurementSensorStep++;
		case 1:
			if((result=Measurement_Power(pConfig->SupplySource, 1)) == RESULT_OK){
				Measure_Timer = oTMR_GetTick(TICKBASE_SYSTICK);
				MeasurementSensorStep++;
				result = RESULT_RUN;
			}
			break;
		case 2:
			if(oTMR_Elapsed(&MeasurementSensorTimer, 200, TICKBASE_SYSTICK)){
				MeasurementSensorStep++;
			}
			break;
		case 3:
			switch(pConfig->TypeOfSensor)
			{
				case IoTSensorType_ArrayDualTilt:
					result = Measurement_ArraySensorDual(ChannelNo, pPacket);
					break;
				case IoTSensorType_ArraySingleTilt:
					result = Measurement_ArraySensorSingle(ChannelNo, pPacket);
					break;
				default:
					result = RESULT_ERROR;
					break;
			}
			break;
	}

EXIT:
	if(result != RESULT_RUN){
		MeasurementSensorStep = 0;

		if(pConfig != NULL){
			Measurement_Power(pConfig->SupplySource, 0);
		}

		if(result == RESULT_OK){
			MIIOT_DT_TO_IOTTIME(&MiIoT_DT, &pPacket->Frame);
		}
	}

	return result;
}

oResult_t Measurement_Supply(uint8_t Count)
{
	static uint8_t ReadSupplyStep = 0;
	static uint32_t ReadSupplyTimer = 0;
	double Analog;
	uint16_t Data = 0;
	oResult_t result = RESULT_RUN;

	switch(ReadSupplyStep)
	{
		case 0:
			ReadSupplyStep++;
			break;
		case 1:
			ReadSupplyTimer = oTMR_GetTick(TICKBASE_SYSTICK);

			GPIOs.DO.ADCRefEnable = 1;
			ReadSupplyStep++;
		case 2:
			if(oTMR_Trigger(&ReadSupplyTimer, 100, 1, TICKBASE_SYSTICK)){
				ReadSupplyStep++;
			}
			break;
		case 3:
			if(Measurement_CalibrateVDD() != RESULT_RUN){
				ReadSupplyStep++;
			}
			break;
		case 4:
			switch(Native_ADCRead(ADC_CHANNEL_3, ADC_SAMPLETIME_640CYCLES_5, &Data, 25))
			{
				case RESULT_RUN:
					break;
				case RESULT_OK:
					Analog = (double)ADC_TO_SUPPLY(Data);
					GPIOs.ADC.SystemSupply = (uint16_t)oLinear(&MiIoT_Parameter.SystemConfig.Calibration.ADC[0], Analog, 0); //Offset, Gain 보정
					ReadSupplyStep++;
					break;
				default:
					ReadSupplyStep++;
					break;
			}
			break;
		case 5:
			switch(Native_ADCRead(ADC_CHANNEL_VBAT, ADC_SAMPLETIME_640CYCLES_5, &Data, 25))
			{
				case RESULT_RUN:
					break;
				case RESULT_OK:
					GPIOs.ADC.InternalBAT = (uint16_t)ADC_TO_VBAT(Data);
					ReadSupplyStep++;
					break;
				default:
					ReadSupplyStep++;
					break;
			}
			break;
		case 6:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		ReadSupplyStep = 1;
		GPIOs.DO.ADCRefEnable = 0;
	}

	return result;
}
