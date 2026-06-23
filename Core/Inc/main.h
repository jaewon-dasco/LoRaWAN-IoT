/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void MX_GPIO_Init(void);
void MX_DMA_Init(void);
void MX_CAN1_Init(void);
void MX_LPUART1_UART_Init(void);
void MX_USART1_UART_Init(void);
void MX_USART3_UART_Init(void);
void MX_RTC_Init(void);
void MX_ADC1_Init(void);
void MX_IWDG_Init(void);
void MX_QUADSPI_Init(void);
void MX_I2C2_Init(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LPUART1_USB_RX_Pin GPIO_PIN_0
#define LPUART1_USB_RX_GPIO_Port GPIOC
#define LPUART1_USB_TX_Pin GPIO_PIN_1
#define LPUART1_USB_TX_GPIO_Port GPIOC
#define ADC_3V_BATTERY_Pin GPIO_PIN_2
#define ADC_3V_BATTERY_GPIO_Port GPIOC
#define DO_POWEROUT_5V_ENABLE_Pin GPIO_PIN_4
#define DO_POWEROUT_5V_ENABLE_GPIO_Port GPIOA
#define DO_LED_OPERATING_Pin GPIO_PIN_5
#define DO_LED_OPERATING_GPIO_Port GPIOA
#define UART3_LORA_TX_Pin GPIO_PIN_4
#define UART3_LORA_TX_GPIO_Port GPIOC
#define UART3_LORA_RX_Pin GPIO_PIN_5
#define UART3_LORA_RX_GPIO_Port GPIOC
#define DO_LORA_ENABLE_Pin GPIO_PIN_2
#define DO_LORA_ENABLE_GPIO_Port GPIOB
#define DO_POWEROUT_CH_SELECT_Pin GPIO_PIN_10
#define DO_POWEROUT_CH_SELECT_GPIO_Port GPIOB
#define DO_AMP_ENALBE_Pin GPIO_PIN_12
#define DO_AMP_ENALBE_GPIO_Port GPIOB
#define DO_CURRENT_MODE_ENABLE_Pin GPIO_PIN_15
#define DO_CURRENT_MODE_ENABLE_GPIO_Port GPIOB
#define DO_PW_EXTCOM_Pin GPIO_PIN_8
#define DO_PW_EXTCOM_GPIO_Port GPIOA
#define UART1_EXTCOM_TX_Pin GPIO_PIN_9
#define UART1_EXTCOM_TX_GPIO_Port GPIOA
#define UART1_EXTCOM_RX_Pin GPIO_PIN_10
#define UART1_EXTCOM_RX_GPIO_Port GPIOA
#define DO_CAN_ENABLE_Pin GPIO_PIN_11
#define DO_CAN_ENABLE_GPIO_Port GPIOC
#define DO_NAND_ENABLE_Pin GPIO_PIN_12
#define DO_NAND_ENABLE_GPIO_Port GPIOC
#define DO_ADC_REF_ENABLE_Pin GPIO_PIN_2
#define DO_ADC_REF_ENABLE_GPIO_Port GPIOD
#define UART1_EXTCOM_DE_Pin GPIO_PIN_3
#define UART1_EXTCOM_DE_GPIO_Port GPIOB
#define DO_POWEROUT_ENABLE_0_Pin GPIO_PIN_4
#define DO_POWEROUT_ENABLE_0_GPIO_Port GPIOB
#define DI_USBC_CONNECTED_Pin GPIO_PIN_5
#define DI_USBC_CONNECTED_GPIO_Port GPIOB
#define DI_USBC_CONNECTED_EXTI_IRQn EXTI9_5_IRQn
#define DO_POWEROUT_ENABLE_1_Pin GPIO_PIN_6
#define DO_POWEROUT_ENABLE_1_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
