#include "servo.h"

static TIM_HandleTypeDef *pwm_tim;

#define SERVO_LOOKUP_SIZE 51
static float lookup[51][2] = {
	{-25, 3500}, {-24, 3550}, {-23, 3600}, {-22, 3650}, {-21, 3650},
	{-20, 3700}, {-19, 3750}, {-18, 3800}, {-17, 3850}, {-16, 3850},
	{-15, 3900}, {-14, 3950}, {-13, 4000}, {-12, 4050}, {-11, 4050},
	{-10, 4100}, { -9, 4150}, { -8, 4200}, { -7, 4250}, { -6, 4250},
	{ -5, 4300}, { -4, 4350}, { -3, 4400}, { -2, 4450}, { -1, 4450},
	{  0, 4800}, {  1, 4850}, {  2, 4900}, {  3, 4950}, {  4, 4950},
	{  5, 5000}, {  6, 5050}, {  7, 5100}, {  8, 5150}, {  9, 5150},
	{ 10, 5200}, { 11, 5250}, { 12, 5300}, { 13, 5350}, { 14, 5350},
	{ 15, 5400}, { 16, 5450}, { 17, 5500}, { 18, 5550}, { 19, 5550},
	{ 20, 5600}, { 21, 5650}, { 22, 5700}, { 23, 5750}, { 24, 5750},
	{ 25, 6400}
	//{28, 6700}, {29, 6900}
};

void servo_init(TIM_HandleTypeDef *pwm) {
	pwm_tim = pwm;
	HAL_TIM_PWM_Start(pwm, SERVO_PWM_CHANNEL);
}

void servo_setVal(uint32_t val) {
	pwm_tim->Instance->CCR2 = val;
}

void servo_setAngle(float angle) {
    // Clamp requested angle to table limits (±SERVO_WIDTH, but also table bounds)
    if (angle < -SERVO_WIDTH) angle = -SERVO_WIDTH;
    if (angle >  SERVO_WIDTH) angle =  SERVO_WIDTH;

    // If angle outside the lookup domain, clamp to nearest endpoint
    if (angle <= lookup[0][0]) {
        servo_setVal((uint32_t)(lookup[0][1] + 0.5f));
        return;
    }
    if (angle >= lookup[SERVO_LOOKUP_SIZE - 1][0]) {
        servo_setVal((uint32_t)(lookup[SERVO_LOOKUP_SIZE - 1][1] + 0.5f));
        return;
    }

    // Search the segment [i, i+1] that contains `angle`
    for (uint8_t i = 0; i < SERVO_LOOKUP_SIZE - 1; i++) {
        float min_angle = lookup[i][0];
        float max_angle = lookup[i + 1][0];
        if (angle >= min_angle && angle <= max_angle) {
            float min_val = lookup[i][1];
            float max_val = lookup[i + 1][1];
            float t = (angle - min_angle) / (max_angle - min_angle);
            float vf = min_val + t * (max_val - min_val);
            uint32_t val = (uint32_t)(vf + 0.5f); // round to nearest
            servo_setVal(val);
            return;
        }
    }

    // Fallback (shouldn't reach here)
    servo_setVal((uint32_t)(lookup[SERVO_LOOKUP_SIZE - 1][1] + 0.5f));
}
