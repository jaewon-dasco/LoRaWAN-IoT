/*
 * ONE_Math.c
 *
 *  Created on: Feb 7, 2025
 *      Author: JONE
 */
#include "ONE_Math.h"

uint8_t oMoveAverage_SetData(oMoveAverage_t *pAvg, void *pData)
{
    if (!pAvg || !pData || pAvg->LengthOfBuffer == 0){
        return 0;
    }

    pAvg->MaxSumCount = MATH_MIN(MOVEAVERAGE_MAX_DATACOUNT, pAvg->MaxSumCount);

    if(pAvg->IsFull && pAvg->CountOfData != pAvg->MaxSumCount){
    	pAvg->Reset = 1;
    }

    if(pAvg->Reset){
    	memset(pAvg->pBuffer, 0, pAvg->LengthOfBuffer * SIZE_OF_DATATYPE(pAvg->DataType));
    	memset(&pAvg->SumOfData, 0, sizeof(pAvg->SumOfData));

    	pAvg->LengthOfBuffer = MATH_MIN(MOVEAVERAGE_MAX_DATACOUNT, pAvg->LengthOfBuffer);
    	pAvg->MaxSumCount = MATH_LIMIT(pAvg->MaxSumCount, 1, pAvg->LengthOfBuffer);

    	pAvg->IndexOfData = 0;
    	pAvg->CountOfData = 0;
    	pAvg->Reset = 0;
    	pAvg->IsFull = 0;
    }

    if (!pAvg->IsFull && pAvg->CountOfData >= pAvg->MaxSumCount) {
    	pAvg->IsFull = 1;
    }

    uint8_t* pCurr = (uint8_t*)pAvg->pBuffer + (pAvg->IndexOfData * SIZE_OF_DATATYPE(pAvg->DataType));

    if(pAvg->IsFull){
		switch (pAvg->DataType) {
			case DataType_Int8:    pAvg->SumOfData.s64 -= *(int8_t*)pCurr; break;
			case DataType_UInt8:   pAvg->SumOfData.s64 -= *(uint8_t*)pCurr; break;
			case DataType_Int16:   pAvg->SumOfData.s64 -= *(int16_t*)pCurr; break;
			case DataType_UInt16:  pAvg->SumOfData.s64 -= *(uint16_t*)pCurr; break;
			case DataType_Int32:   pAvg->SumOfData.s64 -= *(int32_t*)pCurr; break;
			case DataType_UInt32:  pAvg->SumOfData.s64 -= *(uint32_t*)pCurr; break;
			case DataType_Int64:   pAvg->SumOfData.s64 -= *(int64_t*)pCurr; break;
			case DataType_UInt64:  pAvg->SumOfData.u64 -= *(uint64_t*)pCurr; break;
			case DataType_Float:   pAvg->SumOfData.f   -= *(float*)pCurr; break;
			case DataType_Double:  pAvg->SumOfData.d   -= *(double*)pCurr; break;
			default: return 0;
		}
    }

    memcpy(pCurr, pData, SIZE_OF_DATATYPE(pAvg->DataType));

    switch (pAvg->DataType) {
        case DataType_Int8:    pAvg->SumOfData.s64 += *(int8_t*)pCurr; break;
        case DataType_UInt8:   pAvg->SumOfData.s64 += *(uint8_t*)pCurr; break;
        case DataType_Int16:   pAvg->SumOfData.s64 += *(int16_t*)pCurr; break;
        case DataType_UInt16:  pAvg->SumOfData.s64 += *(uint16_t*)pCurr; break;
        case DataType_Int32:   pAvg->SumOfData.s64 += *(int32_t*)pCurr; break;
        case DataType_UInt32:  pAvg->SumOfData.s64 += *(uint32_t*)pCurr; break;
        case DataType_Int64:   pAvg->SumOfData.s64 += *(int64_t*)pCurr; break;
        case DataType_UInt64:  pAvg->SumOfData.u64 += *(uint64_t*)pCurr; break;
        case DataType_Float:   pAvg->SumOfData.f   += *(float*)pCurr; break;
        case DataType_Double:  pAvg->SumOfData.d   += *(double*)pCurr; break;
        default: return 0;
    }

    pAvg->IndexOfData = (pAvg->IndexOfData + 1) % MATH_MAX(pAvg->MaxSumCount, 1);
    pAvg->CountOfData = MATH_LIMIT(pAvg->CountOfData + 1, 0, pAvg->MaxSumCount);

    return 1;
}

uint8_t oMoveAverage_GetData(oMoveAverage_t *pAvg, void *pOut)
{
    if (!pAvg || !pOut || pAvg->CountOfData == 0)
        return 0;

    switch (pAvg->DataType) {
        case DataType_Int8:    *(int8_t*)pOut = (int8_t)(pAvg->SumOfData.s64 / (int64_t)pAvg->CountOfData); break;
        case DataType_UInt8:   *(uint8_t*)pOut = (uint8_t)(pAvg->SumOfData.s64 / (int64_t)pAvg->CountOfData); break;
        case DataType_Int16:   *(int16_t*)pOut = (int16_t)(pAvg->SumOfData.s64 / (int64_t)pAvg->CountOfData); break;
        case DataType_UInt16:  *(uint16_t*)pOut = (uint16_t)(pAvg->SumOfData.s64 / (int64_t)pAvg->CountOfData); break;
        case DataType_Int32:   *(int32_t*)pOut = (int32_t)(pAvg->SumOfData.s64 / (int64_t)pAvg->CountOfData); break;
        case DataType_UInt32:  *(uint32_t*)pOut = (uint32_t)(pAvg->SumOfData.s64 / (int64_t)pAvg->CountOfData); break;
        case DataType_Int64:   *(int64_t*)pOut = pAvg->SumOfData.s64 / (int64_t)pAvg->CountOfData; break;
        case DataType_UInt64:  *(uint64_t*)pOut = pAvg->SumOfData.u64 / (uint64_t)pAvg->CountOfData; break;
        case DataType_Float:   *(float*)pOut = pAvg->SumOfData.f / (float)pAvg->CountOfData; break;
        case DataType_Double:  *(double*)pOut = pAvg->SumOfData.d / (double)pAvg->CountOfData; break;
        default: return 0;
    }

    return 1;
}

oRange_t oMoveAverage_GetRange(oMoveAverage_t *pAvg)
{
	oRange_t result = RANGE_INITIALIZER();
	double data;

    if (!pAvg || pAvg->CountOfData == 0){
    	result.Min = 0;
    	result.Max = 0;
        return result;
    }

	for(uint32_t i=0; i<pAvg->CountOfData; i++){
		uint8_t* pBuf = (uint8_t*)pAvg->pBuffer + (i * SIZE_OF_DATATYPE(pAvg->DataType));

		switch (pAvg->DataType) {
			case DataType_Int8:    data = (double)*((int8_t*)pBuf); break;
			case DataType_UInt8:   data = (double)*((uint8_t*)pBuf); break;
			case DataType_Int16:   data = (double)*((int16_t*)pBuf); break;
			case DataType_UInt16:  data = (double)*((uint16_t*)pBuf); break;
			case DataType_Int32:   data = (double)*((int32_t*)pBuf); break;
			case DataType_UInt32:  data = (double)*((uint32_t*)pBuf); break;
			case DataType_Int64:   data = (double)*((int64_t*)pBuf); break;
			case DataType_UInt64:  data = (double)*((uint64_t*)pBuf); break;
			case DataType_Float:   data = (double)*((float*)pBuf); break;
			case DataType_Double:  data = 		  *((double*)pBuf); break;
			default: return result;
		}

		result.Min = MATH_MIN(result.Min, data);
		result.Max = MATH_MAX(result.Max, data);
	}

	return result;
}

double oLowPassFilter(oLowPassFilter_t *pFilter, double input)
{
    if (pFilter == NULL) {
        return input; // Return input if filter is not initialized
    }

    if(pFilter->Reset){
    	if(pFilter->SamplingFreq == 0 || pFilter->CutoffFreq == 0){
    		pFilter->alpha = 0;
    	}
    	else{
    		pFilter->alpha = ((1.0 / (double)(pFilter->SamplingFreq)) / ( (1.0 / (2.0 * MATH_PI * (double)(pFilter->CutoffFreq))) + (1.0 / (double)(pFilter->SamplingFreq))));
    	}

    	pFilter->output = input;
    	pFilter->Reset = 0;
    }

	if (pFilter->alpha < 0.0 || pFilter->alpha > 1.0) {
		pFilter->output = input; // Return input if alpha is out of bounds
	}
	else{
		// Apply the low-pass filter formula
		pFilter->output = pFilter->alpha * input + (1.0 - pFilter->alpha) * pFilter->output;
	}

    return pFilter->output;
}

oRPY_t oMath_AccelationToRPY(oVector3_t Acc)
{
	oRPY_t RPY = {0, 0, 0};

	if(Acc.Y != 0 || Acc.Z != 0){
		RPY.Pitch = RADIAN_TO_DEGREE(atan(Acc.X / sqrtf(pow(Acc.Y,2) + pow(Acc.Z,2))));
	}

	if(Acc.X != 0 || Acc.Z != 0){
		RPY.Roll = RADIAN_TO_DEGREE(atan(Acc.Y / sqrtf(pow(Acc.X,2) + pow(Acc.Z,2))));
	}

	return RPY;
}

double oMath_Polynomial(double InData, double Polynomial[], uint8_t NumberOfPolynomial)
{
	uint8_t IsEmpty = 1;
	double OutData = 0;

	if(Polynomial == NULL || NumberOfPolynomial > 20){
		return InData;
	}

	for(int i=0; i<NumberOfPolynomial; i++)
	{
		OutData +=  Polynomial[i] * pow(InData, (double)i);

		if(Polynomial[i] != 0){
			IsEmpty = 0;
		}
	}

	if(IsEmpty){
		return InData;
	}

	return OutData;
}

double oMath_Median3(double a, double b, double c)
{
    if (a > b){ double t=a; a=b; b=t; }
    if (b > c){ double t=b; b=c; c=t; }
    if (a > b){ double t=a; a=b; b=t; }

	return b;
}
