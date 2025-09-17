#ifndef INC_SERVO_H_
#define INC_SERVO_H_

#include "main.h"
#include "convert.h"
#include "oled.h"

#define SERVO_WIDTH 26.0f //degrees L/R
#define SERVO_TURN_PERIOD 20.0f //ms before turn is updated
#define SERVO_TURN_STEP 3.0f //degrees to turn

#define SERVO_PWM_CHANNEL TIM_CHANNEL_2

void servo_init(TIM_HandleTypeDef *pwm);
void servo_setVal(uint32_t val);
void servo_setAngle(float angle);
void servo_setCCR(uint32_t us);

#endif /* INC_SERVO_H_ */
