/*
 * TEMP_TMP1075.c
 *
 *  Created on: Aug 4, 2025
 *      Author: JONE
 */
#include "TEMP_TMP1075.h"

#define TMP1075_I2C_ADDRESS 	(0x48 << 1)

oResult_t TMP1075_Write(TMP1075_t *pTMP1075, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	oResult_t result = RESULT_RUN;

	if(!pTMP1075->State.IsOpen){
		return RESULT_ERROR;
	}
	else if(pTMP1075->State.IsReadMode){
		return RESULT_BUSY;
	}
	else if(!pTMP1075->State.IsWriteMode && pTMP1075->State.ProcessStep){
		pTMP1075->State.ProcessStep = 0;
	}

	switch(pTMP1075->State.ProcessStep)
	{
		case 0:
			if(pTMP1075->pI2C->State == HAL_I2C_STATE_READY){
				pTMP1075->State.IsWriteMode = 1;
				pTMP1075->State.ProcessStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 1:
			if(HAL_I2C_Mem_Write(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, RegAddress, I2C_MEMADD_SIZE_8BIT, pData, SizeOfData, 500) == HAL_OK){
				result = RESULT_OK;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
	}

	if(result != RESULT_RUN){
		pTMP1075->State.ProcessStep = 0;
		pTMP1075->State.IsWriteMode = 0;
	}

	return result;
}

oResult_t TMP1075_Read(TMP1075_t *pTMP1075, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	oResult_t result = RESULT_RUN;

	if(!pTMP1075->State.IsOpen){
		return RESULT_ERROR;
	}
	else if(pTMP1075->State.IsWriteMode){
		return RESULT_BUSY;
	}
	else if(!pTMP1075->State.IsReadMode && pTMP1075->State.ProcessStep){
		pTMP1075->State.ProcessStep = 0;
	}

	switch(pTMP1075->State.ProcessStep)
	{
		case 0:
			if(pTMP1075->pI2C->State == HAL_I2C_STATE_READY){
				pTMP1075->State.IsReadMode = 1;
				pTMP1075->State.ProcessStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 1:
			if(HAL_I2C_Mem_Read(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, RegAddress, I2C_MEMADD_SIZE_8BIT, pData, SizeOfData, 500) == HAL_OK){
				result = RESULT_OK;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
	}

	if(result != RESULT_RUN){
		pTMP1075->State.ProcessStep = 0;
		pTMP1075->State.IsReadMode = 0;
	}

	return result;
}

oResult_t TMP1075_GetData(TMP1075_t *pTMP1075, uint8_t Count)
{
	oResult_t result = RESULT_RUN;

	if((result=TMP1075_Read(pTMP1075, TMP1075_REGADR_TEMP, (uint8_t *)&pTMP1075->Register.TEMP, sizeof(pTMP1075->Register.TEMP))) == RESULT_OK){

		if(pTMP1075->State.CountOfSampling == 0){
			pTMP1075->State.SumOfSampling = 0;
		}

		pTMP1075->State.SumOfSampling += TMP1075_TO_CELSIUS(pTMP1075->Register.TEMP);

		if(++pTMP1075->State.CountOfSampling >= Count){
			pTMP1075->Temperature = pTMP1075->State.SumOfSampling / (double)pTMP1075->State.CountOfSampling;
			pTMP1075->State.CountOfSampling = 0;
		}
		else{
			result = RESULT_RUN;
		}
	}

	return result;
}

oResult_t TMP1075_GetParameter(TMP1075_t *pTMP1075)
{
	if(!pTMP1075->State.IsOpen){
		return RESULT_ERROR;
	}

	if(pTMP1075->pI2C->State != HAL_I2C_STATE_READY){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Read(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_CFGR, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.CFGR, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Read(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_LLIM, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.LLIM, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Read(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_HLIM, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.HLIM, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Read(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_DIEID, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.DIEID, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

oResult_t TMP1075_SetParameter(TMP1075_t *pTMP1075)
{
	if(!pTMP1075->State.IsOpen){
		return RESULT_ERROR;
	}

	if(pTMP1075->pI2C->State != HAL_I2C_STATE_READY){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Write(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_CFGR, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.CFGR, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Write(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_LLIM, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.LLIM, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Write(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_HLIM, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.HLIM, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

oResult_t TMP1075_Open(I2C_HandleTypeDef *pI2C, TMP1075_t *pTMP1075)
{
	oResult_t result = RESULT_RUN;

	if(pI2C == NULL || pTMP1075 == NULL){
		return RESULT_ERROR;
	}

	if(pTMP1075->State.IsOpen && !pTMP1075->State.IsOpening){
		return RESULT_OK;
	}
	else if(!pTMP1075->State.IsOpening && pTMP1075->State.OpenStep != 0){
		memset(&pTMP1075->State, 0, sizeof(pTMP1075->State));

		pTMP1075->State.OpenStep = 0;
		pTMP1075->State.IsOpening = 1;
	}

	switch(pTMP1075->State.OpenStep)
	{
		case 0:
			HAL_I2C_DeInit(pI2C);
			pTMP1075->State.OpenStep++;
			break;
		case 1:
			if(pI2C->State != HAL_I2C_STATE_RESET){
				pTMP1075->pI2C = pI2C;
				pTMP1075->State.IsOpen = 1;
				pTMP1075->State.OpenStep++;
			}
			else if(HAL_I2C_Init(pI2C) != HAL_OK){
				result = RESULT_ERROR;
			}
			break;
		case 2:
			if(TMP1075_GetParameter(pTMP1075) == RESULT_OK){
				pTMP1075->State.OpenStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 3:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		pTMP1075->State.OpenStep = 0;
		pTMP1075->State.IsOpening = 0;
	}

	return result;
}

oResult_t TMP1075_Close(TMP1075_t *pTMP1075)
{
	if(!pTMP1075->State.IsOpen){
		return RESULT_OK;
	}

	HAL_I2C_DeInit(pTMP1075->pI2C);

	memset(&pTMP1075->State, 0, sizeof(pTMP1075->State));

	return RESULT_OK;
}
