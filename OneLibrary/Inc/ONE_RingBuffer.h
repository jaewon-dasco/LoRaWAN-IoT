#ifndef ONE_RINGBUFFER_H
#define ONE_RINGBUFFER_H

#define ONE_RINGBUFFER_VERSION		0.1

#ifdef __cplusplus
extern "C" {
#endif

/*Includes ------------------------------------------------------------------*/
#include "ONE_Common.h"

//Ring Buffer
#define RINGBUFFER_COUNT(older, newer, length)	(newer > older ? (newer-older) : (length-older+newer))
#define RINGBUFFER_INCREASE(index, length)		(length > 0 ? ((index+1) % length) : 0)
#define RINGBUFFER_DECREASE(index, length)		(index > 0 ? (index-1) : max(length,1)-1)

typedef struct MEM_RingBufferTypDef{
  uint8_t *MemoryPtr;
  uint16_t Length;
  uint16_t Count;

  uint16_t FirstIndex;
  uint16_t LastIndex;
} oRingBuffer_t;

extern uint8_t oRingBuffer_Init(oRingBuffer_t *RingBuffer, uint8_t *Pntr, uint16_t SizeOfBuffer);

void oRingBuffer_SetPntr(oRingBuffer_t *RingBuffer, uint8_t *pData, uint32_t SizeOfData);
void oRingBuffer_Set8(oRingBuffer_t *RingBuffer, uint8_t Data);
void oRingBuffer_Set16(oRingBuffer_t *RingBuffer, uint16_t Data);
void oRingBuffer_Set32(oRingBuffer_t *RingBuffer, uint32_t Data);
extern uint8_t oRingBuffer_GetPntr(oRingBuffer_t *RingBuffer, uint8_t *pData, uint32_t SizeOfData);
extern uint8_t oRingBuffer_Get8(oRingBuffer_t *RingBuffer, uint8_t *pData);
extern uint8_t oRingBuffer_Get16(oRingBuffer_t *RingBuffer, uint16_t *pData);
extern uint8_t oRingBuffer_Get32(oRingBuffer_t *RingBuffer, uint32_t *pData);
extern uint8_t *oRingBuffer_GetFirstPntr(oRingBuffer_t *RingBuffer);
#endif

/* History

2026-06-26 | v0.1
	- baseline (ONE_RingBuffer.h)
*/
