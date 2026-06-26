#ifndef INC_MI_MAIN_H_
#define INC_MI_MAIN_H_

#define MI_MAIN_VERSION		0.1

#include "ONE_Math.h"
#include "ONE_Signal.h"
#include "ONE_Time.h"
#include "ONE_Serial.h"
#include "main.h"

//#define DEBUGMODE
#define SLEEPMODE_ENABLE	1
#define DAS_SERIALNUMBER(year,week,number)	((MATH_MAX((uint32_t)(year),2022)-2000) * 1000000) + ((MATH_MIN((uint32_t)(week),52)) * 10000) + (uint32_t)(MATH_MIN(number,9999))

#pragma pack(1)
#pragma pack()


typedef struct{
	struct{
		uint8_t LoRaEnable;
		uint8_t BatteryCheck;
		uint8_t CANEnable;
		uint8_t NANDEnable;
		uint8_t MEMSEnable;
		uint8_t TEMPEnable;
		uint8_t OperatingLED;
	}DO;

	struct{
		uint8_t UsbConnected;
		uint8_t MemsReady;
		uint8_t MemsEvent1;
		uint8_t MemsEvent2;
	}DI;

	union{
		struct{
			uint16_t SystemSupply;
			uint16_t InternalBAT;
		};

		uint16_t Buffer[2];
	}ADC;
}GPIOs_t;

extern GPIOs_t GPIOs;

#if defined(IWDG_PRESCALER_4)
extern IWDG_HandleTypeDef hiwdg;
#endif

extern ADC_HandleTypeDef hadc1;
extern CAN_HandleTypeDef hcan1;
extern QSPI_HandleTypeDef hqspi;

extern I2C_HandleTypeDef hi2c2;
extern I2C_HandleTypeDef hi2c3;

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_lpuart_rx;
extern DMA_HandleTypeDef hdma_lpuart_tx;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart3_tx;

extern void MiMain(void);

#endif /* INC_MI_MAIN_H_ */

/* History

2026-06-26 | v0.1
	- baseline (Mi_Main.h)
*/
