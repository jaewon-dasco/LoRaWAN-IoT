#include "ONE_SlidingHistogram.h"

/*****************************************************************************************************************************************************************************
* Sliding histogram
*****************************************************************************************************************************************************************************/
oResult_t oSlidingHistogram_Init(oSlidingHistogram_t *pHstgr, float MinValue, float MaxValue, float Alpha, uint16_t *pHist, uint32_t LengthOfHist, int32_t *pRing, uint32_t LengthOfRing)
{
    if (!pHstgr || !pHist) return RESULT_NULL;
    if (LengthOfRing > UINT16_MAX) return RESULT_FAULT;

    pHstgr->RangeMin = MATH_MIN(MaxValue, MinValue);
    pHstgr->RangeMax = MATH_MAX(MaxValue, MinValue);
    pHstgr->Alpha = Alpha;
    pHstgr->LengthOfRing = LengthOfRing;
    pHstgr->LengthOfHist = LengthOfHist;
    pHstgr->pHistogram = pHist;
    pHstgr->pRing = pRing;

    // 단일 빈 처리
    if (LengthOfHist == 1) {
        pHstgr->BinWidth = 0.0f;
    } else {
        pHstgr->BinWidth = (MaxValue - MinValue) / (float)(LengthOfHist - 1);
        if (pHstgr->BinWidth < 1e-6f) // 너무 좁은 범위 검출
            return RESULT_FAULT;
    }

    for(int i=0; i<pHstgr->LengthOfHist; i++){
        pHstgr->pHistogram[i] = 0;
    }

    if(pHstgr->pRing && LengthOfRing){
		for(int i=0; i<LengthOfRing; i++){
			pHstgr->pRing[i] = -1;
		}
    }

    pHstgr->RingPos = -1;
    pHstgr->HistMax = 0;
	pHstgr->BinCache = 0;
	pHstgr->Result = 0;

    return RESULT_OK;
}

float oSlidingHistogram_Update(oSlidingHistogram_t *pHstgr, float Data)
{
	int32_t k = 0;

    if(!pHstgr){
        return 0;
    }
    if(Data < pHstgr->RangeMin || Data > pHstgr->RangeMax || pHstgr->LengthOfHist <= 0){
        return pHstgr->Result;
    }

    //Bin 계산
    if(pHstgr->BinWidth != 0) {
        k = (int32_t)((Data - pHstgr->RangeMin) / pHstgr->BinWidth);
        k = (k < 0) ? 0 : (k >= pHstgr->LengthOfHist) ? pHstgr->LengthOfHist-1 : k;
    }

    //링 버퍼 갱신
    if(pHstgr->pRing){
    	if(pHstgr->RingPos < 0){
			pHstgr->RingPos = 0;
			pHstgr->Result = Data;
		}

		int old = pHstgr->pRing[pHstgr->RingPos];
		if(old != -1) {
			pHstgr->pHistogram[old]--;
			if(old == pHstgr->BinCache){
				 //내부 최빈값 갱신 로직
				pHstgr->HistMax = 0;
				pHstgr->BinCache = -1; // 초기화
				for(int i=0; i<pHstgr->LengthOfHist; ++i) {
					if((pHstgr->pHistogram[i] > pHstgr->HistMax) ||(pHstgr->pHistogram[i] == pHstgr->HistMax && i > pHstgr->BinCache)) { // 높은 인덱스 우선
						pHstgr->HistMax = pHstgr->pHistogram[i];
						pHstgr->BinCache = i;
					}
				}
			}
		}

		pHstgr->pRing[pHstgr->RingPos] = k;
		pHstgr->RingPos = (pHstgr->RingPos + 1) % pHstgr->LengthOfRing;
    }

    //최빈값 캐시
    pHstgr->pHistogram[k]++;
    if(pHstgr->pHistogram[k] >= pHstgr->HistMax) {
        pHstgr->HistMax = pHstgr->pHistogram[k];
        pHstgr->BinCache = k;
    }

    // 5) EMA 업데이트
    float f_mode = (float)pHstgr->RangeMin + ((float)pHstgr->BinCache * (float)pHstgr->BinWidth);
	pHstgr->Result = ((1.0f-pHstgr->Alpha)*pHstgr->Result) + (pHstgr->Alpha*f_mode);

    return pHstgr->Result;
}

float oSlidingHistogram_GetTopAverage(oSlidingHistogram_t *pHstgr, float TopPercent)
{
    if(!pHstgr)
        return 0;
    if(pHstgr->Result == 0 || pHstgr->HistMax == 0 || TopPercent <= 0 || TopPercent >= 100)
        return pHstgr->Result;

    SlidingHistogramBinEntry_t *pBinEntry;
    uint32_t CountOfSum, TargetCount;

    CountOfSum = 0;

    //정렬
    for (int i = 0; i < pHstgr->LengthOfHist; i++) {
        CountOfSum += pHstgr->pHistogram[i];
    }

    TargetCount = (uint32_t)((float)CountOfSum * (TopPercent / 100.0f));
    TargetCount = MATH_MIN(TargetCount, CountOfSum);
    TargetCount = MATH_MAX(TargetCount, 1);

    if(TargetCount == 0){
    	return 0;
    }

    pBinEntry = malloc(TargetCount*sizeof(SlidingHistogramBinEntry_t));

    if(!pBinEntry){
    	return pHstgr->Result;
    }

    memset(pBinEntry, 0, TargetCount*sizeof(SlidingHistogramBinEntry_t));

    for (int i = 0; i < pHstgr->LengthOfHist; i++) {
        uint16_t Key = pHstgr->pHistogram[i];

        if(pBinEntry[TargetCount-1].count <  Key){
        	pBinEntry[TargetCount-1].count =  Key;
        	pBinEntry[TargetCount-1].index =  i;

            for(int j=TargetCount-1; j > 0; j--){
            	if(pBinEntry[j].count >  pBinEntry[j-1].count){
            		SlidingHistogramBinEntry_t bff = pBinEntry[j];

            		pBinEntry[j] = pBinEntry[j-1];
            		pBinEntry[j-1] = bff;
            	}
            	else{
            		break;
            	}
            }
        }
    }

    float SumOfCache = 0;
	CountOfSum = 0;
    for(int i=0; i<TargetCount; ++i) {
    	float f = pHstgr->RangeMin + ((float)pBinEntry[i].index * pHstgr->BinWidth);
		SumOfCache += f * pBinEntry[i].count;
		CountOfSum += pBinEntry[i].count;
	}

    free(pBinEntry);

    if(CountOfSum == 0){
        return pHstgr->Result;
    }

    return SumOfCache / (float)CountOfSum;
}

float oSlidingHistogram_GetRobust(oSlidingHistogram_t *pHstgr)
{
    if(!pHstgr) return 0;
    const int32_t N = (int32_t)pHstgr->LengthOfHist;
    int32_t center  = (int32_t)pHstgr->BinCache;
    int32_t win_half = (N > 0) ? (N / 2) : 0;

    if (N <= 0 || pHstgr->BinWidth == 0.0f) {
        return pHstgr->RangeMin + (float)pHstgr->BinCache * pHstgr->BinWidth;
    }

    int32_t ks = MATH_LIMIT(center - win_half, 0, N - 1);
    int32_t ke = MATH_LIMIT(center + win_half, 0, N - 1);
    if (ks > ke){ int32_t t = ks; ks = ke; ke = t; }
    if (ke - ks < 2){
        return pHstgr->RangeMin + (float)pHstgr->BinCache * pHstgr->BinWidth;
    }

    // m_{ks-1}, m_{ks}
    int32_t m_km1 = oMath_Median3(
        pHstgr->pHistogram[MATH_LIMIT(ks-2, 0, N-1)],
        pHstgr->pHistogram[MATH_LIMIT(ks-1, 0, N-1)],
        pHstgr->pHistogram[MATH_LIMIT(ks  , 0, N-1)]
    );
    int32_t m_k = oMath_Median3(
        pHstgr->pHistogram[MATH_LIMIT(ks-1, 0, N-1)],
        pHstgr->pHistogram[MATH_LIMIT(ks  , 0, N-1)],
        pHstgr->pHistogram[MATH_LIMIT(ks+1, 0, N-1)]
    );

    int32_t bestScore = -1;
    int32_t k0 = ks;
    int32_t best_a = 0, best_b = 0, best_c = 0;

    for (int32_t k = ks; k <= ke; ++k){
        // m_{k+1}
        int32_t m_kp1 = oMath_Median3(
            pHstgr->pHistogram[MATH_LIMIT(k  , 0, N-1)],
            pHstgr->pHistogram[MATH_LIMIT(k+1, 0, N-1)],
            pHstgr->pHistogram[MATH_LIMIT(k+2, 0, N-1)]
        );

        int32_t S = m_km1 + m_k + m_kp1; // S(k)

        if (S >= bestScore){
            bestScore = S;
            k0 = k;
            best_a = m_km1; best_b = m_k; best_c = m_kp1;
        }

        // roll
        m_km1 = m_k;
        m_k   = m_kp1;
    }

    // 포물선 보간
    float a = (float)best_a, b = (float)best_b, c = (float)best_c;
    float denom = a - 2.0f*b + c;
    float delta = 0.0f;
    if (fabsf(denom) > 1e-6f){
        delta = 0.5f * (a - c) / denom;
        delta = MATH_LIMIT(delta, -0.6f, +0.6f); // 안전 클램프
    }

    // 연속 추정치
    return pHstgr->RangeMin + ((float)k0 + 0.5f + delta) * pHstgr->BinWidth;
}
