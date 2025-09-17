// ---- Self-test using your ICM20948 register defines ----
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "ICM20948_SelfTest.h"
#include "ICM20948.h"

// Your low-level I2C helpers must be available:
HAL_StatusTypeDef _ICM20948_SelectUserBank(I2C_HandleTypeDef * hi2c, uint8_t selectI2cAddress, int userBankNum);
HAL_StatusTypeDef _ICM20948_WriteByte      (I2C_HandleTypeDef * hi2c, uint8_t selectI2cAddress, uint8_t reg, uint8_t val);
HAL_StatusTypeDef _ICM20948_ReadByte       (I2C_HandleTypeDef * hi2c, uint8_t selectI2cAddress, uint8_t reg, uint8_t *val);
HAL_StatusTypeDef _ICM20948_BrustRead      (I2C_HandleTypeDef * hi2c, uint8_t selectI2cAddress, uint8_t startReg, uint16_t regsCount, uint8_t *buf);
extern UART_HandleTypeDef huart3;

// Small UART helper
static void _uart_puts(UART_HandleTypeDef *huart, const char *s) {
    HAL_UART_Transmit(huart, (uint8_t*)s, (uint16_t)strlen(s), 100);
}

static inline int16_t be16(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }

// selectI2cAddress: 0 -> 0x68, 1 -> 0x69


ICM20948_SelfTestResult ICM20948_SelfTest(I2C_HandleTypeDef *hi2c,
                                          UART_HandleTypeDef *huart,
                                          uint8_t selectI2cAddress)
{
    ICM20948_SelfTestResult res = {0};

    // WHO_AM_I
    uint8_t whoami = 0;
    _ICM20948_ReadByte(hi2c, selectI2cAddress,
                       ICM20948__USER_BANK_0__WHO_AM_I__REGISTER, &whoami);
    res.whoami = whoami;

    // Do your averaging here …
    res.gyro_bias  = 0.0f;
    res.accel_bias = 0.0f;

    char msg[64];
    snprintf(msg, sizeof msg, "WHO_AM_I=0x%02X\r\n", whoami);
    HAL_UART_Transmit(huart, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    return res;
}

void ICM20948_DebugPrintAccelGyro(I2C_HandleTypeDef *hi2c, uint8_t addr)
{
    uint8_t buf[12];
    char line[128];

    // Read accel (6) + gyro (6) in one burst
    if (_ICM20948_BrustRead(hi2c, addr,
                            ICM20948__USER_BANK_0__ACCEL_XOUT_H__REGISTER,
                            12, buf) != HAL_OK) {
        snprintf(line, sizeof(line), "ICM burst read failed!\r\n");
        HAL_UART_Transmit(&huart3, (uint8_t*)line, strlen(line), HAL_MAX_DELAY);
        return;
    }

    // Combine high/low bytes
    int16_t ax = (buf[0] << 8) | buf[1];
    int16_t ay = (buf[2] << 8) | buf[3];
    int16_t az = (buf[4] << 8) | buf[5];
    int16_t gx = (buf[6] << 8) | buf[7];
    int16_t gy = (buf[8] << 8) | buf[9];
    int16_t gz = (buf[10] << 8) | buf[11];

    // Convert to g (±2g) and dps (±250 dps)
    float ax_g = ax / 16384.0f;
    float ay_g = ay / 16384.0f;
    float az_g = az / 16384.0f;
    float gx_dps = gx / 131.0f;
    float gy_dps = gy / 131.0f;
    float gz_dps = gz / 131.0f;

    // Print to UART
    snprintf(line, sizeof(line),
             "ACC[g]=[%.2f, %.2f, %.2f] | GYRO[dps]=[%.2f, %.2f, %.2f]\r\n",
             ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps);
    HAL_UART_Transmit(&huart3, (uint8_t*)line, strlen(line), HAL_MAX_DELAY);
}

ICM20948_SelfTestResult ICM20948_FindBias(I2C_HandleTypeDef *hi2c, uint8_t addr, uint16_t samples)
{
    ICM20948_SelfTestResult res = {0};

    double gx_sum = 0, gy_sum = 0, gz_sum = 0;
    double ax_sum = 0, ay_sum = 0, az_sum = 0;

    for (uint16_t i = 0; i < samples; i++) {
        uint8_t buf[12];
        if (_ICM20948_BrustRead(hi2c, addr,
                                ICM20948__USER_BANK_0__ACCEL_XOUT_H__REGISTER,
                                12, buf) != HAL_OK) {
            char err[] = "ICM bias burst read failed\r\n";
            HAL_UART_Transmit(&huart3, (uint8_t*)err, strlen(err), HAL_MAX_DELAY);
            return res;
        }

        int16_t ax = (buf[0] << 8) | buf[1];
        int16_t ay = (buf[2] << 8) | buf[3];
        int16_t az = (buf[4] << 8) | buf[5];
        int16_t gx = (buf[6] << 8) | buf[7];
        int16_t gy = (buf[8] << 8) | buf[9];
        int16_t gz = (buf[10] << 8) | buf[11];

        // Convert to physical units
        gx_sum += (float)gx / 131.0f;      // ±250 dps → 131 LSB/dps
        gy_sum += (float)gy / 131.0f;
        gz_sum += (float)gz / 131.0f;

        ax_sum += (float)ax / 16384.0f;    // ±2g → 16384 LSB/g
        ay_sum += (float)ay / 16384.0f;
        az_sum += (float)az / 16384.0f;

        HAL_Delay(2); // ~500 Hz sample rate
    }

    res.gyro_bias  = (float)(gz_sum / samples);   // store only Z bias (for your robot heading)
    res.accel_bias = (float)(az_sum / samples) - 1.0f; // Z should be ~+1g

    // For debug
    char msg[128];
    snprintf(msg, sizeof msg,
             "Bias calc: GyroZ=%.3f dps | AccelZ=%.3f g (bias=%.3f)\r\n",
             res.gyro_bias, (float)(az_sum / samples), res.accel_bias);
    HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    return res;
}
