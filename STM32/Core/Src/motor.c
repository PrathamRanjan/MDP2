#include "motor.h"
#include "math.h"
#include "stdarg.h"

// ===== External TIM handles from CubeMX
extern TIM_HandleTypeDef htim9;  // PE6:CH1 (B_IN1), PE5:CH2 (B_IN2)
extern TIM_HandleTypeDef htim10; // PB8:CH1 (A_IN2)
extern TIM_HandleTypeDef htim11; // PB9:CH1 (A_IN1)
extern UART_HandleTypeDef huart3;

// Encoders (passed in)
static TIM_HandleTypeDef *l_enc_tim, *r_enc_tim;

// Odom / control state
static int32_t totalCountsAvg = 0;

// Control state
static int16_t pwmValAccel = 0, pwmValTarget = 0, lPwmVal = 0, rPwmVal = 0;
static int16_t lLastCount = 0, rLastCount = 0;
static int8_t curDir = 0; // +1 fwd, -1 rev, 0 brake

// PIDs
static PidDef pidMatch;
static PidDef pidDistTarget;
static PidDef pidDistAway;

const static float Kp_match = 5e4, Ki_match = 7e2, Kd_match = 3e3;
const static float Kp_distTarget = 1.4f, Ki_distTarget = 0.0011f, Kd_distTarget = 0.12f;
const static float Kp_distAway = 1.55f, Ki_distAway = 7e-5f, Kd_distAway = 0.25f;

// ===== helpers

static void timer_reset(TIM_HandleTypeDef *htim) { __HAL_TIM_SET_COUNTER(htim, 0); }

static void dbg_printf(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0)
        HAL_UART_Transmit(&huart3, (uint8_t *)buf, (uint16_t)n, 10);
}

static inline uint32_t arr9(void) { return __HAL_TIM_GET_AUTORELOAD(&htim9); }
static inline uint32_t arr10(void) { return __HAL_TIM_GET_AUTORELOAD(&htim10); }
static inline uint32_t arr11(void) { return __HAL_TIM_GET_AUTORELOAD(&htim11); }

static inline uint16_t clamp_to_arr(uint16_t v, uint32_t arr)
{
    return (v > arr) ? (uint16_t)arr : v;
}

static int right_seen_activity = 0;
static inline int use_single_encoder(void) { return (r_enc_tim == NULL); }
static inline void note_right_activity(int16_t prev, int16_t now) {
    if (now != prev) right_seen_activity = 1;
}
static inline int right_is_dead(void) { return !right_seen_activity && !use_single_encoder(); }
/*  H-bridge truth (coast/brake by PWM, not GPIO):

    Forward:  IN1 = 0%,  IN2 = duty
    Reverse:  IN1 = duty, IN2 = 0%
    Brake:    IN1 = 100%, IN2 = 100%  (if your driver implements fast decay on both high)
    Coast:    IN1 = 0%,   IN2 = 0%    (we use this when duty == 0 and dir != 0)

   A (Left):  A_IN1 = TIM11_CH1 (PB9), A_IN2 = TIM10_CH1 (PB8)
   B (Right): B_IN1 = TIM9_CH1  (PE6), B_IN2 = TIM9_CH2  (PE5)
*/

// Motor A (Left)
static void motorA_apply(uint16_t duty, int8_t dir)
{
    uint32_t A11 = arr11(), A10 = arr10();
    duty = clamp_to_arr(duty, (A11 < A10 ? A11 : A10));

    if (dir < 0)
    {                                                        // Forward
        __HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, 0);    // IN1 low
        __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, duty); // IN2 PWM
    }
    else if (dir > 0)
    {                                                        // Reverse
        __HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, duty); // IN1 PWM
        __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, 0);    // IN2 low
    }
    else if (dir == 0)
    {                                                       // Brake (both HIGH)
        __HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, A11); // IN1 = ARR → 100% HIGH
        __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, A10); // IN2 = ARR → 100% HIGH
    }
    else
    {
        __HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, 0);
    }
}

// Motor B (Right)
static void motorB_apply(uint16_t duty, int8_t dir)
{
    uint32_t A9 = arr9();
    duty = clamp_to_arr(duty, A9);

    if (dir < 0)
    {                                                       // Forward
        __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, duty); // IN1 = PWM
        __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_2, 0);    // IN2 = 0
    }
    else if (dir > 0)
    {                                                       // Reverse
        __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, 0);    // IN1 = 0
        __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_2, duty); // IN2 = PWM
    }
    else if (dir == 0)
    {                                                     // Brake
        __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, A9); // IN1 = ARR
        __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_2, A9); // IN2 = ARR
    }
    else
    {
        __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_2, 0);
    }
}

// Change direction/brake, keep current duty magnitudes
void setDriveDir(int8_t dir)
{
    curDir = (dir > 0) ? 1 : (dir < 0) ? -1
                                       : 0;

    uint16_t a_mag = __HAL_TIM_GET_COMPARE(&htim10, TIM_CHANNEL_1); // whichever was active last
    uint16_t b_mag = __HAL_TIM_GET_COMPARE(&htim9, TIM_CHANNEL_2);

    if (a_mag == 0 && b_mag == 0 && curDir != 0)
    {
        // keep coasting at 0% when speed is zero
        motorA_apply(0, +2);
        motorB_apply(0, +2);
    }
    else
    {
        motorA_apply(a_mag, curDir);
        motorB_apply(b_mag, curDir);
    }
}

// ===== public API

void motor_init(TIM_HandleTypeDef *l_enc, TIM_HandleTypeDef *r_enc)
{
    l_enc_tim = l_enc;
    r_enc_tim = r_enc;

    // Encoders
    HAL_TIM_Encoder_Start_IT(l_enc_tim, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(r_enc_tim, TIM_CHANNEL_ALL);

    // Start all PWM outputs
    HAL_TIM_PWM_Start(&htim11, TIM_CHANNEL_1); // A_IN1 (PB9)
    HAL_TIM_PWM_Start(&htim10, TIM_CHANNEL_1); // A_IN2 (PB8)
    HAL_TIM_PWM_Start(&htim9, TIM_CHANNEL_1);  // B_IN1 (PE6)
    HAL_TIM_PWM_Start(&htim9, TIM_CHANNEL_2);  // B_IN2 (PE5)

    // 🚨 Force STOP: set all CCR = 0 (coast)
    __HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_2, 0);

    curDir = 0; // no direction
    pwmValAccel = 0;
    pwmValTarget = 0;
    lPwmVal = rPwmVal = 0;

    // Reset encoder counters
    lLastCount = l_enc_tim ? __HAL_TIM_GET_COUNTER(l_enc_tim) : 0;
    rLastCount = r_enc_tim ? __HAL_TIM_GET_COUNTER(r_enc_tim) : 0;
    totalCountsAvg = 0;

    right_seen_activity = 0; // reset the runtime detector

    // Init PIDs
    pid_init(&pidMatch, Kp_match, Ki_match, Kd_match);
    pid_init(&pidDistTarget, Kp_distTarget, Ki_distTarget, Kd_distTarget);
    pid_init(&pidDistAway, Kp_distAway, Ki_distAway, Kd_distAway);
}

void brakeMotors(void)
{
    uint32_t A11 = arr11(), A10 = arr10(), A9 = arr9();

    // --- 1) Apply active brake for a short moment ---
    __HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, A11); // A_IN1 = 100%
    __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, A10); // A_IN2 = 100%
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, A9);   // B_IN1 = 100%
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_2, A9);   // B_IN2 = 100%

    // small delay (few ms) – enough to dissipate inertia
    HAL_Delay(1000);

    // --- 2) Release into coast (all low) ---
    __HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_2, 0);

    // clear state
    curDir    = 0;
    pwmValAccel = 0;
    lPwmVal   = 0;
    rPwmVal   = 0;
}

static int16_t getCount(TIM_HandleTypeDef *enc_tim)
{
    return (int16_t)__HAL_TIM_GET_COUNTER(enc_tim);
}

float motor_getDist(void)
{
    // Always read left (it’s our trusted encoder)
    int16_t lNow = l_enc_tim ? getCount(l_enc_tim) : 0;

    // Right may be absent or dead
    int16_t rNow = r_enc_tim ? getCount(r_enc_tim) : rLastCount;

    int16_t lDelta16 = -(int16_t)(lNow - lLastCount);
    int16_t rDelta16 = (int16_t)(rNow - rLastCount);

    // Update last samples
    lLastCount = lNow;
    if (r_enc_tim) rLastCount = rNow;

    // Apply your sign convention (left inverted)
    int32_t lDelta = (int32_t)lDelta16;
    int32_t rDelta = (int32_t)rDelta16;

    // Runtime note if right is showing life
    if (r_enc_tim) note_right_activity(rDelta16, 0); // any non-zero delta flips the flag

    int32_t inc;
    if (use_single_encoder() || right_is_dead()) {
        // SINGLE ENCODER MODE: trust left only
        // Using left delta alone equals the average when both sides are equal,
        // so no scaling is required.
        inc = lDelta;
    } else {
        // Dual encoders available: use mean as before
        inc = (lDelta + rDelta) / 2;
    }

    totalCountsAvg += inc;
    return get_distance_cm(totalCountsAvg);
}


static float motor_getSpeed(TIM_HandleTypeDef *enc_tim, int16_t *lastCount)
{
    int16_t count = getCount(enc_tim);
    int16_t diff = abs_int16(count - *lastCount);
    *lastCount = count;
    return (float)diff;
}

static int16_t getSpeedPwm(uint8_t speed)
{
    if (speed == 0)
        return 0;
    int32_t val = ((int32_t)MOTOR_PWM_MAX * (int32_t)speed) / 100;
    if (val > 0 && val < MOTOR_PWM_MIN)
        val = MOTOR_PWM_MIN;
    return (int16_t)val;
}

static void setPwmLR(void)
{
    int16_t lCmd = (lPwmVal == 0) ? 0 : clamp_s16(lPwmVal, MOTOR_PWM_MIN, MOTOR_PWM_MAX);
    int16_t rCmd = (rPwmVal == 0) ? 0 : clamp_s16(rPwmVal, MOTOR_PWM_MIN, MOTOR_PWM_MAX);

    lCmd = apply_deadzone(lCmd);
    rCmd = apply_deadzone(rCmd);

    // If both commands are zero, actively brake to stop immediately
    if (lCmd == 0 && rCmd == 0)
    {
        brakeMotors();
        return;
    }

    // Otherwise, keep default behavior on each side
    // Note: if a single side reaches 0 while moving, we still coast that side
    if (lCmd == 0 && curDir != 0)
        motorA_apply(0, +2);
    else
        motorA_apply((uint16_t)lCmd, curDir);
    if (rCmd == 0 && curDir != 0)
        motorB_apply(0, +2);
    else
        motorB_apply((uint16_t)rCmd, curDir);
}

void motor_pwmCorrection(float wDiff, float rBack, float distDiff, float brakingDist,
                         CmdDistType distType, uint8_t speedNext)
{
    // 1 Hz telemetry
//    static uint32_t tickCount = 0, lastPrint = 0;
//    tickCount++;
//    uint32_t now = HAL_GetTick();
//    if (now - lastPrint >= 1000)
//    {
//        dbg_printf("corrHz=%lu/s\r\n", tickCount);
//        tickCount = 0;
//        lastPrint = now;
//        debug_pwm_values(); // prints ARR/CCR for TIM9/10/11 + dir
//    }


    // ----- 1) Instant target (no accel/decel ramp) -----
//    {
//        int16_t desired = (distDiff < brakingDist && brakingDist > 0.0f)
//                              ? getSpeedPwm(speedNext)
//                              : pwmValTarget;
//        if (desired < 0) desired = 0;
//        if (desired > MOTOR_PWM_MAX) desired = MOTOR_PWM_MAX;
//        pwmValAccel = desired;
//    }

//int16_t desired = (brakingDist > 0.0f && distDiff <= brakingDist) ? 0 : pwmValTarget;

    int16_t desired = pwmValTarget;

    if (desired < 0) desired = 0;
    if (desired > MOTOR_PWM_MAX) desired = MOTOR_PWM_MAX;
    pwmValAccel = desired; // no ramp/taper here

    // ----- 2) Direction handling (keep PWM magnitudes positive) -----
    if (pwmValAccel == 0)
    {
        setDriveDir(0);
        lPwmVal = rPwmVal = 0;
        setPwmLR(); // both zero → active brake
        return;
    }

    if (pwmValAccel < 0)
    {
        setDriveDir(-curDir);
        wDiff = -wDiff;
        pwmValAccel = -pwmValAccel;
    }
    else
    {
        setDriveDir(curDir);
    }

    // ----- 3) Curvature scaling (guard rBack ~ 0) -----
    float lScale = 1.0f, rScale = 1.0f;
    if (fabsf(rBack) > EPS_F)
    {
        float B2 = WHEELBASE_CM_BACK / 2.0f;
        if (rBack < 0.0f)
        {
            lScale = (-rBack - B2) / -rBack;
            rScale = (-rBack + B2) / -rBack;
        }
        else
        {
            lScale = (rBack + B2) / rBack;
            rScale = (rBack - B2) / rBack;
        }
        if (!isfinite(lScale))
            lScale = 1.0f;
        if (!isfinite(rScale))
            rScale = 1.0f;
        if (lScale < 0.0f)
            lScale = 0.0f;
        if (rScale < 0.0f)
            rScale = 0.0f;
    }

    // ----- 4) Steering offset bounded by %-of-target and headroom -----
    float base = (float)pwmValAccel;
    float offset = 0.0f;

    float dt = MS_FRAME * 0.001f;         // if correction runs every frame
    float u  = pid_adjust(&pidMatch, wDiff, dt);
    offset   = (pwmValTarget > 0) ? (u * base / (float)pwmValTarget) : 0.0f;

//    if (pwmValTarget > 0)
//    {
//        // pid_match output scaled by current accel level, normalized by target
//        offset = pid_adjust(&pidMatch, wDiff, 1.0f) * base / (float)pwmValTarget;
//    }

    // cap by fraction of current command
    float offset_abs = fabsf(offset);
    float cap_frac = MOTOR_OFFSET_FRAC * base;
    if (offset_abs > cap_frac)
        offset = (offset < 0.0f) ? -cap_frac : cap_frac;

    // cap by headroom after curvature scaling
    float lBase = base * lScale;
    float rBase = base * rScale;

    float headL = (float)MOTOR_PWM_MAX - lBase;
    float headR = (float)MOTOR_PWM_MAX - rBase;
    float head = fminf(fabsf(headL), fabsf(headR));
    if (head < 0.0f)
        head = 0.0f;
    if (fabsf(offset) > head)
        offset = (offset < 0.0f) ? -head : head;

    // ----- 5) Build raw L/R, normalize if needed, apply dead-zone -----
    float lRaw = lBase - offset;
    float rRaw = rBase + offset;

    float maxAbs = fmaxf(fabsf(lRaw), fabsf(rRaw));
    if (maxAbs > (float)MOTOR_PWM_MAX)
    {
        float scale = (float)MOTOR_PWM_MAX / maxAbs;
        lRaw *= scale;
        rRaw *= scale;
    }

    lPwmVal = (int16_t)((lRaw < (float)MOTOR_PWM_MIN * 0.5f) ? 0 : lRaw);
    rPwmVal = (int16_t)((rRaw < (float)MOTOR_PWM_MIN * 0.5f) ? 0 : rRaw);

    // Final output to TIM11/TIM10/TIM9
    setPwmLR();
}

void motor_setDrive(int8_t dir, uint8_t speed)
{
    dbg_printf("motor_setDrive(dir=%d, speed=%u)\r\n", dir, speed);

    coastMotors();

    if (dir == 0 || speed == 0)
    {
        // Treat as hard brake
        curDir = 0;
        brakeMotors();
        pwmValAccel = 0;
        lPwmVal = rPwmVal = 0;
        return;
    }

    pwmValTarget = getSpeedPwm(speed);
    pwmValAccel  = pwmValTarget;            // instant jump: no accel ramp
    lPwmVal = rPwmVal = pwmValAccel;

    lLastCount = getCount(l_enc_tim);
    rLastCount = getCount(r_enc_tim);
    pid_reset(&pidMatch);
    pid_reset(&pidDistTarget);
    pid_reset(&pidDistAway);

    curDir = (dir > 0) ? 1 : -1;
    setPwmLR();
}

int16_t clamp_s16(int16_t v, int16_t lo, int16_t hi)
{
    return (v < lo) ? lo : (v > hi) ? hi
                                    : v;
}

int16_t apply_deadzone(int16_t v)
{
    if (v == 0)
        return 0;
    if (v > 0 && v < MOTOR_PWM_MIN)
        return MOTOR_PWM_MIN;
    return v;
}

void motor_dist_reset(void)
{
    lLastCount = l_enc_tim ? getCount(l_enc_tim) : 0;
    rLastCount = r_enc_tim ? getCount(r_enc_tim) : 0;
    totalCountsAvg = 0;
    right_seen_activity = 0;
}

static inline void coastMotors(void) {
    __HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim9,  TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim9,  TIM_CHANNEL_2, 0);
}

//===== pwm debug code =====
void debug_pwm_values(void)
{
    char buf[160];
    int len = snprintf(buf, sizeof(buf),
                       "ARR9=%lu ARR10=%lu ARR11=%lu | "
                       "A_IN1(PB9,T11C1)=%lu A_IN2(PB8,T10C1)=%lu | "
                       "B_IN1(PE6,T9C1)=%lu B_IN2(PE5,T9C2)=%lu | dir=%d\r\n",
                       (unsigned long)arr9(), (unsigned long)arr10(), (unsigned long)arr11(),
                       (unsigned long)__HAL_TIM_GET_COMPARE(&htim11, TIM_CHANNEL_1),
                       (unsigned long)__HAL_TIM_GET_COMPARE(&htim10, TIM_CHANNEL_1),
                       (unsigned long)__HAL_TIM_GET_COMPARE(&htim9, TIM_CHANNEL_1),
                       (unsigned long)__HAL_TIM_GET_COMPARE(&htim9, TIM_CHANNEL_2),
                       (int)curDir);
    HAL_UART_Transmit(&huart3, (uint8_t *)buf, len, HAL_MAX_DELAY);
}

// For manual tests: keep direction, change speed
void setSpeedDuty(uint16_t dutyA, uint16_t dutyB)
{
    // If duty==0 and dir!=0 we coast (both inputs 0%)
    if (dutyA == 0 && curDir != 0)
        motorA_apply(0, +2 /*coast*/);
    else
        motorA_apply(dutyA, curDir);

    if (dutyB == 0 && curDir != 0)
        motorB_apply(0, +2 /*coast*/);
    else
        motorB_apply(dutyB, curDir);
}
