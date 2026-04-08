#ifndef INC_MI_MAIN_H_
#define INC_MI_MAIN_H_

#include "main.h"
#include "ONE_Math.h"
#include "ONE_Signal.h"
#include "ONE_Serial.h"
#include "ONE_Time.h"

//#define DEBUGMODE

typedef struct{
	struct{
		uint8_t VwDe;
		uint8_t Vo5vEnable;
		uint8_t Vo12vEnable;
		uint8_t BatCheck;
		uint8_t Dc5vEnable;
		uint8_t Rs485Enable;
		uint8_t Rs232Enable;
		uint8_t LoRaEnable;
		uint8_t NANDEnable;
		uint8_t VwEnable;
		uint8_t AnalogPCurrentEnable;
		uint8_t AnalogMCurrentEnable;
		uint8_t AnalogAmpEnable;
		uint8_t VwModeEnable;
		uint8_t TempModeEnable;
		uint8_t AnalogModeEnable;

		uint8_t LedPower;
		uint8_t LedCom;

		uint8_t Ch1Enable;
		uint8_t Ch2Enable;

		union{
			struct{
				uint8_t Port1Enable;
				uint8_t Port2Enable;
				uint8_t Port3Enable;
				uint8_t Port4Enable;
				uint8_t Port5Enable;
				uint8_t Port6Enable;
				uint8_t Port7Enable;
				uint8_t Port8Enable;
				uint8_t Port9Enable;
				uint8_t Port10Enable;
				uint8_t Port11Enable;
				uint8_t Port12Enable;
				uint8_t Port13Enable;
				uint8_t Port14Enable;
				uint8_t Port15Enable;
				uint8_t Port16Enable;
			};

			uint8_t PortArray[16];
		};
	}DO;

	struct{
		uint8_t UsbConnected;
	}DI;

	struct{
		uint16_t ExternalSupply;
		uint16_t SystemSupply;
		uint16_t InternalBAT;
	}ADC;
}GPIOs_t;

extern GPIOs_t GPIOs;

#if defined(IWDG_PRESCALER_4)
extern IWDG_HandleTypeDef hiwdg;
#endif

extern SPI_HandleTypeDef hspi1;
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
