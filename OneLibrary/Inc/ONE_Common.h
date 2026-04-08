/*
 * ONE_Timer.h
 *
 *  Created on: Aug 27, 2024
 *      Author: JONE
 */

#ifndef INC_ONE_COMMON_H_
#define INC_ONE_COMMON_H_

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "main.h"

#ifndef ArrayLen
#define ArrayLen(a)							(sizeof(a)/ sizeof((a)[0]))
#endif

#define InOfRange(x, min, max)				(min<max ? (x>=min&&x<=max) : (x<=min&&x>=max))
#define OutOfRange(x, min, max)				(min<max ? (x<min||x>max) : (x>min||x<max))

#define CONVERT_PNTR_TO_FLOAT(p)			(float)(*((float *)(p)))
#define CONVERT_PNTR_TO_INT16(p)			(int16_t)(*((int16_t *)(p)))
#define CONVERT_PNTR_TO_UINT16(p)			(uint16_t)(*((uint16_t *)(p)))
#define CONVERT_PNTR_TO_INT32(p)			(int32_t)(*((int32_t *)(p)))
#define CONVERT_PNTR_TO_UINT32(p)			(uint32_t)(*((uint32_t *)(p)))

#define CONVERT_V_TO_mV(v)					((double)(v)*1000.0)
#define CONVERT_V_TO_uV(v)					((double)(v)*1000000.0)
#define CONVERT_mV_TO_uV(v)					((double)(v)*1000.0)
#define CONVERT_mV_TO_V(v)					((double)(v)/1000.0)
#define CONVERT_uV_TO_V(v)					((double)(v)/1000000.0)
#define CONVERT_uV_TO_mV(v)					((double)(v)/1000.0)
#define CONVERT_mV_TO_mA(mV,load)			((double)(mV)/(double)(load))
#define CONVERT_Ohm_TO_mOhm(r)				((double)(r)*1000)
#define CONVERT_mOhm_TO_Ohm(r)				((double)(r)/1000)
#define CONVERT_mV_TO_Ohm1(VRef,VOut,R2)	((double)(R2)*((double)(VRef)/(double)(VOut)-1)) //PullDown circuit [ VRef - > Resistant -> Vout ]
#define CONVERT_mV_TO_Ohm2(VRef,VOut,R1)	((double)(R1)*((double)(VOut)/((double)(VRef)-(double)(VOut)))) //PullUp circuit [ Vout - > Resistant -> GND ]
#define CONVERT_Hz_TO_ms(hz)				(uint32_t)((double)(1)/(double)(hz)*1000.0)
#define CONVERT_ms_TO_Hz(ms)				(uint32_t)((double)(1)/(double)(ms)*1000.0)

#define STRINGLINE_MAXCOUNT 				100

#define SIZE_OF_DATATYPE(type)				((uint32_t)(type%9))

typedef enum {
	False = 0,
	True = 1,
} oBool_t;

typedef enum {
	RESULT_FAULT 	= -9,
	RESULT_TIMEOUT 	= -8,
	RESULT_PAUSE	= -7,
	RESULT_WAIT		= -6,
	RESULT_DONE		= -5,
	RESULT_OK		= -4,
	RESULT_RUN		= -3,
	RESULT_BUSY		= -2,
	RESULT_ERROR 	= -1,
	RESULT_NULL		= 0,
} oResult_t;

typedef enum {
	DataType_Unknown = 0,
	DataType_Int8 = 1,
	DataType_Int16 = 2,
	DataType_Int32 = 4,
	DataType_Int64 = 8,

	DataType_UInt8 = 10,
	DataType_UInt16 = 11,
	DataType_UInt32 = 13,
	DataType_UInt64 = 17,

	DataType_Float = 22,
	DataType_Double = 26,

	DataType_Max = 255,
} oDataType_t;

typedef struct StringLineTypeDef{
	char *Line[STRINGLINE_MAXCOUNT];
	uint32_t Count;
} oStringLine_t;

typedef oResult_t (*ResultCallbackHandler_t)(void);
typedef void (*VoidCallbackHandler_t)(void);
typedef void* (*PointerCallbackHandler_t)(void);

extern oResult_t oJSON_GetDoubleArray(const char *JsonStr, const char *Keyword, double *pArray, uint32_t SizeOfArray);
extern oResult_t oJSON_GetValue(const char *JsonStr, const char *Keyword, char *pData, uint32_t SizeOfData);

extern uint8_t oHexCharToByte(char c);
extern char oByteToHexChar(uint8_t b);
extern uint32_t oHexStringToBytes(char *HexString, uint8_t *pBytes, uint32_t SizeOfBytes);
extern uint32_t oHexStringFromBytes(char *HexString, uint8_t *pBytes, uint32_t SizeOfBytes);

extern uint8_t oString_LineAdd(oStringLine_t *Line, char *AddString);
extern void oString_LineFlush(oStringLine_t* Line);
extern uint8_t oString_Delete(char *Source, char *Delete);
extern void oString_Left(char *Source, char *Destination, uint32_t Count);
extern void oString_Right(char *Source, char *Destination, uint32_t Count);
#endif /* INC_ONE_TIMER_H_ */
