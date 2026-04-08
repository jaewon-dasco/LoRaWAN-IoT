/*
 * ONE_IoCtrl.h
 *
 *  Created on: Dec 5, 2024
 *      Author: JONE
 */

#ifndef INC_ONE_SIGNAL_H_
#define INC_ONE_SIGNAL_H_

#include "ONE_Common.h"

#define IO_LOW					0
#define IO_HIGH					(!IO_LOW)

#ifdef GPIO_PIN_MASK
#define IO_WRITE(io, state)		HAL_GPIO_WritePin(io.Port, io.Pin, (state==IO_HIGH) ? io.ActiveLevel : !io.ActiveLevel)
#define IO_READ(io)				(HAL_GPIO_ReadPin(io.Port, io.Pin) == io.ActiveLevel ? IO_HIGH : IO_LOW)
#else
#define IO_WRITE(io, state)
#define IO_READ(io)
#endif


#define	CALIBRATION_INITIALIZER(iMn,iMx,oMn,oMx,Limit)	{0,0,iMn,iMx,oMn,oMx,Limit}
#define	DEBOUNCE_INITIALIZER(Rising, Falling)			{0,0,0,Rising,Falling,0}

#define	SIGNAL_PATTERN_INITIALIZER(fq, pt, len)	{fq,pt,len}
#define SIGNAL_PATTERN_MAXSIZE					32

#define	BLINK_INITIALIZER(fq, pt, len)			{SIGNAL_PATTERN_INITIALIZER(fq,pt ,MATH_MIN(len,BLINK_MAXLENGTH)),1,0,0,0,0,0}
#define BLINK_MAXLENGTH							60000

#define THREASHOLD_INITIALIZER(LowLevel, HighLevel, DelayTime)	{LowLevel, HighLevel, 0, DelayTime, 0}

#define	TRIGGER_INITIALIZER(edge)				{0,edge,0,0,0} //falling edge=0 | rising edge=1
#define DELAY_INITIALIZER(DelayTime)			{1,0,0,0,DelayTime}

#define LINEAR_IS_EMPTY(pLin)	(\
	pLin == NULL || \
	pLin->InMin == 0xffffffffffffffff || \
	pLin->InMax == 0xffffffffffffffff || \
	pLin->OutMin == 0xffffffffffffffff ||  \
	pLin->OutMax == 0xffffffffffffffff || \
	memcmp(&pLin->InMin, &pLin->InMax, sizeof(double)) == 0 || \
	memcmp(&pLin->OutMin, &pLin->OutMax, sizeof(double)) == 0 \
)

 #ifdef GPIO_PIN_MASK
 typedef struct{
	 GPIO_TypeDef *Port;
	 uint16_t Pin;
	 uint8_t ActiveLevel;
 }oIO_t;
 #else
 typedef struct{
	 uint32_t Port;
	 uint16_t Pin;
	 uint8_t ActiveLevel;
 }oIO_t;
 #endif

typedef struct{
	double LowLevel;
	double HighLevel;
	double State;

	uint32_t DelayTime; //Delay time in milliseconds
	uint32_t Timer; //Delay time in milliseconds
}oThreshold_t;

typedef struct{
	double InMin;
	double InMax;
	double OutMin;
	double OutMax;
}oLinear_t;

typedef struct{
	uint8_t Input;
	uint8_t Output;
	uint8_t Reset;

	uint16_t RisingDelayTime; //Low to High delay
	uint16_t FallingDelayTime; //High to Low delay
	uint32_t Timestamp;
}oDebounce_t;

typedef struct{
	uint8_t Input;
	uint8_t Edge;
	uint16_t Count;

	uint8_t PastSignal;
	uint8_t Output;
}oTrig_t;

typedef struct{
	float			Frequency;
	uint32_t		Pattern;
	uint16_t		Length;
}oSignalPattern_t;

typedef struct{
	oSignalPattern_t Pulse;

	uint8_t		Enable;
	uint8_t		Reverse;
	uint8_t 	Output;
	uint32_t 	Timer;
	uint16_t 	IndexOfPulse;
	uint8_t 	IsEndOfIndex;
}oBlinker_t;

typedef struct{
	uint8_t Enable;
	uint8_t Input;
	uint8_t Output;
	uint32_t Timestamp;
	uint32_t DelayTime;
}oDelay_t;

extern uint8_t oThreashold(double Signal, oThreshold_t *pThreashold);
extern double oLinear(oLinear_t *pLinear, double Input, uint8_t RangeLimit);
extern void oBlink(oBlinker_t *Blinker);
extern uint8_t oDebounce(oDebounce_t *pDebounce, uint8_t Signal);
extern uint8_t oTrigger(oTrig_t *pTrig, uint8_t Signal);

#ifdef GPIOA
extern uint8_t oTrigger_GPIO(oIO_t IO, oTrig_t *pTrig, oDebounce_t* pDebounce);
extern uint8_t oDebounce_GPIO(oIO_t IO, oDebounce_t *pDebounce);
extern void oBlink_GPIO(oIO_t IO, oBlinker_t *Blinker);
#endif

extern uint8_t oDelay_Rising(oDelay_t *pDelay);
extern uint8_t oDelay_Falling(oDelay_t *pDelay);
extern uint8_t oDelay_AutoOff(oDelay_t *pDelay);

#endif /* INC_ONE_SIGNAL_H_ */
