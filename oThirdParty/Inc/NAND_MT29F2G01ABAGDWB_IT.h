/*
 * NAND_MT29F2G01ABAGDWB_IT.h
 *
 *  Created on: Feb 25, 2025
 *      Author: JONE
 */

#ifndef NAND_MT29F2G01ABAGDWB_IT_H_
#define NAND_MT29F2G01ABAGDWB_IT_H_

#include "main.h"
#include "ONE_Common.h"

#ifdef QSPI_DUALFLASH_ENABLE

#ifdef __cplusplus
extern "C"
{
#endif

#define MT29F2G_ENABLED

 #define MT29F2G_ADDRESS(block, page, column)  ( \
	((uint32_t)(column)  & 0xFFF) | \
	(((uint32_t)(page)   & 0x3F)  << 12) | \
    (((uint32_t)(block)  & 0x7FF) << 18) \
)

#define MT29F2G_ADDRESS_BLOCK(address)  	(((uint32_t)(address) >> 18) & 0x7FF)
#define MT29F2G_ADDRESS_PAGE(address)  		(((uint32_t)(address) >> 12) & 0x3F)
#define MT29F2G_ADDRESS_COLUMN(address)  	((uint32_t)(address) & 0xFFF)

/*
#define MT29F2G_ADDRESS(block, page, column)  ( \
    (((uint32_t)(column) & 0x7FF)      ) |  \
    (((uint32_t)(page)   & 0x3F) << 11 ) |  \
    (((uint32_t)(block)  & 0x7FF)<< 17 )    \
)

#define MT29F2G_ADDRESS_BLOCK(address)   (((uint32_t)(address) >> 17) & 0x7FF)
#define MT29F2G_ADDRESS_PAGE(address)    (((uint32_t)(address) >> 11) & 0x3F)
#define MT29F2G_ADDRESS_COLUMN(address)  ((uint32_t)(address) & 0x7FF)
*/

#define MT29F2G_SPARE_COLUMN      	0x800  // OOB 시작 컬럼

#define MT29F2G_ADDRESS_MAX 		0x0FFFFFFF
#define MT29F2G_MAINDATA_LENGTH 	2048
#define MT29F2G_PAGE_LENGTH 		64
#define MT29F2G_BLOCK_LENGTH		2048
#define MT29F2G_TOTLA_PAGE_LENGTH 	(MT29F2G_BLOCK_LENGTH*MT29F2G_PAGE_LENGTH)


/* Status Register - Page.43 */
#define MT29F2G_SR_WIP ((uint8_t)0x01)   /*!< Write in progress */
#define MT29F2G_SR_WREN ((uint8_t)0x02)  /*!< Write enable latch */
#define MT29F2G_SR_EFAIL ((uint8_t)0x04) /*!< Erase fail */
#define MT29F2G_SR_PFAIL ((uint8_t)0x08) /*!< Program fail */
#define MT29F2G_SR_ECCFAULT ((uint8_t)0x20)   /*!< ECC status */
#define MT29F2G_SR_CRBSY ((uint8_t)0x80) /*!< Cache read busy */
#define MT29F2G_SR_ECC ((uint8_t)0x70)   /*!< ECC status */

#define MT29F2G_READ_MODE 4
#define MT29F2G_WRITE_MODE 4

#define MT29F2G_PFAIL ((uint8_t)0x01)
#define MT29F2G_EFAIL ((uint8_t)0x02)
#define MT29F2G_ECC_FAIL ((uint8_t)0x4)

typedef struct
{
    uint32_t Signature;                     /*!< Parameter page signature */
    uint16_t Revision;                      /*!< Revision number */
    uint16_t Feature;                       /*!< Feature support */
    uint16_t OptionalCommands;              /*!< Optional commands support */
    uint8_t Reserved1[22];                  /*!< Reserved */
    uint8_t DeviceManufacturer[12];         /*!< Device manufacturer */
    uint8_t DeviceModel[20];                /*!< Device model */
    uint8_t ManufacturerID;                 /*!< Manufacturer ID */
    uint16_t DateCode;                      /*!< Date code */
    uint8_t Reserved2[12];                  /*!< Reserved */
    uint32_t PageSize;                      /*!< Number of data bytes per page */
    uint16_t SpareSize;                     /*!< Number of spare bytes per page */
    uint16_t PartialPageSize;               /*!< Number of data bytes per partial page */
    uint8_t Reserved3[2];                   /*!< Reserved */
    uint16_t PartialSpareSize;              /*!< Number of spare bytes per partial page */
    uint32_t BlockSize;                   /*!< Number of pages per block */
    uint32_t UnitSize;                    /*!< Number of blocks per unit */
    uint8_t LogicalUnit;                    /*!< Number of logical units */
    uint8_t AddressCycles;                  /*!< Number of address cycles */
    uint8_t BitsPerCell;                    /*!< Number of bits per cell */
    uint8_t BadBlocksMaximumPerUnit;        /*!< Bad blocks maximum per unit */
    uint8_t Reserved4;                      /*!< Reserved */
    uint16_t BlockEndurance;              /*!< Block endurance */
    uint8_t GuaranteedBlock;                /*!< Guaranteed valid blocks at beginning of target */
    uint16_t BlockEnduranceGuaranteed;      /*!< Block endurance for guaranteed valid blocks */
    uint8_t ProgramsPerPage;                /*!< Number of programs per page */
    uint8_t PartialProgrammingAttributes;   /*!< Partial programming attributes */
    uint8_t ECCBits;                        /*!< Number of ECC bits */
    uint8_t InterleavedAddressBits;         /*!< Number of interleaved address bits */
    uint8_t InterleavedOperationAttributes; /*!< Interleaved operation attributes */
    uint8_t Reserved5[13];                  /*!< Reserved */
    uint8_t PinCapacitance;                 /*!< I/O pin capacitance */
    uint8_t TimingModeSupport;              /*!< Timing mode support */
    uint16_t ProgramCacheTiming;            /*!< Program cache timing */
    uint8_t Reserved6;                      /*!< Reserved */
    uint8_t tPROG[2];                       /*!< tPROG maximum page program time */
    uint8_t tERS[2];                        /*!< tERS maximum block erase time */
    uint8_t tR[2];                          /*!< tR maximum page read time */
    uint8_t tCCS[2];                        /*!< tCCS minimum */
    uint8_t Reserved7[23];                  /*!< Reserved */
    uint16_t VendorSpecificRevision;        /*!< Vendor-specific revision number */
    uint8_t VendorSpecific[14];             /*!< Vendor specific */
    uint8_t Reserved8[68];                  /*!< Reserved */
    uint8_t ECCCorrectAbility;              /*!< ECC maximum correct ability */
    uint8_t DieSelectFeature;               /*!< Die select feature */
    uint8_t Reserved9[4];                   /*!< Reserved */
    uint16_t IntegrityCRC;                /*!< Integrity CRC */
} MT29F2G_Info_t;

typedef union {
    uint32_t Address;  // 전체 주소 (1바이트 단위 논리 주소)

    struct {
        uint32_t Data		: 12;	// A[11:0]  → Data address (0~2175)
        uint32_t Page		: 6;	// A[17:12] → Page number within block (0~63)
        uint32_t Block		: 11;	// A[28:18] → Block number (0~2047)
        							// A[31:29] → 상위 비트 여유
    };
} MT29F2G_Address_t;

typedef enum {
	MT29F2G_RESULT_FATAL_FAULT				= -14,
	MT29F2G_RESULT_WREN_FAIL 				= -13, // write enable fail
	MT29F2G_RESULT_ECC_ERROR 				= -12,
	MT29F2G_RESULT_ERASE_FAIL				= -11,
	MT29F2G_RESULT_PROGRAM_FAIL			= -10,
	MT29F2G_RESULT_AUTOPOLLING_ERROR 		= -9,
	MT29F2G_RESULT_STATE_FAIL 				= -8,
	MT29F2G_RESULT_RX_ERROR 				= -7,
	MT29F2G_RESULT_TX_ERROR 				= -6,
	MT29F2G_RESULT_COM_ERROR			 	= -5,
	MT29F2G_RESULT_TIMEOUT					= -4,
	MT29F2G_RESULT_BADBLOCK				= -3,
	MT29F2G_RESULT_BUSY					= -2,
	MT29F2G_RESULT_PARAMETER_ERROR			= -1,
	MT29F2G_RESULT_NULL					= 0,
	MT29F2G_RESULT_RUN,
	MT29F2G_RESULT_OK,
} MT29F2G_Result_t;


//MT29F2G01ABAGDWB_IT
typedef struct
{
	QSPI_HandleTypeDef *pHandle;
	//MT29F2G_Info_t Info;

	uint8_t State;

	uint8_t IsOpen;
} MT29F2G_t;

extern oResult_t MT29F2G_Init(MT29F2G_t *pMT29F2G, QSPI_HandleTypeDef *pHandle);
extern oResult_t MT29F2G_DeInit(MT29F2G_t *pMT29F2G);
extern oResult_t MT29F2G_Erase(MT29F2G_t *pMT29F2G, uint32_t NumberOfBlock);
extern oResult_t MT29F2G_BadBlockCheck(MT29F2G_t *pMT29F2G, uint32_t BlockAddress);
extern oResult_t MT29F2G_Reset(MT29F2G_t *pMT29F2G);
extern oResult_t MT29F2G_ReadPage(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint8_t *pData, uint32_t Size);
extern oResult_t MT29F2G_WritePage(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint8_t *pData, uint32_t Size);
extern oResult_t MT29F2G_ReadBlock(MT29F2G_t *pMT29F2G, uint32_t Block, uint8_t *pData, uint32_t Size);
extern oResult_t MT29F2G_WriteBlock(MT29F2G_t *pMT29F2G, uint32_t Block, uint8_t *pData, uint32_t Size);
extern oResult_t MT29F2G_ReadByte(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint32_t ByteIndex, uint8_t *pData, uint32_t Size);
extern oResult_t MT29F2G_WriteByte(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint32_t ByteIndex, uint8_t *pData, uint32_t Size);

#ifdef __cplusplus
}
#endif

#endif
#endif /* NAND_MT29F2G01ABAGDWB_IT_H_ */
