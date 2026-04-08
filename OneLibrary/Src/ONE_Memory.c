#include "ONE_Memory.h"

oResult_t oMEM_IsAllZero(uint8_t* pData, uint32_t Size)
{
	if (pData == NULL || Size == 0) {
		return RESULT_ERROR;
	}

	uint32_t i = 0;

	while(i < Size){
		if(*(pData + i) != 0){
			return RESULT_ERROR;
		}

		i++;
	}

	return RESULT_OK;
}

void oMEM_ChangeByteOrder(uint8_t* pData, uint32_t Size)
{
	if (pData == NULL || Size < 2) {
		return;
	}

	uint32_t i = 0;
	uint8_t Buffer;

	while(i < (Size/2)){
		Buffer = *(pData + i);
		*(pData + i) = *(pData + Size - i - 1);
		*(pData + Size - i - 1) = Buffer;

		i++;
	}
}

uint8_t oMEM_CRC8(uint8_t *block, uint32_t blockLength, uint8_t crc)
{
	if (block == NULL || blockLength == 0) {
		return crc;
	}

	uint32_t i;
	uint8_t tmp;

    for(i=0U; i<blockLength; i++)
    {
    	tmp = crc ^ *block;
    	crc = CRC8_TABLE[tmp];

    	block++;
    }
    return crc;
}

uint16_t oMEM_CRC16(uint8_t *block, uint32_t blockLength, uint16_t crc)
{
	if (block == NULL || blockLength == 0) {
		return crc;
	}

	uint32_t i;
	uint16_t tmp;

    for(i=0U; i<blockLength; i++)
    {
        tmp = (crc >> 8) ^ (uint16_t)(*block);
        crc = ((uint16_t)(crc << 8)) ^ CRC16_TABLE[tmp];

        block++;
    }

    return crc;
}

uint32_t oMEM_CRC32(uint8_t *block, uint32_t blockLength, uint32_t crc)
{
	if (block == NULL || blockLength == 0) {
		return crc;
	}

	uint32_t i;
	uint32_t tmp;

    for(i=0; i<blockLength; i++)
    {
        tmp = (crc >> 24) ^ (uint32_t)(*block);
        crc = ((uint32_t)(crc << 8)) ^ CRC32_TABLE[tmp];

        block++;
    }
    return crc;
}
