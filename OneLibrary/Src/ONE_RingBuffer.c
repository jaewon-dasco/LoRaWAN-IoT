#include "ONE_Memory.h"
#include "ONE_RingBuffer.h"

uint8_t oRingBuffer_Init(oRingBuffer_t *RingBuffer, uint8_t *Pntr, uint16_t SizeOfBuffer)
{
	if(RingBuffer == NULL || Pntr == 0 || SizeOfBuffer <= 0) return 0;

	RingBuffer->MemoryPtr = Pntr;
	RingBuffer->Length = SizeOfBuffer;
	RingBuffer->Count = 0;
	RingBuffer->FirstIndex = 0;
	RingBuffer->LastIndex = 0;

	return 1;
}

void oRingBuffer_SetPntr(oRingBuffer_t *RingBuffer, uint8_t *pData, uint32_t SizeOfData)
{
	if(RingBuffer->MemoryPtr == 0 || RingBuffer->Length <= 0) return;

	while(SizeOfData > 0)
	{
		oRingBuffer_Set8(RingBuffer, *pData);
		pData++;
		SizeOfData--;
	}
}

void oRingBuffer_Set8(oRingBuffer_t *RingBuffer, uint8_t Data)
{
	if(RingBuffer->MemoryPtr == 0 || RingBuffer->Length <= 0) return;

	uint8_t *Pntr = (uint8_t*)(RingBuffer->MemoryPtr + RingBuffer->FirstIndex);

	*Pntr = Data;

	RingBuffer->Count++;
	RingBuffer->FirstIndex = (RingBuffer->FirstIndex+1) % RingBuffer->Length;

	if(RingBuffer->Count > RingBuffer->Length){
		RingBuffer->Count = RingBuffer->Length;
		RingBuffer->LastIndex = (RingBuffer->LastIndex+1) % RingBuffer->Length;
	}
}

void oRingBuffer_Set16(oRingBuffer_t *RingBuffer, uint16_t Data)
{
	if(RingBuffer->MemoryPtr == 0 || RingBuffer->Length <= 0) return;

	oRingBuffer_Set8(RingBuffer, (uint8_t)(Data & 0xFF));
	oRingBuffer_Set8(RingBuffer, (uint8_t)(Data >> 8 & 0xFF));
}

void oRingBuffer_Set32(oRingBuffer_t *RingBuffer, uint32_t Data)
{
	if(RingBuffer->MemoryPtr == 0 || RingBuffer->Length <= 0) return;

	oRingBuffer_Set8(RingBuffer, (uint8_t)(Data & 0xFF));
	oRingBuffer_Set8(RingBuffer, (uint8_t)(Data >> 8 & 0xFF));
	oRingBuffer_Set8(RingBuffer, (uint8_t)(Data >> 16 & 0xFF));
	oRingBuffer_Set8(RingBuffer, (uint8_t)(Data >> 24 & 0xFF));
}

uint8_t oRingBuffer_GetPntr(oRingBuffer_t *RingBuffer, uint8_t *pData, uint32_t SizeOfData)
{
	if(RingBuffer->MemoryPtr == 0 || RingBuffer->Length <= 0 || RingBuffer->Count <= 0 || pData == 0) return 0;

	while(SizeOfData > 0 && RingBuffer->Count > 0)
	{
		oRingBuffer_Get8(RingBuffer, pData);
		pData++;
		SizeOfData--;
	}

	return 1;
}

uint8_t oRingBuffer_Get8(oRingBuffer_t *RingBuffer, uint8_t *pData)
{
	if(RingBuffer->MemoryPtr == 0 || RingBuffer->Length <= 0 || RingBuffer->Count <= 0 || pData == 0) return 0;

	uint8_t *Pntr = (uint8_t*)(RingBuffer->MemoryPtr + RingBuffer->LastIndex);

	RingBuffer->Count--;
	RingBuffer->LastIndex = (RingBuffer->LastIndex+1) % RingBuffer->Length;

	*pData = *Pntr;

	return 1;
}

uint8_t oRingBuffer_Get16(oRingBuffer_t *RingBuffer, uint16_t *pData)
{
	uint8_t low = 0, high = 0;

	if(RingBuffer->MemoryPtr == 0 || RingBuffer->Length <= 0 || RingBuffer->Count < 2 || pData == 0) return 0;

	if(!oRingBuffer_Get8(RingBuffer, &low)){
		return 0;
	}

	if(!oRingBuffer_Get8(RingBuffer, &high)){
		return 0;
	}

	*pData = (uint16_t)low | ((uint16_t)high << 8);

	return 1;
}

uint8_t oRingBuffer_Get32(oRingBuffer_t *RingBuffer, uint32_t *pData)
{
	uint16_t low = 0, high = 0;

	if(RingBuffer->MemoryPtr == 0 || RingBuffer->Length <= 0 || RingBuffer->Count < 4 || pData == 0) return 0;

	if(!oRingBuffer_Get16(RingBuffer, &low))
	{
		return 0;
	}

	if(!oRingBuffer_Get16(RingBuffer, &high)){
		return 0;
	}

	*pData = (uint32_t)low | ((uint32_t)high << 16);

	return 1;
}

uint8_t *oRingBuffer_GetFirstPntr(oRingBuffer_t *RingBuffer)
{
	if(RingBuffer->MemoryPtr == 0 || RingBuffer->Length <= 0) return 0;

	return (uint8_t*)(RingBuffer->MemoryPtr + RingBuffer->FirstIndex);
}

/* History

2026-06-26 | v0.1
	- baseline (ONE_RingBuffer.c)
*/
