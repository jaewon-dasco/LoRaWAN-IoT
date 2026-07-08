/*
 * IoT_Sensor.h
 *
 *  Created on: Nov 26, 2024
 *      Author: JONE
 */

#ifndef INC_MI_IOT_H_
#define INC_MI_IOT_H_

#define MI_IOT_VERSION		0.4

#include "ONE_Time.h"
#include "ONE_Signal.h"
#include "Mi_Main.h"

#define MIIOT_SLEEP_TIME						SECOND_TO_MS(10)
#define MIIOT_PAYLOAD_MAXSIZE					(MIIOT_IOTDATA_SIZE_DATAARRAY(70))

//MailBox
#define MIIOT_MAILBOX_INITIALIZER				{{0,0,{0,}},0,0,0,0,0}
#define MIIOT_MAILBOX_QOS_MAX					0xFE
#define MIIOT_MAILBOX_MAXCOUNT					20

#define MIIOT_CHANNEL_MAXCOUNT					64

#define MIIOT_SENSORDATA_SIZE_TILT_DUAL			6
#define MIIOT_SENSORDATA_SIZE_TILT_SINGLE		3
#define MIIOT_SENSORDATA_SIZE_VW				6
#define MIIOT_SENSORDATA_SIZE_ANALOG			5
#define MIIOT_SENSORDATA_SIZE_TEMP				2
#define MIIOT_SENSORDATA_SIZE_DATAARRAY			6
#define MIIOT_SENSORDATA_SIZE_VIBRATION			10

#define MIIOT_IOTDATA_SIZE_TIME					6
#define MIIOT_IOTDATA_SIZE_ARRAYDUALTILT(cnt)	(MIIOT_IOTDATA_SIZE_TIME + 1 + MIIOT_SENSORDATA_SIZE_TEMP + ((uint32_t)(cnt)*MIIOT_SENSORDATA_SIZE_TILT_DUAL)) 	 	// time(6) + channel(1) + temp(2) +  dualaxis(6)
#define MIIOT_IOTDATA_SIZE_ARRAYSINGLETILT(cnt)	(MIIOT_IOTDATA_SIZE_TIME + 1 + MIIOT_SENSORDATA_SIZE_TEMP + ((uint32_t)(cnt)*MIIOT_SENSORDATA_SIZE_TILT_SINGLE)) 	// time(6) + channel(1) + temp(2) +  singlelaxis(3)
#define MIIOT_IOTDATA_SIZE_ANALOG(cnt)			(MIIOT_IOTDATA_SIZE_TIME + ((uint32_t)(cnt)*MIIOT_SENSORDATA_SIZE_ANALOG)) 											// time(6) + anlaog(5)
#define MIIOT_IOTDATA_SIZE_TILT					(MIIOT_IOTDATA_SIZE_TIME + MIIOT_SENSORDATA_SIZE_TEMP + MIIOT_SENSORDATA_SIZE_TILT_DUAL)							// time(6) + temp(2) + angle(6)
#define MIIOT_IOTDATA_SIZE_TEMP					(MIIOT_IOTDATA_SIZE_TIME + MIIOT_SENSORDATA_SIZE_TEMP) 																// time(6) + temp(2)
#define MIIOT_IOTDATA_SIZE_DATAARRAY(cnt)		(MIIOT_IOTDATA_SIZE_TIME + ((uint32_t)(cnt)*MIIOT_SENSORDATA_SIZE_DATAARRAY)) 										// time(6) + dataarray(6)
#define MIIOT_IOTDATA_SIZE_VIBRATION(cnt)	(MIIOT_IOTDATA_SIZE_TIME + ((uint32_t)(cnt)*MIIOT_SENSORDATA_SIZE_VIBRATION))	// time(6) + vibration(10)

#define MIIOT_PAYLOAD_TO_DUALARRAY_COUNT(dlc)	(dlc <= 0 ? 0 : (dlc-MIIOT_IOTDATA_SIZE_ARRAYDUALTILT(0)) / MIIOT_SENSORDATA_SIZE_TILT_DUAL)
#define MIIOT_PAYLOAD_TO_SINGLEARRAY_COUNT(dlc)	(dlc <= 0 ? 0 : (dlc-MIIOT_IOTDATA_SIZE_ARRAYDUALTILT(0)) / MIIOT_SENSORDATA_SIZE_TILT_SINGLE)
#define MIIOT_PAYLOAD_TO_ANALOG_COUNT(dlc)		(dlc <= 0 ? 0 : (dlc-MIIOT_IOTDATA_SIZE_ANALOG(0)) / MIIOT_SENSORDATA_SIZE_ANALOG)
#define MIIOT_PAYLOAD_TO_DATAARRAY_COUNT(dlc)	(dlc <= 0 ? 0 : (dlc-MIIOT_IOTDATA_SIZE_DATAARRAY(0)) / MIIOT_SENSORDATA_SIZE_DATAARRAY)

#define MIIOT_ARRAYSENSOR_MAX_COUNT				30
#define MIIOT_VW_CH_MAX_COUNT					5
#define MIIOT_VIBRATION_MAX_COUNT			10
#define MIIOT_ANALOG_CH_MAX_COUNT				9
#define MIIOT_DATA_ARRAY_MAX_COUNT				MIIOT_CHANNEL_MAXCOUNT

//Data convert
#define MIIOT_DATA_ENCODE_ANGLE(x)				(uint32_t)((MATH_LIMIT((double)(x), -180.0, 180.0) + 180.0) / 0.001)
#define MIIOT_DATA_ENCODE_TEMP(x)				(uint16_t)((MATH_LIMIT((double)(x), -40.0, 120.0) + 40.0) / 0.01)
#define MIIOT_DATA_ENCODE_FREQUENCY(fq)			(uint32_t)((MATH_LIMIT((double)(fq), 0, 167772) / 0.01))
#define MIIOT_DATA_ENCODE_mmPerSec(x)			(uint16_t)(MATH_LIMIT((double)(x), 0, 655.35) / 0.01)
#define MIIOT_DATA_ENCODE_uV(uV)				(uint32_t)((MATH_LIMIT((double)(uV), -34000000, 34000000) + 34000000))
#define MIIOT_DATA_ENCODE_mV(mV)				(uint32_t)((MATH_LIMIT((double)(mV), -34000, 34000) + 34000) / 0.001)
#define MIIOT_DATA_ENCODE_mA(mA)				(uint32_t)((MATH_LIMIT((double)(mA), -34000, 34000) + 34000) / 0.001)
#define MIIOT_DATA_ENCODE_Ohm(ohm)				(uint32_t)((MATH_LIMIT((double)(ohm), -34000, 34000) + 34000) / 0.001)

#define MIIOT_DATA_DECODE_ANGLE(x)				(double)MATH_LIMIT((double)(x) * 0.001 - 180.0, -180.0, 180.0)
#define MIIOT_DATA_DECODE_TEMP(x)				(double)MATH_LIMIT((double)(x) * 0.01 - 40.0, -40.0, 120.0)
#define MIIOT_DATA_DECODE_FREQUENCY(x)			(double)MATH_LIMIT((double)(x) * 0.01, 0, 167772)
#define MIIOT_DATA_DECODE_uV(uV)				(double)MATH_LIMIT((double)(uV) - 34000000.0, -34000000.0, 34000000.0)
#define MIIOT_DATA_DECODE_mV(mV)				(double)MATH_LIMIT((double)(mV) * 0.001 - 34000, -34000, 34000)
#define MIIOT_DATA_DECODE_mA(mA)				(double)MATH_LIMIT((double)(mA) * 0.001- 34000, -34000, 34000)
#define MIIOT_DATA_DECODE_Ohm(ohm)				(double)MATH_LIMIT((double)(ohm) * 0.001- 34000, -34000, 34000)


#define MIIOT_DUMMYDATA							0xFF

#define MIIOT_DT_TO_IOTTIME(pDT, pIT)			((IoTDateAndTime_t *)pIT)->Year 	= (uint8_t)(MATH_LIMIT(((oDateAndTime_t *)pDT)->Year, 2000, 2255) - 2000); \
												((IoTDateAndTime_t *)pIT)->Month 	= ((oDateAndTime_t *)pDT)->Month; \
												((IoTDateAndTime_t *)pIT)->Day 		= ((oDateAndTime_t *)pDT)->Day; \
												((IoTDateAndTime_t *)pIT)->Hour 	= ((oDateAndTime_t *)pDT)->Hour; \
												((IoTDateAndTime_t *)pIT)->Minute 	= ((oDateAndTime_t *)pDT)->Minute; \
												((IoTDateAndTime_t *)pIT)->Second 	= ((oDateAndTime_t *)pDT)->Second;

#define MIIOT_IOTTIME_TO_DT(pIT, pDT)			((oDateAndTime_t *)pDT)->Year 	= ((uint16_t)(((IoTDateAndTime_t *)pIT)->Year) + 2000); \
												((oDateAndTime_t *)pDT)->Month 	= ((IoTDateAndTime_t *)pIT)->Month; \
												((oDateAndTime_t *)pDT)->Day 	= ((IoTDateAndTime_t *)pIT)->Day; \
												((oDateAndTime_t *)pDT)->Hour 	= ((IoTDateAndTime_t *)pIT)->Hour; \
												((oDateAndTime_t *)pDT)->Minute = ((IoTDateAndTime_t *)pIT)->Minute; \
												((oDateAndTime_t *)pDT)->Second = ((IoTDateAndTime_t *)pIT)->Second;

typedef enum{
	IoTOperatingMode_NULL				= 0,
	IoTOperatingMode_Operating			= 1,
	IoTOperatingMode_PreOperation		= 253,
	IoTOperatingMode_Stop				= 254,
}IoTOperatingMode_t;

typedef enum{
	IoTProductType_NULL		 			= 0,
	IoTProductType_Gateway			 	= 1,
	IoTProductType_SIV100_5C			= 2,
	IoTProductType_SIC100_2C		 	= 3,
	IoTProductType_SIM100_6C		 	= 4,
	IoTProductType_SIA100_SD		 	= 5,
	IoTProductType_VMX3264			 	= 6,
	IoTProductType_Max		 			= 255,
}IoTProductType_t;

typedef enum{
	IoTSensorType_NULL		 			= 0,
	IoTSensorType_mV		 			= 1,
	IoTSensorType_FullBridge			= 2, //Full-Bridge
	IoTSensorType_mA	 				= 3,
	IoTSensorType_Differential 			= 4,
	IoTSensorType_Resistance 			= 5,
	IoTSensorType_Thermistor 			= 6,
	IoTSensorType_VibratingWire			= 7,
	IoTSensorType_ArrayDualTilt		 	= 8,
	IoTSensorType_ArraySingleTilt		= 9,
	IoTSensorType_Tilt					= 10,
	IoTSensorType_Vibration				= 11,
	IoTSensorType_Max		 			= 255,
}IoTSensorType_t;

typedef enum{
	IoTDataType_NULL 					= 0,

	IoTDataType_ArrayDualTilt			= 1,
	IoTDataType_ArraySingleTilt			= 2,
	IoTDataType_Analog 					= 3,
	IoTDataType_Tilt					= 4,
	IoTDataType_Vibration				= 5,

	IoTDataType_DataArray_Type1			= 100,
	IoTDataType_DataArray_Type2			= 101,

	IoTDataType_Status 					= 200,
	IoTDataType_Operating				= 201,
	IoTDataType_Max 					= 233,
}IoTDataType_t;

#pragma pack(1)
///***************************************************************************************************************************
// Sensor Data
//****************************************************************************************************************************
typedef uint16_t IoTSensorTemp_t;

typedef struct
{
	uint32_t			Data		: 32;
	IoTSensorType_t		Type		: 8;
	uint8_t				Channel		: 8;
} IoTDataArrayItem_t; //6byte

typedef struct
{
	uint32_t			Analog		: 32;
	IoTSensorType_t		Type		: 8;
} IoTSensorAnalog_t; //5byte

typedef struct
{
	uint32_t			AxisX		: 24;
	uint32_t			AxisY		: 24;
} IoTSensorTiltDual_t; //6byte

typedef struct
{
	uint32_t			Axis		: 24;
} IoTSensorTiltSingle_t; //3byte

typedef struct
{
	IoTSensorTemp_t		Temperature;
	union{
		IoTSensorTiltDual_t		Dual[MIIOT_ARRAYSENSOR_MAX_COUNT];
		IoTSensorTiltSingle_t	Single[MIIOT_ARRAYSENSOR_MAX_COUNT];
	};

	IoTSensorType_t		Type;
	uint8_t				Channel		: 8;
} IoTSensorTiltArray_t;

typedef struct
{
	uint16_t				PPV_X;		// mm/s × 100
	uint16_t				PPV_Y;		// mm/s × 100
	uint16_t				PPV_Z;		// mm/s × 100
	uint16_t				PVS;		// mm/s × 100
	uint16_t				Freq;		// Hz × 100 (PPV 최대 축 주파수)
} IoTSensorVibrationPPV_t; //10byte

typedef struct
{
	uint16_t				MTVV_X;		// 축별 MTVV (가속도 running-RMS 최댓값, mm/s² × 100)
	uint16_t				MTVV_Y;		// 축별 MTVV
	uint16_t				MTVV_Z;		// 축별 MTVV
	uint16_t				MTVV;		// 대표값 (최대 축 or 벡터합)
	uint16_t				Freq;		// Hz × 100 (MTVV 최대 축 주파수)
} IoTSensorVibrationMTVV_t; //10byte

///***************************************************************************************************************************
// IoT Data
//****************************************************************************************************************************
typedef struct
{
	uint8_t 	Year;
	uint8_t 	Month;
	uint8_t 	Day;
	uint8_t 	Hour;
	uint8_t 	Minute;
	uint8_t 	Second;
} IoTDateAndTime_t; //6byte

typedef struct
{
	IoTDateAndTime_t		Time;
	IoTDataArrayItem_t		Analog;	
	IoTSensorTiltArray_t	TiltArray;
	uint32_t 				CountOfArraySensor;
} IoTDataSIC100_2C_t;

typedef struct
{
	IoTDateAndTime_t		Time;
	uint8_t					Channel;
	IoTSensorTemp_t			Temperature;
	IoTSensorTiltDual_t		TiltArray[MIIOT_ARRAYSENSOR_MAX_COUNT];
} IoTDataArrayDualTilt_t;

typedef struct
{
	IoTDateAndTime_t		Time;
	uint8_t					Channel;
	IoTSensorTemp_t			Temperature;
	IoTSensorTiltSingle_t	TiltArray[MIIOT_ARRAYSENSOR_MAX_COUNT];
} IoTDataArraySingleTilt_t;

typedef struct
{
	IoTDateAndTime_t		Time;
	IoTSensorAnalog_t		Channel[MIIOT_ANALOG_CH_MAX_COUNT];
} IoTDataAnalog_t;

typedef struct
{
	IoTDateAndTime_t		Time;
	IoTDataArrayItem_t		Items[MIIOT_DATA_ARRAY_MAX_COUNT];
} IoTDataArray_t;

typedef struct
{
	IoTDateAndTime_t		Time;
	IoTSensorTemp_t			Temperature;
	IoTSensorTiltDual_t		Sensor;
} IoTDataTilt_t;

typedef struct
{
	IoTDateAndTime_t		Time;
	IoTSensorVibrationPPV_t	Data[MIIOT_VIBRATION_MAX_COUNT];
} IoTDataVibration_t;

typedef union
{
	struct{
		uint8_t	Okay : 1;
		uint8_t	DisconnectedLoRa : 1;
		uint8_t	SystemSupplyVoltTooLow : 1;
		uint8_t	ClockSupplyVoltTooLow : 1;
		uint8_t	DisconnectedSensor : 1;
	};

	uint32_t  		Bits;
} IoTStatusBit_t;

typedef struct
{
	uint16_t		SystemSupply;	//2
	IoTStatusBit_t	StatusBits;		//4
	uint16_t		TroubleCode;	//2
	uint16_t		SoftwareVersion;//2
} IoTDataStatus_t;

typedef struct{
	union{
		struct{
			uint8_t			SamplingPeriod : 1;
			uint8_t			SamplingSensor : 1;
			uint8_t			SamplingSupply : 1;
			uint8_t			BusyLora : 1;
			uint8_t			BusyMemory : 1;
			uint8_t			BusyGPIO : 1;
			uint8_t			Pause : 1;
		};

		uint32_t IsBusy;
	};

	uint8_t			SleepMode;
} IoTProcessState_t;

typedef struct IoTDataStatusTypeDef
{
	uint16_t			SoftwareVersion;//2
	uint16_t			SystemSupply;
	uint16_t			ClockSupply;
	uint16_t			TroubleCode;
	IoTStatusBit_t		StatusBits;

	uint32_t 			UpdateSupplyTimestamp;

	uint8_t 			IsNew;
	uint8_t 			IsUpdateInformation;
} IoTStatus_t;

///***************************************************************************************************************************
// IoT TypeDef
//****************************************************************************************************************************
typedef struct{
	IoTOperatingMode_t	OperatingMode;
	uint16_t 			SamplingInterval;
	uint16_t 			UpdateInterval;
} IoTOperatingParameter_t;

typedef struct{
	IoTProductType_t	ProductCode;
	char 				SerialNo[10];
} IoTInformation_t;

typedef struct{
	uint16_t	SensorId;
	uint16_t	CountOfSensor;
	uint64_t DisableSensorBit;
}IoTChannelPropertiesArray_t;

typedef struct{
	char		Mode[7];
	float		StartFreq;
	float		EndFreq;
}IoTChannelPropertiesVibratingWire_t;

typedef struct{
	char		Mode[7]; //3K/10K/NTC/PTC
	float		Min;
	float		Max;
}IoTChannelPropertiesThermistor_t;

typedef struct{
	uint16_t	CountOfSampling;
}IoTChannelPropertiesTilt_t;

typedef struct{
	float		Warnning;	// PPV 경고 임계값 (mm/s), 0이면 컴파일 기본값 사용
}IoTChannelPropertiesVibration_t;

typedef struct{
	IoTSensorType_t		TypeOfSensor;
	uint16_t 			WarmupTime; //ms
	uint8_t 			SupplySource; //Power(12V) supply port (SIM100_6C=1~3 / SIC100_2C=1~2)
	uint8_t 			RetryCount;
	uint8_t 			RetryInterval; //second
	float				ErrorTolerance;
	union{
		IoTChannelPropertiesArray_t 		Array;
		IoTChannelPropertiesVibratingWire_t 			VibrationWire;
		IoTChannelPropertiesTilt_t 			Tilt;
		IoTChannelPropertiesThermistor_t	Thermistor;
		IoTChannelPropertiesVibration_t		Vibration;
	}Properties;
} IoTChannelConfig_t;

typedef struct{
	uint32_t	ActualVRef;
	uint32_t	Actual5Vdc;

	oLinear_t 	Battery;

	union{
		struct{
			double PolynomialX[6];
			double PolynomialY[6];

			oRange_t RangeX;
			oRange_t RangeY;

			uint16_t CountOfAverage;
		}Tilt;

		oLinear_t 	ADC[20];
	}Calibration;
} IoTSystemConfig_t;


typedef struct{
	IoTInformation_t		Information;
	IoTOperatingParameter_t	Operating;
	IoTChannelConfig_t		ChannelConfig[MIIOT_CHANNEL_MAXCOUNT];
	IoTSystemConfig_t		SystemConfig;
} __attribute__((aligned(8))) IoTParameter_t;
#pragma pack()


///***************************************************************************************************************************
// IoT Mailbox / IoT Data
//****************************************************************************************************************************
typedef struct{
	IoTDataType_t		TypeOfData;
	uint32_t 			DLC;
	uint8_t				Frame[MIIOT_PAYLOAD_MAXSIZE];
}IoT_DataPacket_t;

typedef struct{
	IoT_DataPacket_t	Payload;
	oResult_t			Result;
	uint8_t				IsDone;
	uint8_t				IsSuccess;
	uint8_t				IsBusy;
	uint8_t				IsError;
	uint8_t				QoS;
	uint32_t			Timestamp;
	uint32_t			Timer;
}IoT_MailboxItem_t;

typedef struct{
	IoT_MailboxItem_t 	Item[MIIOT_MAILBOX_MAXCOUNT];
	uint16_t 				FirstIndex;
	uint16_t 				LastIndex;
	uint16_t 				Count;
}IoT_Mailbox_t;

///***************************************************************************************************************************
// IoT Node
//****************************************************************************************************************************
typedef struct{
	uint8_t 					LastMessageNumber;
	uint32_t 					Address;
	IoTStatus_t					Status;
	IoTOperatingParameter_t		OperatingParameter;
	IoTInformation_t			Information;

	struct{
		uint32_t				ReceiveRadioTimestamp;
		uint32_t				ConfigUpdateTimestamp;
	}Timers;

	union{
		struct{
			uint8_t UpdatedStatus : 1;
			uint8_t UpdatedInformation : 1;
			uint8_t UpdatedOperatingParameter : 1;
		};

		uint32_t Bits;
	} Flag;
} IoTDevice_t;

typedef oResult_t (*IoTDataPacketCallbackHandler_t)(IoT_DataPacket_t **ppPacket);

///***************************************************************************************************************************
// IoT Callback
//****************************************************************************************************************************
extern IoTDataPacketCallbackHandler_t MiIoT_MeasurementCallback;
extern IoTProcessState_t MiIoT_ProcessState;
extern IoTDataPacketCallbackHandler_t MiIoT_SamplingCallback;
extern IoTDataPacketCallbackHandler_t MiIoT_StatusCallback;
extern ResultCallbackHandler_t MiIoT_SleepCallback;
extern ResultCallbackHandler_t MiIoT_IOControlCallback;
extern VoidCallbackHandler_t MiIoT_GPIOInitCallback;
extern VoidCallbackHandler_t MiIoT_GPIODeInitCallback;

///***************************************************************************************************************************
// Variable
//****************************************************************************************************************************
extern oBlinker_t MiIoT_BlinkIdle;
extern oBlinker_t MiIoT_BlinkBusy;
extern oDateAndTime_t MiIoT_MeasurementDT;
extern IoT_Mailbox_t MiIoT_LoRaMailBox;
extern IoTParameter_t MiIoT_Parameter;
extern const IoTParameter_t MiIoT_DefaultParameter;
extern IoTStatus_t MiIoT_Status;
extern oDateAndTime_t MiIoT_DT;
extern uint8_t MiIoT_IsBusy;
extern uint8_t MiIoT_IsPowerSaveMode;
extern uint8_t MiIoT_LED;


///***************************************************************************************************************************
// Function
//****************************************************************************************************************************
extern char* MiIoT_DataTypeToString(IoTDataType_t Type);
extern char* MiIoT_SensorTypeToString(IoTSensorType_t Type);
extern oResult_t MiIoT_IsValidParameter(IoTParameter_t *pParameter);
extern oResult_t MiIoT_IsSensorData(IoTProductType_t ProductCode, IoT_DataPacket_t *pPayload);
extern IoT_MailboxItem_t* MiIoT_MailBox_GetLastItem(IoT_Mailbox_t *pMailBox);
extern void IoT_MailBox_Sort(IoT_Mailbox_t *pMailBox);
extern IoT_MailboxItem_t* MiIoT_MailBox_AddItem(IoT_Mailbox_t *pMailBox, IoT_MailboxItem_t *pItem);
extern IoT_MailboxItem_t* MiIoT_MailBox_NewItem(IoT_Mailbox_t *pMailBox, IoT_DataPacket_t *pPacket, uint8_t QoS, uint32_t Timer);
extern IoT_MailboxItem_t* MiIoT_MailBox_GetItem(IoT_Mailbox_t* pMailBox);
extern void MiIoT_MailBox_Remove(IoT_Mailbox_t* pMailBox, IoT_MailboxItem_t *pItem);
extern IoT_MailboxItem_t* MiIoT_MailBox_Find(IoT_Mailbox_t *pMailBox, IoTDataType_t TypeOfData);
extern oResult_t MiIoT_MailBox_IsExist(IoT_Mailbox_t *pMailBox, IoTDataType_t TypeOfData);
extern void MiIoT();

#endif /* INC_MI_IOT_H_ */

/* History

2026-06-26 | v0.1
	- baseline (Mi_IoT.h)
2026-06-29 | v0.2
	- IoTProcessState_t 신규 도입 (bitfield union + uint32_t IsBusy 오버레이)
	  · 필드: SamplingPeriod/SamplingSensor/SamplingSupply/BusyLora/BusyMemory/BusyGPIO/Pause (bit) + SleepMode(byte)
	  · IsBusy==0 한 번으로 7-flag 통합 검사 (Sleep 진입 조건 단순화)
	- 개별 전역 flag → ProcessState 필드로 통합
	  · MiIoT_IsRunIO / MiIoT_IsRunSamplingSensor / MiIoT_IsRunSamplingSupply 제거
	  · MiIoT_IsSleep / MiIoT_IsPause 제거 → ProcessState.SleepMode / .Pause
	- IoTStatusBit_t typedef 순서 정정 (IoTDataStatus_t보다 앞으로 이동, forward reference 해결)
	- IoTStatus_t 필드 정리: SoftwareVersion 추가, StatusBit → StatusBits 명명 통일
	- MiIoT_StatusCallback typedef 변경: ResultCallbackHandler_t → IoTDataPacketCallbackHandler_t
	  · UpdateStatus가 IoT_DataPacket_t 반환하도록 시그니처 확장
2026-07-07 | v0.3
	- MiIoT_Sleep 상태머신 정리:
	  · default: SleepMode=0 클리어를 case 0에서 default로 이동 + fall-through 제거 (break)
	  · case 4: wake 완료 시 step=0 직접 복귀 → step++(→default 경유 리셋)로 변경
	    (SleepMode 클리어 지점을 default 한 곳으로 일원화)
	  · case 0: 5ms idle 유예 + SleepMode=1 설정 + step 전진의 원자적 결합 유지
	    (유예 구간은 period/sensor/status가 busy 비트를 선점하는 시간 — 설계 의도 주석 추가)
2026-07-07 | v0.4
	- MiIoT_UpdatePeriod 재측정 대기 중 sleep 허용:
	  · RESULT_WAIT 시 SamplingPeriod=0 클리어 + 100ms holdoff 타이머 설정
	    → case 1 재진입을 100ms 지연시켜 플래그 연속 0 유지 → 5ms idle 디바운스 통과 → STOP 진입 가능
	  · 기존: WAIT 루프(1→2→3→1)가 SamplingPeriod=1 유지 → RetryInterval 내내(최대 수 분) MCU active
	  · STOP 중 tick은 Native_SleepMode의 wake 후 보상(+MIIOT_SLEEP_TIME)으로 RetryInterval 판정 유지
	  · case 0에서 holdoff 리셋 — 정상 측정 흐름(첫 진입·OK 재진입)은 지연 없음
	- MiIoT_UpdatePeriod 내부 명명 정리: MiIoT_MeasurementStep/MeasurementStarted/MeasurementPeriodOk
	  → UpdatePeriodStep/UpdatePeriodStarted/UpdatedPeriod_IsOk
*/
