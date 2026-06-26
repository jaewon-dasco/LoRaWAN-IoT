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

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define DO_TMEP_ENABLE_Pin GPIO_PIN_13
#define DO_TMEP_ENABLE_GPIO_Port GPIOC
#define I2C3_SCL_TEMP_Pin GPIO_PIN_0
#define I2C3_SCL_TEMP_GPIO_Port GPIOC
#define I2C3_SDA_TEMP_Pin GPIO_PIN_1
#define I2C3_SDA_TEMP_GPIO_Port GPIOC
#define ADC_3V_BATTERY_Pin GPIO_PIN_2
#define ADC_3V_BATTERY_GPIO_Port GPIOC
#define DO_MEMS_ENABLE_Pin GPIO_PIN_4
#define DO_MEMS_ENABLE_GPIO_Port GPIOA
#define DO_CAN_ENABLE_Pin GPIO_PIN_5
#define DO_CAN_ENABLE_GPIO_Port GPIOA
#define DO_LORA_ENABLE_Pin GPIO_PIN_2
#define DO_LORA_ENABLE_GPIO_Port GPIOB
#define I2C2_SCL_MEMS_Pin GPIO_PIN_13
#define I2C2_SCL_MEMS_GPIO_Port GPIOB
#define I2C2_SDA_MEMS_Pin GPIO_PIN_14
#define I2C2_SDA_MEMS_GPIO_Port GPIOB
#define DI_MEMS_INT1_Pin GPIO_PIN_6
#define DI_MEMS_INT1_GPIO_Port GPIOC
#define DI_MEMS_INT1_EXTI_IRQn EXTI9_5_IRQn
#define DI_MEMS_INT2_Pin GPIO_PIN_7
#define DI_MEMS_INT2_GPIO_Port GPIOC
#define DI_MEMS_INT2_EXTI_IRQn EXTI9_5_IRQn
#define UART1_USB_TX_Pin GPIO_PIN_9
#define UART1_USB_TX_GPIO_Port GPIOA
#define UART1_USB_RX_Pin GPIO_PIN_10
#define UART1_USB_RX_GPIO_Port GPIOA
#define DO_LED_OPERATING_Pin GPIO_PIN_15
#define DO_LED_OPERATING_GPIO_Port GPIOA
#define USART3_TX_LORA_Pin GPIO_PIN_10
#define USART3_TX_LORA_GPIO_Port GPIOC
#define USART3_RX_LORA_Pin GPIO_PIN_11
#define USART3_RX_LORA_GPIO_Port GPIOC
#define DO_NAND_ENABLE_Pin GPIO_PIN_12
#define DO_NAND_ENABLE_GPIO_Port GPIOC
#define DO_BATTERY_CHECK_Pin GPIO_PIN_4
#define DO_BATTERY_CHECK_GPIO_Port GPIOB
#define DI_USBC_CONNECTED_Pin GPIO_PIN_5
#define DI_USBC_CONNECTED_GPIO_Port GPIOB
#define DI_USBC_CONNECTED_EXTI_IRQn EXTI9_5_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
