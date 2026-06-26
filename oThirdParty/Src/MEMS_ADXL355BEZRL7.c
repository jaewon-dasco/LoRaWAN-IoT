/*
 * MEMS_ADXL355BEZRL7.c
 *
 *  Created on: Aug 1, 2025
 *      Author: JONE
 */


#include "ONE_Common.h"
#include "Mi_Main.h"
#include "MEMS_ADXL355BEZRL7.h"

#define ADXL355_I2C_ADDRESS 			(0x1D << 1)

#define ADXL355_REG_ADDRESS_STATUS 		0x04
#define ADXL355_REG_ADDRESS_FIFO_ENTRIES 0x05
#define ADXL355_REG_ADDRESS_DATA 		0x06
#define ADXL355_REG_ADDRESS_FIFO_DATA	0x11
#define ADXL355_REG_ADDRESS_PARAMETER 	0x1E
#define ADXL355_REG_ADDRESS_WO 			0x2F

#define ADXL355_I2C_RECOVERY_THRESHOLD	5      /* HAL_ERROR 누적 임계 */

/* ADXL355_Recovery
 *  I2C 페리페럴 DeInit/Init으로 BUSY·STUCK 상태 회복.
 *  STM32L4 STOP1 wake 후 또는 슬레이브가 SDA를 잡고 있는 상태 복구에 사용.
 */
oResult_t ADXL355_Recovery(ADXL355_t *pADXL355)
{
	if(pADXL355 == NULL || pADXL355->pI2C == NULL){
		return RESULT_NULL;
	}

	HAL_I2C_DeInit(pADXL355->pI2C);
	if(HAL_I2C_Init(pADXL355->pI2C) != HAL_OK){
		return RESULT_ERROR;
	}

	pADXL355->State.CountOfI2CError = 0;
	return RESULT_OK;
}

static void ADXL355_OnHalError(ADXL355_t *pADXL355)
{
	if(pADXL355->State.CountOfI2CError < 0xFF){
		pADXL355->State.CountOfI2CError++;
	}
	if(pADXL355->State.CountOfI2CError >= ADXL355_I2C_RECOVERY_THRESHOLD){
		ADXL355_Recovery(pADXL355);
	}
}

oResult_t ADXL355_Write(ADXL355_t *pADXL355, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	if(!pADXL355->State.IsOpen){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Write(pADXL355->pI2C, ADXL355_I2C_ADDRESS, RegAddress, I2C_MEMADD_SIZE_8BIT, pData, SizeOfData, HAL_MAX_DELAY) != HAL_OK){
		ADXL355_OnHalError(pADXL355);
		return RESULT_ERROR;
	}

	pADXL355->State.CountOfI2CError = 0;
	return RESULT_OK;
}

oResult_t ADXL355_Read(ADXL355_t *pADXL355, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	if(!pADXL355->State.IsOpen){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Read(pADXL355->pI2C, ADXL355_I2C_ADDRESS, RegAddress, I2C_MEMADD_SIZE_8BIT, pData, SizeOfData, HAL_MAX_DELAY) != HAL_OK){
		ADXL355_OnHalError(pADXL355);
		return RESULT_ERROR;
	}

	pADXL355->State.CountOfI2CError = 0;
	return RESULT_OK;
}

oResult_t ADXL355_GetStatus(ADXL355_t *pADXL355)
{
	return ADXL355_Read(pADXL355, ADXL355_REG_ADDRESS_STATUS, (uint8_t *)&pADXL355->Register.Status, sizeof(pADXL355->Register.Status));
}

oResult_t ADXL355_GetParameter(ADXL355_t *pADXL355)
{
	return ADXL355_Read(pADXL355, ADXL355_REG_ADDRESS_PARAMETER, (uint8_t *)&pADXL355->Register.Parameter, sizeof(pADXL355->Register.Parameter));
}

oResult_t ADXL355_SetParameter(ADXL355_t *pADXL355)
{
	return ADXL355_Write(pADXL355, ADXL355_REG_ADDRESS_PARAMETER, (uint8_t *)&pADXL355->Register.Parameter, sizeof(pADXL355->Register.Parameter));
}

oResult_t ADXL355_GetData(ADXL355_t *pADXL355)
{
	oResult_t result = RESULT_RUN;

	if(!pADXL355->State.IsRun){
		return RESULT_ERROR;
	}

	if(ADXL355_GetStatus(pADXL355) == RESULT_ERROR){
		return RESULT_ERROR;
	}

	if(!pADXL355->Register.Status.DATA_RDY){
		return RESULT_RUN;
	}

	if((result=ADXL355_Read(pADXL355, ADXL355_REG_ADDRESS_DATA, (uint8_t *)&pADXL355->Register.Data, sizeof(pADXL355->Register.Data))) == RESULT_OK){
		pADXL355->Temperature = ADXL355_TEMP_TO_Degree(pADXL355->Register.Data.TEMP);

		switch(pADXL355->Register.Parameter.Range.GravityRange)
		{
			default:
			case ADXL355_G_RANGE_2G:
				pADXL355->Acceleration.X  = ADXL355_ACC_TO_2ug(pADXL355->Register.Data.XDATA);
				pADXL355->Acceleration.Y  = ADXL355_ACC_TO_2ug(pADXL355->Register.Data.YDATA);
				pADXL355->Acceleration.Z  = ADXL355_ACC_TO_2ug(pADXL355->Register.Data.ZDATA);
				break;
			case ADXL355_G_RANGE_4G:
				pADXL355->Acceleration.X  = ADXL355_ACC_TO_4ug(pADXL355->Register.Data.XDATA);
				pADXL355->Acceleration.Y  = ADXL355_ACC_TO_4ug(pADXL355->Register.Data.YDATA);
				pADXL355->Acceleration.Z  = ADXL355_ACC_TO_4ug(pADXL355->Register.Data.ZDATA);
				break;
			case ADXL355_G_RANGE_8G:
				pADXL355->Acceleration.X  = ADXL355_ACC_TO_8ug(pADXL355->Register.Data.XDATA);
				pADXL355->Acceleration.Y  = ADXL355_ACC_TO_8ug(pADXL355->Register.Data.YDATA);
				pADXL355->Acceleration.Z  = ADXL355_ACC_TO_8ug(pADXL355->Register.Data.ZDATA);
				break;
		}
	}

	return result;
}

/* ADXL355_GetCountOfFIFOEntries
 *  FIFO_ENTRIES 레지스터(0x05) 읽기 — 현재 슬롯 수(0~96, 1슬롯=축 1개분)
 */
oResult_t ADXL355_GetCountOfFIFOEntries(ADXL355_t *pADXL355, uint8_t *pEntries)
{
	if(pADXL355 == NULL || pEntries == NULL){
		return RESULT_NULL;
	}
	return ADXL355_Read(pADXL355, ADXL355_REG_ADDRESS_FIFO_ENTRIES, pEntries, 1);
}

/* ADXL355_ReadFIFO
 *  FIFO_DATA 레지스터(0x11)에서 SetCount(=XYZ 세트 수) 만큼 읽기.
 *  슬롯 = 9 bytes × SetCount. 슬레이브가 FIFO를 자동 pop.
 */
oResult_t ADXL355_ReadFIFO(ADXL355_t *pADXL355, uint8_t SetCount, uint8_t *pBuf)
{
	if(pADXL355 == NULL || pBuf == NULL || SetCount == 0 || SetCount > 32){
		return RESULT_NULL;
	}
	return ADXL355_Read(pADXL355, ADXL355_REG_ADDRESS_FIFO_DATA, pBuf, (uint32_t)SetCount * 9U);
}

/* ADXL355_ParseFIFOSet
 *  9-byte 세트 → X/Y/Z µg 단위 oVector3_t 로 변환.
 *  현재 GravityRange 설정에 따라 LSB 환산.
 */
void ADXL355_ParseFIFOSet(ADXL355_t *pADXL355, const uint8_t *p9, oVector3_t *pOut)
{
	if(pADXL355 == NULL || p9 == NULL || pOut == NULL){
		return;
	}

	/* FIFO read는 MSB-first: p9[0]=X MSB, p9[1]=X MID, p9[2]=X LSB ... */
	uint32_t x = ((uint32_t)p9[0] << 16) | ((uint32_t)p9[1] << 8) | (uint32_t)p9[2];
	uint32_t y = ((uint32_t)p9[3] << 16) | ((uint32_t)p9[4] << 8) | (uint32_t)p9[5];
	uint32_t z = ((uint32_t)p9[6] << 16) | ((uint32_t)p9[7] << 8) | (uint32_t)p9[8];

	/* 24-bit raw → 20-bit signed (하위 4 bit flag drop, bit 23 = sign) */
	int32_t xRaw = (int32_t)((x >> 4) | ((x & 0x800000U) ? 0xFFF00000U : 0U));
	int32_t yRaw = (int32_t)((y >> 4) | ((y & 0x800000U) ? 0xFFF00000U : 0U));
	int32_t zRaw = (int32_t)((z >> 4) | ((z & 0x800000U) ? 0xFFF00000U : 0U));

	double scale;
	switch(pADXL355->Register.Parameter.Range.GravityRange){
		default:
		case ADXL355_G_RANGE_2G: scale = 3.8147;  break;
		case ADXL355_G_RANGE_4G: scale = 7.6294;  break;
		case ADXL355_G_RANGE_8G: scale = 15.2588; break;
	}

	pOut->X = (double)xRaw * scale;
	pOut->Y = (double)yRaw * scale;
	pOut->Z = (double)zRaw * scale;
}

/* ADXL355_GetAngle
 *
 * Input
 * 	pADXL355| Pointer of ADXL355 structure
 * 	Count	| Count of sampling
 * 	TC		| Temperature Compensation
 *
 * Output
 * 	oResult
 */
oResult_t ADXL355_GetAngle(ADXL355_t *pADXL355, uint8_t Count, oRPY_t *pAngle)
{
	oResult_t result = RESULT_RUN;
	oRPY_t RPY;

	if(pADXL355 == NULL || Count <= 0){
		return RESULT_NULL;
	}

	switch(ADXL355_GetData(pADXL355))
	{
		default:
			break;
		case RESULT_ERROR:
			pADXL355->State.CountOfError++;
			break;
		case RESULT_OK:
			if(pADXL355->State.CountOfOverSampling == 0){
				memset(&pADXL355->OverSampling, 0, sizeof(pADXL355->OverSampling));
			}

			RPY = oMath_AccelationToRPY(pADXL355->Acceleration);

			pADXL355->OverSampling.Pitch += RPY.Pitch;
			pADXL355->OverSampling.Roll += RPY.Roll;
			pADXL355->State.CountOfOverSampling++;
			break;

	}

	if(pADXL355->State.CountOfError >= Count){
		result = RESULT_ERROR;
	}
	else if(pADXL355->State.CountOfOverSampling >= Count){
		pADXL355->Angle.Pitch = pADXL355->OverSampling.Pitch / (double)pADXL355->State.CountOfOverSampling;
		pADXL355->Angle.Roll = pADXL355->OverSampling.Roll / (double)pADXL355->State.CountOfOverSampling;

		*pAngle = pADXL355->Angle;
		result = RESULT_OK;
	}

	if(result != RESULT_RUN){
		pADXL355->State.CountOfOverSampling = 0;
		pADXL355->State.CountOfError = 0;
	}

	return result;
}

void ADXL355_SetFilter(ADXL355_t *pADXL355, double HPF, double LPF)
{
	if(LPF > 500){
		pADXL355->Register.Parameter.Filter.ODR_LPF = ADXL355_ODR_4000hz_LPF_1000hz;
		pADXL355->LowPassFreqeuncy = 1000;
	}
	else if(LPF > 250){
		pADXL355->Register.Parameter.Filter.ODR_LPF = ADXL355_ODR_2000hz_LPF_500hz;
		pADXL355->LowPassFreqeuncy = 500;
	}
	else if(LPF > 125){
		pADXL355->Register.Parameter.Filter.ODR_LPF = ADXL355_ODR_1000hz_LPF_250hz;
		pADXL355->LowPassFreqeuncy = 250;
	}
	else if(LPF > 62.5){
		pADXL355->Register.Parameter.Filter.ODR_LPF = ADXL355_ODR_500hz_LPF_125hz;
		pADXL355->LowPassFreqeuncy = 125;
	}
	else if(LPF > 31.25){
		pADXL355->Register.Parameter.Filter.ODR_LPF = ADXL355_ODR_250hz_LPF_62_5hz;
		pADXL355->LowPassFreqeuncy = 62.5;
	}
	else if(LPF > 15.625){
		pADXL355->Register.Parameter.Filter.ODR_LPF = ADXL355_ODR_125hz_LPF_31_2hz;
		pADXL355->LowPassFreqeuncy = 31.25;
	}
	else if(LPF > 7.813){
		pADXL355->Register.Parameter.Filter.ODR_LPF = ADXL355_ODR_62_5hz_LPF_15_6hz;
		pADXL355->LowPassFreqeuncy = 15.625;
	}
	else if(LPF > 3.906){
		pADXL355->Register.Parameter.Filter.ODR_LPF = ADXL355_ODR_31_2hz_LPF_7_8hz;
		pADXL355->LowPassFreqeuncy = 7.813;
	}
	else if(LPF > 1.953){
		pADXL355->Register.Parameter.Filter.ODR_LPF = ADXL355_ODR_15_6hz_LPF_3_9hz;
		pADXL355->LowPassFreqeuncy = 3.906;
	}
	else if(LPF > 0.977){
		pADXL355->Register.Parameter.Filter.ODR_LPF = ADXL355_ODR_7_8hz_LPF_1_9hz;
		pADXL355->LowPassFreqeuncy = 1.953;
	}
	else{
		pADXL355->Register.Parameter.Filter.ODR_LPF = ADXL355_ODR_3_9_2hz_LPF_0_9hz;
		pADXL355->LowPassFreqeuncy = 0.977;
	}

	pADXL355->DataRate = pADXL355->LowPassFreqeuncy * 4;

	if(HPF > (0.00062084 * pADXL355->DataRate)){
		pADXL355->HighPassFreqeuncy = 0.00247 * pADXL355->DataRate;
		pADXL355->Register.Parameter.Filter.HPF_CORNER = ADXL355_HPF_24_7;
	}
	else if(HPF > (0.00015545 * pADXL355->DataRate)){
		pADXL355->HighPassFreqeuncy = 0.00062084 * pADXL355->DataRate;
		pADXL355->Register.Parameter.Filter.HPF_CORNER = ADXL355_HPF_6_2084;
	}
	else if(HPF > (0.00003862 * pADXL355->DataRate)){
		pADXL355->HighPassFreqeuncy = 0.00015545 * pADXL355->DataRate;
		pADXL355->Register.Parameter.Filter.HPF_CORNER = ADXL355_HPF_1_5545;
	}
	else if(HPF > (0.00000954 * pADXL355->DataRate)){
		pADXL355->HighPassFreqeuncy = 0.00003862 * pADXL355->DataRate;
		pADXL355->Register.Parameter.Filter.HPF_CORNER = ADXL355_HPF_0_3862;
	}
	else if(HPF > (0.00000238 * pADXL355->DataRate)){
		pADXL355->HighPassFreqeuncy = 0.00000954 * pADXL355->DataRate;
		pADXL355->Register.Parameter.Filter.HPF_CORNER = ADXL355_HPF_0_0954;
	}
	else if(HPF > 0){
		pADXL355->HighPassFreqeuncy = 0.00000238 * pADXL355->DataRate;
		pADXL355->Register.Parameter.Filter.HPF_CORNER = ADXL355_HPF_0_0238;
	}
	else{
		pADXL355->HighPassFreqeuncy = 0;
		pADXL355->Register.Parameter.Filter.HPF_CORNER = ADXL355_HPF_NOFILTER;
	}
}

oResult_t ADXL355_Open(I2C_HandleTypeDef *pI2C, ADXL355_t *pADXL355)
{
	oResult_t result = RESULT_RUN;

	if(pI2C == NULL || pADXL355 == NULL){
		return RESULT_ERROR;
	}

	if(pADXL355->State.IsOpen && !pADXL355->State.IsBusy){
		return RESULT_OK;
	}
	else if(!pADXL355->State.IsBusy && pADXL355->State.SequenceStep != 0){
		memset(&pADXL355->State, 0, sizeof(pADXL355->State));

		pADXL355->State.SequenceStep = 0;
		pADXL355->State.IsBusy = 1;
		pADXL355->State.IsRun = 0;
	}

	switch(pADXL355->State.SequenceStep)
	{
		case 0:
			HAL_I2C_DeInit(pI2C);
			pADXL355->State.SequenceStep++;
			break;
		case 1:
			if(pI2C->State != HAL_I2C_STATE_RESET){
				pADXL355->pI2C = pI2C;
				pADXL355->State.IsOpen = 1;
				pADXL355->State.SequenceStep++;
			}
			else if(HAL_I2C_Init(pI2C) != HAL_OK)
			{
				result = RESULT_ERROR;
			}
			break;
		case 2:
			if(ADXL355_GetParameter(pADXL355) == RESULT_OK){
				pADXL355->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 3:
			pADXL355->Register.Parameter.POWER_CTL.Byte = 0; //clear
			pADXL355->State.SequenceStep++;
		case 4:
			if(ADXL355_SetParameter(pADXL355) == RESULT_OK){
				pADXL355->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 5:
			result = RESULT_OK;
			pADXL355->State.IsRun = 1;
			break;
	}

	if(result != RESULT_RUN){
		pADXL355->State.SequenceStep = 0;
		pADXL355->State.IsBusy = 0;
	}

	return result;
}

oResult_t ADXL355_Close(ADXL355_t *pADXL355)
{
	oResult_t result = RESULT_RUN;

	if(!pADXL355->State.IsOpen){
		return RESULT_OK;
	}

	switch(pADXL355->State.SequenceStep)
	{
		case 0:
			pADXL355->State.IsBusy = 1;

			pADXL355->Register.Parameter.POWER_CTL.Standby = 1;
			pADXL355->Register.Parameter.POWER_CTL.DRDY_OFF = 1;
			pADXL355->Register.Parameter.POWER_CTL.TEMP_OFF = 1;
			pADXL355->Register.Parameter.FIFO_SAMPLES = 0;
			pADXL355->Register.Parameter.ACT_EN = 0;
			pADXL355->Register.Parameter.INT_MAP.Byte = 0;
			pADXL355->State.SequenceStep++;
		case 1:
			if(ADXL355_SetParameter(pADXL355) == RESULT_OK){
				pADXL355->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 2:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		HAL_I2C_DeInit(pADXL355->pI2C);
		memset(&pADXL355->State, 0, sizeof(pADXL355->State));
	}

	return result;
}

/* History

2026-06-26 | v0.1
	- baseline (MEMS_ADXL355BEZRL7.c)
*/
