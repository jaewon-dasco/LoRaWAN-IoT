/*
 * TEMP_TMP1075.h
 *
 *  Created on: Aug 4, 2025
 *      Author: JONE
 */

#ifndef INC_TEMP_TMP1075_H_
#define INC_TEMP_TMP1075_H_

#include "Mi_Main.h"

#define TMP1075_RESOLUTION					16
#define TMP1075_DATACONVERT_TO_TEMP(x)		(int16_t)(((((x & 0xFF00) >> 8) | ((x & 0x00FF) << 8)) >> 4) | ((x & 0x0080) ? 0xF000 : 0))
#define TMP1075_TO_CELSIUS(x)				((double)(TMP1075_DATACONVERT_TO_TEMP(x)) / TMP1075_RESOLUTION)



typedef enum{
	TMP1075_REGADR_TEMP = 0x00,
	TMP1075_REGADR_CFGR = 0x01,
	TMP1075_REGADR_LLIM = 0x02,
	TMP1075_REGADR_HLIM = 0x03,
	TMP1075_REGADR_DIEID = 0x0F,
}TMP1075_RegisterAddress;

typedef enum{
	TMP1075_CONVERSIONRATE_27 = 0b00,
	TMP1075_CONVERSIONRATE_55 = 0b01,
	TMP1075_CONVERSIONRATE_110 = 0b10,
	TMP1075_CONVERSIONRATE_220 = 0b11,
}TMP1075_CoversionRate;

typedef enum{
	TMP1075_ALERT_TRIGFUN_1FAULT = 0b00,
	TMP1075_ALERT_TRIGFUN_2FAULT = 0b01,
	TMP1075_ALERT_TRIGFUN_3FAULT = 0b10,
	TMP1075_ALERT_TRIGFUN_4FAULT = 0b11,
}TMP1075_AlertTrigFunction;

#pragma pack(1)
typedef struct{
	I2C_HandleTypeDef *pI2C;

	struct{
		uint16_t TEMP;

		struct{
			uint8_t 					OneShotMode			: 1;
			TMP1075_CoversionRate		ConversionRate		: 2;
			TMP1075_AlertTrigFunction 	AlertFun 			: 2;
			uint8_t 					AlertPinPolarity	: 1;
			uint8_t 					AlertInterruptMode	: 1;
			uint8_t 					ShutdownMode		: 1;
			uint8_t 										: 8; //reserved
		}CFGR;

		uint16_t LLIM;
		uint16_t HLIM;
		uint16_t DIEID;
	}Register;

	double Temperature;

	struct{
		double SumOfSampling;
		uint8_t CountOfSampling;
		uint8_t OpenStep;
		uint8_t ProcessStep;
		uint8_t IsReadMode;
		uint8_t IsWriteMode;
		uint8_t IsOpening;
		uint8_t IsOpen;
	}State;

} TMP1075_t;
#pragma pack()

extern oResult_t TMP1075_GetData(TMP1075_t *pTMP1075, uint8_t Count);
extern oResult_t TMP1075_GetParameter(TMP1075_t *pTMP1075);
extern oResult_t TMP1075_SetParameter(TMP1075_t *pTMP1075);
extern oResult_t TMP1075_Open(I2C_HandleTypeDef *pI2C, TMP1075_t *pTMP1075);
extern oResult_t TMP1075_Close(TMP1075_t *pTMP1075);
#endif
