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
#include "stm32f4xx_hal.h"

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
#define MOTORB_IN2_Pin GPIO_PIN_5
#define MOTORB_IN2_GPIO_Port GPIOE
#define MOTORB_IN1_Pin GPIO_PIN_6
#define MOTORB_IN1_GPIO_Port GPIOE
#define IR_R_V0_Pin GPIO_PIN_4
#define IR_R_V0_GPIO_Port GPIOA
#define IR_L_V0_Pin GPIO_PIN_5
#define IR_L_V0_GPIO_Port GPIOA
#define LED3_Pin GPIO_PIN_8
#define LED3_GPIO_Port GPIOE
#define ICM_SCL_Pin GPIO_PIN_10
#define ICM_SCL_GPIO_Port GPIOB
#define ICM_SDA_Pin GPIO_PIN_11
#define ICM_SDA_GPIO_Port GPIOB
#define SERVO_PWM_Pin GPIO_PIN_15
#define SERVO_PWM_GPIO_Port GPIOB
#define OLED_DATA_COMMAND__Pin GPIO_PIN_11
#define OLED_DATA_COMMAND__GPIO_Port GPIOD
#define OLED_RESET__Pin GPIO_PIN_12
#define OLED_RESET__GPIO_Port GPIOD
#define OLED_SDIN_Pin GPIO_PIN_13
#define OLED_SDIN_GPIO_Port GPIOD
#define OLED_SCLK_Pin GPIO_PIN_14
#define OLED_SCLK_GPIO_Port GPIOD
#define Buzzer_Pin GPIO_PIN_8
#define Buzzer_GPIO_Port GPIOA
#define US_DATA_Pin GPIO_PIN_12
#define US_DATA_GPIO_Port GPIOC
#define US_DATA_EXTI_IRQn EXTI15_10_IRQn
#define US_TRIG_Pin GPIO_PIN_2
#define US_TRIG_GPIO_Port GPIOD
#define MOTORA_IN2_Pin GPIO_PIN_8
#define MOTORA_IN2_GPIO_Port GPIOB
#define MOTORA_IN1_Pin GPIO_PIN_9
#define MOTORA_IN1_GPIO_Port GPIOB
#define BTN_USER_Pin GPIO_PIN_0
#define BTN_USER_GPIO_Port GPIOE
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
