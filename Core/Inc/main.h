/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define DO_5V_ENABLE_Pin GPIO_PIN_13
#define DO_5V_ENABLE_GPIO_Port GPIOC
#define ADC_TEMP_PLUSE_Pin GPIO_PIN_0
#define ADC_TEMP_PLUSE_GPIO_Port GPIOC
#define ADC_TEMP_MINUS_Pin GPIO_PIN_1
#define ADC_TEMP_MINUS_GPIO_Port GPIOC
#define DO_AI_0548M_MA_ENABLE_Pin GPIO_PIN_2
#define DO_AI_0548M_MA_ENABLE_GPIO_Port GPIOC
#define ADC_AI_0545P_Pin GPIO_PIN_3
#define ADC_AI_0545P_GPIO_Port GPIOC
#define ADC_AI_0545M_Pin GPIO_PIN_0
#define ADC_AI_0545M_GPIO_Port GPIOA
#define SPI_SCK_IOEXT_Pin GPIO_PIN_1
#define SPI_SCK_IOEXT_GPIO_Port GPIOA
#define UART2_TX_Pin GPIO_PIN_2
#define UART2_TX_GPIO_Port GPIOA
#define DO_RS232_ENABLE_Pin GPIO_PIN_4
#define DO_RS232_ENABLE_GPIO_Port GPIOA
#define PI_VW_Pin GPIO_PIN_5
#define PI_VW_GPIO_Port GPIOA
#define DO_VW_ENABLE_Pin GPIO_PIN_2
#define DO_VW_ENABLE_GPIO_Port GPIOB
#define DI_USB_CONNECTED_Pin GPIO_PIN_12
#define DI_USB_CONNECTED_GPIO_Port GPIOB
#define DI_USB_CONNECTED_EXTI_IRQn EXTI15_10_IRQn
#define DO_LORA_ENABLE_Pin GPIO_PIN_13
#define DO_LORA_ENABLE_GPIO_Port GPIOB
#define DO_AI_0548P_MA_ENABLE_Pin GPIO_PIN_15
#define DO_AI_0548P_MA_ENABLE_GPIO_Port GPIOB
#define DO_IOEXT_RESET_Pin GPIO_PIN_6
#define DO_IOEXT_RESET_GPIO_Port GPIOC
#define DO_IOEXT_CS_Pin GPIO_PIN_7
#define DO_IOEXT_CS_GPIO_Port GPIOC
#define DO_NAND_ENALBE_Pin GPIO_PIN_8
#define DO_NAND_ENALBE_GPIO_Port GPIOC
#define DO_VW_DE_Pin GPIO_PIN_9
#define DO_VW_DE_GPIO_Port GPIOC
#define PWM_VW_Pin GPIO_PIN_8
#define PWM_VW_GPIO_Port GPIOA
#define SPI_MISO_IOEXT_Pin GPIO_PIN_11
#define SPI_MISO_IOEXT_GPIO_Port GPIOA
#define SPI_MOSI_IOEXT_Pin GPIO_PIN_12
#define SPI_MOSI_IOEXT_GPIO_Port GPIOA
#define DO_RS485_ENABLE_Pin GPIO_PIN_15
#define DO_RS485_ENABLE_GPIO_Port GPIOA
#define DO_AI_AMP_EANBLE_Pin GPIO_PIN_10
#define DO_AI_AMP_EANBLE_GPIO_Port GPIOC
#define DO_CH1_ENABLE_Pin GPIO_PIN_11
#define DO_CH1_ENABLE_GPIO_Port GPIOC
#define DO_CH2_ENABLE_Pin GPIO_PIN_12
#define DO_CH2_ENABLE_GPIO_Port GPIOC
#define DO_VW_MODE_ENABLE_Pin GPIO_PIN_2
#define DO_VW_MODE_ENABLE_GPIO_Port GPIOD
#define DO_TEMP_MODE_ENABLE_Pin GPIO_PIN_3
#define DO_TEMP_MODE_ENABLE_GPIO_Port GPIOB
#define DO_AI_MODE_ENABLE_Pin GPIO_PIN_4
#define DO_AI_MODE_ENABLE_GPIO_Port GPIOB
#define DO_VO5V_ENABLE_Pin GPIO_PIN_5
#define DO_VO5V_ENABLE_GPIO_Port GPIOB
#define DO_VO12V_ENABLE_Pin GPIO_PIN_6
#define DO_VO12V_ENABLE_GPIO_Port GPIOB
#define DO_BAT_CHECK_Pin GPIO_PIN_7
#define DO_BAT_CHECK_GPIO_Port GPIOB
#define DO_LED_OPERATING_Pin GPIO_PIN_8
#define DO_LED_OPERATING_GPIO_Port GPIOB
#define DO_LED_COM_Pin GPIO_PIN_9
#define DO_LED_COM_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
void main_GPIOInit();
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
