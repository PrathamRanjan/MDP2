#ifndef INC_SENSOR_TEST_H_
#define INC_SENSOR_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"   // adjust if using another STM32 family
#include <stdint.h>

// Periodic tester: call this from your main loop or a RTOS task.
// It pings the ultrasonic module, and prints US + IR telemetry via printf→UART.
void sensors_test_tick(void);
uint32_t us_blocking_measure_us(uint32_t wait_rise_ms, uint32_t wait_fall_ms);


#ifdef __cplusplus
}
#endif

#endif /* INC_SENSOR_TEST_H_ */
