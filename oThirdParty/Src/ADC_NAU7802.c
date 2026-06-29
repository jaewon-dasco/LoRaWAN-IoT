/*
 * ADC_NAU7802.c
 *
 *  Version: 0.1 (2026-06-29)
 */
#include "ONE_Time.h"
#include "ADC_NAU7802.h"

#ifdef __HAL_I2C_ENABLE

oResult_t NAU7802_WriteMemory(NAU7802_t *pDev, uint16_t MemoryReg, uint8_t Data)
{
	if(pDev == NULL || pDev->pI2C == NULL){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Write(pDev->pI2C, NAU7802_ADDRESS, MemoryReg, 1, &Data, 1, 100) == HAL_OK){
		return RESULT_OK;
	}

	return RESULT_ERROR;
}

oResult_t NAU7802_ReadMemory(NAU7802_t *pDev, uint16_t MemoryReg, uint8_t *pData)
{
	if(pDev == NULL || pDev->pI2C == NULL){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Read(pDev->pI2C, NAU7802_ADDRESS, MemoryReg, 1, pData, 1, 100) == HAL_OK){
		return RESULT_OK;
	}

	return RESULT_ERROR;
}

oResult_t NAU7802_ReadBit(NAU7802_t *pDev, uint16_t MemoryReg, uint8_t Bit)
{
	uint8_t Data = 0;

	if(NAU7802_ReadMemory(pDev, MemoryReg, &Data) == RESULT_OK){
		if(Data & Bit){
			return RESULT_OK;
		}
		else{
			return RESULT_NULL;
		}
	}

	return RESULT_ERROR;
}

oResult_t NAU7802_WriteBit(NAU7802_t *pDev, uint16_t MemoryReg, uint8_t Bit)
{
	uint8_t Data = 0;

	if(NAU7802_ReadMemory(pDev, MemoryReg, &Data) != RESULT_OK){
		return RESULT_ERROR;
	}

	Data |= Bit;

	if(NAU7802_WriteMemory(pDev, MemoryReg, Data) != RESULT_OK){
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

oResult_t NAU7802_ResetBit(NAU7802_t *pDev, uint16_t MemoryReg, uint8_t Bit, uint8_t Mask)
{
	uint8_t Data = 0;
	uint8_t CheckData = 0;

	if(NAU7802_ReadMemory(pDev, MemoryReg, &Data) != RESULT_OK){
		return RESULT_ERROR;
	}

	Data &= (uint8_t)(~Mask);
	Data |= Bit;

	if(NAU7802_WriteMemory(pDev, MemoryReg, Data) != RESULT_OK){
		return RESULT_ERROR;
	}

	if(NAU7802_ReadMemory(pDev, MemoryReg, &CheckData) != RESULT_OK){
		return RESULT_ERROR;
	}

	return Data == CheckData ? RESULT_OK : RESULT_ERROR;
}

/* NAU7802_SetVLDO
 * Input
 *   Voltage | Unit=1V
 */
oResult_t NAU7802_SetVLDO(NAU7802_t *pDev, NAU7802_VLDO_t VLDO)
{
	oResult_t result = RESULT_NULL;
	uint8_t Data = 0;
	double Voltage = 0;

	switch(VLDO)
	{
		default:
			VLDO = NAU7802_VLDO_4_5V;
		case NAU7802_VLDO_4_5V:
			Voltage = 4500;
			break;
		case NAU7802_VLDO_4_2V:
			Voltage = 4200;
			break;
		case NAU7802_VLDO_3_9V:
			Voltage = 3900;
			break;
		case NAU7802_VLDO_3_6V:
			Voltage = 3600;
			break;
		case NAU7802_VLDO_3_3V:
			Voltage = 3300;
			break;
		case NAU7802_VLDO_3_0V:
			Voltage = 3000;
			break;
		case NAU7802_VLDO_2_7V:
			Voltage = 2700;
			break;
		case NAU7802_VLDO_2_4V:
			Voltage = 2400;
			break;
	}

	if(pDev->VREF == Voltage){
		return RESULT_OK;
	}

	Data |= VLDO;

	if((result = NAU7802_ResetBit(pDev, NAU7802_REG_CTRL1, Data, NAU7802_CTRL1_VLDO_MASK)) != RESULT_OK){
		return result;
	}

	pDev->VREF = Voltage;
	pDev->Registers.VLDO = VLDO;

	return RESULT_OK;
}

/* NAU7802_SetGain
 * Input
 *   Gain 	| ADC = Vin x Gain / VREF x ADCMax | Uint=1
 *   VREF	| REFP-REFN = VREF 	| Uint=1V
 */
oResult_t NAU7802_SetGain(NAU7802_t *pDev, NAU7802_Gain_t Gain)
{
	oResult_t result = RESULT_NULL;
	uint8_t Data = 0;
	double AmpGain =0;

	switch(Gain)
	{
		case NAU7802_GAIN_X1:	AmpGain = 1;	break;
		case NAU7802_GAIN_X2:	AmpGain = 2;	break;
		case NAU7802_GAIN_X4:	AmpGain = 4;	break;
		case NAU7802_GAIN_X8:	AmpGain = 8;	break;
		case NAU7802_GAIN_X16:	AmpGain = 16;	break;
		case NAU7802_GAIN_X32:	AmpGain = 32;	break;
		case NAU7802_GAIN_X64:	AmpGain = 64;	break;
		case NAU7802_GAIN_X128:	AmpGain = 128;	break;
		case NAU7802_GAIN_BYPASS: AmpGain = 1;	break;
		default: return RESULT_ERROR;
	}

	if(pDev->Registers.Gain == Gain){
		return RESULT_OK;
	}

	if(Gain == NAU7802_GAIN_BYPASS){
		/* PGA bypass: PGA_CTRL.BYPASS = 1 (CTRL1.GAIN은 변경 안 함) */
		if((result = NAU7802_WriteBit(pDev, NAU7802_REG_PGA_CTRL, NAU7802_PGA_BYPASS_ENABLE)) != RESULT_OK){
			return result;
		}
	}
	else{
		/* PGA active: 먼저 BYPASS 비트 클리어 (이전이 BYPASS였을 수 있음) → CTRL1.GAIN 설정 */
		if((result = NAU7802_ResetBit(pDev, NAU7802_REG_PGA_CTRL, 0, NAU7802_PGA_BYPASS_ENABLE)) != RESULT_OK){
			return result;
		}

		Data = (uint8_t)Gain & NAU7802_CTRL1_GAIN_MASK;
		if((result = NAU7802_ResetBit(pDev, NAU7802_REG_CTRL1, Data, NAU7802_CTRL1_GAIN_MASK)) != RESULT_OK){
			return result;
		}
	}

	pDev->AmpGain = AmpGain;
	pDev->Registers.Gain = Gain;

	return RESULT_OK;
}

/* NAU7802_SetSampling
 * Input
 *   Channel 	| Input max voltage | Uint=1V
 *   VREF 		| REFP-REFN = VREF 	| Uint=1V
 */
oResult_t NAU7802_SetSampling(NAU7802_t *pDev, uint8_t Channel, NAU7802_SamplingRate_t SamplingRate)
{
	oResult_t result = RESULT_NULL;
	uint8_t RegChannel = 0;
	uint8_t RegSamplingRate = 0;

	if(Channel >= 2){
		RegChannel = NAU7802_CTRL2_CHS_CH2;
	}

	RegSamplingRate = (SamplingRate & NAU7802_CTRL2_CRS_MASK);

	if(pDev->Registers.Channel == RegChannel && pDev->Registers.SamplingRate == RegSamplingRate){
		return RESULT_OK;
	}

	if((result = NAU7802_ResetBit(pDev, NAU7802_REG_CTRL2, RegChannel|RegSamplingRate, NAU7802_CTRL2_CHS_MASK|NAU7802_CTRL2_CRS_MASK)) != RESULT_OK){
		return result;
	}

	pDev->Registers.Channel = RegChannel;
	pDev->Registers.SamplingRate = RegSamplingRate;
	pDev->CurrentChannel = Channel;

	return RESULT_OK;
}

/* NAU7802_Init
 * Input
 *   pI2C | HAL_I2C pointer
 *   VLDO | VLDO output voltage | Uint=1V
 */
oResult_t NAU7802_Init(NAU7802_t *pDev, I2C_HandleTypeDef *pI2C, NAU7802_VLDO_t VLDO)
{
	oResult_t result = RESULT_RUN;
	uint8_t Data = 0;

	if(pDev == NULL || pI2C == NULL){
		return RESULT_NULL;
	}

	if(!pDev->IsOpening && pDev->ProcessStep){
		pDev->ProcessStep = 0;
		pDev->IsOpening = 1;
	}

	switch(pDev->ProcessStep)
	{
		case 0:
			pDev->pI2C = pI2C;

			NAU7802_DeInit(pDev);
			pDev->Timer = oTMR_GetTick(TICKBASE_SYSTICK);
			pDev->ErrorCount = 0;
			pDev->ProcessStep++;
		case 1:
			if(oTMR_Trigger(&pDev->Timer, 10, 1, TICKBASE_SYSTICK)){
				if(pI2C->State != HAL_I2C_STATE_RESET){
					pDev->Timer = oTMR_GetTick(TICKBASE_SYSTICK);
					pDev->ProcessStep++;
				}
				else{
					HAL_I2C_Init(pI2C);
				}
			}
			break;
		case 2:
			if(oTMR_Trigger(&pDev->Timer, 10, 1, TICKBASE_SYSTICK)){
				pDev->ProcessStep++;
			}
			else if(pDev->IsOpen){
				result = RESULT_OK;
			}
			break;
		case 3:
			if(NAU7802_ReadMemory(pDev, NAU7802_REG_CHIP_ID, &Data) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				result = RESULT_ERROR;
			}
			break;
		case 4:
			if(NAU7802_WriteBit(pDev, NAU7802_REG_PUCTRL, NAU7802_PUCTRL_RR) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				result = RESULT_ERROR;
			}
			break;
		case 5:
			if(NAU7802_ResetBit(pDev, NAU7802_REG_PUCTRL, 0, NAU7802_PUCTRL_RR) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				result = RESULT_ERROR;
			}
			break;
		case 6:
			if(NAU7802_WriteBit(pDev, NAU7802_REG_PUCTRL, NAU7802_PUCTRL_PUD) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				result = RESULT_ERROR;
			}
			break;
		case 7:
			switch(NAU7802_ReadBit(pDev, NAU7802_REG_PUCTRL, NAU7802_PUCTRL_PUR))
			{
				default:
					break;
				case RESULT_OK:
					pDev->ProcessStep++;
					break;
			}
			break;
		case 8:
			if(NAU7802_WriteBit(pDev, NAU7802_REG_PUCTRL, NAU7802_PUCTRL_PUA|NAU7802_PUCTRL_CS|NAU7802_PUCTRL_AVDDS) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				result = RESULT_ERROR;
			}
			break;
		case 9:
			if(NAU7802_WriteBit(pDev, NAU7802_REG_ADC_CTRL, 0x30) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				result = RESULT_ERROR;
			}
			break;
		case 10:
			if(NAU7802_WriteBit(pDev, NAU7802_REG_CTRL1, NAU7802_CTRL1_GAIN_X1) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				result = RESULT_ERROR;
			}
			break;
		case 11:
			if(NAU7802_SetVLDO(pDev, VLDO) ==  RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				result = RESULT_ERROR;
			}
			break;
		default:
			result = RESULT_OK;
			pDev->IsOpen = 1;
			pDev->OpenTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);
			break;
	}

	if(result != RESULT_RUN){
		pDev->ProcessStep = 0;
		pDev->ErrorCount = 0;
		pDev->IsOpening = 0;
	}

	return result;
}

/* NAU7802_DeInit */
oResult_t NAU7802_DeInit(NAU7802_t *pDev)
{
	pDev->ProcessStep = 0;
	pDev->ReadData = 0;
	pDev->AmpGain = 0;
	pDev->CurrentChannel = 0;
	pDev->OpenTimestamp = 0;

	pDev->Registers.Channel = 0;
	pDev->Registers.Gain = 0;
	pDev->Registers.VLDO = 0;
	pDev->Registers.SamplingRate = 0;

	if(pDev->pI2C == NULL || !pDev->IsOpen){
		return RESULT_NULL;
	}

	HAL_I2C_DeInit(pDev->pI2C);

	pDev->pI2C = NULL;
	pDev->IsOpen = 0;

	return RESULT_OK;
}

oResult_t NAU7802_IsDataReady(NAU7802_t *pDev)
{
	uint8_t Data = 0;

	if(!pDev->IsOpen){
		return RESULT_ERROR;
	}

	if(NAU7802_ReadMemory(pDev, NAU7802_REG_PUCTRL, &Data) != RESULT_OK){
		return RESULT_ERROR;
	}

	if(Data & NAU7802_PUCTRL_CR){
		return RESULT_OK;
	}

	return RESULT_NULL;
}

oResult_t NAU7802_ConversionStop(NAU7802_t *pDev)
{
	return NAU7802_ResetBit(pDev, NAU7802_REG_PUCTRL, 0, NAU7802_PUCTRL_CS);
}

/* NAU7802_ADCRead
 * Input
 *   Channel 		| Channel number 		| 1 or 2
 *   SamplingRate 	| Sampling frequency 	| Uint=1Hz
 *   pADC 			| ADC output pointer 	| 24bit data
 */
oResult_t NAU7802_ADCRead(NAU7802_t *pDev, uint8_t Channel, NAU7802_SamplingRate_t SamplingRate, int32_t *pADC)
{
	oResult_t result = RESULT_RUN;
	uint8_t Data = 0;

	if(!pDev->IsOpen || pDev->pI2C == NULL){
		return RESULT_ERROR;
	}

	if(!pDev->IsADConversion && pDev->ProcessStep){
		pDev->ProcessStep = 0;
		pDev->IsADConversion = 1;
	}

	switch(pDev->ProcessStep)
	{
		case 0:
			if((result = NAU7802_SetSampling(pDev, Channel, SamplingRate)) == RESULT_OK){
				pDev->ProcessStep++;
				result = RESULT_RUN;
			}
			break;
		case 1:
			if((result = NAU7802_WriteBit(pDev, NAU7802_REG_PUCTRL, NAU7802_PUCTRL_CS)) == RESULT_OK){
				pDev->ProcessStep++;
				result = RESULT_RUN;
			}
			break;
		case 2:
			switch(NAU7802_ReadBit(pDev, NAU7802_REG_PUCTRL, NAU7802_PUCTRL_CR))
			{
				case RESULT_OK:
					pDev->ProcessStep++;
					break;
				default:
					result = RESULT_RUN;
					break;
				case RESULT_ERROR:
					result = RESULT_ERROR;
					break;
			}
			break;
		case 3:
			if((result = NAU7802_ReadMemory(pDev, NAU7802_REG_ADC_B2, &Data)) == RESULT_OK){
				pDev->ReadData = (uint32_t)Data;
				pDev->ReadData <<= 8;

				pDev->ProcessStep++;
				result = RESULT_RUN;
			}
			else{
				break;
			}
		case 4:
			if((result = NAU7802_ReadMemory(pDev, NAU7802_REG_ADC_B1, &Data)) == RESULT_OK){
				pDev->ReadData |= (uint32_t)Data;
				pDev->ReadData <<= 8;

				pDev->ProcessStep++;
				result = RESULT_RUN;
			}
			else{
				break;
			}
		case 5:
			if((result = NAU7802_ReadMemory(pDev, NAU7802_REG_ADC_B0, &Data)) == RESULT_OK){
				pDev->ReadData |= (uint32_t)Data;
				pDev->ReadData &= 0x00FFFFFF;

				if(pDev->ReadData > (NAU7802_ADC_MAXVALUE/2)){
					pDev->ReadData -= NAU7802_ADC_MAXVALUE;
				}

				*pADC = pDev->ReadData;
			}
			break;
	}

	if(result != RESULT_RUN){
		pDev->ProcessStep = 0;
		pDev->IsADConversion = 0;
	}

	return result;
}

/* NAU7802_AnalogRead
 * Input
 *   Channel 		| Channel number 					| 1 or 2
 *   pAnalog 		| Analog output pointer 			| Unit=mV
 */
oResult_t NAU7802_AnalogRead(NAU7802_t *pDev, uint8_t Channel, NAU7802_SamplingRate_t SamplingRate, double *pAnalog)
{
	int32_t ReadCount=0;
	int32_t ADC=0;
	oResult_t result = RESULT_RUN;

	if(!pDev->IsOpen || pDev->pI2C == NULL){
		return RESULT_ERROR;
	}

	if(pDev->CurrentChannel != Channel){
		while(ReadCount < 3){
			if(NAU7802_ADCRead(pDev, Channel, SamplingRate, &ADC) != RESULT_RUN){
				ReadCount++;
			}
		}
	}

	if((result = NAU7802_ADCRead(pDev, Channel, SamplingRate, &ADC)) == RESULT_OK){
		*pAnalog = (double)(ADC) / (double)(NAU7802_ADC_MAXVALUE/2) * (pDev->VREF/2 / MATH_MAX(pDev->AmpGain, 1));

		if(Channel == 1){
			ADC = 0;
		}
	}

	return result;
}

oResult_t NAU7802_AnalogOverSampling(NAU7802_t *pDev, uint8_t Channel, NAU7802_SamplingRate_t SamplingRate, uint8_t SamplingCount, double *pAnalog)
{
	double ReadAnalog = 0;
	oResult_t result = RESULT_RUN;

	if(SamplingCount == 0 || pAnalog == NULL){
		pDev->OverSampling.ReadCount = 0;
		return RESULT_NULL;
	}

	if(!pDev->OverSampling.ReadCount){
		pDev->OverSampling.SumOfAnalog = 0;
	}

	switch(NAU7802_AnalogRead(pDev, Channel, SamplingRate, &ReadAnalog))
	{
		default:
			break;
		case RESULT_ERROR:
			if(++pDev->ErrorCount >= 3){
				result = RESULT_ERROR;
			}
			break;
		case RESULT_OK:
			pDev->ErrorCount = 0;
			pDev->OverSampling.SumOfAnalog += ReadAnalog;

			if(++pDev->OverSampling.ReadCount >= SamplingCount){
				result = RESULT_OK;
			}
			break;
	}

	if(result != RESULT_RUN){
		if(pDev->OverSampling.ReadCount > 0){
			*pAnalog = pDev->OverSampling.SumOfAnalog/(double)pDev->OverSampling.ReadCount;
		}

		pDev->OverSampling.ReadCount = 0;
	}

	return result;
}
#endif

/* History

2026-06-26 | v0.1
	- baseline (ADC_NAU7802.c)
*/
