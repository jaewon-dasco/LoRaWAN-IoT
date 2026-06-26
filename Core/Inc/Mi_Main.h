#ifndef INC_MI_MAIN_H_
#define INC_MI_MAIN_H_

#define MI_MAIN_VERSION		0.1

#include "main.h"
#include "ONE_Math.h"
#include "ONE_Signal.h"
#include "ONE_Serial.h"
#include "ONE_Time.h"

#define SLEEPMODE_ENABLE	1
#define DAS_SERIALNUMBER(year,week,number)	((MATH_MAX((uint32_t)(year),2022)-2000) * 1000000) + ((MATH_MIN((uint32_t)(week),52)) * 10000) + (uint32_t)(MATH_MIN(number,9999))

typedef struct{
	struct{
		uint8_t LoRaEnable;
		uint8_t ADCRefEnable;
		uint8_t NANDEnable;
		uint8_t AMPEnable;
		uint8_t Enable12V;
		uint8_t MAModeEnable;

		uint8_t uVEnable;
		uint8_t uV_A0;
		uint8_t uV_A1;
		uint8_t uV_A2;

		uint8_t mVEnable;
		uint8_t mV_A0;
		uint8_t mV_A1;
		uint8_t mV_A2;

		uint8_t PwrSupplyCh1;
		uint8_t PwrSupplyCh2;
		uint8_t PwrSupplyCh3;

		uint8_t OperatingLED;
	}DO;

	struct{
		uint8_t UsbConnected;
	}DI;

	struct{
		uint16_t SystemSupply;
		uint16_t InternalBAT;
		uint16_t Thermistor[3];
	}ADC;
}GPIOs_t;

extern GPIOs_t GPIOs;

#if defined(IWDG_PRESCALER_4)
extern IWDG_HandleTypeDef hiwdg;
#endif

extern QSPI_HandleTypeDef hqspi;
extern I2C_HandleTypeDef hi2c2;
extern ADC_HandleTypeDef hadc1;

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
