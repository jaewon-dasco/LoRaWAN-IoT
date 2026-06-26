/*
 * Mi_Sensor.c
 *
 *  Created on: Dec 9, 2024
 *      Author: JONE
 */
#include "ONE_Math.h"
#include "ONE_Filter.h"
#include "ONE_Common.h"
#include "Mi_Native.h"
#include "Mi_IoT.h"
#include "Mi_Main.h"
#include "Mi_Measurement.h"
#include "Mi_Serial.h"

/* VREFINT_CAL_ADDR / VREFINT_CAL_VREF 은 HAL (stm32l4xx_ll_adc.h)에서 제공 — VDDA=3.0V(=3000mV) 기준 공장 교정값 */

#define ADC_VREF						(MiIoT_Parameter.SystemConfig.ActualVRef != 0 ? MiIoT_Parameter.SystemConfig.ActualVRef : g_VddaActual_mV)
#define ADC_MAXDIGIT					4095.0f
#define ADC_TO_AI(x)					((double)(x)/(double)ADC_MAXDIGIT*(double)ADC_VREF)
#define ADC_TO_SUPPLY(x)				(ADC_TO_AI(x)/VOLT_DIV_RATIO(6980,20000))
#define ADC_TO_VBAT(x)					(ADC_TO_AI(x)*3)

#define SUPPLY_AVERAGE_SIZE				15
#define ADXL_AVERAGE_SIZE				30
#define TMP_AVERAGE_SIZE				15
#define ANGLE_AVERAGE_SIZE				100

uint16_t SupplyAverageBuffer[2][SUPPLY_AVERAGE_SIZE];
oMoveAverage_t SupplyAverage[2] = {\
	MOVEAVERAGE_INITIALIZER(&SupplyAverageBuffer[0], sizeof(SupplyAverageBuffer[0]), DataType_UInt16),\
	MOVEAVERAGE_INITIALIZER(&SupplyAverageBuffer[1], sizeof(SupplyAverageBuffer[1]), DataType_UInt16),\
};

double ADXLAvrgBuffer[2][ADXL_AVERAGE_SIZE];
oMoveAverage_t ADXLAvrg[2] = {\
	MOVEAVERAGE_INITIALIZER(&ADXLAvrgBuffer[0], sizeof(ADXLAvrgBuffer[0]), DataType_Double),\
	MOVEAVERAGE_INITIALIZER(&ADXLAvrgBuffer[1], sizeof(ADXLAvrgBuffer[1]), DataType_Double),\
};

double AngleAvrgBuffer[2][ANGLE_AVERAGE_SIZE];
oMoveAverage_t AngleAvrg[2] = {\
	MOVEAVERAGE_INITIALIZER(&AngleAvrgBuffer[0], sizeof(AngleAvrgBuffer[0]), DataType_Double),\
	MOVEAVERAGE_INITIALIZER(&AngleAvrgBuffer[1], sizeof(AngleAvrgBuffer[1]), DataType_Double),\
};

double TMPAvrgBuffer[TMP_AVERAGE_SIZE];
oMoveAverage_t TMPAvrg = MOVEAVERAGE_INITIALIZER(&TMPAvrgBuffer[0], sizeof(TMPAvrgBuffer), DataType_Double);

ADXL355_t ADXL355;
TMP1075_t TMP1075;
oRPY_t Measure_RawAngle;
oRPY_t Measure_CalibratedAngle;

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
oRPY_t Measure_FinalAngle;
double Measure_Temperature;

uint8_t Measure_SequenceStep = 0;
uint32_t Measure_Timer;

oResult_t Measurement_ReadAngle(oRPY_t *pRPY)
{
	static uint8_t ReadAngleStep = 0;
	static uint32_t ReadAngleTimer = 0;
	oRPY_t Angle;
	oResult_t result = RESULT_RUN;

	switch(ReadAngleStep)
	{
		default:
			ReadAngleStep = 0;
		case 0:
			if(GPIOs.DO.MEMSEnable && ADXL355.State.IsOpen){
				ReadAngleStep = 4;
			}
			else{
				if(ADXL355.State.IsOpen){
					ADXL355_Close(&ADXL355); // adxl close
				}

				GPIOs.DO.MEMSEnable = 0;

				if(oTMR_Trigger(&ReadAngleTimer, 100, 1, TICKBASE_SYSTICK)){
					ReadAngleStep++;

					GPIOs.DO.MEMSEnable = 1;
					AngleAvrg[0].Reset = 1;
					AngleAvrg[1].Reset = 1;
				}
			}
			break;
		case 1:
			if(oTMR_Trigger(&ReadAngleTimer, 100, 1, TICKBASE_SYSTICK)){
				ReadAngleStep++;
			}
			break;
		case 2:
			switch(ADXL355_Open(&hi2c2, &ADXL355))
			{
				default:
					break;
				case RESULT_OK:
					ReadAngleStep++;
					break;
				case RESULT_ERROR:
					result = RESULT_ERROR;
					break;
			}
			break;
		case 3:
			ADXL355_SetFilter(&ADXL355, 0, 62);
			ADXL355.Register.Parameter.POWER_CTL.DRDY_OFF = 1;
			ADXL355.Register.Parameter.Range.GravityRange = ADXL355_G_RANGE_2G;

			if(ADXL355_SetParameter(&ADXL355) == RESULT_OK){
				ReadAngleStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 4:
			switch(ADXL355_GetAngle(&ADXL355, 1, &Angle))
			{
				default:
					break;
				case RESULT_ERROR:
					result = RESULT_ERROR;
					break;
				case RESULT_OK:
					Angle.Pitch *= -1;

					//final angle
					oMoveAverage_SetData(&ADXLAvrg[0], &Angle.Pitch);
					oMoveAverage_SetData(&ADXLAvrg[1], &Angle.Roll);

					oMoveAverage_GetData(&ADXLAvrg[0], &Measure_RawAngle.Pitch);
					oMoveAverage_GetData(&ADXLAvrg[1], &Measure_RawAngle.Roll);

					//calibration raw angle
					Measure_CalibratedAngle.Pitch = oMath_Polynomial(Measure_RawAngle.Pitch * 1000, MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialX, 6) / 1000;
					Measure_CalibratedAngle.Pitch = MATH_LIMIT(Measure_CalibratedAngle.Pitch, MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeX.Min, MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeX.Max);

					Measure_CalibratedAngle.Roll = oMath_Polynomial(Measure_RawAngle.Roll * 1000, MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialY, 6) / 1000;
					Measure_CalibratedAngle.Roll = MATH_LIMIT(Measure_CalibratedAngle.Roll, MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeY.Min, MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeY.Max);

					oMoveAverage_SetData(&AngleAvrg[0], &Measure_CalibratedAngle.Pitch);
					oMoveAverage_SetData(&AngleAvrg[1], &Measure_CalibratedAngle.Roll);

					if(pRPY){
						*pRPY = Measure_CalibratedAngle;
					}

					ReadAngleStep++;
					break;
			}
			break;
		case 5:
			if(ADXL355_Close(&ADXL355) != RESULT_RUN){
				result = RESULT_OK;
			}
			break;
	}

	if(result != RESULT_RUN){
		GPIOs.DO.MEMSEnable = 0;
		ReadAngleStep = 0;
	}

	return result;
}

oResult_t Measurement_ReadTemp(double *pTemp)
{
	static uint8_t ReadTempStep = 0;
	static uint32_t ReadTempTimer = 0;
	oResult_t result = RESULT_RUN;

	switch(ReadTempStep)
	{
		default:
			ReadTempStep = 0;
		case 0:
			if(GPIOs.DO.TEMPEnable && TMP1075.State.IsOpen){
				ReadTempStep = 3;
			}
			else{
				if(TMP1075.State.IsOpen){
					TMP1075_Close(&TMP1075);
				}

				GPIOs.DO.TEMPEnable = 0;

				if(oTMR_Trigger(&ReadTempTimer, 100, 1, TICKBASE_SYSTICK)){
					ReadTempStep++;

					GPIOs.DO.TEMPEnable = 1;
					TMPAvrg.Reset = 1;
				}
			}
			break;
		case 1:
			if(oTMR_Trigger(&ReadTempTimer, 100, 1, TICKBASE_SYSTICK)){
				ReadTempStep++;
			}
			break;
		case 2:
			switch(TMP1075_Open(&hi2c3, &TMP1075))
			{
				default:
					break;
				case RESULT_OK:
					ReadTempStep++;
					break;
				case RESULT_ERROR:
					result = RESULT_ERROR;
					break;
			}
			break;
		case 3:
			switch(TMP1075_GetData(&TMP1075, 1))
			{
				default:
					break;
				case RESULT_OK:
					result = RESULT_OK;
					oMoveAverage_SetData(&TMPAvrg, &TMP1075.Temperature);

					if(pTemp){
						oMoveAverage_GetData(&TMPAvrg, pTemp);
					}
					break;
				case RESULT_ERROR:
					result = RESULT_ERROR;
					break;
			}
			break;
	}

	if(result == RESULT_ERROR){
		TMP1075_Close(&TMP1075);
		GPIOs.DO.TEMPEnable = 0;
	}

	if(result != RESULT_RUN){
		ReadTempStep = 0;
	}

	return result;
}

oResult_t Measurement_Sensor(IoT_DataPacket_t *pPacket)
{
	static uint8_t MeasurementSensorStep = 0;
	static uint32_t MeasurementReadCount = 0;
	static uint32_t MeasurementReloadTimer = 0;
	static uint32_t MeasurementSensorTimer = 0;
	oResult_t result = RESULT_RUN;

	MiIoT_Status.StatusBits.DisconnectedSensor = False;
	MiIoT_Status.TroubleCode = 0;

	if(MeasurementSensorStep != 0 && oTMR_Elapsed(&MeasurementReloadTimer, 1000, TICKBASE_SYSTICK)){
		MeasurementSensorStep = 0;
	}

	MeasurementReloadTimer = oTMR_GetTick(TICKBASE_SYSTICK);

	switch(MeasurementSensorStep)
	{
		case 0:
			pPacket->DLC = 0;
			pPacket->TypeOfData = IoTDataType_NULL;
			MeasurementReadCount = 0;
			MeasurementSensorTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			MeasurementSensorStep++;
		case 1:
			if(Measurement_ReadAngle(&Measure_FinalAngle) != RESULT_RUN){
				MeasurementSensorStep++;
			}
			break;
		case 2:
			if(Measurement_ReadTemp(&Measure_Temperature) != RESULT_RUN){
				MeasurementSensorStep++;
			}
			break;
		case 3:
			if(oTMR_Elapsed(&MeasurementSensorTimer, MATH_LIMIT(MiIoT_Parameter.ChannelConfig[0].WarmupTime, 1000, 3000), TICKBASE_SYSTICK)){
				MeasurementReadCount++;
				MiSerial_SensorSamplingProgress = (uint8_t)((float)MeasurementReadCount/15.0f*100);
			}

			if(MiSerial_UpdateSensorCmd && MiSerial_StopSensorCmd){
				result = RESULT_OK;    /* Stop — 현재까지 측정값으로 완료 */
			}
			else if(MeasurementReadCount < 15){
				MeasurementSensorStep = 1;
			}
			else{
				result = RESULT_OK;
			}
			break;
	}

	if(result != RESULT_RUN){
		ADXL355_Close(&ADXL355);
		TMP1075_Close(&TMP1075);

		GPIOs.DO.MEMSEnable = 0;
		GPIOs.DO.TEMPEnable = 0;
		MeasurementSensorStep = 0;

		if(result == RESULT_OK && pPacket){
			pPacket->TypeOfData = IoTDataType_Tilt;
			pPacket->DLC = MIIOT_IOTDATA_SIZE_TILT;

			IoTDataTilt_t *pIoTDataTilt = (IoTDataTilt_t *)&pPacket->Frame;

			memset(pIoTDataTilt, 0, sizeof(IoTDataTilt_t));
			MIIOT_DT_TO_IOTTIME(&MiIoT_DT, &pIoTDataTilt->Time);

			pIoTDataTilt->Temperature = MIIOT_DATA_ENCODE_TEMP(Measure_Temperature);
			pIoTDataTilt->Sensor.AxisX = MIIOT_DATA_ENCODE_ANGLE(Measure_FinalAngle.Pitch);
			pIoTDataTilt->Sensor.AxisY = MIIOT_DATA_ENCODE_ANGLE(Measure_FinalAngle.Roll);
		}
	}

	return result;
}

oResult_t Measurement_Supply(uint8_t Count)
{
	static uint8_t ReadSupplyStep = 0;
	static uint32_t ReadSupplyTimer = 0;

	uint16_t Data = 0;
	oResult_t result = RESULT_RUN;

	switch(ReadSupplyStep)
	{
		case 0:
			memset(&SupplyAverageBuffer, 0, sizeof(SupplyAverageBuffer));
			ReadSupplyStep++;
		case 1:
			ReadSupplyTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			GPIOs.DO.BatteryCheck = 1;
			ReadSupplyStep++;
		case 2:
			if(oTMR_Trigger(&ReadSupplyTimer, 5, 1, TICKBASE_SYSTICK)){
				ReadSupplyStep++;
			}
			break;
		case 3:
			if(Measurement_CalibrateVDD() != RESULT_RUN){
				ReadSupplyStep++;
			}
			break;
		case 4:
			switch(Native_ADCRead(ADC_CHANNEL_3, ADC_SAMPLETIME_247CYCLES_5, &Data, Count))
			{
				case RESULT_RUN:
					break;
				case RESULT_OK:
					Data = ADC_TO_SUPPLY(Data);
					oMoveAverage_SetData(&SupplyAverage[0], &Data);
					oMoveAverage_GetData(&SupplyAverage[0], &GPIOs.ADC.SystemSupply);
					ReadSupplyStep++;
					break;
				default:
					ReadSupplyStep++;
					break;
			}
			break;
		case 5:
			switch(Native_ADCRead(ADC_CHANNEL_VBAT, ADC_SAMPLETIME_247CYCLES_5, &Data, Count))
			{
				case RESULT_RUN:
					break;
				case RESULT_OK:
					Data = ADC_TO_VBAT(Data);
					oMoveAverage_SetData(&SupplyAverage[1], &Data);
					oMoveAverage_GetData(&SupplyAverage[1], &GPIOs.ADC.InternalBAT);
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
		GPIOs.DO.BatteryCheck = 0;
	}

	return result;
}

/* History

2026-06-26 | v0.1
	- baseline (Mi_Measurement.c)
*/
