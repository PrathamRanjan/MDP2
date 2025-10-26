/*
 * ICM20948_SelfTest.h
 *
 *  Created on: Sep 10, 2025
 *      Author: nboey
 */

#ifndef INC_ICM20948_SELFTEST_H_
#define INC_ICM20948_SELFTEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"   // adjust if using another STM32 family
#include <stdint.h>

/**
 * @brief Run ICM20948 self-test
 *
 * This function will:
 *   1) Select USER BANK 0
 *   2) Read WHO_AM_I (expect 0xEA)
 *   3) Reset and wake sensor
 *   4) Enable accel + gyro
 *   5) Collect N=200 samples while stationary
 *   6) Print statistics and pass/fail message over UART
 *
 * @param hi2c   Pointer to initialized I2C handle
 * @param huart  Pointer to initialized UART handle for debug output
 * @param selectI2cAddress  0 = I2C address 0x68 (AD0=0),
 *                          1 = I2C address 0x69 (AD0=1)
 */

typedef struct {
    uint8_t whoami;
    float gyro_bias;
    float accel_bias;
} ICM20948_SelfTestResult;

ICM20948_SelfTestResult ICM20948_SelfTest(I2C_HandleTypeDef *hi2c,
                                          UART_HandleTypeDef *huart,
                                          uint8_t selectI2cAddress);

void ICM20948_DebugPrintAccelGyro(I2C_HandleTypeDef *hi2c, uint8_t addr);

ICM20948_SelfTestResult ICM20948_FindBias(I2C_HandleTypeDef *hi2c,
                                          uint8_t addr,
                                          uint16_t samples);

#ifdef __cplusplus
}
#endif


#endif /* INC_ICM20948_SELFTEST_H_ */
