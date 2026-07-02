/*
 * ONE_Time.c
 *
 *  Version: 0.1 (2026-06-29)
 */
#include "ONE_Math.h"
#include "ONE_Time.h"

#define DT_USED_RTC

uint32_t oDT_Tick;
uint32_t oTMR_Tick_us;
uint32_t oTMR_Tick_ms;
oDateAndTime_t SysDataAndTime = {0,0,0,0,0,0};
uint32_t CycleCount_us = 0;
uint32_t CycleCount_ms = 0;

static void oTMR_Update()
{
	uint32_t Tick;
	uint32_t CycleInterval;
	static uint8_t DWTInitialized = 0;

	//Enable core cycle counting
	if(!DWTInitialized){
		// DWT enable
		if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)) {
			CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
		}

		CycleCount_us = 0;
		CycleCount_ms = 0;
		DWT->CYCCNT = 0; // CYCCNT reset
		DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; // CYCCNT enable

		oTMR_Tick_us = 0;
		oTMR_Tick_ms = 0;

		DWTInitialized = 1;
	}

	//Microsecond
	if(DWT->CYCCNT < CycleCount_us){
		// CYCCNT overflow
		CycleInterval = (0xFFFFFFFF - CycleCount_us) + DWT->CYCCNT + 1;
	}
	else{
		CycleInterval = DWT->CYCCNT - CycleCount_us;
	}

	Tick = (uint32_t)(((uint64_t)CycleInterval * 1000000ULL) / SystemCoreClock); // us 계산

	if(Tick > 0){
		oTMR_Tick_us += Tick;
        CycleCount_us += Tick * (SystemCoreClock / 1000000);
	}

	//Millisecond
	if(DWT->CYCCNT < CycleCount_ms){
		// CYCCNT overflow
		CycleInterval = (0xFFFFFFFF - CycleCount_ms) + DWT->CYCCNT + 1;
	}
	else{
		CycleInterval = DWT->CYCCNT - CycleCount_ms;
	}

	Tick = (CycleInterval * 1000) / SystemCoreClock; // ms 계산

	if(Tick > 0){
		oTMR_Tick_ms += Tick;
        CycleCount_ms += Tick * (SystemCoreClock / 1000);
	}
}

uint32_t oTMR_GetTick(oTickBase_t TickBase)
{
	uint32_t result = 0;

	oTMR_Update();

	switch(TickBase)
	{
		default:
		case TICKBASE_SYSTICK:
			if(uwTick == TMR_TICK_MAXVALUE){
				uwTick++;
			}

			result = uwTick;
			break;

		case TICKBASE_CORECYCLE_MS:
			if(oTMR_Tick_ms == TMR_TICK_MAXVALUE){
				oTMR_Tick_ms++;
			}

			result = oTMR_Tick_ms;
			break;

		case TICKBASE_CORECYCLE_US:
			if(oTMR_Tick_us == TMR_TICK_MAXVALUE){
				oTMR_Tick_us++;
			}

			result = oTMR_Tick_us;
			break;
	}

	return result;
}

void oTMR_SetTick(oTickBase_t TickBase, uint32_t Tick)
{
	switch(TickBase)
	{
		case TICKBASE_SYSTICK:
			uwTick = Tick;
			break;
		case TICKBASE_CORECYCLE_MS:
			oTMR_Tick_ms = Tick;
			break;
		case TICKBASE_CORECYCLE_US:
			oTMR_Tick_us = Tick;
			break;
	}
}

uint32_t oTMR_Interval(uint32_t Tick1, uint32_t Tick2)
{
	uint32_t result = 0;

	if(Tick1 >= Tick2){
		result = Tick1-Tick2;
	}
	else{
		result = (TMR_TICK_MAXVALUE-Tick2)+Tick1;
	}

	return result;
}

uint8_t oTMR_Elapsed(uint32_t *pTimer, uint32_t Interval, oTickBase_t TickBase)
{
	if(pTimer == NULL || Interval == 0){
		return 0;
	}
	
	return oTMR_Interval(oTMR_GetTick(TickBase), *pTimer) >= (uint32_t)Interval ? 1 : 0;
}

uint32_t oTMR_CountDown(uint32_t *pTimer, uint32_t Count, oTickBase_t TickBase)
{
	uint32_t Interval = 0;

	if(pTimer == NULL || Count == 0){
		return 0;
	}

	Interval = oTMR_Interval(oTMR_GetTick(TickBase), *pTimer);

	if(Interval < Count){
		return Count - Interval;
	}

	return 0;
}

uint8_t oTMR_Trigger(uint32_t *pTimer, uint32_t Interval, uint8_t AutoReset, oTickBase_t TickBase)
{
	uint32_t result = 0;

	if(pTimer == NULL || Interval == 0){
		return 0;
	}

	if(*pTimer != TMR_TICK_MAXVALUE){
		result = oTMR_Elapsed(pTimer, Interval, TickBase);

		if(result){
			if(AutoReset){
				*pTimer = oTMR_GetTick(TickBase);
			}
			else{
				*pTimer = TMR_TICK_MAXVALUE;
			}
		}
	}
	else if(AutoReset){
		*pTimer = oTMR_GetTick(TickBase);
	}

	return result;
}

uint32_t oTMR_Rand(uint32_t *pSeed)
{
    uint32_t seed = 0;

    if (pSeed == NULL || *pSeed == 0) {
        seed = oTMR_GetTick(TICKBASE_CORECYCLE_US);
		if (seed == 0) {
			seed = oTMR_GetTick(TICKBASE_CORECYCLE_US);
		}
    } else {
        seed = *pSeed;
    }

    // LCG parameters (glibc)
    seed = (seed * 1103515245U + 12345U) & 0x7FFFFFFF;

    if (pSeed != NULL)
        *pSeed = seed;

    return seed;
}

uint32_t oTMR_RandRange(uint32_t *pSeed, uint32_t Min, uint32_t Max)
{
	uint32_t range = MATH_INTERVAL(Min, Max);

    if (Min > Max){
    	return Min;
    }

    uint32_t rnd = oTMR_Rand(pSeed);

    if(range != 0){
    	return Min + (rnd % (range + 1));
    }
    else{
    	return rnd;
    }
}

oDateAndTime_t oDT_UpdateNow(void)
{
	if(!oTMR_Trigger(&oDT_Tick, 1000, 1, TICKBASE_SYSTICK)){
		return SysDataAndTime;
	}

	#if defined(DT_USED_RTC) && defined(RTC_OUTPUT_DISABLE)
	oRTC_GetDateAndTime(&SysDataAndTime);
	#else
	if(oDT_IsEmpty(&SysDataAndTime)){
		#if defined(RTC_BACKUPFLAG)
		oRTC_GetDateAndTime(&SysDataAndTime);
		#endif
	}
	else{
		oDT_AddSec(&SysDataAndTime, 1);
	}
	#endif

	return SysDataAndTime;
}
void oDT_Reset(oDateAndTime_t *pDT)
{
	if(pDT == NULL){
		return;
	}

	*pDT = oDT_UpdateNow();
}

int8_t oDT_Compare(oDateAndTime_t *pBase, oDateAndTime_t *pTarget)
{
	if(pBase == NULL || pTarget == NULL){
		return -1;
	}

	if(pBase->Year > pTarget->Year){
		return -1;
	}
	else if(pBase->Year < pTarget->Year){
		return 1;
	}

	if(pBase->Month > pTarget->Month){
		return -1;
	}
	else if(pBase->Month < pTarget->Month){
		return 1;
	}

	if(pBase->Day > pTarget->Day){
		return -1;
	}
	else if(pBase->Day < pTarget->Day){
		return 1;
	}

	if(pBase->Hour > pTarget->Hour){
		return -1;
	}
	else if(pBase->Hour < pTarget->Hour){
		return 1;
	}

	if(pBase->Minute > pTarget->Minute){
		return -1;
	}
	else if(pBase->Minute < pTarget->Minute){
		return 1;
	}

	if(pBase->Second > pTarget->Second){
		return -1;
	}
	else if(pBase->Second < pTarget->Second){
		return 1;
	}

	return 0;
}

uint8_t oDT_IsEmpty(oDateAndTime_t *pDT)
{
	if(pDT == NULL){
		return 1;
	}
	else if(pDT->Year == 0){
		return 1;
	}
	else if(pDT->Month == 0 || pDT->Month > 12){
		return 1;
	}
	else if(pDT->Day == 0 || pDT->Day > 31){
		return 1;
	}
	else if(pDT->Hour >= 24){
		return 1;
	}
	else if(pDT->Minute >= 60){
		return 1;
	}
	else if(pDT->Second >= 60){
		return 1;
	}

	return 0;
}

uint8_t oDT_IsTimeOver(oDateAndTime_t *pDT)
{
	if(oDT_IsEmpty(pDT) || oDT_IsEmpty(&SysDataAndTime)){
		return 0;
	}

	return oDT_Compare(pDT, &SysDataAndTime) >= 0 ? 1 : 0;
}
void oDT_SetNow(oDateAndTime_t *pDT)
{
	if(oDT_IsEmpty(pDT)){
		return;
	}

#if defined(DT_USED_RTC) && defined(RTC_OUTPUT_DISABLE)
	oRTC_SetDateAndTime(pDT);
#else
	SysDataAndTime = *pDT;
#endif
}

void oDT_GetNow(oDateAndTime_t *pDT)
{
	if(pDT == NULL){
		return;
	}

	*pDT = oDT_UpdateNow();
}

uint32_t oDT_Sub(oDateAndTime_t DT1, oDateAndTime_t DT2)
{
	uint32_t unix1 = oDT_ToUnixTime(DT1);
	uint32_t unix2 = oDT_ToUnixTime(DT2);

	return MATH_MAX(unix1,unix2) - MATH_MIN(unix1,unix2);
}

uint32_t oDT_Add(oDateAndTime_t DT1, oDateAndTime_t DT2)
{
    return oDT_ToUnixTime(DT1) + oDT_ToUnixTime(DT2);
}

void oDT_AddSec(oDateAndTime_t *pDT, int32_t Sec)
{
    if(!pDT || Sec == 0) return;

    uint32_t t = oDT_ToUnixTime(*pDT);
    t = (uint32_t)((int64_t)t + (int64_t)Sec);

    *pDT = oDT_FromUnixTime(t);
}

void oDT_AddMinute(oDateAndTime_t *pDT, int32_t Minute)
{
    if(!pDT || Minute == 0) return;

    uint32_t t = oDT_ToUnixTime(*pDT);
    t = (uint32_t)((int64_t)t + ((int64_t)Minute * 60LL));

    *pDT = oDT_FromUnixTime(t);
}

void oDT_AddHour(oDateAndTime_t *pDT, int32_t Hour)
{
    if(!pDT || Hour == 0) return;

    uint32_t t = oDT_ToUnixTime(*pDT);
    t = (uint32_t)((int64_t)t + ((int64_t)Hour * 3600LL));

    *pDT = oDT_FromUnixTime(t);
}

void oDT_AddDay(oDateAndTime_t *pDT, int32_t Day)
{
    if(!pDT || Day == 0) return;

    uint32_t t = oDT_ToUnixTime(*pDT);
    t = (uint32_t)((int64_t)t + ((int64_t)Day * 86400LL));

    *pDT = oDT_FromUnixTime(t);
}

uint32_t oDT_ToUnixTime(oDateAndTime_t DT)
{
    /* 월별 일수(평년), 월 시작 전일까지 누적일(평년) */
    const uint8_t  dim_tbl[12]          = {31,28,31,30,31,30,31,31,30,31,30,31};
    const uint16_t days_before_month[12]= { 0,31,59,90,120,151,181,212,243,273,304,334};

    /* 기본 검증 */
    if (DT.Year < 1970 || DT.Month < 1 || DT.Month > 12 || DT.Hour > 23 || DT.Minute > 59 || DT.Second > 59){
    	return 0;
    }

    /* 윤년 여부 */
    uint8_t leap = ((DT.Year % 4 == 0 && DT.Year % 100 != 0) || (DT.Year % 400 == 0)) ? 1u : 0u;

    /* 해당 월의 일수 계산 */
    uint8_t dim = dim_tbl[DT.Month - 1];
    if (DT.Month == 2 && leap) dim = 29;
    if (DT.Day < 1 || DT.Day > dim){
    	return 0;
    }

    /* 1970년 ~ (Year-1)까지의 일수 합산 */
    uint32_t days = 0;
    for (uint16_t y = 1970; y < DT.Year; y++) {
        uint8_t ly = ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) ? 1u : 0u;
        days += ly ? 366u : 365u;
    }

    /* 올해 월 전까지의 누적일 + 윤년 보정 */
    days += days_before_month[DT.Month - 1];
    if (DT.Month > 2 && leap) days += 1u;

    /* 오늘 이전 일수 */
    days += (uint32_t)(DT.Day - 1u);

    /* 총 초 계산 */
    return days * 86400u + (uint32_t)DT.Hour * 3600u + (uint32_t)DT.Minute * 60u + (uint32_t)DT.Second;
}

oDateAndTime_t oDT_FromUnixTime(uint32_t UnixTime)
{
    static const uint8_t dim_tbl[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    oDateAndTime_t DT;

    uint32_t days = UnixTime / 86400u;
    uint32_t secs = UnixTime % 86400u;

    DT.Hour   = (uint8_t)(secs / 3600u);
    secs      %= 3600u;
    DT.Minute = (uint8_t)(secs / 60u);
    DT.Second = (uint8_t)(secs % 60u);

    /* 연도 결정 */
    uint16_t year = 1970;
    while (1) {
        uint8_t  ly    = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 1u : 0u;
        uint16_t dyear = ly ? 366u : 365u;
        if (days >= dyear) {
            days -= dyear;
            year++;
        } else {
            break;
        }
        if (year > 2106) break; /* 상한 가드 */
    }
    DT.Year = year;

    /* 월/일 결정 */
    uint8_t leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 1u : 0u;
    uint8_t month = 1;
    while (month <= 12) {
        uint8_t dim = dim_tbl[month - 1];
        if (month == 2 && leap) dim = 29;

        if (days >= dim) {
            days -= dim;
            month++;
        } else {
            break;
        }
    }
    DT.Month = month;
    DT.Day   = (uint8_t)(days + 1u);

    return DT;
}


uint8_t oDT_ElapsedSec(oDateAndTime_t *pDT, uint32_t Interval)
{
	oDateAndTime_t Ref;

	if(oDT_IsEmpty(&SysDataAndTime)){
		return 0;
	}
	else if(oDT_IsEmpty(pDT)){
		oDT_Reset(pDT);
		return 0;
	}

	Ref = *pDT;

	oDT_AddSec(&Ref, Interval);

	return oDT_IsTimeOver(&Ref);
}
uint8_t oDT_ElapsedMin(oDateAndTime_t *pDT, uint32_t Interval)
{
	return oDT_ElapsedSec(pDT, Interval * 60U);
}
uint8_t oDT_ElapsedHour(oDateAndTime_t *pDT, uint32_t Interval)
{
	return oDT_ElapsedSec(pDT, Interval * 3600U);
}
uint8_t oDT_ElapsedDay(oDateAndTime_t *pDT, uint32_t Interval)
{
	oDateAndTime_t Ref;

	if(oDT_IsEmpty(&SysDataAndTime)){
		return 0;
	}
	else if(oDT_IsEmpty(pDT)){
		oDT_Reset(pDT);
		return 0;
	}

	Ref = *pDT;

	oDT_AddDay(&Ref, Interval);

	return oDT_IsTimeOver(&Ref);
}

#if defined(RTC_BACKUPFLAG)
//////////////////////////////////////////////////////////////////////////////////////////////
void oRTC_Init(void)
{
	static uint8_t RTC_IsInit = 0;

	if(!RTC_IsInit){
		HAL_PWR_EnableBkUpAccess();
		__HAL_RCC_RTC_ENABLE();//인터럽트 켜줌.
		RTC_IsInit = 1;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////
oResult_t oRTC_GetDateAndTime(oDateAndTime_t *pDT)
{
	if(pDT == NULL){
		return RESULT_ERROR;
	}

	RTC_TimeTypeDef RTC_Time = {0,0,0,0,0,0,0,0};
	RTC_DateTypeDef RTC_Date = {0,0,0,0};
	oResult_t result = RESULT_OK;

	oRTC_Init();

	if (HAL_RTC_GetTime(&hrtc, &RTC_Time, RTC_FORMAT_BIN) != HAL_OK) {
		result = RESULT_ERROR;
	}
	else if (HAL_RTC_GetDate(&hrtc, &RTC_Date, RTC_FORMAT_BIN) != HAL_OK) {
		result = RESULT_ERROR;
	}

	if(RTC_Date.Year == 0 || RTC_Date.Month == 0 || RTC_Date.Date == 0){
		result = RESULT_ERROR;
	}

	if(result == RESULT_OK){
		pDT->Year = (uint16_t)(RTC_Date.Year) + 2000;
		pDT->Month = RTC_Date.Month;
		pDT->Day = RTC_Date.Date;
		pDT->Hour = RTC_Time.Hours;
		pDT->Minute = RTC_Time.Minutes;
		pDT->Second = RTC_Time.Seconds;
	}

	return result;
}

oResult_t oRTC_SetDateAndTime (oDateAndTime_t *pDT)
{
	RTC_TimeTypeDef RTC_Time = {0,0,0,0,0,0,0,0};
	RTC_DateTypeDef RTC_Date = {0,0,0,0};
	oResult_t result = RESULT_OK;

	oRTC_Init();

	if(oDT_IsEmpty(pDT)){
		return RESULT_ERROR;
	}

	RTC_Date.Year = (uint8_t)(MATH_MAX(pDT->Year, 2000) - 2000);
	RTC_Date.Month = pDT->Month;
	RTC_Date.Date = pDT->Day;
	RTC_Time.Hours = pDT->Hour;
	RTC_Time.Minutes = pDT->Minute;
	RTC_Time.Seconds = pDT->Second;

	if (HAL_RTC_SetTime(&hrtc, &RTC_Time, RTC_FORMAT_BIN) != HAL_OK) {
		result = RESULT_ERROR;
	}
	else if(HAL_RTC_SetDate(&hrtc, &RTC_Date, RTC_FORMAT_BIN) != HAL_OK) {
		result = RESULT_ERROR;
	}

	if(result == RESULT_OK){
		HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0x32F2);

		SysDataAndTime = *pDT;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////
void RTC_SetDate (uint8_t Year, uint8_t Month, uint8_t Date)
{
	RTC_DateTypeDef RTC_Date;

	oRTC_Init();

	RTC_Date.Year = Year;
	RTC_Date.Month = Month;
	RTC_Date.Date = Date;

	HAL_RTC_SetDate(&hrtc, &RTC_Date, RTC_FORMAT_BIN);
}
#endif
