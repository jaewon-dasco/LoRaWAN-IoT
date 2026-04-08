/*
 * MCP23S17.h
 *
 *  Created on: Mar 12, 2026
 *      Author: JONE
 *
 *  MCP23S17TE 16-bit SPI I/O Expander Driver
 *  - 2x 8-bit ports (PORTA, PORTB)
 *  - SPI interface (max 10MHz)
 *  - Per-pin direction, pull-up, interrupt
 *  - Hardware address (A2,A1,A0) for up to 8 devices
 */

#ifndef INC_MCP23S17_H_
#define INC_MCP23S17_H_

#include "ONE_Common.h"
#include "ONE_Signal.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ─── Register Addresses (IOCON.BANK=0, Sequential) ─── */
#define MCP23S17_REG_IODIRA		0x00
#define MCP23S17_REG_IODIRB		0x01
#define MCP23S17_REG_IPOLA		0x02
#define MCP23S17_REG_IPOLB		0x03
#define MCP23S17_REG_GPINTENA	0x04
#define MCP23S17_REG_GPINTENB	0x05
#define MCP23S17_REG_DEFVALA	0x06
#define MCP23S17_REG_DEFVALB	0x07
#define MCP23S17_REG_INTCONA	0x08
#define MCP23S17_REG_INTCONB	0x09
#define MCP23S17_REG_IOCON		0x0A
#define MCP23S17_REG_GPPUA		0x0C
#define MCP23S17_REG_GPPUB		0x0D
#define MCP23S17_REG_INTFA		0x0E
#define MCP23S17_REG_INTFB		0x0F
#define MCP23S17_REG_INTCAPA	0x10
#define MCP23S17_REG_INTCAPB	0x11
#define MCP23S17_REG_GPIOA		0x12
#define MCP23S17_REG_GPIOB		0x13
#define MCP23S17_REG_OLATA		0x14
#define MCP23S17_REG_OLATB		0x15

/* ─── IOCON Bits ─── */
#define MCP23S17_IOCON_BANK		((uint8_t)0x80)
#define MCP23S17_IOCON_MIRROR	((uint8_t)0x40)
#define MCP23S17_IOCON_SEQOP	((uint8_t)0x20)
#define MCP23S17_IOCON_DISSLW	((uint8_t)0x10)
#define MCP23S17_IOCON_HAEN		((uint8_t)0x08)
#define MCP23S17_IOCON_ODR		((uint8_t)0x04)
#define MCP23S17_IOCON_INTPOL	((uint8_t)0x02)

/* ─── SPI Control Byte ─── */
#define MCP23S17_CTRL_WRITE(addr)	(0x40 | (((addr) & 0x07) << 1))
#define MCP23S17_CTRL_READ(addr)	(0x41 | (((addr) & 0x07) << 1))

/* ─── Timeout ─── */
#define MCP23S17_SPI_TIMEOUT	100U

/* ─── Port Enum ─── */
typedef enum {
	MCP23S17_PORTA = 0,
	MCP23S17_PORTB = 1,
} MCP23S17_Port_t;

/* ─── Pin Direction ─── */
typedef enum {
	MCP23S17_DIR_OUTPUT = 0,
	MCP23S17_DIR_INPUT  = 1,
} MCP23S17_Dir_t;

/* ─── Pin Pull-Up ─── */
typedef enum {
	MCP23S17_PULLUP_DISABLE = 0,
	MCP23S17_PULLUP_ENABLE  = 1,
} MCP23S17_PullUp_t;

/* ─── Parameter Structure (Per-Pin) ─── */
typedef struct {
	/* Direction: MCP23S17_DIR_INPUT(1) or MCP23S17_DIR_OUTPUT(0) */
	uint8_t Dir_GPA0;
	uint8_t Dir_GPA1;
	uint8_t Dir_GPA2;
	uint8_t Dir_GPA3;
	uint8_t Dir_GPA4;
	uint8_t Dir_GPA5;
	uint8_t Dir_GPA6;
	uint8_t Dir_GPA7;
	uint8_t Dir_GPB0;
	uint8_t Dir_GPB1;
	uint8_t Dir_GPB2;
	uint8_t Dir_GPB3;
	uint8_t Dir_GPB4;
	uint8_t Dir_GPB5;
	uint8_t Dir_GPB6;
	uint8_t Dir_GPB7;

	/* Pull-Up: 1=Enable, 0=Disable */
	uint8_t PullUp_GPA0;
	uint8_t PullUp_GPA1;
	uint8_t PullUp_GPA2;
	uint8_t PullUp_GPA3;
	uint8_t PullUp_GPA4;
	uint8_t PullUp_GPA5;
	uint8_t PullUp_GPA6;
	uint8_t PullUp_GPA7;
	uint8_t PullUp_GPB0;
	uint8_t PullUp_GPB1;
	uint8_t PullUp_GPB2;
	uint8_t PullUp_GPB3;
	uint8_t PullUp_GPB4;
	uint8_t PullUp_GPB5;
	uint8_t PullUp_GPB6;
	uint8_t PullUp_GPB7;
} MCP23S17_Param_t;

/* ─── Device Structure ─── */
typedef struct {
	SPI_HandleTypeDef *pSPI;
	oIO_t *pCS;				// CS pin pointer
	oIO_t *pRST;			// Reset pin pointer
	uint8_t HWAddress;		// 0~7 (A2,A1,A0)
	uint8_t IsOpen;
	uint8_t IsFault;

	/* Cached register values (for delta write) */
	uint8_t CacheDirA;
	uint8_t CacheDirB;
	uint8_t CachePullUpA;
	uint8_t CachePullUpB;
	uint8_t CacheOlatA;
	uint8_t CacheOlatB;
} MCP23S17_t;

/* ─── Init / DeInit ─── */
extern oResult_t MCP23S17_Init(MCP23S17_t *pDev, SPI_HandleTypeDef *pSPI, oIO_t *pCS, oIO_t *pRST, uint8_t HWAddr, MCP23S17_Param_t *pParam);
extern oResult_t MCP23S17_DeInit(MCP23S17_t *pDev);
extern oResult_t MCP23S17_Reset(MCP23S17_t *pDev);

/* ─── Register Access ─── */
extern oResult_t MCP23S17_WriteRegister(MCP23S17_t *pDev, uint8_t Reg, uint8_t Data);
extern oResult_t MCP23S17_ReadRegister(MCP23S17_t *pDev, uint8_t Reg, uint8_t *pData);

/* ─── Configuration ─── */
extern oResult_t MCP23S17_SetParameter(MCP23S17_t *pDev, MCP23S17_Param_t *pParam);

/* ─── GPIO Read / Write ─── */
extern oResult_t MCP23S17_ReadPort(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t *pData);
extern oResult_t MCP23S17_WritePort(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t Data);
extern oResult_t MCP23S17_ReadPin(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t Pin, uint8_t *pState);
extern oResult_t MCP23S17_WritePin(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t Pin, uint8_t State);
extern oResult_t MCP23S17_TogglePin(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t Pin);

/* ─── Interrupt ─── */
extern oResult_t MCP23S17_SetInterrupt(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t Mask, uint8_t Mode);
extern oResult_t MCP23S17_GetInterruptFlag(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t *pFlag);
extern oResult_t MCP23S17_GetInterruptCapture(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t *pCapture);

#ifdef __cplusplus
}
#endif

#endif /* INC_MCP23S17_H_ */
