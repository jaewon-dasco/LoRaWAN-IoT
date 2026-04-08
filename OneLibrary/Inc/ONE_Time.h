/*
 * ONE_Timer.h
 *
 *  Created on: Aug 27, 2024
 *      Author: JONE
 */

#ifndef INC_ONE_TIME_H_
#define INC_ONE_TIME_H_

#include "ONE_Common.h"

#if defined(RTC_OUTPUT_DISABLE)
#define	RTC_BACKUPFLAG	0x32F2
#endif

#define TMR_TICK_MAXVALUE 			(uint32_t)(-1)
#define TMR_DT_TO_STRING(d, x) 		(snprintf((x), 20, "%04d-%02d-%02d-%02d:%02d:%02d", (d)->Year, (d)->Month, (d)->Day, (d)->Hour, (d)->Minute, (d)->Second))

#define DT_EMPTY_INITIALIZER		{0,0,0,0,0,0}

#define HOUR_MAX_VALUE				24
#define MINUTE_MAX_VALUE			60
#define SECOND_MAX_VALUE			60
#define DAY_MINUTE_MAX_VALUE		(HOUR_MAX_VALUE*60)
#define DAY_SECOND_MAX_VALUE		(DAY_MINUTE_MAX_VALUE*60)

#define MS_TO_HOUR(x)				(uint32_t)(MS_TO_MINUTE(x)/60)
#define MS_TO_MINUTE(x)				(uint32_t)(MS_TO_SECOND(x)/60)
#define MS_TO_SECOND(x)				(uint32_t)((x)/1000)
#define SECOND_TO_MINUTE(x)			(uint32_t)((x)/60)
#define SECOND_TO_MS(x)				(uint32_t)((x)*1000)
#define MINUTE_TO_HOURE(x)			((uint32_t)(x)/60)
#define MINUTE_TO_SECOND(x)			((uint32_t)(x)*60)
#define MINUTE_TO_MS(x)				(MINUTE_TO_SECOND(x)*1000)
#define HOUR_TO_MINUTE(x)			((uint32_t)(x)*60)
#define HOUR_TO_SECOND(x)			(HOUR_TO_MINUTE(x)*60)
#define HOUR_TO_MS(x)				(HOUR_TO_SECOND(x)*1000)

#define HOUR_TO_TOTAL_MINUTE(x)		((uint32_t)(x.Minute) + ((uint32_t)(x.Hour)*60))
#define HOUR_TO_TOTAL_SECOND(x)		((uint32_t)(x.Second) + ((uint32_t)(x.Minute)*60) + ((uint32_t)(x.Hour)*60*60))
#define DAY_TO_TOTAL_MINUTE(x)		((uint32_t)(x.Minute) + (((uint32_t)x.Hour)*60) + (((uint32_t)(x.Day)*24*60))
#define DAY_TO_TOTAL_SECOND(x)		((uint32_t)(x.Second) + (((uint32_t)x.Minute)*60) + (((uint32_t)(x.Hour)*60*60) + (((uint32_t)(x.Day)*24*60*60))

typedef enum {
    UTC_MINUS_12_00 = -720,
    UTC_MINUS_11_00 = -660,
    UTC_MINUS_10_00 = -600,
    UTC_MINUS_09_30 = -570,
    UTC_MINUS_09_00 = -540,
    UTC_MINUS_08_00 = -480,
    UTC_MINUS_07_00 = -420,
    UTC_MINUS_06_00 = -360,
    UTC_MINUS_05_00 = -300,
    UTC_MINUS_04_00 = -240,
    UTC_MINUS_03_30 = -210,
    UTC_MINUS_03_00 = -180,
    UTC_MINUS_02_00 = -120,
    UTC_MINUS_01_00 =  -60,
    UTC_PLUS_00_00  =    0,
    UTC_PLUS_01_00  =   60,
    UTC_PLUS_02_00  =  120,
    UTC_PLUS_03_00  =  180,
    UTC_PLUS_03_30  =  210,
    UTC_PLUS_04_00  =  240,
    UTC_PLUS_04_30  =  270,
    UTC_PLUS_05_00  =  300,
    UTC_PLUS_05_30  =  330,
    UTC_PLUS_05_45  =  345,
    UTC_PLUS_06_00  =  360,
    UTC_PLUS_06_30  =  390,
    UTC_PLUS_07_00  =  420,
    UTC_PLUS_08_00  =  480,
    UTC_PLUS_08_45  =  525,
    UTC_PLUS_09_00  =  540,
    UTC_PLUS_09_30  =  570,
    UTC_PLUS_10_00  =  600,
    UTC_PLUS_10_30  =  630,
    UTC_PLUS_11_00  =  660,
    UTC_PLUS_12_00  =  720,
    UTC_PLUS_12_45  =  765,
    UTC_PLUS_13_00  =  780,
    UTC_PLUS_14_00  =  840
} oUtcOffsetMinute_t; //UTC offset minute

typedef enum {
    TICKBASE_SYSTICK = 0,
	TICKBASE_CORECYCLE_MS = 1,
	TICKBASE_CORECYCLE_US = 2,
} oTickBase_t;

#pragma pack(1)
typedef struct DateAndTimeTypeDef{
	uint16_t 	Year;
	uint8_t 	Month;
	uint8_t 	Day;
	uint8_t 	Hour;
	uint8_t 	Minute;
	uint8_t 	Second;
}oDateAndTime_t; // Total size of 56 bit
#pragma pack()


static const oDateAndTime_t DT_MAX_VALUE = {65535,12,31,0,0,0};
static const oDateAndTime_t DT_MIN_VALUE = {1970,1,1,0,0,0};


#if defined(RTC_BACKUPFLAG)
extern RTC_HandleTypeDef hrtc;

extern void oRTC_Init(void);
extern oResult_t oRTC_GetDateAndTime(oDateAndTime_t *pDT);
extern oResult_t oRTC_SetDateAndTime (oDateAndTime_t *pDT);
#endif

extern uint32_t oTMR_GetTick(oTickBase_t TickBase);
extern void oTMR_SetTick(oTickBase_t TickBase, uint32_t Tick);
extern uint32_t oTMR_Interval(uint32_t Tick1, uint32_t Tick2);
extern uint8_t oTMR_Elapsed(uint32_t *pTimer, uint32_t Interval, oTickBase_t TickBase);
extern uint32_t oTMR_CountDown(uint32_t *pTimer, uint32_t Count, oTickBase_t TickBase);
extern uint8_t oTMR_Trigger(uint32_t *pTimer, uint32_t Interval, uint8_t AutoReset, oTickBase_t TickBase);

extern uint32_t oTMR_Rand(uint32_t *pSeed);
extern  uint32_t oTMR_RandRange(uint32_t *pSeed, uint32_t Min, uint32_t Max);

extern void oDT_SetNow(oDateAndTime_t *pDT);
extern void oDT_GetNow(oDateAndTime_t *pDT);
extern uint32_t oDT_Sub(oDateAndTime_t DT1, oDateAndTime_t DT2);
extern uint32_t oDT_Add(oDateAndTime_t DT1, oDateAndTime_t DT2);
extern void oDT_AddYear(oDateAndTime_t *pDT, int32_t Year);
extern void oDT_AddMonth(oDateAndTime_t *pDT, int32_t Month);
extern void oDT_AddDay(oDateAndTime_t *pDT, int32_t Day);
extern void oDT_AddHour(oDateAndTime_t *pDT, int32_t Hour);
extern void oDT_AddMinute(oDateAndTime_t *pDT, int32_t Minute);
extern void oDT_AddSec(oDateAndTime_t *pDT, int32_t Sec);
extern oDateAndTime_t oDT_UpdateNow(void);
extern void oDT_Reset(oDateAndTime_t *pDT);
extern uint8_t oDT_IsEmpty(oDateAndTime_t *pDT);
extern uint8_t oDT_IsTimeOver(oDateAndTime_t *pDT);
extern uint8_t oDT_ElapsedSec(oDateAndTime_t *pDT, uint32_t Interval);
extern uint8_t oDT_ElapsedMin(oDateAndTime_t *pDT, uint32_t Interval);
extern uint8_t oDT_ElapsedHour(oDateAndTime_t *pDT, uint32_t Interval);
extern uint8_t oDT_ElapsedDay(oDateAndTime_t *pDT, uint32_t Interval);
extern int8_t oDT_Compare(oDateAndTime_t *pDT1, oDateAndTime_t *pDT2);
extern uint32_t oDT_ToUnixTime(oDateAndTime_t DT);
extern oDateAndTime_t oDT_FromUnixTime(uint32_t UnixTime);

#endif /* INC_ONE_TIME_H_ */
