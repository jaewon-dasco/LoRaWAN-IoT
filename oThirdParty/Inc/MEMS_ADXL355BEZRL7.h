/*0
 * MEMS_ADXL355BEZRL7.h
 *
 *  Created on: Aug 1, 2025
 *      Author: JONE
 *
 ************************************************************
 * Sensitivity (LSB/g):
 * ±2g mode → 256,000 LSB/g
 * ±4g mode → 128,000 LSB/g
 * ±8g mode → 64,000 LSB/g
 *************************************************************
 */

#ifndef INC_MEMS_ADXL355BEZRL7_H_
#define INC_MEMS_ADXL355BEZRL7_H_
#include "Mi_Main.h"

#define ADXL355_DATACONVERT_TO_ACC(x)	(int32_t)((((x & 0xFF0000) >> 16 | (x & 0x00FF00) | (x & 0x0000FF) << 16) >> 4) | ((x & 0x000080) ? 0xFFF00000 : 0))
#define ADXL355_ACC_TO_2ug(x)			((double)(ADXL355_DATACONVERT_TO_ACC(x)) * 3.8147)
#define ADXL355_ACC_TO_4ug(x)			((double)(ADXL355_DATACONVERT_TO_ACC(x)) * 7.6294)
#define ADXL355_ACC_TO_8ug(x)			((double)(ADXL355_DATACONVERT_TO_ACC(x)) * 15.2588)

//#define ADXL355_DATACONVERT_TO_TEMP(x)	(int16_t)((((x & 0xFF00) >> 8 | (x & 0x00FF) << 8) >> 4) | ((x & 0x0080) ? 0xF000 : 0))
#define ADXL355_DATACONVERT_TO_TEMP(x)	(int16_t)(((x & 0xFF0) >> 4 | (x & 0x0F) << 8) | ((x & 0x0008) ? 0xF000 : 0))
#define ADXL355_TEMP_TO_Degree(x)		(25.0 + (double)(ADXL355_DATACONVERT_TO_TEMP(x) - 1852) / -9.05)

typedef enum{
	ADXL355_HPF_NOFILTER	= 0,
	ADXL355_HPF_24_7 		= 0b001,  //0.00247 * ODR = Hz
	ADXL355_HPF_6_2084 		= 0b010,  //0.00062084 * ODR = Hz
	ADXL355_HPF_1_5545 		= 0b011,  //0.00015545 * ODR = Hz
	ADXL355_HPF_0_3862 		= 0b100,  //0.00003862 * ODR = Hz
	ADXL355_HPF_0_0954 		= 0b101,  //0.00000954 * ODR = Hz
	ADXL355_HPF_0_0238 		= 0b110   //0.00000238 * ODR = Hz
}ADXL355_HPF;

typedef enum{
	ADXL355_ODR_4000hz_LPF_1000hz	= 0b0000,
	ADXL355_ODR_2000hz_LPF_500hz	= 0b0001,
	ADXL355_ODR_1000hz_LPF_250hz	= 0b0010,
	ADXL355_ODR_500hz_LPF_125hz		= 0b0011,
	ADXL355_ODR_250hz_LPF_62_5hz	= 0b0100,
	ADXL355_ODR_125hz_LPF_31_2hz	= 0b0101,
	ADXL355_ODR_62_5hz_LPF_15_6hz	= 0b0110,
	ADXL355_ODR_31_2hz_LPF_7_8hz	= 0b0111,
	ADXL355_ODR_15_6hz_LPF_3_9hz	= 0b1000,
	ADXL355_ODR_7_8hz_LPF_1_9hz		= 0b1001,
	ADXL355_ODR_3_9_2hz_LPF_0_9hz	= 0b1010,
}ADXL355_ODR_LPF;

typedef enum{
	ADXL355_G_RANGE_2G				= 0b01,
	ADXL355_G_RANGE_4G				= 0b10,
	ADXL355_G_RANGE_8G				= 0b11,
}ADXL355_GRagne_t;

#pragma pack(1)
typedef struct{
	I2C_HandleTypeDef *pI2C;

	struct{
		uint8_t  				: 8; //DEVID_AD
		uint8_t  				: 8; //DEVID_MST
		uint8_t  				: 8; //PARTID
		uint8_t  				: 8; //REVID

		union{
			struct{
				uint8_t DATA_RDY 	: 1;
				uint8_t FIFO_FULL 	: 1;
				uint8_t FIFO_OVR 	: 1;
				uint8_t Activity 	: 1;
				uint8_t NVM_BUSY 	: 1;
				uint8_t				: 3;
			};

			uint8_t Byte;
		}Status;

		uint8_t  FIFO_ENTRIES	: 8;

		struct{
			uint16_t TEMP			: 16;
			uint32_t XDATA			: 24;
			uint32_t YDATA			: 24;
			uint32_t ZDATA			: 24;
		}Data;

		uint8_t  FIFO_DATA		: 8;

		struct{
			uint16_t XOFFSET		: 16;
			uint16_t YOFFSET		: 16;
			uint16_t ZOFFSET		: 16;
			uint8_t  ACT_EN			: 8;
			uint16_t ACT_THRESH		: 16;
			uint8_t  ACT_COUNT		: 8;

			union{
				struct{
					uint8_t ODR_LPF		: 4;
					uint8_t HPF_CORNER	: 3;
					uint8_t 			: 1;
				};

				uint8_t Byte;
			}Filter;

			uint8_t  FIFO_SAMPLES	: 8;

			union{
				struct{
					uint8_t RDY_EN1		: 1;
					uint8_t FULL_EN1	: 1;
					uint8_t OVR_EN1		: 1;
					uint8_t ACT_EN1		: 1;
					uint8_t RDY_EN2		: 1;
					uint8_t FULL_EN2	: 1;
					uint8_t OVR_EN2		: 1;
					uint8_t ACT_EN2		: 1;
				};

				uint8_t Byte;
			}INT_MAP;

			uint8_t 				: 8; //Sync

			union{
				struct{
					ADXL355_GRagne_t GravityRange	: 2; //01=±2g | 10=±4g | 11=±8g
					uint8_t							: 4; //Reserved
					uint8_t			 INT_POL		: 1;
					uint8_t			 I2C_HS			: 1;
				};

				uint8_t Byte;
			}Range;

			union{
				struct{
					uint8_t Standby		: 1; //1=Standby 0=Operating
					uint8_t TEMP_OFF	: 1;
					uint8_t DRDY_OFF	: 1;
					uint8_t				: 5;
				};

				uint8_t Byte;
			}POWER_CTL;

			uint8_t 				: 8; //SELF_TEST
		}Parameter;

		uint8_t  Reset			: 8; //write code 0x52 to reset the device.
	}Register;

	double Temperature;
	double DataRate;
	double LowPassFreqeuncy;
	double HighPassFreqeuncy;

	oVector3_t 	Acceleration; //mg
	oRPY_t 		Angle;
	oRPY_t 		OverSampling;

	struct{
		uint8_t SequenceStep;
		uint8_t CountOfOverSampling;
		uint8_t CountOfError;
		uint8_t IsRun;
		uint8_t IsBusy;
		uint8_t IsOpen;
	}State;

}ADXL355_t;
#pragma pack()

extern oResult_t ADXL355_GetStatus(ADXL355_t *pADXL355);
extern oResult_t ADXL355_GetParameter(ADXL355_t *pADXL355);
extern oResult_t ADXL355_SetParameter(ADXL355_t *pADXL355);
extern oResult_t ADXL355_GetData(ADXL355_t *pADXL355);
extern oResult_t ADXL355_GetAngle(ADXL355_t *pADXL355, uint8_t Count, oRPY_t *pAngle);
extern void ADXL355_SetFilter(ADXL355_t *pADXL355, double HPF, double LPF);
extern oResult_t ADXL355_Open(I2C_HandleTypeDef *pI2C, ADXL355_t *pADXL355);
extern oResult_t ADXL355_Close(ADXL355_t *pADXL355);
#endif /* INC_MEMS_ADXL355BEZRL7_H_ */
