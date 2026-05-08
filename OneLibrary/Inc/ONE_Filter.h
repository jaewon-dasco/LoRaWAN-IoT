/*
 * ONE_Filter.h
 *
 *  Created on: Apr 14, 2026
 *      Author: JONE
 *
 * 2nd-order Biquad IIR Filters (Audio EQ Cookbook)
 * LPF, HPF, BPF, Low Shelf, High Shelf
 * Moving Average Filter
 */

#ifndef INC_ONE_FILTER_H_
#define INC_ONE_FILTER_H_

#include "ONE_Math.h"

/* ============================================================
 * Initializer Macros
 * ============================================================
 * sample_freq : Sampling frequency (Hz)
 * cutoff_freq : Cutoff frequency (Hz), BPF: center frequency
 * q           : Quality factor (0.707 = Butterworth flat response)
 * gain_db     : Boost/Cut gain in dB (Shelf filters only)
 *
 * Usage:
 *   oFilter_t lpf = FILTER_PASS_INITIALIZER(1000.0, 50.0, 0.707);
 *   oFilter_t shelf = FILTER_SHELF_INITIALIZER(1000.0, 200.0, 6.0, 0.707);
 */
#define FILTER_PASS_INITIALIZER(sample_freq, cutoff_freq, q) { \
    .SamplingFreq = (sample_freq), \
    .CutoffFreq = (cutoff_freq), \
    .Q = (q), \
    .Reset = 1, \
}

#define FILTER_SHELF_INITIALIZER(sample_freq, cutoff_freq, gain_db, q) { \
    .SamplingFreq = (sample_freq), \
    .CutoffFreq = (cutoff_freq), \
    .GainDB = (gain_db), \
    .Q = (q), \
    .Reset = 1, \
}

/* ============================================================
 * Biquad Filter Structure
 * ============================================================ */
typedef struct {
	double SamplingFreq;		/* Fs (Hz) */
	double CutoffFreq;			/* Fc (Hz), BPF: center frequency */
	double Q;					/* Quality factor (0.707 = Butterworth) */
	double GainDB;				/* Gain in dB (Shelf filters only) */

	/* Biquad coefficients (normalized by a0) */
	double b0, b1, b2;
	double a1, a2;

	/* Delay line (Direct Form I) */
	double x1, x2;				/* previous inputs */
	double y1, y2;				/* previous outputs */

	uint8_t Reset;
} oFilter_t;

/* ============================================================
 * Moving Average Filter
 * ============================================================ */
#define MOVEAVERAGE_MAX_DATACOUNT		1000

#define MOVEAVERAGE_INITIALIZER(buffer, sizeofbuffer, datatype) { \
	.pBuffer = (uint8_t *)buffer,\
	.LengthOfBuffer = MATH_MIN(sizeofbuffer / SIZE_OF_DATATYPE(datatype), MOVEAVERAGE_MAX_DATACOUNT),\
	.MaxSumCount = MATH_MIN(sizeofbuffer / SIZE_OF_DATATYPE(datatype), MOVEAVERAGE_MAX_DATACOUNT),\
	.CountOfData = 0,\
	.DataType = datatype,\
	.IndexOfData = 0,\
	.SumOfData.d = 0,\
	.IsFull = 0,\
	.Reset = 1,\
}

typedef struct{
	uint8_t *pBuffer;
	uint32_t IndexOfData;
	uint32_t CountOfData;
	uint32_t MaxSumCount;
	uint32_t LengthOfBuffer;
	oDataType_t DataType;

	union {
	    int64_t   s64;
	    uint64_t  u64;
	    float     f;
	    double    d;
	} SumOfData;

	uint8_t Reset;
	uint8_t IsFull;
}oMoveAverage_t;

/* ============================================================
 * Filter Functions
 * ============================================================ */
extern double oFilter_LPF(oFilter_t *pFilter, double input);
extern double oFilter_HPF(oFilter_t *pFilter, double input);
extern double oFilter_BPF(oFilter_t *pFilter, double input);
extern double oFilter_LSF(oFilter_t *pFilter, double input);
extern double oFilter_HSF(oFilter_t *pFilter, double input);

extern uint8_t oMoveAverage_SetData(oMoveAverage_t *pAverage, void *pData);
extern uint8_t oMoveAverage_GetData(oMoveAverage_t *pAverage, void *pData);
extern oRange_t oMoveAverage_GetRange(oMoveAverage_t *pAvg);

#endif /* INC_ONE_FILTER_H_ */
