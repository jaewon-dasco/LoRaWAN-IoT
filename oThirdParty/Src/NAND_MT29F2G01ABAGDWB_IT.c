/*
 * NAND_MT29F2G01ABAGDWB_IT.c
 *
 * Created on: Feb 25, 2025
 * Author: JONE
 *
 * 이 파일은 QSPI Dual Flash 환경에서 Micron MT29F2G01ABAGDWB-IT NAND Flash를 제어하기 위한
 * 초기화, 읽기, 쓰기, 블록 삭제 등 기본 기능을 구현한 예제 코드입니다.
 */

#include "NAND_MT29F2G01ABAGDWB_IT.h"

#include "string.h"
#include "ONE_Math.h"
#include "ONE_Memory.h"
#include "ONE_Time.h"
#include "ONE_Serial.h"

#ifdef MT29F2G_ENABLED
#define NAND_TIMEOUT_VALUE	1000U //HAL_QPSI_TIMEOUT_DEFAULT_VALUE
#define NAND_TEST_ADDRESS	(MT29F2G_ADDRESS(4,0,0) & (uint32_t)0xFFFFFF)

MT29F2G_Result_t MT29F2G_GetFeature(MT29F2G_t *pMT29F2G, uint32_t Address, uint8_t *pData)
{
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return MT29F2G_RESULT_NULL;
	}

	memset(&s_command, 0, sizeof(s_command));

	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0x0F;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_8_BITS;
	s_command.Address         = Address & 0xFF;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_1_LINE;
	s_command.DummyCycles     = 0;
	s_command.NbData          = 1;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if (HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}

	if (HAL_QSPI_Receive(pMT29F2G->pHandle, pData, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_RX_ERROR;
	}

	return MT29F2G_RESULT_OK;
}

MT29F2G_Result_t MT29F2G_GetStatusRegister(MT29F2G_t *pMT29F2G)
{
	return MT29F2G_GetFeature(pMT29F2G, 0xC0, (uint8_t *)&pMT29F2G->State);
}

int8_t MT29F2G_CheckStatusRegister(MT29F2G_t *pMT29F2G, uint8_t Mask)
{
	if(MT29F2G_GetStatusRegister(pMT29F2G) != MT29F2G_RESULT_OK){
		return -1;
	}

	if(Mask != 0){
		if((pMT29F2G->State & Mask) != 0){
			return 1;
		}
		else{
			return 0;
		}
	}

	return 1;
}

/**
  * @brief  QSPI 주변장치를 강제 복구한다 (Abort 실패 시 RCC 리셋 + 재초기화).
  */
static void MT29F2G_RecoverQSPI(MT29F2G_t *pMT29F2G)
{
	HAL_QSPI_Abort(pMT29F2G->pHandle);
	if(pMT29F2G->pHandle->State != HAL_QSPI_STATE_READY){
		__HAL_RCC_QSPI_FORCE_RESET();
		__HAL_RCC_QSPI_RELEASE_RESET();
		pMT29F2G->pHandle->State = HAL_QSPI_STATE_RESET;
		HAL_QSPI_Init(pMT29F2G->pHandle);
	}
}

/**
  * @brief  메모리 준비(Write In Progress 해제)를 확인하기 위해 SR를 자동 폴링한다.
  * @retval MT29F2G_RESULT_OK 또는 RESULT_ERROR
  */
MT29F2G_Result_t MT29F2G_AutoPolling(MT29F2G_t *pMT29F2G, uint8_t Match, uint8_t Mask)
{
	QSPI_CommandTypeDef s_command;
	QSPI_AutoPollingTypeDef s_config;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return MT29F2G_RESULT_NULL;
	}

	memset(&s_command, 0, sizeof(s_command));

	// 메모리 준비 완료 상태를 확인 (WIP 비트가 0인지)
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0x0F;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_8_BITS;
	s_command.Address         = 0xC0;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_1_LINE;
	s_command.DummyCycles     = 0;
	s_command.NbData			= 0;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;

	s_config.Match            = Match;
	s_config.Mask             = Mask;
	s_config.MatchMode        = QSPI_MATCH_MODE_AND;
	s_config.StatusBytesSize  = 1;
	s_config.Interval         = 0x10;
	s_config.AutomaticStop    = QSPI_AUTOMATIC_STOP_ENABLE;

	if (HAL_QSPI_AutoPolling(pMT29F2G->pHandle, &s_command, &s_config, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		MT29F2G_RecoverQSPI(pMT29F2G);
		return MT29F2G_RESULT_AUTOPOLLING_ERROR;
	}

	return MT29F2G_RESULT_OK;
}


/**
  * @brief  Write Enable 명령을 전송하고, 쓰기 활성화 상태가 될 때까지 대기한다.
  * @retval MT29F2G_RESULT_OK 또는 RESULT_ERROR
  */
MT29F2G_Result_t MT29F2G_WriteEnable(MT29F2G_t *pMT29F2G)
{
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return MT29F2G_RESULT_NULL;
	}

	memset(&s_command, 0, sizeof(s_command));

	// Write Enable 명령 (0x06) 전송
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0x06;
	s_command.AddressMode     = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_NONE;
	s_command.DummyCycles     = 0;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if (HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}

	// 쓰기 활성화 상태를 확인하기 위해 자동 폴링 모드 설정
	if(MT29F2G_AutoPolling(pMT29F2G, MT29F2G_SR_WREN, MT29F2G_SR_WREN) != MT29F2G_RESULT_OK){
		return MT29F2G_RESULT_AUTOPOLLING_ERROR;
	}

	return MT29F2G_RESULT_OK;
}

MT29F2G_Result_t MT29F2G_SetFeature(MT29F2G_t *pMT29F2G, uint32_t Address, uint8_t Data)
{
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return MT29F2G_RESULT_NULL;
	}

	// SetFeature(0x1F)는 Write Enable 불필요 (Micron 데이터시트)
	// WREN은 Program/Erase 명령에만 필요

	memset(&s_command, 0, sizeof(s_command));

	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0x1F;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_8_BITS;
	s_command.Address         = Address & 0xFF;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_1_LINE;
	s_command.DummyCycles     = 0;
	s_command.NbData          = 1;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if (HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}

	if (HAL_QSPI_Transmit(pMT29F2G->pHandle, &Data, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}

	return MT29F2G_RESULT_OK;
}

MT29F2G_Result_t MT29F2G_ReadMemory(MT29F2G_t *pMT29F2G, uint32_t Address, uint8_t *pData, uint32_t Size)
{
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL || pData == NULL || Size == 0){
		return MT29F2G_RESULT_NULL;
	}

	// Recover QSPI if stuck from previous failed operation
	if(pMT29F2G->pHandle->State != HAL_QSPI_STATE_READY){
		MT29F2G_RecoverQSPI(pMT29F2G);
	}

	if(MT29F2G_CheckStatusRegister(pMT29F2G, MT29F2G_SR_WIP|MT29F2G_SR_CRBSY) == 1){
		return MT29F2G_RESULT_BUSY;
	}

	memset(&s_command, 0, sizeof(s_command));

	// 1. PAGE READ 명령 전송 (0x13)
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0x13;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_24_BITS;
	s_command.Address     		= (Address >> 12) & 0x0FFFFFFF;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_NONE;
	s_command.DummyCycles     = 0;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}

	if(MT29F2G_AutoPolling(pMT29F2G, 0, MT29F2G_SR_CRBSY) != MT29F2G_RESULT_OK){
		return MT29F2G_RESULT_AUTOPOLLING_ERROR;
	}

	memset(&s_command, 0, sizeof(s_command));

	// 3. 실제 읽기 명령 전송 (READ FROM CACHE)
	#if MT29F2G_READ_MODE == 1
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0x03;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_16_BITS;
	s_command.Address         = Address & 0x0FFF;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_1_LINE;
	s_command.DummyCycles     = 0;
	s_command.NbData          = Size;
	#elif MT29F2G_READ_MODE == 2
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0x3B;
	s_command.AddressMode     = QSPI_ADDRESS_2_LINES;
	s_command.AddressSize     = QSPI_ADDRESS_16_BITS;
	s_command.Address         = Address & 0x0FFF;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_2_LINES;
	s_command.DummyCycles     = 8;
	s_command.NbData          = Size;
	#elif MT29F2G_READ_MODE == 4
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0x6B;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_16_BITS;
	s_command.Address         = Address & 0x0FFF;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_4_LINES;
	s_command.DummyCycles     = 8;
	s_command.NbData          = Size;
	#endif

	if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}

	if(HAL_QSPI_Receive(pMT29F2G->pHandle, pData, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_RX_ERROR;
	}

	// ECC 상태 확인 후 결과 리턴
	if(MT29F2G_CheckStatusRegister(pMT29F2G, MT29F2G_SR_ECCFAULT) == 1){ //ECC fault
		return MT29F2G_RESULT_ECC_ERROR;
	}

	return MT29F2G_RESULT_OK;
}


MT29F2G_Result_t MT29F2G_WriteMemory(MT29F2G_t *pMT29F2G, uint32_t Address, uint8_t *pData, uint32_t Size)
{
	MT29F2G_Result_t result;
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL || pData == NULL || Size == 0){
		return MT29F2G_RESULT_NULL;
	}

	if(MT29F2G_CheckStatusRegister(pMT29F2G, MT29F2G_SR_WIP) == 1){
		return MT29F2G_RESULT_BUSY;
	}

	// 쓰기 활성화 명령 전송
	if((result=MT29F2G_WriteEnable(pMT29F2G)) != MT29F2G_RESULT_OK){
		return result;
	}

	memset(&s_command, 0, sizeof(s_command));

	// PROGRAM Load 명령 전송
	#if MT29F2G_WRITE_MODE == 1
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0x02; // PROGRAM LOAD
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_16_BITS;
	s_command.Address         = Address & 0x0FFF;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_1_LINE;
	s_command.DummyCycles     = 0;
	s_command.NbData          = Size;
	#elif MT29F2G_WRITE_MODE == 4
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0x32; // PROGRAM LOAD (4-Lines)
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_16_BITS;
	s_command.Address         = Address & 0x0FFF; //Column address
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_4_LINES;
	s_command.DummyCycles     = 0;
	s_command.NbData          = Size;
	#endif

	if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}

	if(HAL_QSPI_Transmit(pMT29F2G->pHandle, pData, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}

	memset(&s_command, 0, sizeof(s_command));

	// PROGRAM EXECUTE 명령 전송 (0x10)
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0x10;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_24_BITS;
	s_command.Address         = (Address >> 12) & 0x00FFFFFF;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_NONE;
	s_command.DummyCycles     = 0;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;
	s_command.NbData          = 0;

	if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK){
		return MT29F2G_RESULT_TX_ERROR;
	}

	// 쓰기 완료까지 대기
	if((result = MT29F2G_AutoPolling(pMT29F2G, 0, MT29F2G_SR_WIP)) != MT29F2G_RESULT_OK){
		return result;
	}

	 //program failed
    if(MT29F2G_CheckStatusRegister(pMT29F2G, MT29F2G_SR_PFAIL) == 1){
    	return MT29F2G_RESULT_PROGRAM_FAIL;
    }

	return MT29F2G_RESULT_OK;
}

MT29F2G_Result_t MT29F2G_GetInfo(MT29F2G_t *pMT29F2G, MT29F2G_Info_t *pInfo)
{
	QSPI_CommandTypeDef s_command;
	uint8_t status;
	uint8_t status_tmp;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return MT29F2G_RESULT_NULL;
	}

	// 상태 레지스터를 두 번 읽어 안정화
	MT29F2G_GetStatusRegister(pMT29F2G);

	memset(&s_command, 0, sizeof(s_command));

	// 정보 읽기를 위한 명령 구성 (0x0F, 주소 0xB0)
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0x0F;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_8_BITS;
	s_command.Address         = 0xB0;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_1_LINE;
	s_command.DummyCycles     = 0;
	s_command.NbData          = 1;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}
	if(HAL_QSPI_Receive(pMT29F2G->pHandle, &status, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_RX_ERROR;
	}

	status_tmp = status;

	// OTP_EN 활성화를 위해 비트 수정
	CLEAR_BIT(status_tmp, 0xC2);
	SET_BIT(status_tmp, 0x40);

	// OTP_EN 설정 (WriteEnable 포함)
	if(MT29F2G_SetFeature(pMT29F2G, 0xB0, status_tmp) != MT29F2G_RESULT_OK){
		return MT29F2G_RESULT_TX_ERROR;
	}

	// 정보 페이지 읽기 (주소 0x1000) 후 pInfo에 저장
	MT29F2G_ReadMemory(pMT29F2G, 0x1000, (uint8_t *)pInfo, sizeof(MT29F2G_Info_t));

	// 원래 상태 복원 (WriteEnable 포함)
	if(MT29F2G_SetFeature(pMT29F2G, 0xB0, status) != MT29F2G_RESULT_OK){
		return MT29F2G_RESULT_TX_ERROR;
	}

	return MT29F2G_RESULT_OK;
}

oResult_t MT29F2G_Init(MT29F2G_t *pMT29F2G, QSPI_HandleTypeDef *pHandle)
{
	static uint8_t MT29F2G_InitStep = 0;
	static uint32_t MT29F2G_InitTimer = 0;

	if(pMT29F2G == NULL || pHandle == NULL){
		MT29F2G_InitStep = 0;
		return RESULT_ERROR;
	}

	oResult_t result = RESULT_RUN;

	switch(MT29F2G_InitStep)
	{
		case 0:
			pMT29F2G->pHandle = pHandle;
			pMT29F2G->IsOpen = 0;

			HAL_QSPI_DeInit(pMT29F2G->pHandle);

			if(pMT29F2G->pHandle->State == HAL_QSPI_STATE_READY){
				// QSPI 이미 초기화됨 (DeInit에서 QSPI 유지) → Reset 단계로 직행
				oSerial_Log("MT29F2G", "Init QSPI already READY");
				MT29F2G_InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				MT29F2G_InitStep = 2;
				break;
			}
			else{
				MT29F2G_InitStep++;
			}
			break;
		case 1:
			if(HAL_QSPI_Init(pMT29F2G->pHandle) != HAL_OK){
				oSerial_Log("MT29F2G", "Init QSPIInit FAIL");
				result = RESULT_FAULT;
				break;
			}
			MT29F2G_InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			MT29F2G_InitStep = 2;
			break;
		case 2:
			// 50ms 대기 → Reset 전송
			if(oTMR_Elapsed(&MT29F2G_InitTimer, 50, TICKBASE_SYSTICK)){
				QSPI_CommandTypeDef rst_cmd = {0};
				rst_cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
				rst_cmd.Instruction     = 0xFF;
				rst_cmd.AddressMode     = QSPI_ADDRESS_NONE;
				rst_cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
				rst_cmd.DataMode        = QSPI_DATA_NONE;
				rst_cmd.DummyCycles     = 0;
				rst_cmd.DdrMode         = QSPI_DDR_MODE_DISABLE;
				rst_cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
				rst_cmd.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;
				HAL_QSPI_Command(pMT29F2G->pHandle, &rst_cmd, 1000);
				MT29F2G_InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				MT29F2G_InitStep = 3;
			}
			break;
		case 3:
			if(oTMR_Elapsed(&MT29F2G_InitTimer, 5, TICKBASE_SYSTICK)){
				MT29F2G_InitStep++;
			}
			break;
		case 4:
			// Protection register (0xA0) - unlock blocks + WP-E enable
			if(MT29F2G_SetFeature(pMT29F2G, 0xA0, 0x02) != MT29F2G_RESULT_OK){
				oSerial_Log("MT29F2G", "Init SetProt FAIL");
				result = RESULT_FAULT;
				break;
			}

			MT29F2G_InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			MT29F2G_InitStep++;
			break;
		case 5:
			// Configuration register (0xB0) - ECC Enable + Quad Enable
			if(MT29F2G_SetFeature(pMT29F2G, 0xB0, 0x11) != MT29F2G_RESULT_OK){
				oSerial_Log("MT29F2G", "Init SetCfg FAIL");
				result = RESULT_FAULT;
				break;
			}
			MT29F2G_InitStep++;
			break;
		case 6:
			MT29F2G_GetStatusRegister(pMT29F2G);
			pMT29F2G->IsOpen = 1;
			oSerial_Log("MT29F2G", "Init OK SR=0x%02X", pMT29F2G->State);
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		MT29F2G_InitStep = 0;
	}

	return result;
}

oResult_t MT29F2G_DeInit(MT29F2G_t *pMT29F2G)
{
	static uint8_t DeInitStep = 0;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		DeInitStep = 0;
		return RESULT_OK;
	}

	oResult_t result = RESULT_RUN;

	switch(DeInitStep)
	{
		case 0:
			// 1. Protection register (0xA0) POR 디폴트 원복
			if(MT29F2G_SetFeature(pMT29F2G, 0xA0, 0x7C) != MT29F2G_RESULT_OK){
				oSerial_Log("MT29F2G", "DeInit SetProt FAIL->skip");
			}
			DeInitStep++;
			break;
		case 1:
			// 2. Configuration register (0xB0) POR 디폴트 원복
			if(MT29F2G_SetFeature(pMT29F2G, 0xB0, 0x10) != MT29F2G_RESULT_OK){
				oSerial_Log("MT29F2G", "DeInit SetCfg FAIL->skip");
			}
			DeInitStep++;
			break;
		case 2:
			// 3. OIP Ready 상태까지 Polling 대기
			if(MT29F2G_AutoPolling(pMT29F2G, 0, MT29F2G_SR_WIP) != MT29F2G_RESULT_OK){
				oSerial_Log("MT29F2G", "DeInit Poll FAIL->skip");
			}
			DeInitStep++;
			break;
		case 3:
			// 4. QSPI DeInit
			HAL_QSPI_DeInit(pMT29F2G->pHandle);
			pMT29F2G->IsOpen = 0;
			oSerial_Log("MT29F2G", "DeInit OK");
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		DeInitStep = 0;
	}

	return result;
}

oResult_t MT29F2G_Erase(MT29F2G_t *pMT29F2G, uint32_t NumberOfBlock)
{
	uint32_t Address = MT29F2G_ADDRESS(NumberOfBlock, 0, 0);
	MT29F2G_Result_t result = MT29F2G_RESULT_RUN;
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || !pMT29F2G->IsOpen){
		return RESULT_NULL;
	}

	// 쓰기 활성화
	if((result = MT29F2G_WriteEnable(pMT29F2G)) != MT29F2G_RESULT_OK){
		return RESULT_FAULT;
	}

	memset(&s_command, 0, sizeof(s_command));

	// BLOCK ERASE 명령 (0xd8) 전송
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0xD8;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_24_BITS;
	s_command.Address         = (Address >> 12) & 0x00FFFFFF;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_NONE;
	s_command.DummyCycles     = 0;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK){
		return RESULT_FAULT;
	}

	// 블록 삭제 완료까지 대기
	if((result = MT29F2G_AutoPolling(pMT29F2G, 0, MT29F2G_SR_WIP)) != MT29F2G_RESULT_OK){
		return RESULT_FAULT;
	}

    if(MT29F2G_CheckStatusRegister(pMT29F2G, MT29F2G_SR_EFAIL) == 1){ //erase failed
    	return RESULT_FAULT;
    }

    return RESULT_OK;
}


oResult_t MT29F2G_BadBlockCheck(MT29F2G_t *pMT29F2G, uint32_t NumberOfBlock)
{
	uint8_t spare[2];

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return RESULT_NULL;
	}

	for(uint8_t page = 0; page < 6; page++)
	{
		uint32_t Address = MT29F2G_ADDRESS(NumberOfBlock, page, MT29F2G_SPARE_COLUMN);

		MT29F2G_Result_t rr = MT29F2G_ReadMemory(pMT29F2G, Address, (uint8_t *)&spare, sizeof(spare));
	    if(rr != MT29F2G_RESULT_OK){
	    	oSerial_Log("MT29F2G", "BBC B=%u P=%u err(%d)", NumberOfBlock, page, rr);
	    	return RESULT_FAULT;
	    }

		if(spare[0] != 0xFF || spare[1] != 0xFF) //배드블럭
		{
			return RESULT_ERROR;
		}
	}

	return RESULT_OK;
}

oResult_t MT29F2G_Reset(MT29F2G_t *pMT29F2G)
{
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return RESULT_NULL;
	}

	memset(&s_command, 0, sizeof(s_command));

	// 리셋 명령 (0xFF) 전송
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = 0xFF;
	s_command.AddressMode     = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_NONE;
	s_command.DummyCycles     = 0;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
	  return RESULT_FAULT;
	}

	// 리셋 후 메모리 준비 완료까지 자동 폴링
	if(MT29F2G_AutoPolling(pMT29F2G, 0, MT29F2G_SR_WIP) != MT29F2G_RESULT_OK){
		return RESULT_FAULT;
	}

	MT29F2G_GetStatusRegister(pMT29F2G);

	return RESULT_OK;
}

oResult_t MT29F2G_ReadByte(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint32_t ByteIndex, uint8_t *pData, uint32_t Size)
{
    if(pMT29F2G == NULL || pData == NULL || Block >= MT29F2G_BLOCK_LENGTH || Page >= MT29F2G_PAGE_LENGTH || ByteIndex >= MT29F2G_MAINDATA_LENGTH){
    	return RESULT_FAULT;
    }

    if(pMT29F2G->pHandle->State != HAL_QSPI_STATE_READY){
    	return RESULT_FAULT;
    }

	Size = MATH_MIN(Size, MT29F2G_MAINDATA_LENGTH - ByteIndex);

    if(MT29F2G_ReadMemory(pMT29F2G, MT29F2G_ADDRESS(Block, Page, ByteIndex), pData, Size) != MT29F2G_RESULT_OK){
    	return RESULT_FAULT;
    }

    return RESULT_OK;
}

oResult_t MT29F2G_ReadPage(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint8_t *pData, uint32_t Size)
{
    return MT29F2G_ReadByte(pMT29F2G, Block, Page, 0, pData, Size);
}

oResult_t MT29F2G_WriteByte(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint32_t ByteIndex, uint8_t *pData, uint32_t Size)
{
	if(pMT29F2G == NULL || pData == NULL || Block >= MT29F2G_BLOCK_LENGTH || Page >= MT29F2G_PAGE_LENGTH || ByteIndex >= MT29F2G_MAINDATA_LENGTH){
    	return RESULT_FAULT;
    }

    if(pMT29F2G->pHandle->State != HAL_QSPI_STATE_READY){
    	return RESULT_FAULT;
    }

	Size = MATH_MIN(Size, MT29F2G_MAINDATA_LENGTH - ByteIndex);

	if(MT29F2G_WriteMemory(pMT29F2G, MT29F2G_ADDRESS(Block, Page, ByteIndex), pData, Size) != MT29F2G_RESULT_OK){
		return RESULT_FAULT;
	}

	return RESULT_OK;
}

oResult_t MT29F2G_WritePage(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint8_t *pData, uint32_t Size)
{
	return MT29F2G_WriteByte(pMT29F2G, Block, Page, 0, pData, Size);
}
#endif
