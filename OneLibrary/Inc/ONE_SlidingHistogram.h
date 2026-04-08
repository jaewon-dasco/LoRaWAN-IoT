#ifndef INC_ONE_ALGORITHM_H_
#define INC_ONE_ALGORITHM_H_

#include "ONE_Common.h"
#include "ONE_Memory.h"
#include "ONE_Time.h"
#include "ONE_Math.h"


/*****************************************************************************************************************************************************************************
* Sliding histogram
*****************************************************************************************************************************************************************************/
typedef struct {
    /* 파라미터 */
    float RangeMin, RangeMax, BinWidth, Alpha;
    uint16_t LengthOfRing, LengthOfHist;

    /* 상태 (멤버 변수 직접 접근) */
    uint16_t *pHistogram;  			// 사용자 제공 히스토그램 버퍼
    int32_t *pRing;            		 // 사용자 제공 링 버퍼
    int16_t RingPos;

    /* 최빈값 캐시 */
    uint16_t BinCache;
    uint16_t HistMax;

    float Result;
} oSlidingHistogram_t;

typedef struct {
	uint16_t index;
    uint16_t count;
} SlidingHistogramBinEntry_t;

extern oResult_t oSlidingHistogram_Init(oSlidingHistogram_t *c, float MinValue, float MaxValue, float Alpha, uint16_t *pHist, uint32_t LengthOfHist, int32_t *pRing, uint32_t LengthOfRing);
extern float oSlidingHistogram_Update(oSlidingHistogram_t *c, float Data);
extern float oSlidingHistogram_GetTopAverage(oSlidingHistogram_t *c, float TopPercent);
extern float oSlidingHistogram_GetRobust(oSlidingHistogram_t *pHstgr);
#endif
