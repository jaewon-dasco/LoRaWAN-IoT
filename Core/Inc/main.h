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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void main_GPIOInit(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADC_TEMP_PLUSE_Pin GPIO_PIN_0
#define ADC_TEMP_PLUSE_GPIO_Port GPIOC
#define ADC_TEMP_MINUS_Pin GPIO_PIN_1
#define ADC_TEMP_MINUS_GPIO_Port GPIOC
#define ADC_3V_BATTERY_Pin GPIO_PIN_2
#define ADC_3V_BATTERY_GPIO_Port GPIOC
#define DO_BAT_CHECK_Pin GPIO_PIN_3
#define DO_BAT_CHECK_GPIO_Port GPIOC
#define PI_VW_Pin GPIO_PIN_5
#define PI_VW_GPIO_Port GPIOA
#define DO_LORA_ENABLE_Pin GPIO_PIN_2
#define DO_LORA_ENABLE_GPIO_Port GPIOB
#define DO_MUX_SEL2_Pin GPIO_PIN_13
#define DO_MUX_SEL2_GPIO_Port GPIOB
#define DO_MUX_B_Pin GPIO_PIN_14
#define DO_MUX_B_GPIO_Port GPIOB
#define DO_MUX_A_Pin GPIO_PIN_15
#define DO_MUX_A_GPIO_Port GPIOB
#define DO_MUX_SEL1_Pin GPIO_PIN_6
#define DO_MUX_SEL1_GPIO_Port GPIOC
#define DO_PW_12V_Pin GPIO_PIN_7
#define DO_PW_12V_GPIO_Port GPIOC
#define DO_MX489_DE_Pin GPIO_PIN_9
#define DO_MX489_DE_GPIO_Port GPIOC
#define PWM_MX489_DI_Pin GPIO_PIN_8
#define PWM_MX489_DI_GPIO_Port GPIOA
#define DO_LED_OPERATING_Pin GPIO_PIN_15
#define DO_LED_OPERATING_GPIO_Port GPIOA
#define DO_NAND_ENABLE_Pin GPIO_PIN_12
#define DO_NAND_ENABLE_GPIO_Port GPIOC
#define DO_VW_RELAY1_Pin GPIO_PIN_2
#define DO_VW_RELAY1_GPIO_Port GPIOD
#define DO_AMPSW_Pin GPIO_PIN_3
#define DO_AMPSW_GPIO_Port GPIOB
#define DO_VW_RELAY2_Pin GPIO_PIN_4
#define DO_VW_RELAY2_GPIO_Port GPIOB
#define DI_USBC_CONNECTED_Pin GPIO_PIN_5
#define DI_USBC_CONNECTED_GPIO_Port GPIOB
#define DI_USBC_CONNECTED_EXTI_IRQn EXTI9_5_IRQn
#define DO_VW_RELAY3_Pin GPIO_PIN_6
#define DO_VW_RELAY3_GPIO_Port GPIOB
#define DO_VW_RELAY4_Pin GPIO_PIN_7
#define DO_VW_RELAY4_GPIO_Port GPIOB
#define DO_VW_RELAY5_Pin GPIO_PIN_8
#define DO_VW_RELAY5_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
