/*
 * ONE_Filter.c
 *
 *  Version: 0.1 (2026-06-29)
 */

#include "ONE_Filter.h"

/* ============================================================
 * Common Biquad Processing (Direct Form I)
 * ============================================================ */
static double oFilter_Process(oFilter_t *f, double input)
{
	double output;

	output = f->b0 * input + f->b1 * f->x1 + f->b2 * f->x2
	                       - f->a1 * f->y1 - f->a2 * f->y2;

	/* Update delay line */
	f->x2 = f->x1;
	f->x1 = input;
	f->y2 = f->y1;
	f->y1 = output;

	return output;
}

/* ============================================================
 * Reset delay line and seed with first input
 * ============================================================ */
static void oFilter_ResetState(oFilter_t *f, double input)
{
	f->x1 = input;
	f->x2 = input;
	f->y1 = input;
	f->y2 = input;
	f->Reset = 0;
}

/* ============================================================
 * Low Pass Filter
 * ============================================================ */
double oFilter_LPF(oFilter_t *pFilter, double input)
{
	if(pFilter == NULL){
		return input;
	}

	if(pFilter->Reset){
		if(pFilter->SamplingFreq <= 0 || pFilter->CutoffFreq <= 0 || pFilter->Q <= 0){
			return input;
		}

		double w0 = 2.0 * M_PI * pFilter->CutoffFreq / pFilter->SamplingFreq;
		double cosw0 = cos(w0);
		double alpha = sin(w0) / (2.0 * pFilter->Q);
		double a0 = 1.0 + alpha;

		pFilter->b0 = ((1.0 - cosw0) / 2.0) / a0;
		pFilter->b1 = (1.0 - cosw0) / a0;
		pFilter->b2 = ((1.0 - cosw0) / 2.0) / a0;
		pFilter->a1 = (-2.0 * cosw0) / a0;
		pFilter->a2 = (1.0 - alpha) / a0;

		oFilter_ResetState(pFilter, input);
	}

	return oFilter_Process(pFilter, input);
}

/* ============================================================
 * High Pass Filter
 * ============================================================ */
double oFilter_HPF(oFilter_t *pFilter, double input)
{
	if(pFilter == NULL){
		return input;
	}

	if(pFilter->Reset){
		if(pFilter->SamplingFreq <= 0 || pFilter->CutoffFreq <= 0 || pFilter->Q <= 0){
			return input;
		}

		double w0 = 2.0 * M_PI * pFilter->CutoffFreq / pFilter->SamplingFreq;
		double cosw0 = cos(w0);
		double alpha = sin(w0) / (2.0 * pFilter->Q);
		double a0 = 1.0 + alpha;

		pFilter->b0 = ((1.0 + cosw0) / 2.0) / a0;
		pFilter->b1 = (-(1.0 + cosw0)) / a0;
		pFilter->b2 = ((1.0 + cosw0) / 2.0) / a0;
		pFilter->a1 = (-2.0 * cosw0) / a0;
		pFilter->a2 = (1.0 - alpha) / a0;

		oFilter_ResetState(pFilter, input);
	}

	return oFilter_Process(pFilter, input);
}

/* ============================================================
 * Band Pass Filter (constant skirt gain)
 * ============================================================ */
double oFilter_BPF(oFilter_t *pFilter, double input)
{
	if(pFilter == NULL){
		return input;
	}

	if(pFilter->Reset){
		if(pFilter->SamplingFreq <= 0 || pFilter->CutoffFreq <= 0 || pFilter->Q <= 0){
			return input;
		}

		double w0 = 2.0 * M_PI * pFilter->CutoffFreq / pFilter->SamplingFreq;
		double cosw0 = cos(w0);
		double alpha = sin(w0) / (2.0 * pFilter->Q);
		double a0 = 1.0 + alpha;

		pFilter->b0 = alpha / a0;
		pFilter->b1 = 0.0;
		pFilter->b2 = -alpha / a0;
		pFilter->a1 = (-2.0 * cosw0) / a0;
		pFilter->a2 = (1.0 - alpha) / a0;

		oFilter_ResetState(pFilter, input);
	}

	return oFilter_Process(pFilter, input);
}

/* ============================================================
 * Low Shelf Filter
 * ============================================================ */
double oFilter_LSF(oFilter_t *pFilter, double input)
{
	if(pFilter == NULL){
		return input;
	}

	if(pFilter->Reset){
		if(pFilter->SamplingFreq <= 0 || pFilter->CutoffFreq <= 0 || pFilter->Q <= 0){
			return input;
		}

		double A = pow(10.0, pFilter->GainDB / 40.0);
		double w0 = 2.0 * M_PI * pFilter->CutoffFreq / pFilter->SamplingFreq;
		double cosw0 = cos(w0);
		double alpha = sin(w0) / (2.0 * pFilter->Q);
		double sqrtA2alpha = 2.0 * sqrt(A) * alpha;

		double a0 = (A + 1.0) + (A - 1.0) * cosw0 + sqrtA2alpha;

		pFilter->b0 = (A * ((A + 1.0) - (A - 1.0) * cosw0 + sqrtA2alpha)) / a0;
		pFilter->b1 = (2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0)) / a0;
		pFilter->b2 = (A * ((A + 1.0) - (A - 1.0) * cosw0 - sqrtA2alpha)) / a0;
		pFilter->a1 = (-2.0 * ((A - 1.0) + (A + 1.0) * cosw0)) / a0;
		pFilter->a2 = ((A + 1.0) + (A - 1.0) * cosw0 - sqrtA2alpha) / a0;

		oFilter_ResetState(pFilter, input);
	}

	return oFilter_Process(pFilter, input);
}

/* ============================================================
 * High Shelf Filter
 * ============================================================ */
double oFilter_HSF(oFilter_t *pFilter, double input)
{
	if(pFilter == NULL){
		return input;
	}

	if(pFilter->Reset){
		if(pFilter->SamplingFreq <= 0 || pFilter->CutoffFreq <= 0 || pFilter->Q <= 0){
			return input;
		}

		double A = pow(10.0, pFilter->GainDB / 40.0);
		double w0 = 2.0 * M_PI * pFilter->CutoffFreq / pFilter->SamplingFreq;
		double cosw0 = cos(w0);
		double alpha = sin(w0) / (2.0 * pFilter->Q);
		double sqrtA2alpha = 2.0 * sqrt(A) * alpha;

		double a0 = (A + 1.0) - (A - 1.0) * cosw0 + sqrtA2alpha;

		pFilter->b0 = (A * ((A + 1.0) + (A - 1.0) * cosw0 + sqrtA2alpha)) / a0;
		pFilter->b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0)) / a0;
		pFilter->b2 = (A * ((A + 1.0) + (A - 1.0) * cosw0 - sqrtA2alpha)) / a0;
		pFilter->a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cosw0)) / a0;
		pFilter->a2 = ((A + 1.0) - (A - 1.0) * cosw0 - sqrtA2alpha) / a0;

		oFilter_ResetState(pFilter, input);
	}

	return oFilter_Process(pFilter, input);
}

/* ============================================================
 * Moving Average - Set Data
 * ============================================================ */
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

/* ============================================================
 * Moving Average - Get Data
 * ============================================================ */
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

/* ============================================================
 * Moving Average - Get Range (Min/Max)
 * ============================================================ */
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
