/*
 * Mi_Measurement.c
 *
 *  Version: 0.1 (2026-06-29)
 */

#include "Mi_Native.h"
#include "Mi_IoT.h"
#include "Mi_Main.h"
#include "Mi_Measurement.h"
#include "ADC_NAU7802.h"
#include "ONE_CAN.h"
#include "ONE_Common.h"
#include "ONE_Math.h"
#include "ONE_Memory.h"

/* VREFINT_CAL_ADDR / VREFINT_CAL_VREF 은 HAL (stm32l4xx_ll_adc.h)에서 제공 — VDDA=3.0V(=3000mV) 기준 공장 교정값 */

#define ADC_VREF						(MiIoT_Parameter.SystemConfig.ActualVRef != 0 ? MiIoT_Parameter.SystemConfig.ActualVRef : g_VddaActual_mV)
#define ADC_MAXDIGIT					4095.0f
#define ADC_TO_MV(x)					((double)(x)/(double)ADC_MAXDIGIT*(double)ADC_VREF)
#define ADC_TO_SUPPLY(x)				(ADC_TO_MV(x)/VOLT_DIV_RATIO(6980,20000))
#define ADC_TO_VBAT(x)					(ADC_TO_MV(x)*3)

#define AI_TO_MV(x)						(x*-5.066999494+11217.99778)
#define AI_TO_MA(x)						(ADC_TO_MV(x)/249)

#define MEASURE_AMP_I2C					&hi2c2
#define MEASURE_SAMPLING_INTERVAL(try)	(5+(try*2))
#define MEASURE_RETRY_MAXCOUNT			5
#define MEASURE_AVERAGE_SIZE			15 //16bit x 50개
#define MEASURE_CAN_ERROR_MAXCOUNT		3

GPIOs_t Measure_SupplyVolt;
NAU7802_t NAU7802;

uint32_t Measure_ScanIdList[MEASUREMENT_NODE_MAXCOUNT];
uint8_t Measure_Buffer[MIIOT_PAYLOAD_MAXSIZE];
IoTDataSIC100_2C_t *pIoTData = (IoTDataSIC100_2C_t *)&Measure_Buffer;
uint8_t Measure_SequenceStep = 0;
uint8_t Measure_CanErrorCount = 0;

oCANVxD_t *pVxD;
uint32_t ReceivedId;
uint32_t SensorStartId;
uint32_t Measure_Timer;
oCANMessage_t ReceiveMessage;
oCANMessage_t TransmitMessage = CAN_MESSAGE_INITIALIZER(0,8,False);

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

	if(pIoTData == NULL || Message->ID == 0 || Message->DLC != 8 || Message->Data[1] != 0x80 || SensorStartId <= 0){
		return;
	}

	ReceivedId = Message->ID;

	if(ReceivedId >= SensorStartId && (ReceivedId-SensorStartId) <= MIIOT_ARRAYSENSOR_MAX_COUNT)
	{
		ReceiveMessage = *Message;

		if(Message->Data[0] == 0x09){//temp
			memcpy(&s16, &Message->Data[2], 2);

			if(s16 != 0){
				pIoTData->TiltArray.Temperature = MIIOT_DATA_ENCODE_TEMP((double)s16 * 0.01);
			}
		}
		else if(Message->Data[0] == 0x1D && Message->Data[2] == 0x01){ //angle
			memcpy(&s32, &Message->Data[4], 4);
			dData = MATH_MAX(-179.9999, (double)s32 * 0.001);

			if(s32 != 0){
				if(Message->Data[3] == 0x00){
					pIoTData->TiltArray.Dual[ReceivedId-SensorStartId].AxisX = MIIOT_DATA_ENCODE_ANGLE(dData);
				}
				else if(Message->Data[3] == 0x01){
					pIoTData->TiltArray.Dual[ReceivedId-SensorStartId].AxisY = MIIOT_DATA_ENCODE_ANGLE(dData);
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

	if(pIoTData == NULL || Message->ID == 0 || Message->DLC != 8 || Message->Data[1] != 0x80 || SensorStartId <= 0){
		return;
	}

	ReceivedId = Message->ID;

	if(ReceivedId >= SensorStartId && (ReceivedId-SensorStartId) <= MIIOT_ARRAYSENSOR_MAX_COUNT)
	{
		ReceiveMessage = *Message;

		if(Message->Data[0] == 0x09){//temp
			memcpy(&s16, &Message->Data[2], 2);

			if(s16 != 0){
				pIoTData->TiltArray.Temperature = MIIOT_DATA_ENCODE_TEMP((double)s16 * 0.01);
			}
		}
		else if(Message->Data[0] == 0x1D && Message->Data[2] == 0x01){ //angle
			memcpy(&s32, &Message->Data[4], 4);
			dData = MATH_MAX(-179.9999, (double)s32 * 0.001);

			if(s32 != 0){
				if(Message->Data[3] == 0x00){
					pIoTData->TiltArray.Single[ReceivedId-SensorStartId].Axis = MIIOT_DATA_ENCODE_ANGLE(dData);
				}
			}
		}
	}
}

oResult_t Measurement_PowerOn(int8_t Channel)
{
	GPIOs.DO.CANEnable = 0;
	GPIOs.DO.PwrEnable_U0 = 0;
	GPIOs.DO.PwrEnable_U1 = 0;
	GPIOs.DO.PwrSupplySel = 0;
	GPIOs.DO.PwrSupply5VEnable = 0;
	GPIOs.DO.AmpEnable = 0;

	if(Channel >= 0){
		GPIOs.DO.PwrEnable_U0 = 1;
		GPIOs.DO.PwrEnable_U1 = 1;

		if(Channel == 1){
			GPIOs.DO.CANEnable = 1;
			GPIOs.DO.PwrSupplySel = 1;
		}
		else if(Channel == 2){
			GPIOs.DO.AmpEnable = 1;
			GPIOs.DO.PwrSupply5VEnable = 1;
		}
	}

	return RESULT_OK;
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
			if((result=Measurement_PowerOn(Channel)) == RESULT_OK){
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
		Measurement_PowerOn(-1);
		oCAN_Close(pVxD);
	}

	return result;
}

oResult_t Measurement_ArraySensorDual(IoTChannelConfig_t *pConfig)
{
	static uint32_t Index = 0;
	static uint16_t Trycount = 0;
	uint8_t MaxIndex = 0;
	oResult_t result = RESULT_RUN;
	uint8_t Empty = 0;

	MaxIndex = MATH_MIN((uint8_t)MIIOT_ARRAYSENSOR_MAX_COUNT, pConfig->Properties.Array.CountOfSensor);
	SensorStartId = pConfig->Properties.Array.SensorId;

	if(oCAN_IsError(pVxD) == RESULT_ERROR || (Measure_SequenceStep > 0 && Measure_CanErrorCount >= MEASURE_CAN_ERROR_MAXCOUNT)){
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
			else if(pIoTData->TiltArray.Temperature != 0){
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
			else if(pIoTData->TiltArray.Dual[Index].AxisX != 0 || oMEM_GetBit(&pConfig->Properties.Array.DisableSensorBit, Index)){
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
			else if(pIoTData->TiltArray.Dual[Index].AxisY != 0 || oMEM_GetBit(&pConfig->Properties.Array.DisableSensorBit, Index)){
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
					if(!oMEM_GetBit(&pConfig->Properties.Array.DisableSensorBit, Index) && (pIoTData->TiltArray.Dual[Index].AxisX == 0 || pIoTData->TiltArray.Dual[Index].AxisY == 0)){
						Empty = 1;
					}

					if(Empty && MiIoT_Status.StatusBits.DisconnectedSensor == False){
						MiIoT_Status.TroubleCode = SensorStartId + Index;
						MiIoT_Status.StatusBits.DisconnectedSensor = True;
					}
				}

				if(++Trycount >= MEASURE_RETRY_MAXCOUNT || !Empty){
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
		pIoTData->TiltArray.Type = IoTSensorType_ArrayDualTilt;
		pIoTData->CountOfArraySensor = MaxIndex;

		Measure_SequenceStep = 0;
		Measure_CanErrorCount = 0;
		oCAN_ResetReceiveCallback(pVxD, Measurement_RecieveCallback_ArrayDual);
		oCAN_Close(pVxD);
	}

	return result;
}


oResult_t Measurement_ArraySensorSingle(IoTChannelConfig_t *pConfig)
{
	static uint32_t Index = 0;
	static uint16_t Trycount = 0;
	uint8_t MaxIndex = 0;
	oResult_t result = RESULT_RUN;
	uint8_t Empty = 0;

	MaxIndex = MATH_MIN((uint8_t)MIIOT_ARRAYSENSOR_MAX_COUNT, pConfig->Properties.Array.CountOfSensor);
	SensorStartId = pConfig->Properties.Array.SensorId;

	if(oCAN_IsError(pVxD) == RESULT_ERROR || (Measure_SequenceStep > 0 && Measure_CanErrorCount >= MEASURE_CAN_ERROR_MAXCOUNT)){
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
				oCAN_SetMaskReceiveCallback(pVxD, 0x7FE, 0, 0, Measurement_RecieveCallback_ArraySingle, 0);
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
			else if(pIoTData->TiltArray.Temperature != 0){
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
			else if(pIoTData->TiltArray.Single[Index].Axis != 0 || oMEM_GetBit(&pConfig->Properties.Array.DisableSensorBit, Index)){
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
			if(oTMR_Elapsed(&Measure_Timer, 100, TICKBASE_SYSTICK)){
				for(Index=0; Index<MaxIndex && Empty == 0; Index++){
					if(!oMEM_GetBit(&pConfig->Properties.Array.DisableSensorBit, Index) && pIoTData->TiltArray.Single[Index].Axis == 0){
						Empty = 1;
					}

					if(Empty && MiIoT_Status.StatusBits.DisconnectedSensor == False){
						MiIoT_Status.TroubleCode = SensorStartId + Index;
						MiIoT_Status.StatusBits.DisconnectedSensor = True;
					}
				}

				if(++Trycount >= MEASURE_RETRY_MAXCOUNT || !Empty){
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
		pIoTData->TiltArray.Type = IoTSensorType_ArraySingleTilt;
		pIoTData->CountOfArraySensor = MaxIndex;

		Measure_SequenceStep = 0;
		Measure_CanErrorCount = 0;
		oCAN_ResetReceiveCallback(0, Measurement_RecieveCallback_ArraySingle);
		oCAN_Close(pVxD);
	}

	return result;
}

oResult_t Measurement_Analog(IoTChannelConfig_t *pConfig)
{
	static uint8_t ErrorCount = 0;
	static uint8_t SumOfCount = 0;
	static double SumAnalog = 0;
	double ReadAnalog = 0;
	double FinalAnalog = 0;
	oResult_t result = RESULT_RUN;

	switch(Measure_SequenceStep)
	{
		case 0://Initialize NAU7802
			if(NAU7802.IsOpen){
				switch(NAU7802_SetGain(&NAU7802, NAU7802_GAIN_BYPASS))
				{
					default:
						break;
					case RESULT_OK:
						oSerial_Log("Mearment", "NAU7802 Gain SET OK (errCnt=%d)", (int)ErrorCount);
						Measure_SequenceStep++;
						ErrorCount = 0;
						break;
					case RESULT_ERROR:
						ErrorCount++;
						oSerial_Log("Mearment", "NAU7802 Gain SET FAIL (errCnt=%d)", (int)ErrorCount);
						break;
				}
			}
			else{
				switch(NAU7802_Init(&NAU7802, MEASURE_AMP_I2C, NAU7802_VLDO_4_5V))
				{
					case RESULT_RUN:
						break;
					case RESULT_OK:
						oSerial_Log("Mearment", "NAU7802 Init OK (errCnt=%d)", (int)ErrorCount);
						ErrorCount = 0;
						break;
					default:
						ErrorCount++;
						oSerial_Log("Mearment", "NAU7802 Init FAIL (errCnt=%d)", (int)ErrorCount);
						break;
				}
			}
			break;
		case 1:
			if(NAU7802_AnalogRead(&NAU7802, 1, NAU7802_SAMPLING_320SPS, &ReadAnalog) != RESULT_RUN && oTMR_Elapsed(&NAU7802.OpenTimestamp, MATH_MAX(pConfig->WarmupTime, 500), TICKBASE_SYSTICK)){
				Measure_SequenceStep++;
			}
			break;
		case 2://Read NAU7802 adc - average count 20
			switch(NAU7802_AnalogRead(&NAU7802, 1, NAU7802_SAMPLING_320SPS, &ReadAnalog))
			{
				case RESULT_RUN:
					break;
				case RESULT_OK:
					ErrorCount = 0;
					SumAnalog += AI_TO_MV(ReadAnalog);

					if(++SumOfCount >= 10){
						ReadAnalog = SumAnalog/SumOfCount;
						FinalAnalog = oLinear(&MiIoT_Parameter.SystemConfig.Calibration.ADC[1], ReadAnalog, 0); //Offset, Gain 보정;
						result = RESULT_OK;

						oSerial_Log("Mearment", "DONE avg=%.4f uV=%.1f (n=%d)", SumAnalog/(double)MATH_MAX(10, 1), FinalAnalog, (int)10);
					}
					else{
						oSerial_Log("Mearment", "Sample[%d/%d]=%.4f sum=%.4f", (int)SumOfCount, (int)10, ReadAnalog, SumAnalog);
					}
					break;
				default:
					ErrorCount++;
					oSerial_Log("Mearment", "Read FAIL (errCnt=%d, n=%d/%d)", (int)ErrorCount, (int)SumOfCount, (int)10);
					break;

			}
			break;
	}

	if(ErrorCount > 5){
		oSerial_Log("Mearment", "ERROR errCnt=%d > 5, abort", (int)ErrorCount);
		result = RESULT_ERROR;
	}

	if(result != RESULT_RUN){
		if(result == RESULT_OK){
			pIoTData->Analog.Type = pConfig->TypeOfSensor;
			if(pConfig->TypeOfSensor == IoTSensorType_mA){
				pIoTData->Analog.Data = MIIOT_DATA_ENCODE_mA(FinalAnalog/249);
			}
			else{
				pIoTData->Analog.Data = MIIOT_DATA_ENCODE_mV(FinalAnalog);
			}
		}

		ErrorCount = 0;
		SumAnalog = 0;
		SumOfCount = 0;
		Measure_SequenceStep = 0;
		NAU7802_DeInit(&NAU7802);
	}

	return result;
}

oResult_t Measurement_Sensor(IoT_DataPacket_t *pPacket)
{
	static uint8_t MeasurementSensorStep = 0;
	static uint8_t ChannelNo = 0;
	static IoTChannelConfig_t *pConfig = NULL;
	oResult_t result = RESULT_RUN;

	switch(MeasurementSensorStep)
	{
		default:
			MeasurementSensorStep = 0;
		case 0: // 초기화 — 누적 버퍼/상태 reset, 채널1부터 순회 시작
			MiIoT_Status.StatusBits.DisconnectedSensor = False;
			MiIoT_Status.TroubleCode = 0;

			memset(pIoTData, 0, sizeof(IoTDataSIC100_2C_t));

			pPacket->TypeOfData = IoTDataType_NULL;
			pPacket->DLC = 0;
			ChannelNo = 1;
			MeasurementSensorStep++;
			// fallthrough
		case 1: // 채널 선택 — TypeOfSensor 와 ChannelNo 매칭 검사
			if(ChannelNo > MEASUREMENT_CHANNEL_MAXCOUNT){
				result = RESULT_OK;
				break;
			}

			pConfig = &MiIoT_Parameter.ChannelConfig[ChannelNo-1];

			switch(pConfig->TypeOfSensor)
			{
				case IoTSensorType_ArrayDualTilt:
				case IoTSensorType_ArraySingleTilt:
					if(ChannelNo == 1){
						MeasurementSensorStep++; // 매칭 → PowerOn 단계로
					}
					else{
						ChannelNo++; // 채널 불일치 — 다음 채널 검사
					}
					break;
				case IoTSensorType_mV:
				case IoTSensorType_mA:
					if(ChannelNo == 2){
						MeasurementSensorStep++;
					}
					else{
						ChannelNo++;
					}
					break;
				default:
					ChannelNo++; // 미정의 타입 — 스킵
					break;
			}
			break;
		case 2: // PowerOn — 채널별 전원 인가
			if(Measurement_PowerOn(pConfig->SupplySource) == RESULT_OK){
				Measure_Timer = oTMR_GetTick(TICKBASE_SYSTICK);
				MeasurementSensorStep++;
			}
			break;
		case 3: // Warm-up 대기
			if(oTMR_Elapsed(&Measure_Timer, 200, TICKBASE_SYSTICK)){
				MeasurementSensorStep++;
			}
			break;
		case 4: // 측정 디스패치 — sub-function 호출
			switch(pConfig->TypeOfSensor)
			{
				case IoTSensorType_ArrayDualTilt:
					result = Measurement_ArraySensorDual(pConfig);
					break;
				case IoTSensorType_ArraySingleTilt:
					result = Measurement_ArraySensorSingle(pConfig);
					break;
				case IoTSensorType_mV:
				case IoTSensorType_mA:
					result = Measurement_Analog(pConfig);
					break;
				default:
					result = RESULT_ERROR;
					break;
			}

			if(result != RESULT_RUN){
				ChannelNo++;
				MeasurementSensorStep = 1; // 다음 채널 검사 루프 복귀
				result = RESULT_RUN; // 전체 미완료 — RUN 유지
			}
			break;
	}

	if(result != RESULT_RUN){
		MeasurementSensorStep = 0;
		ChannelNo = 0;

		Measurement_PowerOn(-1);

		if(result == RESULT_OK){
			pPacket->DLC = MIIOT_PAYLOAD_MAXSIZE;
			pPacket->TypeOfData = IoTDataType_DataArray_Type2;
			MIIOT_DT_TO_IOTTIME(&MiIoT_DT, &pPacket->Frame);
			memcpy(&pPacket->Frame[0] + sizeof(IoTDateAndTime_t), (uint8_t *)pIoTData + sizeof(IoTDateAndTime_t), sizeof(IoTDataSIC100_2C_t) - sizeof(IoTDateAndTime_t));
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

/* History

2026-06-26 | v0.1
	- baseline (Mi_Measurement.c)
*/
