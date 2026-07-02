/*
 * Mi_Storage.c
 *
 *  Version: 0.1 (2026-06-29)
 */
#include "ONE_Memory.h"
#include "Mi_IoT.h"
#include "Mi_Native.h"
#include "NAND_MT29F2G01ABAGDWB_IT.h"
#include "Mi_Storage.h"

static IoTOperatingParameter_t MiStorage_LastOperating;
MiStorage_NandHeader_t MiStorage_NandHeader;
uint32_t MiStorage_AutoOffTimer;
uint8_t MiStorage_IsOpen = 0;
uint8_t MiStorage_IsBusy = 0;
int8_t MiStorage_ParameterSaveCmd;
MT29F2G_t MT29F2G;

oResult_t MiStorage_Open()
{
	static uint8_t NandOpenStep = 0;
	static uint32_t NandOpenTimer = 0;
	oResult_t result = RESULT_RUN;

	if(MiStorage_IsOpen){
		NandOpenStep = 0;
		GPIOs.DO.NANDEnable = 1;
		return RESULT_OK;
	}

	switch(NandOpenStep)
	{
		default:
			NandOpenStep = 0; // @suppress("No break at end of case")
		case 0:
			NandOpenTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			NandOpenStep++; // @suppress("No break at end of case")
		case 1:
			GPIOs.DO.NANDEnable = 0;
			if(oTMR_Trigger(&NandOpenTimer, 200, 1, TICKBASE_SYSTICK)){
				// 전원 ON 전에 NCS(PB11)를 GPIO HIGH로 설정
				// → 전원 인가 시 NAND가 선택되지 않도록 방지
				{
					GPIO_InitTypeDef gpio = {0};
					gpio.Pin = GPIO_PIN_11;
					gpio.Mode = GPIO_MODE_OUTPUT_PP;
					gpio.Pull = GPIO_NOPULL;
					gpio.Speed = GPIO_SPEED_FREQ_LOW;
					HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET);
					HAL_GPIO_Init(GPIOB, &gpio);
				}
				NandOpenStep++;
			}
			break;
		case 2:
			GPIOs.DO.NANDEnable = 1;
			if(oTMR_Trigger(&NandOpenTimer, 10, 1, TICKBASE_SYSTICK)){
				NandOpenStep++;
			}
			break;
		case 3:
			switch(MT29F2G_Init(&MT29F2G, &hqspi))
			{
				case RESULT_RUN:
					break;
				case RESULT_OK:
					MiStorage_AutoOffTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					MiStorage_IsOpen = 1;
					result = RESULT_OK;
					break;
				default:
					GPIOs.DO.NANDEnable = 0;
					result = RESULT_ERROR;
					break;
			}
			break;
	}

	if(result != RESULT_RUN){
		NandOpenStep = 0;
		if(result == RESULT_OK){
			oSerial_Log("MiStorage", "Open OK");
		}
		else{
			oSerial_Log("MiStorage", "Open FAIL");
		}
	}

    return result;
}

oResult_t MiStorage_Close()
{
	static uint8_t NandCloseStep = 0;
	oResult_t result = RESULT_RUN;

	if(!MiStorage_IsOpen){
		GPIOs.DO.NANDEnable = 0;
		NandCloseStep = 0;
		return RESULT_OK;
	}

	switch(NandCloseStep)
	{
		case 0:
			if((result=MT29F2G_DeInit(&MT29F2G)) == RESULT_OK){
				NandCloseStep++;
				result = RESULT_RUN;
			}
			break;
		case 1:
		{
			GPIOs.DO.NANDEnable = 0;
			result = RESULT_OK;
			break;
		}
	}

	if(result != RESULT_RUN){
		GPIOs.DO.NANDEnable = 0;
		MiStorage_IsOpen = 0;
		NandCloseStep = 0;
	}

    return result;
}

oResult_t MiStorage_Read(uint32_t Address, MiStorage_PageHeader_t *pHeader, uint8_t *pData, uint32_t SizeOfData)
{
	static uint8_t ReadStep = 0;
	uint32_t BlockNo = MISTORAGE_ADR_BLOCK(Address);
	uint32_t PageNo = MISTORAGE_ADR_PAGE(Address);
	uint32_t CalcCRC;
	oResult_t result = RESULT_RUN;

	if(!pHeader || !pData || SizeOfData > MISTORAGE_DATA_MAXSIZE){
		return RESULT_NULL;
	}

	if(!MiStorage_IsOpen){
		ReadStep = 0;
	}

	switch(ReadStep)
	{
		default:
			ReadStep = 0; // @suppress("No break at end of case")
		case 0:
			if(MiStorage_IsOpen){
				ReadStep++;
			}
			else{
				if(MiStorage_Open() == RESULT_ERROR){
					result = RESULT_ERROR;
				}
				break;
			} // @suppress("No break at end of case")
		case 1:
			if((result = MT29F2G_ReadByte(&MT29F2G, BlockNo, PageNo, 0, (uint8_t *)pHeader, sizeof(MiStorage_PageHeader_t))) == RESULT_OK){
				if(pHeader->UnixTime == 0xFFFFFFFF || pHeader->DLC == 0xFFFFFFFF || pHeader->DLC == 0 || pHeader->Sign != MISTORAGE_DATAFRAME_SIGN){
					result = RESULT_ERROR;
				}
				else{
					ReadStep++;
					result = RESULT_RUN;
				}
			}
			break;
		case 2:
			pHeader->DLC = MATH_MIN(pHeader->DLC, SizeOfData);

			if((result = MT29F2G_ReadByte(&MT29F2G, BlockNo, PageNo, sizeof(MiStorage_PageHeader_t), pData, pHeader->DLC)) == RESULT_OK){
				CalcCRC = oMEM_CRC32(pData, pHeader->DLC, 0);

				if(CalcCRC != pHeader->CRCValue){
					result = RESULT_ERROR;
				}
			}
			break;
	}

	if(result != RESULT_RUN){
		ReadStep = 0;
	}

	switch(result)
	{
		default:
			break;
		case RESULT_RUN:
			MiStorage_AutoOffTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			break;
		case RESULT_FAULT:
			MiStorage_NandHeader.Status.IsFault = 1;
			MiStorage_NandHeader.Status.IsInitialized = 0;
			oSerial_Log("MiStorage", "Read FAULT Adr=%u B=%u P=%u", Address, BlockNo, PageNo);
			break;
		case RESULT_ERROR:
			MiStorage_NandHeader.Status.IsError = 1;
			break;
		case RESULT_OK:
			MiStorage_NandHeader.Status.IsFault = 0;
			MiStorage_NandHeader.Status.IsError = 0;
			break;
	}

	return result;
}

oResult_t MiStorage_Write(uint32_t Address, uint8_t *pData, uint32_t SizeOfData)
{
	static uint8_t WriteStep = 0;
	static uint8_t Buffer[MT29F2G_MAINDATA_LENGTH];
	uint32_t BlockNo = MISTORAGE_ADR_BLOCK(Address);
	uint32_t PageNo = MISTORAGE_ADR_PAGE(Address);
	MiStorage_PageHeader_t Header;
	oResult_t result = RESULT_RUN;

	if(SizeOfData + sizeof(MiStorage_PageHeader_t) > MT29F2G_MAINDATA_LENGTH){
		oSerial_Log("MiStorage", "Write OVERFLOW Size=%u Max=%u", SizeOfData, (uint32_t)(MT29F2G_MAINDATA_LENGTH - sizeof(MiStorage_PageHeader_t)));
		return RESULT_ERROR;
	}

	if(!MiStorage_IsOpen){
		WriteStep = 0;
	}

	switch(WriteStep)
	{
		default:
			WriteStep = 0; // @suppress("No break at end of case")
		case 0:
			if(MiStorage_IsOpen){
				WriteStep++;
			}
			else{
				if(MiStorage_Open() == RESULT_ERROR){
					result = RESULT_FAULT;
				}
				break;
			} // @suppress("No break at end of case")
		case 1:
			if(PageNo != 0){
				WriteStep++;
			}
			else if(MT29F2G_Erase(&MT29F2G, BlockNo) != RESULT_OK){
				result = RESULT_ERROR;
				break;
			}
			else{
				WriteStep++;
			} // @suppress("No break at end of case")
		case 2:
			if(MT29F2G_BadBlockCheck(&MT29F2G, BlockNo) != RESULT_OK){
				result = RESULT_ERROR;
				break;
			}
			else{
				WriteStep++;
			} // @suppress("No break at end of case")
		case 3:
			Header.UnixTime = oDT_ToUnixTime(oDT_UpdateNow());
			Header.Sign = MISTORAGE_DATAFRAME_SIGN;
			Header.DLC = SizeOfData;
			Header.CRCValue = oMEM_CRC32(pData, SizeOfData, 0);

			memcpy(&Buffer[0], &Header, sizeof(MiStorage_PageHeader_t));
			memcpy(&Buffer[sizeof(MiStorage_PageHeader_t)], pData, SizeOfData);

			if(MT29F2G_WriteByte(&MT29F2G, BlockNo, PageNo, 0, (uint8_t *)&Buffer, sizeof(MiStorage_PageHeader_t)+SizeOfData) != RESULT_OK){
				result = RESULT_FAULT;
			}
			else{
				result = RESULT_OK;
			}
			break;
	}

	if(result != RESULT_RUN){
		WriteStep = 0;
	}

	switch(result)
	{
		default:
			break;
		case RESULT_RUN:
			MiStorage_AutoOffTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			break;
		case RESULT_FAULT:
			MiStorage_NandHeader.Status.IsFault = 1;
			MiStorage_NandHeader.Status.IsInitialized = 0;
			oSerial_Log("MiStorage", "Write FAULT Adr=%u B=%u P=%u", Address, BlockNo, PageNo);
			break;
		case RESULT_ERROR:
			MiStorage_NandHeader.Status.IsError = 1;
			oSerial_Log("MiStorage", "Write ERR Adr=%u B=%u P=%u", Address, BlockNo, PageNo);
			break;
		case RESULT_OK:
			MiStorage_NandHeader.Status.IsFault = 0;
			MiStorage_NandHeader.Status.IsError = 0;
			break;
	}

	return result;
}

oResult_t MiStorage_ReadIoTParameter(IoTParameter_t *pParameter)
{
	static uint8_t ReadParameterStep = 0;
	static uint8_t NandReadFailed = 0;
	static uint8_t NandPageIndex = 0;
	static MiStorage_PageHeader_t Header;
	oResult_t result = RESULT_RUN;

	if(pParameter == NULL){
		ReadParameterStep = 0;
		return RESULT_NULL;
	}

	switch(ReadParameterStep)
	{
		case 0:
			if(MiStorage_NandHeader.Status.IsFault){
				oSerial_Log("MiStorage", "ReadParam NAND skip(IsFault)");
				NandReadFailed = 1;
				ReadParameterStep = 2;
			}
			else{
				NandPageIndex = 0;
				ReadParameterStep = 1;
				break;
			} // @suppress("No break at end of case")
		case 1:
		{
			// Step 1: NAND 멀티페이지 읽기
			uint32_t offset = (uint32_t)NandPageIndex * MISTORAGE_DATA_MAXSIZE;
			uint32_t remaining = sizeof(IoTParameter_t) - offset;
			uint32_t chunkSize = (remaining > MISTORAGE_DATA_MAXSIZE) ? MISTORAGE_DATA_MAXSIZE : remaining;

			result = MiStorage_Read(MISTORAGE_PARAMETER_STARTADR + NandPageIndex, &Header, (uint8_t *)pParameter + offset, chunkSize);

			if(result == RESULT_OK){
				NandPageIndex++;
				if(offset + chunkSize < sizeof(IoTParameter_t)){
					result = RESULT_RUN;
					break;
				}
				oSerial_Log("MiStorage", "ReadParam NAND OK (%d pages)", NandPageIndex);
			}
			else if(result != RESULT_RUN){
				if(result == RESULT_FAULT){
					MiStorage_NandHeader.Status.IsFault = 0;
				}
				oSerial_Log("MiStorage", "ReadParam NAND FAIL(%d)->MCU fallback", result);
				NandReadFailed = 1;
				ReadParameterStep = 2;
				result = RESULT_RUN;
			}
			break;
		}
		case 2:
			// Step 2: MCU 내부 플래시 읽기 (fallback)
			result = Native_FlashRead((uint8_t *)pParameter, sizeof(IoTParameter_t));

			if(result == RESULT_OK){
				oSerial_Log("MiStorage", "ReadParam MCU OK");
				if(NandReadFailed && !MiStorage_NandHeader.Status.IsFault){
					ReadParameterStep = 3;
					NandPageIndex = 0;
					result = RESULT_RUN;
				}
			}
			else if(result != RESULT_RUN){
				oSerial_Log("MiStorage", "ReadParam MCU FAIL");
				result = RESULT_ERROR;
			}
			break;
		case 3:
		{
			// Step 3: MCU 데이터를 NAND에 복원 (멀티페이지)
			uint32_t offset = (uint32_t)NandPageIndex * MISTORAGE_DATA_MAXSIZE;
			uint32_t remaining = sizeof(IoTParameter_t) - offset;
			uint32_t chunkSize = (remaining > MISTORAGE_DATA_MAXSIZE) ? MISTORAGE_DATA_MAXSIZE : remaining;

			result = MiStorage_Write(MISTORAGE_PARAMETER_STARTADR + NandPageIndex, (uint8_t *)pParameter + offset, chunkSize);

			if(result == RESULT_OK){
				NandPageIndex++;
				if(offset + chunkSize < sizeof(IoTParameter_t)){
					result = RESULT_RUN;
					break;
				}
				oSerial_Log("MiStorage", "ReadParam NAND restored (%d pages)", NandPageIndex);
			}
			else if(result != RESULT_RUN){
				if(result == RESULT_FAULT){
					MiStorage_NandHeader.Status.IsFault = 0;
				}
				oSerial_Log("MiStorage", "ReadParam NAND restore FAIL(%d)", result);
				result = RESULT_OK;
			}
			break;
		}
	}

	if(result != RESULT_RUN){
		MiStorage_LastOperating = pParameter->Operating;
		ReadParameterStep = 0;
		NandReadFailed = 0;
	}

	return result;
}

oResult_t MiStorage_WriteIoTParameter(IoTParameter_t *pParameter)
{
	static uint8_t WriteParameterStep = 0;
	static uint8_t WriteParameterOk = 0;
	static uint8_t NandPageIndex = 0;
	IoT_DataPacket_t DataPacket;
	oResult_t result = RESULT_RUN;

	if(pParameter == NULL){
		WriteParameterStep = 0;
		return RESULT_NULL;
	}

	switch(WriteParameterStep)
	{
		default:
			oSerial_Log("MiStorage", "WriteParam INVALID step=%d", WriteParameterStep);
			WriteParameterStep = 0;
			result = RESULT_ERROR;
			break;
		case 0:
			WriteParameterStep++;
			WriteParameterOk = 0;
			NandPageIndex = 0; // @suppress("No break at end of case")
		case 1:
		{
			// Step 1: NAND 멀티페이지 저장 (실패해도 계속 진행)
			if(MiStorage_NandHeader.Status.IsFault){
				oSerial_Log("MiStorage", "WriteParam NAND skip(IsFault)");
				WriteParameterStep = 2;
			}
			else{
				uint32_t offset = (uint32_t)NandPageIndex * MISTORAGE_DATA_MAXSIZE;
				uint32_t remaining = sizeof(IoTParameter_t) - offset;
				uint32_t chunkSize = (remaining > MISTORAGE_DATA_MAXSIZE) ? MISTORAGE_DATA_MAXSIZE : remaining;

				result = MiStorage_Write(MISTORAGE_PARAMETER_STARTADR + NandPageIndex, (uint8_t *)pParameter + offset, chunkSize);

				if(result != RESULT_RUN){
					if(result == RESULT_OK){
						NandPageIndex++;
						if(offset + chunkSize < sizeof(IoTParameter_t)){
							result = RESULT_RUN;
							break;
						}
						WriteParameterOk |= 0x01;
						oSerial_Log("MiStorage", "WriteParam NAND OK (%d pages)", NandPageIndex);
					}
					else if(result == RESULT_FAULT){
						MiStorage_NandHeader.Status.IsFault = 0;
						oSerial_Log("MiStorage", "WriteParam NAND FAULT->recovered");
					}
					else{
						oSerial_Log("MiStorage", "WriteParam NAND ERR(%d) pg=%d", result, NandPageIndex);
					}
					WriteParameterStep = 2;
					result = RESULT_RUN;
				}
				break;
			} // @suppress("No break at end of case")
		}
		case 2:
			// Step 2: MCU 내부 플래시 저장
			if((result = Native_FlashWrite((uint8_t *)pParameter, sizeof(IoTParameter_t))) != RESULT_RUN){
				if(result == RESULT_OK){
					WriteParameterOk |= 0x02;
					oSerial_Log("MiStorage", "WriteParam MCU OK");
				}
				else{
					oSerial_Log("MiStorage", "WriteParam MCU FAIL(%d)", result);
				}
				WriteParameterStep++;
				result = RESULT_RUN;
			}
			break;
		case 3:
			if(WriteParameterOk & 0x02){
				result = RESULT_OK;
			}
			else{
				result = RESULT_ERROR;
			}
			oSerial_Log("MiStorage", "WriteParam Done(0x%02X) %s", WriteParameterOk, (WriteParameterOk & 0x02) ? "OK" : "FAIL");
			break;
	}

	if(result != RESULT_RUN){
		if(memcmp(&MiStorage_LastOperating, &pParameter->Operating, sizeof(IoTOperatingParameter_t)) != 0){
			IoT_MailboxItem_t *pItem = MiIoT_MailBox_Find(&MiIoT_LoRaMailBox, IoTDataType_Operating);

			if(pItem != NULL){
				pItem->Payload.DLC = sizeof(IoTOperatingParameter_t);
				memcpy(&pItem->Payload.Frame, &pParameter->Operating, pItem->Payload.DLC);
				pItem->QoS = 1;
				pItem->Timer = 0;
				oSerial_Log("MiStorage", "Operating update mailbox");
			}
			else{
				DataPacket.TypeOfData = IoTDataType_Operating;
				DataPacket.DLC = sizeof(IoTOperatingParameter_t);
				memcpy(&DataPacket.Frame, &pParameter->Operating, DataPacket.DLC);

				if(MiIoT_MailBox_NewItem(&MiIoT_LoRaMailBox, &DataPacket, 1, 0) != NULL){
					oSerial_Log("MiStorage", "Operating set mailbox");
				}
			}
		}

		MiStorage_LastOperating = pParameter->Operating;
		WriteParameterStep = 0;
	}

    return result;
}

oResult_t MiStorage_ReadIoTData(uint32_t Address, IoT_DataPacket_t* pPayload, MiStorage_PageHeader_t *pHeader)
{
	static MiStorage_PageHeader_t Header;
	oResult_t result = RESULT_RUN;

	if(pPayload == NULL){
		return RESULT_NULL;
	}

	if((result=MiStorage_Read(Address, &Header, (uint8_t *)pPayload, sizeof(IoT_DataPacket_t))) == RESULT_OK){
		if(MiIoT_IsSensorData(MiIoT_Parameter.Information.ProductCode, pPayload) != RESULT_OK){
			result = RESULT_ERROR;
		}

		if(pHeader){
			*pHeader = Header;
		}
	}

	return result;
}

oResult_t MiStorage_WriteIoTData(IoT_DataPacket_t* pPayload)
{
	oResult_t result;

	if(pPayload == NULL || MiIoT_IsSensorData(MiIoT_Parameter.Information.ProductCode, pPayload) != RESULT_OK){
		return RESULT_NULL;
	}

	uint32_t Address = MiStorage_NandHeader.SensorData.NewerAddress + 1;
	if(Address > MISTORAGE_DATA_ENDADR || Address < MISTORAGE_DATA_STARTADR){
		Address = MISTORAGE_DATA_STARTADR;
	}

	if((result=MiStorage_Write(Address, (uint8_t *)pPayload, sizeof(IoT_DataPacket_t))) == RESULT_OK){
		MiStorage_NandHeader.SensorData.NewerAddress = Address;
		oDT_GetNow(&MiStorage_NandHeader.SensorData.NewerDateTime);
		oSerial_Log("MiStorage", "WriteData OK Adr=%u Cnt=%d", Address, MiStorage_NandHeader.SensorData.CountOfData);
	}
	else if(result != RESULT_RUN){
		oSerial_Log("MiStorage", "WriteData FAIL(%d) Adr=%u", result, Address);
	}

	return result;
}

oResult_t MiStorage_SensorDataIndexing()
{
	static uint8_t NandInitStep = 0;
	static uint32_t SearchAddress = 0;
	static uint32_t FaultCount;
	static uint32_t ErrorCount;
	static uint32_t ReadCount;
	static uint32_t BadBlockCount;
	IoT_DataPacket_t IoTData;
	uint32_t Block = MISTORAGE_ADR_BLOCK(SearchAddress);
	uint32_t Page = MISTORAGE_ADR_PAGE(SearchAddress);
	oResult_t result = RESULT_RUN;
	MiStorage_PageHeader_t Header;
	oDateAndTime_t DT;

	if(!MiStorage_IsOpen){
		oSerial_Log("MiStorage", "Indexing skip(!IsOpen)");
		NandInitStep = 0;
		return RESULT_OK;
	}

	if(MiStorage_NandHeader.Status.IsFault){
		oSerial_Log("MiStorage", "Indexing abort(IsFault)");
		NandInitStep = 0;
		return RESULT_FAULT;
	}

	switch(NandInitStep)
	{
		case 0:
			memset(&MiStorage_NandHeader.SensorData, 0, sizeof(MiStorage_NandHeader.SensorData));
			memset(&MiStorage_NandHeader.Status, 0, sizeof(MiStorage_NandHeader.Status));

			MiStorage_NandHeader.SensorData.OlderDateTime = DT_MAX_VALUE;
			MiStorage_NandHeader.SensorData.NewerDateTime = DT_MIN_VALUE;

			FaultCount = 0;
			ErrorCount = 0;
			ReadCount = 0;
			BadBlockCount = 0;
			SearchAddress = MISTORAGE_DATA_STARTADR;
			oSerial_Log("MiStorage", "Indexing start");
			NandInitStep++;
			break;
		case 1:
			if(Page != 0){
				NandInitStep = 2;
				break;
			}

			if((result=MT29F2G_BadBlockCheck(&MT29F2G, Block)) != RESULT_OK){
				switch(result)
				{
					case RESULT_FAULT:
						FaultCount++;
						SearchAddress = MISTORAGE_ADDRESS(Block+1, 0);
						break;
					default:
						SearchAddress = MISTORAGE_ADDRESS(Block+1, 0);
						BadBlockCount++;
						break;
				}
				break;
			}

			NandInitStep++;
			break;
		case 2:
		{
			oResult_t r = MiStorage_ReadIoTData(SearchAddress, &IoTData, &Header);
			switch(r)
			{
				case RESULT_RUN:
					break;
				case RESULT_FAULT:
					FaultCount++;
					if(Page == 0){
						SearchAddress = MISTORAGE_ADDRESS(Block+1, 0);
					}
					else{
						SearchAddress++;
					}
					NandInitStep = 1;
					break;
				case RESULT_ERROR:
					ErrorCount++;
					if(Page == 0){
						SearchAddress = MISTORAGE_ADDRESS(Block+1, 0);
					}
					else{
						SearchAddress++;
					}
					NandInitStep = 1;
					break;
				case RESULT_OK:
					ReadCount++;
					MiStorage_NandHeader.SensorData.CountOfData++;
					DT = oDT_FromUnixTime(Header.UnixTime);

					if(oDT_Compare(&MiStorage_NandHeader.SensorData.OlderDateTime, &DT) == 1){
						MiStorage_NandHeader.SensorData.OlderAddress = SearchAddress;
						MiStorage_NandHeader.SensorData.OlderDateTime = DT;
					}

					if(oDT_Compare(&MiStorage_NandHeader.SensorData.NewerDateTime, &DT) <= 0){
						MiStorage_NandHeader.SensorData.NewerAddress = SearchAddress;
						MiStorage_NandHeader.SensorData.SyncAddress = SearchAddress;
						MiStorage_NandHeader.SensorData.NewerDateTime = DT;
					}

					SearchAddress++;
					NandInitStep = 1;
					break;
				default:
					SearchAddress++;
					NandInitStep = 1;
					break;
			}
            break;
		}
	}

	if((SearchAddress >= MISTORAGE_DATA_ENDADR) || (ReadCount > 0 && ErrorCount > 3)){
		MiStorage_NandHeader.Status.IsOkay = 1;
		MiStorage_NandHeader.Status.IsInitialized = 1;
		NandInitStep = 0;
		oSerial_Log("MiStorage", "Indexing OK Cnt=%u Err=%u Flt=%u Bad=%u Newer=%d Older=%d",
			ReadCount, ErrorCount, FaultCount, BadBlockCount,
			MiStorage_NandHeader.SensorData.NewerAddress,
			MiStorage_NandHeader.SensorData.OlderAddress);
		return RESULT_OK;
	}
	else if(ReadCount == 0 && ErrorCount > 1){
		// Empty NAND - no data written yet, treat as initialized
		MiStorage_NandHeader.Status.IsOkay = 1;
		MiStorage_NandHeader.Status.IsInitialized = 1;
		NandInitStep = 0;
		oSerial_Log("MiStorage", "Indexing EMPTY Err=%u Bad=%u", ErrorCount, BadBlockCount);
		return RESULT_OK;
	}
	else if(ErrorCount > 1){
		NandInitStep = 0;
		MiStorage_NandHeader.Status.IsError = 1;
		oSerial_Log("MiStorage", "Indexing ERR Cnt=%u Err=%u Flt=%u Adr=%u", ReadCount, ErrorCount, FaultCount, SearchAddress);
		return RESULT_ERROR;
	}
	else if(FaultCount > 1){
		NandInitStep = 0;
		MiStorage_NandHeader.Status.IsFault = 1;
		oSerial_Log("MiStorage", "Indexing FAULT Cnt=%u Err=%u Flt=%u Adr=%u", ReadCount, ErrorCount, FaultCount, SearchAddress);
		return RESULT_FAULT;
	}

    return RESULT_RUN;
}

oResult_t MiStorage_EraseDataArea()
{
	static uint32_t EraseBlock = 0;
	static uint8_t EraseStep = 0;
	oResult_t result = RESULT_RUN;

	switch(EraseStep)
	{
		case 0:
			if(MiStorage_IsOpen){
				EraseStep++;
				EraseBlock = MISTORAGE_ADR_BLOCK(MISTORAGE_DATA_STARTADR);
				oSerial_Log("MiStorage", "Erase start B=%u~%u", EraseBlock, (uint32_t)MISTORAGE_ADR_BLOCK(MISTORAGE_DATA_ENDADR));
			}
			else{
				if(MiStorage_Open() == RESULT_ERROR){
					oSerial_Log("MiStorage", "Erase FAIL(Open)");
					result = RESULT_ERROR;
				}
			}
			break;
		case 1:
			if(EraseBlock > MISTORAGE_ADR_BLOCK(MISTORAGE_DATA_ENDADR)){
				memset(&MiStorage_NandHeader.SensorData, 0, sizeof(MiStorage_NandHeader.SensorData));
				MiStorage_NandHeader.Status.IsInitialized = 1;
				MiStorage_NandHeader.Status.IsOkay = 1;
				oSerial_Log("MiStorage", "Erase OK");
				result = RESULT_OK;
			}
			else{
				if(MT29F2G_BadBlockCheck(&MT29F2G, EraseBlock) == RESULT_OK){
					MT29F2G_Erase(&MT29F2G, EraseBlock);
				}
				EraseBlock++;
			}
			break;
	}

	if(result != RESULT_RUN){
		EraseStep = 0;
	}

	return result;
}

void MiStorage()
{
	static uint8_t MiStorageStep = 0;

	MiStorage_IsBusy = GPIOs.DO.NANDEnable || MiStorage_IsOpen;

	switch(MiStorageStep)
	{
		default:
			break;
		case 0:
			switch(MiStorage_ReadIoTParameter(&MiIoT_Parameter))
			{
				case RESULT_RUN:
					break;
				case RESULT_OK:
					oSerial_Log("MiStorage", "Storage[0] ReadParam OK->Indexing");

					if(MiIoT_DefaultParameter.Information.ProductCode != MiIoT_Parameter.Information.ProductCode){
						MiIoT_Parameter = MiIoT_DefaultParameter;
						oSerial_Log("MiStorage", "Miss matched product code->DefaultParam");
					}

					MiStorageStep += 2;
					break;
				default:
					oSerial_Log("MiStorage", "Storage[0] ReadParam FAIL->DefaultParam");
					MiIoT_Parameter = MiIoT_DefaultParameter;
					MiStorageStep += 1;
					break;
			}
			break;
		case 1:
			if(MiStorage_WriteIoTParameter(&MiIoT_Parameter) != RESULT_RUN){
				oSerial_Log("MiStorage", "Storage[1] WriteParam->Indexing");
				MiStorageStep++;
			}
			break;
		case 2:
			if(MiStorage_SensorDataIndexing() != RESULT_RUN){
				oSerial_Log("MiStorage", "Storage[2] Indexing done Init=%d Fault=%d OK=%d",
					MiStorage_NandHeader.Status.IsInitialized,
					MiStorage_NandHeader.Status.IsFault,
					MiStorage_NandHeader.Status.IsOkay);
				MiStorageStep++;
			}
			break;
		case 3:
			if(MiStorage_ParameterSaveCmd == 1){
				switch(MiStorage_WriteIoTParameter(&MiIoT_Parameter))
				{
					case RESULT_RUN:
						break;
					case RESULT_OK:
						MiStorage_ParameterSaveCmd = 2;
						break;
					default:
						MiStorage_ParameterSaveCmd = -1;
						break;
				}
			}

			if(MiStorage_IsOpen && oTMR_Elapsed(&MiStorage_AutoOffTimer, 1000, TICKBASE_SYSTICK)){
				MiStorage_Close();
			}
			break;
	}

	if(MiStorage_NandHeader.Status.CountOfError > 3){
		MiStorage_NandHeader.Status.IsOkay = 0;
		MiStorage_NandHeader.Status.IsFault = 1;
		oSerial_Log("MiStorage", "ErrCnt>3->IsFault");
	}
}
