/*
 * Mi_Storage.h
 *
 *  Created on: Feb 25, 2025
 *      Author: JONE
 */

#ifndef INC_MI_STORAGE_H_
#define INC_MI_STORAGE_H_

#include "Mi_Main.h"
#include "NAND_MT29F2G01ABAGDWB_IT.h"

#define MISTORAGE_PAGEHEADER_SIZE			13
#define MISTORAGE_DATA_MAXSIZE				(MT29F2G_MAINDATA_LENGTH-MATH_MAX(50,MISTORAGE_PAGEHEADER_SIZE))

#define MISTORAGE_ADDRESS(Block, Page)		((MT29F2G_PAGE_LENGTH) * (Block) + (Page))
#define MISTORAGE_ADR_BLOCK(Address)		((Address) / (MT29F2G_PAGE_LENGTH))
#define MISTORAGE_ADR_PAGE(Address)			((Address) % (MT29F2G_PAGE_LENGTH))

#define MISTORAGE_PARAMETER_STARTADR		MISTORAGE_ADDRESS(2,0) //block=2 / pageindex=0
#define MISTORAGE_PARAMETER_ENDPADR			MISTORAGE_ADDRESS(2,MT29F2G_PAGE_LENGTH-1) //block=2 / pageindex=63

#define MISTORAGE_DATAFRAME_SIGN			0xDF
#define MISTORAGE_DATAFRAME_MAXSIZE			MIIOT_PAYLOAD_MAXSIZE
#define MISTORAGE_DATA_STARTADR				MISTORAGE_ADDRESS(3,0) //block=2 / pageindex=0
#define MISTORAGE_DATA_ENDADR				MISTORAGE_ADDRESS(2045, MT29F2G_PAGE_LENGTH-1) //block=2045 / pageindex=63

#pragma pack(1)
typedef struct{
	uint32_t 			CRCValue;
	uint8_t				Sign;
	uint32_t			UnixTime;
	uint32_t			DLC;
} MiStorage_PageHeader_t;
#pragma pack()

typedef struct{
	struct{
		int32_t OlderAddress;
		oDateAndTime_t OlderDateTime;

		int32_t NewerAddress;
		oDateAndTime_t NewerDateTime;

		int32_t CountOfData;

		int32_t SyncAddress;
	} SensorData;

	struct {
		uint8_t CountOfError;
		uint8_t IsInitialized;
		uint8_t IsFault;
		uint8_t IsHardFault;
		uint8_t IsError;
		uint8_t IsOkay;
	}Status;
}MiStorage_NandHeader_t;

extern uint8_t MiStorage_IsOpen;
extern int8_t MiStorage_ParameterSaveCmd;
extern MiStorage_NandHeader_t MiStorage_NandHeader;

extern oResult_t MiStorage_ReadIoTParameter(IoTParameter_t* pParameter);
extern oResult_t MiStorage_WriteIoTParameter(IoTParameter_t* pParameter);
extern oResult_t MiStorage_ReadIoTData(uint32_t Address, IoT_DataPacket_t* pPayload, MiStorage_PageHeader_t *pHeader);
extern oResult_t MiStorage_WriteIoTData(IoT_DataPacket_t* pPayload);
extern oResult_t MiStorage_Open();
extern oResult_t MiStorage_Close();
extern void MiStorage();

#endif /* INC_MI_STORAGE_H_ */

