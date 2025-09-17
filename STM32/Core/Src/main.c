/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdlib.h>
#include <math.h>
#include "convert.h"
#include "commands.h"
#include "dist.h"
#include "angle.h"
#include "delay_us.h"
#include "oled.h"
#include "sensors.h"
#include "motor.h"
#include "servo.h"
#include "stdio.h"
#include "string.h"
#include "ICM20948_SelfTest.h"
#include "tim6_pulse_capt.h"
#include "sensor_test.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
//8MHz / 16000 = 2.0ms frame.
#define MS_FRAME 2.0f
#define SERIAL_BUF_SIZE 20
#define SERIAL_RING_SIZE 1000
#define DIST_DIFF_DEFAULT 50
#define BRAKE_SPEED 30
#define TICKS_ES_MAX 10
#define V_MAX_PHYS_CM_S   200.0f

#define A_BRAKE_CM_S2     120.0f   // effective decel when short-braking
#define TAU_LATENCY_S       0.08f  // loop + actuation latency
#define BRAKE_MARGIN_CM      2.0f  // safety buffer

// TEMP fix for odometry scale from your data; refine later in get_distance_cm()
#define ODOM_SCALE           1.00f
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
 ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;
TIM_HandleTypeDef htim9;
TIM_HandleTypeDef htim10;
TIM_HandleTypeDef htim11;
TIM_HandleTypeDef htim12;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
//sensors variables.
Sensors sensors;
MagCalParams magCalParams;

//serial buffers.
volatile uint16_t ring_i = 0, track_i = 0;
uint16_t buf_i = 0;

uint8_t buf_serial[SERIAL_BUF_SIZE];
uint8_t ring_serial[SERIAL_RING_SIZE];

uint8_t rxByte;                    // GLOBAL 1-byte RX buffer
char    cmdBuf[64];
uint16_t cmdLen = 0;

volatile uint32_t rx_isr_hits = 0; // debug counter

//ultrasound pulse width measurement.
volatile uint8_t isRisingCaptured = 0;
volatile uint16_t usWrap = 0;
volatile uint32_t counter;
volatile float    usDist_raw_cm = 0.0f;
volatile uint16_t usStartTick6 = 0;

//straight line motion
static uint8_t speedPrimed = 0;
static uint8_t odomPrimed = 0;
static float   speedFilt   = 0.0f;
static float   speedRaw    = 0.0f;
float estDist     = 0.0f;   // moved out of main()
float estDistOld  = 0.0f;
//paced loop.
volatile uint8_t newTick = 0;
volatile uint32_t tim7_hits = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM12_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM7_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM9_Init(void);
static void MX_TIM10_Init(void);
static void MX_TIM11_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//increment index in a ring.
static void serial_inc_ring(volatile uint16_t *i) {
	*i = (*i + 1) % SERIAL_RING_SIZE;
}

//serial in.
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
    	ring_serial[ring_i] = rxByte;
        ring_i = (ring_i + 1) % SERIAL_RING_SIZE;
        rx_isr_hits++;  // debug: count ISR fires
        HAL_UART_Receive_IT(&huart3, &rxByte, 1); // re-arm
    }
}


/* --- Start: Timer Management --- */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
//	if (htim == &htim5) { //tim4
//		usWrap++;
//	}

	if (htim == &htim7) {
        tim7_hits++;
		newTick = 1;
	}
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    //if (htim != &htim5) return;

    if (!isRisingCaptured) {
        // RISING
        usCaptureComplete = 0;
        usWrap = 0;                              // only if ARR=65535
        __HAL_TIM_SET_COUNTER(htim, 0);
        isRisingCaptured = 1;
        __HAL_TIM_SET_CAPTUREPOLARITY(htim, US_IC_CHANNEL, TIM_INPUTCHANNELPOLARITY_FALLING);
    } else {
        // FALLING
        uint32_t cnt = HAL_TIM_ReadCapturedValue(htim, US_IC_CHANNEL);
        cnt += (uint32_t)usWrap * 65536U;        // only if ARR=65535
        usPulse_us = cnt;                        // 1 tick = 1 µs (PSC=15 on 16 MHz)
        usDist_raw_cm = (float)usPulse_us / 58.0f;
        lastEchoMs = HAL_GetTick();
        usCaptureComplete = 1;

        // keep your filtered value too, but don't use it for stop threshold
        sensors_read_usDist((float)cnt * 1e-6f);

        isRisingCaptured = 0;
        __HAL_TIM_SET_CAPTUREPOLARITY(htim, US_IC_CHANNEL, TIM_INPUTCHANNELPOLARITY_RISING);

        usCaptureComplete = 1;
    }
}

void encoders_zero(void) {
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    __HAL_TIM_SET_COUNTER(&htim3, 0);
}

void odom_reset_all(void) {
	encoders_zero();

	(void)motor_getDist();

    dist_reset(0.0f);

    estDist     = 0.0f;   // now writing the global
    estDistOld  = 0.0f;

    speedPrimed = 0;
    speedFilt   = 0.0f;
    speedRaw    = 0.0f;

    const char *msg = "ODOM_RESET\r\n";
    HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 10);
}

void TIM6_Set1MHz(void)
{
  uint32_t timclk = HAL_RCC_GetPCLK1Freq();
  if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) timclk *= 2U;

  uint32_t psc = (timclk / 1000000U) - 1U;

  __HAL_TIM_DISABLE(&htim6);
  __HAL_TIM_SET_PRESCALER(&htim6, (uint16_t)psc);
  __HAL_TIM_SET_AUTORELOAD(&htim6, 65535);
  __HAL_TIM_SET_COUNTER(&htim6, 0);
  __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
  __HAL_TIM_ENABLE(&htim6);
}

/* --- Start: Debug  --- */
void dump_tim8_regs(void)
{
    char buf[160];
    int n = snprintf(buf, sizeof(buf),
        "ARR=%lu CCR1=%lu CCR3=%lu BDTR=0x%08lX CCER=0x%08lX CR1=0x%08lX CCMR1=0x%08lX CCMR2=0x%08lX\r\n",
        TIM8->ARR, TIM8->CCR1, TIM8->CCR3, TIM8->BDTR, TIM8->CCER, TIM8->CR1, TIM8->CCMR1, TIM8->CCMR2);

    if (n > 0) {
        if (n > (int)sizeof(buf)) n = sizeof(buf);   // clamp just in case
        HAL_UART_Transmit(&huart3, (uint8_t*)buf, (uint16_t)n, 100);
    }
}

void test_gyro_raw(void) {
    float gz = 0;
    read_gyroZ(&gz);   // your low-level driver
    char buf[64];
    snprintf(buf, sizeof(buf), "gyroZ raw=%.2f dps\r\n", gz);
    HAL_UART_Transmit(&huart3, (uint8_t*)buf, strlen(buf), 10);
}

static inline int32_t enc_read_L(void) { return (int32_t)__HAL_TIM_GET_COUNTER(&htim2); }
static inline int32_t enc_read_R(void) { return (int32_t)__HAL_TIM_GET_COUNTER(&htim3); }
static inline void    enc_zero(void)   { __HAL_TIM_SET_COUNTER(&htim2, 0); __HAL_TIM_SET_COUNTER(&htim3, 0); }

static void print_enc_once(void) {
    char buf[96];
    int n = snprintf(buf, sizeof buf, "ENC L=%ld R=%ld\r\n",
                     (long)enc_read_L(), (long)enc_read_R());
    HAL_UART_Transmit(&huart3, (uint8_t*)buf, (uint16_t)n, 20);
}

/* --- End: Debug --- */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART3_UART_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM12_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_I2C2_Init();
  MX_TIM6_Init();
  MX_TIM7_Init();
  MX_TIM3_Init();
  MX_TIM9_Init();
  MX_TIM10_Init();
  MX_TIM11_Init();
  /* USER CODE BEGIN 2 */

  /* ----- Start: Initialize libraries ----- */
  OLED_Init();												//initialize OLED display.
  magcal_init(&hi2c2, &magCalParams);						//initialize magnetometer calibration.
  sensors_init(&hi2c2, &hadc1, &hadc2, &sensors); 	//initialize motion sensors.
  motor_init(&htim2, &htim3); 						//initialize motor PWM and encoders.
  servo_init(&htim12); 										//initialize servo PWM.
  delay_us_init(&htim6);									//initialize us timer.
  dist_init();												//initialize distance tracking.
  us_pulse_capture_init();
  TIM6_Set1MHz();
  /* ----- End: Initialize libraries ----- */

  /* ----- Start: Car setup ----- */
  //magcal_calc_params();

  //reset car.
  servo_setAngle(0);
  motor_setDrive(0, 0);

  OLED_ShowString(0, 0, "Press USER when ready...");
  OLED_Refresh_Gram();
  while (!user_is_pressed());	//wait for user to place car.
  HAL_Delay(500);
  OLED_Clear();
  OLED_ShowString(0, 0, "Calibrating...");
  OLED_Refresh_Gram();

  sensors_set_bias(500); 		// set initial bias.

  OLED_ShowString(0, 0, "Calibration done.");
  OLED_Refresh_Gram();

  /* ----- End: Car setup ----- */

  /* ----- Start: OS Parameters ----- */

  //ticking for longer timing requirements for ultrasound, and servo turning.
  uint8_t ticksElapsed = 0,
		  ticksUsElapsed = 0,
		  ticksUs = (uint8_t) (US_MIN_DELAY / MS_FRAME) + 1,
		  ticksServo = (uint8_t) (SERVO_TURN_PERIOD / MS_FRAME),
		  ticksServoFull = (uint8_t) (SERVO_TURN_PERIOD * SERVO_WIDTH / SERVO_TURN_STEP) / MS_FRAME,
		  ticksMotor = (uint8_t) (MOTOR_CORRECTION_PERIOD / MS_FRAME),
		  ticksRefresh = lcm_uint8(lcm_uint8(ticksUs, ticksServo), ticksMotor),
		  ticksDelay = ticksServoFull - 1;

  //ticking for error stabilization.
  uint8_t ticksES = 0;

  Command *cmd = NULL;							//current command.
  float steeringAngle = 0;						//current steering angle.
  float motorDist = 0,estAngle  = 0; 						//distance estimations.

  //for distance tracking (for use with INFO_DIST command).
  float distTrack = 0;
  volatile uint8_t shouldTrackDist = 0;

  float distTarget = 0;							//decide distance target.
  float distDiff = 0, brakingDist = 0; 			//current distance difference.
  float distDiffOld = 0;						//track old distance difference (to see if stationary).
  float wDiff = 0, wTarget = 0;					//current angular velocity difference and target.
  float rBack = 0, rRobot = 0;					//turning radii at the back and centre of robot.
  /* ----- End: OS Parameters ----- */

  /* ----- Start: Interrupts ----- */
  //HAL_UART_Receive_IT(&huart3, &ring_serial[ring_i], 1);	//start receiving serial.
  HAL_UART_Receive_IT(&huart3, &rxByte, 1);

  // 3) Start the timer *with* interrupt
  HAL_TIM_Base_Start_IT(&htim7); //start paced loop timer.
  // tim5 ic no longer used for ultrasonic (using exti on pc12)
  //HAL_TIM_Base_Start_IT(&htim5);                  // for update/overflow -> usWrap++
  // HAL_TIM_IC_Start_IT(&htim5, TIM_CHANNEL_1);     // for captures on CH1
  /* ----- End: Interrupts ----- */

  OLED_Clear();
  OLED_ShowString(0, 0, "Active.");
  OLED_Refresh_Gram();

  const char *alive = "ALIVE\r\n";
  HAL_UART_Transmit(&huart3, (uint8_t*)alive, strlen(alive), 10);

  //  	if (cmd == NULL && !(ticksElapsed % 50)) {
  //  	    uint32_t age_ms = HAL_GetTick() - lastEchoMs;  // how fresh the last echo is
  //  	    char line[120];
  //  	    int n = snprintf(line, sizeof line,
  //  	        "US | raw=%.1f cm | filt=%.1f cm | age=%lums | pulse=%lu us\r\n",
  //  	        usDist_raw_cm,          // from TIM5 capture ISR
  //  	        sensors.usDist,         // filtered distance
  //  	        (unsigned long)age_ms,
  //  	        (unsigned long)usPulse_us);
  //  	    HAL_UART_Transmit(&huart3, (uint8_t*)line, n, 10);
  //  	}

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

//  static uint32_t last_ms = 0;
//  while (1) {
//	  uint32_t now = HAL_GetTick();
//	  if (now - last_ms >= 1000) {
//	      last_ms = now;
//	      print_enc_once();  // turn either wheel by hand and watch R change
//	  }
//  }

  while (1)
      {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    	/* ----- Start: Sensor reading ----- */
    	//trigger distance measurement.
    	if (!(ticksElapsed % ticksUs)) {
    		sensors_us_trig();
    	}

    	sensors_read_accel();
    	sensors_read_gyroZ();
    	sensors_read_irDist();

    	/* ----- End: Sensor reading ----- */

    	/* ----- Start: Get next command (if any) ----- */

    	// 0) Always drain ticksDelay if we're idle (no active cmd)
    	if (cmd == NULL && ticksDelay > 0) {
    	    ticksDelay--;
    	}

    	if (cmd == NULL) {
    	    // Peek at the ring buffer queue
    	    Command *peek = commands_peek();

    	    // 1) If delay done OR next thing is not a DRIVE, pop
    	    if (ticksDelay == 0 || (peek != NULL && peek->opType != DRIVE)) {

    	        cmd = commands_pop();
    	        if (cmd != NULL) {
    	            // ----- Command setup -----
    	            switch (cmd->opType) {
    	            case DRIVE:
    	                motor_setDrive(cmd->dir, cmd->speed);
    	                if (cmd->dir != 0) {
    	                    odom_reset_all();

    	                    distTarget  = cmd->val;
    	                    distDiff    = DIST_DIFF_DEFAULT;
    	                    brakingDist = (cmd->distType == TARGET
    	                                   ? MOTOR_BRAKING_DIST_CM_TARGET
    	                                   : MOTOR_BRAKING_DIST_CM_AWAY) * cmd->speed / 100.0f;

    	                    steeringAngle = cmd->steeringAngle;
    	                    servo_setAngle(steeringAngle);

    	                    if (steeringAngle != 0) {
    	                        rBack  = get_turning_r_back_cm(steeringAngle);
    	                        rRobot = get_turning_r_robot_cm(steeringAngle);
    	                        if (cmd->distType == TARGET) {
    	                            distTarget = abs_float(get_arc_length(cmd->val, rBack));
    	                        }
    	                    } else {
    	                        rBack = 0.0f; rRobot = 0.0f; wTarget = 0.0f;
    	                    }
    	                } else {
    	                    commands_end(&huart3, cmd);
    	                    cmd = NULL;
    	                }
    	                break;

    	            case INFO_DIST:
    	                shouldTrackDist = !shouldTrackDist;
    	                if (shouldTrackDist) {
    	                    distTrack = 0;
    	                } else {
    	                    free(cmd->str);
    	                    cmd->str_size = 9;
    	                    cmd->str = malloc(cmd->str_size);
    	                    snprintf(cmd->str, cmd->str_size, "D%6.2f\n", distTrack);
    	                }
    	                commands_end(&huart3, cmd);
    	                cmd = NULL;
    	                break;

    	            case INFO_MARKER:
    	                commands_end(&huart3, cmd);
    	                cmd = NULL;
    	                break;
    	            }
    	        }

    	    } else {
    	        // 2) Delay still running: optionally pre-position servo for the next DRIVE,
    	        //    but ALWAYS drain ticksDelay each loop regardless of queue state
    	        Command *nxt = commands_peek_next_drive();
    	        if (nxt != NULL) {
    	            servo_setAngle(nxt->steeringAngle);
    	        }
    	        if (ticksDelay > 0) ticksDelay--;
    	    }
    	}
    	/* ----- End: Get next command (if any) ----- */

    	/* ----- Start: Command Control Loop ----- */
    		if (cmd != NULL && cmd->dir != 0) {
    			//reset the values
    			if (!odomPrimed) {
    			    (void)motor_getDist();   // consume any residual delta
    			    dist_reset(0.0f);
    			    estDist = estDistOld = 0.0f;
    			    speedPrimed = 0;
    			    speedFilt = speedRaw = 0.0f;
    				odomPrimed = 1;
    			}

    			// --- Distance & speed (do NOT multiply position by cmd->dir) ---
    			motorDist = motor_getDist();
    			float pos_cm = ODOM_SCALE*dist_get_cm(MS_FRAME, sensors.accel[1], motorDist); // cumulative since dist_reset
    			estDist      = pos_cm;


    			float dt = MS_FRAME * 0.001f;              // seconds
    			float delta = estDist - estDistOld;         // cm
    			float vInst = speedPrimed ? (delta / dt) : 0.0f;  // first tick = 0
    			if (!speedPrimed) { speedPrimed = 1; vInst = 0.0f; }

    			// Kill impossible spikes from residual jumps (e.g., after command transition)
    			if (fabsf(vInst) > V_MAX_PHYS_CM_S) vInst = 0.0f;

    			// Low-pass filter (tune 0.3 .. 0.6 as you like)
    			speedFilt = 0.4f * vInst + 0.6f * speedFilt;

    			float vabs = fabsf(speedFilt);
    			brakingDist = (vabs*vabs)/(2.0f*A_BRAKE_CM_S2) + vabs*TAU_LATENCY_S + BRAKE_MARGIN_CM;
    			if (brakingDist < 0.0f) brakingDist = 0.0f;

    			float maxBD = 0.25f * fabsf(distDiff);
    			if (brakingDist > maxBD) brakingDist = maxBD;

    			// Telemetry (just while tuning)
//    			if (!(ticksElapsed % 50)) {
//    			    char ln[96];
//    			    int n = snprintf(ln, sizeof ln, "bd=%.2f d=%.2f v=%.2f\r\n", brakingDist, distDiff, speedFilt);
//    			    HAL_UART_Transmit(&huart3, (uint8_t*)ln, n, 1);
//    			}

    			// Keep for telemetry if you want to see both
    			speedRaw   = vInst;     // only for printing
    			estDistOld = estDist;   // move reference

    			// declare estSpeed *locally* and use it below
    			float estSpeed = speedFilt;

    			//calculate difference in angular velocity.
    			float wGyro;
    			wGyro = cmd->dir * sensors.gyroZ;

    			if (rBack != 0) {
    				wTarget = get_w_ms(estSpeed, rBack);
    			} else wTarget = 0;
    			wDiff = (wGyro - wTarget); //gyro is flipped when going backwards.

    			float angleChange = wGyro * MS_FRAME;
    			if (cmd->steeringAngle < 0) angleChange = -angleChange;
    			estAngle += angleChange;

    			distDiffOld = distDiff;
    			switch (cmd->distType) {
    		        case TARGET:
    		              float path_mag = fabsf(estDist);
    		              distDiff = distTarget - path_mag;
    		            break;
    				case STOP_AWAY:
    					if (usCaptureComplete) {
    						distDiff = (sensors.usDist - cmd->val) * cmd->dir;
    					}
    					break;
    				case STOP_L:
    				case STOP_R:
    					uint8_t i = cmd->distType == STOP_L ? 0 : 1;
    					float irVal = sensors.irDist[i];
    					float cmdVal = cmd->val;
    					if (cmdVal < 0) {
    						cmdVal = -cmdVal;
    						distDiff = irVal > cmdVal ? DIST_DIFF_DEFAULT : 0;
    					} else {
    						distDiff = irVal < cmdVal ? DIST_DIFF_DEFAULT : 0;
    					}
    					break;
    				default:
    					distDiff = DIST_DIFF_DEFAULT;
    					break;
    			}

    			//checking for target motion
    			if (!(ticksElapsed % 100)) {
    			    char line[128];
    			    int n = snprintf(line, sizeof line,
    			        "pos=%.2fcm | v=%.2fcm/s (raw=%.2f) | target=%.2f | d=%.2f | primed=%u\r\n",
    			        estDist, estSpeed, speedRaw, distTarget, distDiff, (unsigned)speedPrimed);
    			    HAL_UART_Transmit(&huart3, (uint8_t*)line, n, 10);
    			}

    			Command *next = commands_peek();
    			if (next != NULL && next->opType == DRIVE && commands_type_match(cmd, next)) {
    				//absorb next command into current command.
    				next = commands_pop();

    				switch (next->distType) {
    					case TARGET:
    						cmd->val += next->val;
    						break;
    					case STOP_AWAY:
    					case STOP_L:
    					case STOP_R:
    						cmd->val = next->val;
    						break;
    				}

    				commands_end(&huart3, next);
    			}

    			next = commands_peek_next_drive();
    			float nextAngle = next != NULL ? next->steeringAngle : 0;
    			float nextAngleDiff = abs_float(nextAngle - cmd->steeringAngle);
    			uint8_t shouldBrake = cmd->distType == STOP_AWAY
    					? 1
    					: next != NULL
    					? next->dir != cmd->dir
    	//					|| next->dir < 0 //avoid smooth turning while reversing.
    						|| nextAngleDiff > SERVO_WIDTH			//too large a turn.
    						|| nextAngle * cmd->steeringAngle < 0	//opposite direction.
    					: 1;
    			uint8_t turnSpeed = next != NULL ? min_uint8(BRAKE_SPEED, next->speed) : BRAKE_SPEED;

    			//motor correction.
//    			 motor_pwmCorrection(wDiff, rBack, distDiff, brakingDist, cmd->distType,
//    			                            shouldBrake ? 0 : turnSpeed);

    			 motor_pwmCorrection(wDiff, rBack, distDiff, brakingDist, cmd->distType,
    			                     /*new*/ cmd->speed);

    			float timeLeft = (fabsf(speedFilt) > 1e-3f)? (fabsf(distDiff) / fabsf(speedFilt)): 1e10f;

    			uint8_t shouldEnd = 0;
    			if (!shouldBrake && !(ticksElapsed % ticksServo)) {
    				//turn a small angle every SERVO_TURN_PERIOD ms.
    				float diff = abs_float(nextAngle - steeringAngle);

    				if (timeLeft < (SERVO_TURN_PERIOD) * (diff / SERVO_TURN_STEP)) {
    					//should increment.
    					float step = min_float(SERVO_TURN_STEP, diff);
    					if (nextAngle < steeringAngle) step = -step;
    					steeringAngle += step;
    					servo_setAngle(steeringAngle);

    					if (diff < SERVO_TURN_STEP) {
    						shouldEnd = 1;
    					}
    				}
    			}

    			float absDistDiff = abs_float(distDiff),
    					absDistDiffChange = abs_float(distDiff - distDiffOld);
    			uint8_t shouldTick = (shouldBrake
    					&& absDistDiff < 1 && absDistDiffChange < 0.2)	//no change in motion.
    					|| absDistDiff < 0.05;								//error threshold satisfied.
    			ticksES = shouldTick ? ticksES + 1 : 0;

    			shouldEnd |= ticksES >= TICKS_ES_MAX	//minimum error threshold ticks met.
    						|| distDiff < -0.5; 		//prevent overshoot.

    			if (shouldEnd) {
    			    ticksES = 0;
    			    commands_end(&huart3, cmd);
    			    cmd = NULL;

    			    servo_setAngle(nextAngle);

    			    // (you can keep your ticksDelay logic)
    			    if (shouldBrake) {
    			        ticksDelay = (SERVO_TURN_PERIOD / MS_FRAME) *
    			                     (abs_float(nextAngle - steeringAngle) / SERVO_TURN_STEP) + 15;

    	    			brakeMotors();
    			    }

    			    // --- unify odometry reset for next command ---
    			  // --- prepare for next command ---
    			  odom_reset_all();

    			    if (shouldTrackDist) distTrack += fabsf(estDist);  // (optional) track magnitude
    			}
    		}
    	/* ----- End: Command Control Loop ----- */

    	/* ----- Start: Paced Loop Control ----- */
    	while (!newTick) {									//wait for new tick.
    		/* ----- Start: Process Ring Buffer ----- */
    		/* We use the down time for paced looping to process commands. */
    		if (track_i != ring_i) {
    			uint8_t c = ring_serial[track_i];
    			buf_serial[buf_i++] = c;
    			if (c == CMD_END) {
    				uint8_t *temp = buf_serial;
    //				float angle = parse_float_until(&temp, CMD_END, 4);
    //				servo_setAngle(angle);
    				if (c == '\n' || c == '\r') {
    				    commands_process(&huart3, buf_serial, buf_i);
    				    buf_i = 0;
    				}
    			}

    			serial_inc_ring(&track_i);
    		}
    		/* ----- End: Process Ring Buffer ----- */
    	}
    	newTick = 0;										//acknowledge flag.

    	ticksElapsed = (ticksElapsed + 1) % ticksRefresh;	//refresh tick count.
    	/* ----- End: Paced Loop Control ----- */
      }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.ScanConvMode = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 16;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 1000;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 10;
  sConfig.IC2Polarity = TIM_ICPOLARITY_FALLING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 10;
  if (HAL_TIM_Encoder_Init(&htim2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 10;
  sConfig.IC2Polarity = TIM_ICPOLARITY_FALLING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 10;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 65535;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief TIM7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM7_Init(void)
{

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 0;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 65535;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */

}

/**
  * @brief TIM9 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM9_Init(void)
{

  /* USER CODE BEGIN TIM9_Init 0 */

  /* USER CODE END TIM9_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM9_Init 1 */

  /* USER CODE END TIM9_Init 1 */
  htim9.Instance = TIM9;
  htim9.Init.Prescaler = 0;
  htim9.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim9.Init.Period = 7200;
  htim9.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim9.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim9) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim9, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim9) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim9, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim9, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM9_Init 2 */

  /* USER CODE END TIM9_Init 2 */
  HAL_TIM_MspPostInit(&htim9);

}

/**
  * @brief TIM10 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM10_Init(void)
{

  /* USER CODE BEGIN TIM10_Init 0 */

  /* USER CODE END TIM10_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM10_Init 1 */

  /* USER CODE END TIM10_Init 1 */
  htim10.Instance = TIM10;
  htim10.Init.Prescaler = 0;
  htim10.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim10.Init.Period = 7200;
  htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim10) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim10) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim10, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM10_Init 2 */

  /* USER CODE END TIM10_Init 2 */
  HAL_TIM_MspPostInit(&htim10);

}

/**
  * @brief TIM11 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM11_Init(void)
{

  /* USER CODE BEGIN TIM11_Init 0 */

  /* USER CODE END TIM11_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM11_Init 1 */

  /* USER CODE END TIM11_Init 1 */
  htim11.Instance = TIM11;
  htim11.Init.Prescaler = 0;
  htim11.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim11.Init.Period = 7200;
  htim11.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim11.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim11) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim11) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim11, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM11_Init 2 */

  /* USER CODE END TIM11_Init 2 */
  HAL_TIM_MspPostInit(&htim11);

}

/**
  * @brief TIM12 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM12_Init(void)
{

  /* USER CODE BEGIN TIM12_Init 0 */

  /* USER CODE END TIM12_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM12_Init 1 */

  /* USER CODE END TIM12_Init 1 */
  htim12.Instance = TIM12;
  htim12.Init.Prescaler = 4;
  htim12.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim12.Init.Period = 65535;
  htim12.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim12.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim12) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim12, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim12) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim12, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM12_Init 2 */

  /* USER CODE END TIM12_Init 2 */
  HAL_TIM_MspPostInit(&htim12);

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, OLED_DATA_COMMAND__Pin|OLED_RESET__Pin|OLED_SDIN_Pin|OLED_SCLK_Pin
                          |US_TRIG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED3_Pin */
  GPIO_InitStruct.Pin = LED3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED3_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : OLED_DATA_COMMAND__Pin OLED_RESET__Pin OLED_SDIN_Pin OLED_SCLK_Pin
                           US_TRIG_Pin */
  GPIO_InitStruct.Pin = OLED_DATA_COMMAND__Pin|OLED_RESET__Pin|OLED_SDIN_Pin|OLED_SCLK_Pin
                          |US_TRIG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : Buzzer_Pin */
  GPIO_InitStruct.Pin = Buzzer_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Buzzer_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : US_DATA_Pin */
  GPIO_InitStruct.Pin = US_DATA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(US_DATA_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_USER_Pin */
  GPIO_InitStruct.Pin = BTN_USER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BTN_USER_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
