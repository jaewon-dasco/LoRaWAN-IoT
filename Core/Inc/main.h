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
#define ADC_3V_BATTERY_Pin GPIO_PIN_2
#define ADC_3V_BATTERY_GPIO_Port GPIOC
#define ADC_THERMISTOR_CH1_Pin GPIO_PIN_3
#define ADC_THERMISTOR_CH1_GPIO_Port GPIOC
#define ADC_THERMISTOR_CH2_Pin GPIO_PIN_0
#define ADC_THERMISTOR_CH2_GPIO_Port GPIOA
#define ADC_THERMISTOR_CH3_Pin GPIO_PIN_1
#define ADC_THERMISTOR_CH3_GPIO_Port GPIOA
#define DO_12V_ENABLE_Pin GPIO_PIN_5
#define DO_12V_ENABLE_GPIO_Port GPIOA
#define DO_MA_MODE_ENABLE_Pin GPIO_PIN_4
#define DO_MA_MODE_ENABLE_GPIO_Port GPIOC
#define DO_LORA_ENABLE_Pin GPIO_PIN_2
#define DO_LORA_ENABLE_GPIO_Port GPIOB
#define DO_uV_ENABLE_Pin GPIO_PIN_12
#define DO_uV_ENABLE_GPIO_Port GPIOB
#define DO_PWSUPPLY_CH3_Pin GPIO_PIN_15
#define DO_PWSUPPLY_CH3_GPIO_Port GPIOB
#define DO_PWSUPPLY_CH2_Pin GPIO_PIN_6
#define DO_PWSUPPLY_CH2_GPIO_Port GPIOC
#define DO_PWSUPPLY_CH1_Pin GPIO_PIN_7
#define DO_PWSUPPLY_CH1_GPIO_Port GPIOC
#define DO_uV_A0_Pin GPIO_PIN_8
#define DO_uV_A0_GPIO_Port GPIOC
#define DO_uV_A1_Pin GPIO_PIN_9
#define DO_uV_A1_GPIO_Port GPIOC
#define DO_uV_A2_Pin GPIO_PIN_8
#define DO_uV_A2_GPIO_Port GPIOA
#define UART1_USB_TX_Pin GPIO_PIN_9
#define UART1_USB_TX_GPIO_Port GPIOA
#define UART1_USB_RX_Pin GPIO_PIN_10
#define UART1_USB_RX_GPIO_Port GPIOA
#define DO_AMP_ENABLE_Pin GPIO_PIN_12
#define DO_AMP_ENABLE_GPIO_Port GPIOA
#define DO_LED_OPERATING_Pin GPIO_PIN_15
#define DO_LED_OPERATING_GPIO_Port GPIOA
#define UART3_LORA_TX_Pin GPIO_PIN_10
#define UART3_LORA_TX_GPIO_Port GPIOC
#define UART3_LORA_RX_Pin GPIO_PIN_11
#define UART3_LORA_RX_GPIO_Port GPIOC
#define DO_NAND_ENABLE_Pin GPIO_PIN_12
#define DO_NAND_ENABLE_GPIO_Port GPIOC
#define DO_ADC_REF_ENABLE_Pin GPIO_PIN_2
#define DO_ADC_REF_ENABLE_GPIO_Port GPIOD
#define DI_USBC_CONNECTED_Pin GPIO_PIN_5
#define DI_USBC_CONNECTED_GPIO_Port GPIOB
#define DI_USBC_CONNECTED_EXTI_IRQn EXTI9_5_IRQn
#define DO_mV_ENABLE_Pin GPIO_PIN_6
#define DO_mV_ENABLE_GPIO_Port GPIOB
#define DO_mV_A0_Pin GPIO_PIN_7
#define DO_mV_A0_GPIO_Port GPIOB
#define DO_mV_A1_Pin GPIO_PIN_8
#define DO_mV_A1_GPIO_Port GPIOB
#define DO_mV_A2_Pin GPIO_PIN_9
#define DO_mV_A2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
