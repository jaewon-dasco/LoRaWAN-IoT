#ifndef INC_MI_MAIN_H_
#define INC_MI_MAIN_H_

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
		uint8_t Dc12VEnable;

		uint8_t MAX489_DE;
		uint8_t MUX_SEL1;
		uint8_t MUX_SEL2;
		uint8_t MUX_A;
		uint8_t MUX_B;
		uint8_t AmpSW;

		uint8_t VWRelay1;
		uint8_t VWRelay2;
		uint8_t VWRelay3;
		uint8_t VWRelay4;
		uint8_t VWRelay5;

		uint8_t OperatingLED;
	}DO;

	struct{
		uint8_t UsbConnected;
	}DI;

	struct{
		uint16_t SystemSupply;
		uint16_t InternalBAT;
	}ADC;
}GPIOs_t;

extern GPIOs_t GPIOs;

#if defined(IWDG_PRESCALER_4)
extern IWDG_HandleTypeDef hiwdg;
#endif

extern QSPI_HandleTypeDef hqspi;
extern ADC_HandleTypeDef hadc1;

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

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
