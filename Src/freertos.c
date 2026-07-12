/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  *
  * COPYRIGHT(c) 2017 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */     
#include "pid.h"
#include "bsp_can.h"
#include "bsp_ibus.h"
#include "remote_control.h"
#include "remote_drive_config.h"
#include "usart.h"
#include "mxconstants.h"
#include "mytype.h"
#include "tim.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Variables -----------------------------------------------------------------*/
osThreadId defaultTaskHandle;

/* USER CODE BEGIN Variables */

/* USER CODE END Variables */

/* Function prototypes -------------------------------------------------------*/
void StartDefaultTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* Hook prototypes */

/* Init FreeRTOS */

void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
       
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
}

//#define CAN_ID_SCAN_CONTROL
#define CAN_CONTROL	//const current control 
//#define PWM_CONTROL	//const speed control
#define UART2_TX_SELF_TEST      1
#define UART2_TX_HEARTBEAT_MS   3000U
#define REMOTE_CMD_PRINT_RATE_HZ 5U
#define REMOTE_CMD_PRINT_PERIOD_MS (1000U / REMOTE_CMD_PRINT_RATE_HZ)
#define REMOTE_REASON_PRINT_MS  1000U
#define REMOTE_LINK_PROBE_MS    1000U
#define REMOTE_LINK_STILL_MS    300U
#define UART2_TX_TIMEOUT_MS     100U

#if (IBUS_DIAGNOSTIC_ONLY == 0)
#define LEFT_WHEEL_SLOT                   0U
#define RIGHT_WHEEL_SLOT                  2U
// Preserves the previous fixed-speed test signs: ID1 negative, ID3 positive.
#define LEFT_WHEEL_DIR                    (-1.0f)
#define RIGHT_WHEEL_DIR                   (+1.0f)
#define REMOTE_BENCH_CURRENT_LIMIT        14500
#define REMOTE_RPM_RAMP_PER_SEC           300.0f
#define REMOTE_DRIVE_PRINT_RATE_HZ        2U
#define REMOTE_DRIVE_PRINT_PERIOD_MS      (1000U / REMOTE_DRIVE_PRINT_RATE_HZ)
#define REMOTE_LED_STATUS_PRINT_RATE_HZ   2U
#define REMOTE_LED_STATUS_PRINT_PERIOD_MS (1000U / REMOTE_LED_STATUS_PRINT_RATE_HZ)
#define REMOTE_STATUS_LED_ON_LEVEL        GPIO_PIN_RESET
#define REMOTE_STATUS_LED_OFF_LEVEL       GPIO_PIN_SET
#define WHEEL_PID_MAX_OUTPUT    14500U
#define WHEEL_PID_I_LIMIT       4800U
#define WHEEL_PID_KP            1.5f
#define WHEEL_PID_KI            0.1f
#define WHEEL_PID_KD            0.0f
#define MOTOR_FEEDBACK_TIMEOUT_MS 100U
#define SCAN_ID_MIN        1
#define SCAN_ID_MAX        8
#define SCAN_CURRENT       400
#define SCAN_ON_TICKS      30
#define SCAN_PERIOD_TICKS  150
#endif
int set_v,set_spd[4];

typedef enum
{
	REMOTE_DRIVE_STATUS_BOOT_WAIT_SA_SAFE = 0,
	REMOTE_DRIVE_STATUS_DISARM_NORMAL,
	REMOTE_DRIVE_STATUS_ARM_BLOCKED_STICKS,
	REMOTE_DRIVE_STATUS_ARMED_GATE_OPEN,
	REMOTE_DRIVE_STATUS_FAULT_IBUS,
	REMOTE_DRIVE_STATUS_FAULT_DRIVE_DISABLED
} remote_drive_status_t;

typedef enum
{
	REMOTE_ARM_BLOCK_NONE = 0,
	REMOTE_ARM_BLOCK_SA_EDGE_NOT_ACCEPTED,
	REMOTE_ARM_BLOCK_CH12_NOT_CENTERED,
	REMOTE_ARM_BLOCK_IBUS_OFFLINE,
	REMOTE_ARM_BLOCK_DRIVE_DISABLED,
	REMOTE_ARM_BLOCK_DIAGNOSTIC_MODE
} remote_arm_block_reason_t;

typedef struct
{
	remote_drive_status_t status;
	remote_arm_block_reason_t last_arm_block_reason;
	uint8_t saw_sa_safe_after_boot;
	uint8_t remote_armed;
	uint8_t drive_gate_open;
	uint8_t serial_online;
	uint8_t motor_left_online;
	uint8_t motor_right_online;
	uint8_t ch1_ch2_centered;
	uint8_t remote_drive_enable;
	uint8_t diagnostic_only;
} remote_drive_debug_t;

volatile remote_drive_debug_t remote_drive_debug;

#if (IBUS_DIAGNOSTIC_ONLY == 0)
static float wheel_target_rpm[4];

static float clamp_float(float value, float min_value, float max_value)
{
	if(value > max_value)
	{
		return max_value;
	}
	if(value < min_value)
	{
		return min_value;
	}
	return value;
}

static float ramp_float(float current, float target, float step)
{
	if(target > current + step)
	{
		return current + step;
	}
	if(target < current - step)
	{
		return current - step;
	}
	return target;
}

static s16 clamp_s16(int value, int min_value, int max_value)
{
	if(value > max_value)
	{
		value = max_value;
	}
	if(value < min_value)
	{
		value = min_value;
	}
	return (s16)value;
}

static float wheel_speed_limit(remote_speed_mode_t speed_mode)
{
	if(speed_mode == REMOTE_SPEED_HIGH)
	{
		return REMOTE_M3508_RPM_HIGH;
	}
	if(speed_mode == REMOTE_SPEED_MID)
	{
		return REMOTE_M3508_RPM_MID;
	}
	return REMOTE_M3508_RPM_LOW;
}

static uint8_t motor_feedback_online(uint8_t index, uint32_t now)
{
	if(moto_chassis[index].msg_cnt <= 50U)
	{
		return 0U;
	}
	return ((uint32_t)(now - moto_chassis[index].last_msg_ms) <= MOTOR_FEEDBACK_TIMEOUT_MS) ? 1U : 0U;
}

static uint8_t remote_drive_gate_is_open(const remote_cmd_t *cmd)
{
	return (REMOTE_DRIVE_ENABLE != 0) &&
	       (IBUS_DIAGNOSTIC_ONLY == 0) &&
	       (cmd->command_valid != 0U) &&
	       (cmd->arm_state == REMOTE_ARMED) &&
	       (cmd->serial_online != 0U);
}

static void stop_wheel_targets(s16 iq[4])
{
	uint8_t i;

	for(i = 0U; i < 4U; i++)
	{
		set_spd[i] = 0;
		wheel_target_rpm[i] = 0.0f;
		iq[i] = 0;
	}
	pid_reset_dynamic_state(&pid_spd[LEFT_WHEEL_SLOT]);
	pid_reset_dynamic_state(&pid_spd[RIGHT_WHEEL_SLOT]);
}

typedef enum
{
	DRIVE_REASON_NONE = 0,
	DRIVE_REASON_REMOTE_DISABLED,
	DRIVE_REASON_SA_SAFE,
	DRIVE_REASON_IBUS_TIMEOUT,
	DRIVE_REASON_INVALID_CHANNEL,
	DRIVE_REASON_FAILSAFE,
	DRIVE_REASON_ARM_SEQUENCE,
	DRIVE_REASON_DISARM
} drive_reason_t;

static const char *drive_arm_state_text(remote_arm_state_t state)
{
	return (state == REMOTE_ARMED) ? "ARMED" : "DISARM";
}

static const char *drive_speed_mode_text(remote_speed_mode_t mode)
{
	if(mode == REMOTE_SPEED_HIGH)
	{
		return "HIGH";
	}
	if(mode == REMOTE_SPEED_MID)
	{
		return "MID";
	}
	return "LOW";
}

static const char *remote_drive_status_text(remote_drive_status_t status)
{
	if(status == REMOTE_DRIVE_STATUS_BOOT_WAIT_SA_SAFE)
	{
		return "BOOT_WAIT_SA_SAFE";
	}
	if(status == REMOTE_DRIVE_STATUS_DISARM_NORMAL)
	{
		return "DISARM_NORMAL";
	}
	if(status == REMOTE_DRIVE_STATUS_ARM_BLOCKED_STICKS)
	{
		return "ARM_BLOCKED_STICKS";
	}
	if(status == REMOTE_DRIVE_STATUS_ARMED_GATE_OPEN)
	{
		return "ARMED_GATE_OPEN";
	}
	if(status == REMOTE_DRIVE_STATUS_FAULT_IBUS)
	{
		return "FAULT_IBUS";
	}
	return "FAULT_DRIVE_DISABLED";
}

static const char *remote_arm_block_reason_text(remote_arm_block_reason_t reason)
{
	if(reason == REMOTE_ARM_BLOCK_SA_EDGE_NOT_ACCEPTED)
	{
		return "SA_EDGE_NOT_ACCEPTED";
	}
	if(reason == REMOTE_ARM_BLOCK_CH12_NOT_CENTERED)
	{
		return "CH12_NOT_CENTERED";
	}
	if(reason == REMOTE_ARM_BLOCK_IBUS_OFFLINE)
	{
		return "IBUS_OFFLINE";
	}
	if(reason == REMOTE_ARM_BLOCK_DRIVE_DISABLED)
	{
		return "DRIVE_DISABLED";
	}
	if(reason == REMOTE_ARM_BLOCK_DIAGNOSTIC_MODE)
	{
		return "DIAGNOSTIC_MODE";
	}
	return "NONE";
}

static drive_reason_t drive_reason_from_remote(const remote_cmd_t *cmd)
{
	if(cmd->serial_online == 0U)
	{
		return DRIVE_REASON_IBUS_TIMEOUT;
	}
	if(cmd->failsafe_detected != 0U)
	{
		return DRIVE_REASON_FAILSAFE;
	}
	if(cmd->channels_valid == 0U)
	{
		return DRIVE_REASON_INVALID_CHANNEL;
	}
	if(cmd->disarm_reason == REMOTE_DISARM_REASON_SA_SAFE)
	{
		return DRIVE_REASON_SA_SAFE;
	}
	if(cmd->disarm_reason == REMOTE_DISARM_REASON_IBUS_SERIAL_TIMEOUT)
	{
		return DRIVE_REASON_IBUS_TIMEOUT;
	}
	if(cmd->disarm_reason == REMOTE_DISARM_REASON_INVALID_CHANNEL)
	{
		return DRIVE_REASON_INVALID_CHANNEL;
	}
	if(cmd->disarm_reason == REMOTE_DISARM_REASON_FAILSAFE_PATTERN)
	{
		return DRIVE_REASON_FAILSAFE;
	}
	if(cmd->disarm_reason == REMOTE_DISARM_REASON_ARM_SEQUENCE)
	{
		return DRIVE_REASON_ARM_SEQUENCE;
	}
	return DRIVE_REASON_DISARM;
}

static const char *drive_reason_text(drive_reason_t reason)
{
	if(reason == DRIVE_REASON_REMOTE_DISABLED)
	{
		return "DRIVE_DISABLED";
	}
	if(reason == DRIVE_REASON_SA_SAFE)
	{
		return "SA_SAFE";
	}
	if(reason == DRIVE_REASON_IBUS_TIMEOUT)
	{
		return "IBUS_TIMEOUT";
	}
	if(reason == DRIVE_REASON_INVALID_CHANNEL)
	{
		return "INVALID_CHANNEL";
	}
	if(reason == DRIVE_REASON_FAILSAFE)
	{
		return "FAILSAFE";
	}
	if(reason == DRIVE_REASON_ARM_SEQUENCE)
	{
		return "ARM_SEQUENCE";
	}
	if(reason == DRIVE_REASON_DISARM)
	{
		return "DISARM";
	}
	return "NONE";
}

static int drive_float_to_centi(float value)
{
	if(value >= 0.0f)
	{
		return (int)(value * 100.0f + 0.5f);
	}
	return (int)(value * 100.0f - 0.5f);
}

static int drive_float_to_deci(float value)
{
	if(value >= 0.0f)
	{
		return (int)(value * 10.0f + 0.5f);
	}
	return (int)(value * 10.0f - 0.5f);
}

static int drive_abs_int(int value)
{
	return (value < 0) ? -value : value;
}

static void uart2_tx_message(const char *text, int len)
{
	if(text == 0 || len <= 0)
	{
		return;
	}
	if(len > 320)
	{
		len = 320;
	}
	(void)HAL_UART_Transmit(&huart2, (uint8_t *)text, (uint16_t)len, UART2_TX_TIMEOUT_MS);
}

static uint8_t remote_drive_led_phase_on(remote_drive_status_t status, uint32_t elapsed_ms)
{
	uint32_t phase;

	if(status == REMOTE_DRIVE_STATUS_FAULT_DRIVE_DISABLED)
	{
		return 0U;
	}
	if(status == REMOTE_DRIVE_STATUS_ARMED_GATE_OPEN)
	{
		return 1U;
	}
	if(status == REMOTE_DRIVE_STATUS_FAULT_IBUS)
	{
		return ((elapsed_ms % 250U) < 125U) ? 1U : 0U;
	}
	if(status == REMOTE_DRIVE_STATUS_BOOT_WAIT_SA_SAFE)
	{
		phase = elapsed_ms % 1000U;
		return (phase < 100U || (phase >= 200U && phase < 300U)) ? 1U : 0U;
	}
	if(status == REMOTE_DRIVE_STATUS_ARM_BLOCKED_STICKS)
	{
		phase = elapsed_ms % 1200U;
		return (phase < 100U ||
		        (phase >= 200U && phase < 300U) ||
		        (phase >= 400U && phase < 500U)) ? 1U : 0U;
	}
	return ((elapsed_ms % 1000U) < 500U) ? 1U : 0U;
}

static void remote_drive_led_write(uint8_t on)
{
	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin,
	                  (on != 0U) ? REMOTE_STATUS_LED_ON_LEVEL : REMOTE_STATUS_LED_OFF_LEVEL);
}

static remote_drive_status_t remote_drive_select_status(const remote_cmd_t *cmd,
                                                        uint8_t gate)
{
	if(REMOTE_DRIVE_ENABLE == 0 || IBUS_DIAGNOSTIC_ONLY != 0)
	{
		return REMOTE_DRIVE_STATUS_FAULT_DRIVE_DISABLED;
	}
	if(cmd->serial_online == 0U)
	{
		return REMOTE_DRIVE_STATUS_FAULT_IBUS;
	}
	if(cmd->saw_sa_safe_after_boot == 0U)
	{
		return REMOTE_DRIVE_STATUS_BOOT_WAIT_SA_SAFE;
	}
	if(cmd->arm_state != REMOTE_ARMED &&
	   cmd->ch_raw[REMOTE_CH_ARM_INDEX] > REMOTE_SWITCH_HIGH_THRESHOLD &&
	   cmd->ch1_ch2_centered == 0U)
	{
		return REMOTE_DRIVE_STATUS_ARM_BLOCKED_STICKS;
	}
	if(gate != 0U && cmd->arm_state == REMOTE_ARMED)
	{
		return REMOTE_DRIVE_STATUS_ARMED_GATE_OPEN;
	}
	return REMOTE_DRIVE_STATUS_DISARM_NORMAL;
}

static remote_arm_block_reason_t remote_drive_select_arm_block_reason(const remote_cmd_t *cmd,
                                                                      uint8_t gate)
{
	if(gate != 0U)
	{
		return REMOTE_ARM_BLOCK_NONE;
	}
	if(cmd->ch_raw[REMOTE_CH_ARM_INDEX] <= REMOTE_SWITCH_HIGH_THRESHOLD)
	{
		return REMOTE_ARM_BLOCK_NONE;
	}
	if(IBUS_DIAGNOSTIC_ONLY != 0)
	{
		return REMOTE_ARM_BLOCK_DIAGNOSTIC_MODE;
	}
	if(REMOTE_DRIVE_ENABLE == 0)
	{
		return REMOTE_ARM_BLOCK_DRIVE_DISABLED;
	}
	if(cmd->serial_online == 0U)
	{
		return REMOTE_ARM_BLOCK_IBUS_OFFLINE;
	}
	if(cmd->ch1_ch2_centered == 0U)
	{
		return REMOTE_ARM_BLOCK_CH12_NOT_CENTERED;
	}
	return REMOTE_ARM_BLOCK_SA_EDGE_NOT_ACCEPTED;
}

static void remote_drive_debug_update(const remote_cmd_t *cmd,
                                      uint8_t gate,
                                      uint8_t left_motor_online,
                                      uint8_t right_motor_online)
{
	remote_drive_debug.status =
		remote_drive_select_status(cmd, gate);
	remote_drive_debug.last_arm_block_reason =
		remote_drive_select_arm_block_reason(cmd, gate);
	remote_drive_debug.saw_sa_safe_after_boot = cmd->saw_sa_safe_after_boot;
	remote_drive_debug.remote_armed = (cmd->arm_state == REMOTE_ARMED) ? 1U : 0U;
	remote_drive_debug.drive_gate_open = gate;
	remote_drive_debug.serial_online = cmd->serial_online;
	remote_drive_debug.motor_left_online = left_motor_online;
	remote_drive_debug.motor_right_online = right_motor_online;
	remote_drive_debug.ch1_ch2_centered = cmd->ch1_ch2_centered;
	remote_drive_debug.remote_drive_enable = (REMOTE_DRIVE_ENABLE != 0) ? 1U : 0U;
	remote_drive_debug.diagnostic_only = (IBUS_DIAGNOSTIC_ONLY != 0) ? 1U : 0U;
}

static void remote_drive_led_task(void)
{
	static remote_drive_status_t last_status = REMOTE_DRIVE_STATUS_FAULT_DRIVE_DISABLED;
	static uint32_t status_since_ms;
	uint32_t now = HAL_GetTick();

	if(remote_drive_debug.status != last_status)
	{
		last_status = remote_drive_debug.status;
		status_since_ms = now;
	}

	remote_drive_led_write(remote_drive_led_phase_on(remote_drive_debug.status,
	                                                 (uint32_t)(now - status_since_ms)));
}

static void remote_drive_print_led_status(void)
{
	static uint32_t last_print_ms;
	static char line[240];
	uint32_t now = HAL_GetTick();
	int len;

	if((uint32_t)(now - last_print_ms) < REMOTE_LED_STATUS_PRINT_PERIOD_MS)
	{
		return;
	}

	len = snprintf(line, sizeof(line),
	               "LED_STATUS status=%s gate=%u arm=%u block=%s sa_seen=%u ch12_center=%u serial=%u motorL_diag=%u motorR_diag=%u enable=%u diag=%u\r\n",
	               remote_drive_status_text(remote_drive_debug.status),
	               remote_drive_debug.drive_gate_open,
	               remote_drive_debug.remote_armed,
	               remote_arm_block_reason_text(remote_drive_debug.last_arm_block_reason),
	               remote_drive_debug.saw_sa_safe_after_boot,
	               remote_drive_debug.ch1_ch2_centered,
	               remote_drive_debug.serial_online,
	               remote_drive_debug.motor_left_online,
	               remote_drive_debug.motor_right_online,
	               remote_drive_debug.remote_drive_enable,
	               remote_drive_debug.diagnostic_only);
	if(len >= (int)sizeof(line))
	{
		len = (int)sizeof(line) - 1;
	}
	uart2_tx_message(line, len);
	last_print_ms = now;
}

static void remote_drive_send_currents(const s16 iq[4])
{
	set_moto_current(&hcan1, iq[0], iq[1], iq[2], iq[3]);
	set_moto_current(&hcan2, iq[0], iq[1], iq[2], iq[3]);
	set_moto_current_5_8(&hcan1, 0, 0, 0, 0);
	set_moto_current_5_8(&hcan2, 0, 0, 0, 0);
}

static void remote_drive_send_zero_current(void)
{
	s16 iq[4] = {0};
	remote_drive_send_currents(iq);
}

static void remote_drive_force_safe_stop(s16 iq[4])
{
	stop_wheel_targets(iq);
	remote_drive_send_zero_current();
}

static void remote_drive_print_status(uint8_t gate,
                                      const remote_cmd_t *cmd,
                                      uint8_t left_motor_online,
                                      uint8_t right_motor_online,
                                      float left_target,
                                      float right_target,
                                      s16 left_current,
                                      s16 right_current,
                                      drive_reason_t reason)
{
	static uint32_t last_print_ms;
	static char line[320];
	uint32_t now = HAL_GetTick();
	int fwd = drive_float_to_centi(cmd->forward);
	int turn = drive_float_to_centi(cmd->turn);
	int ltar = drive_float_to_deci(left_target);
	int rtar = drive_float_to_deci(right_target);
	int fwd_abs = drive_abs_int(fwd);
	int turn_abs = drive_abs_int(turn);
	int ltar_abs = drive_abs_int(ltar);
	int rtar_abs = drive_abs_int(rtar);
	int len;

	if((uint32_t)(now - last_print_ms) < REMOTE_DRIVE_PRINT_PERIOD_MS)
	{
		return;
	}

	len = snprintf(line, sizeof(line),
	               "DRIVE gate=%u arm=%s serial=%u mode=%s fwd=%s%d.%02d turn=%s%d.%02d Ltar=%s%d.%01d Rtar=%s%d.%01d Lrpm=%d Rrpm=%d Liq=%d Riq=%d motorL_diag=%u motorR_diag=%u reason=%s\r\n",
	               gate,
	               drive_arm_state_text(cmd->arm_state),
	               cmd->serial_online,
	               drive_speed_mode_text(cmd->speed_mode),
	               (fwd < 0) ? "-" : "", fwd_abs / 100, fwd_abs % 100,
	               (turn < 0) ? "-" : "", turn_abs / 100, turn_abs % 100,
	               (ltar < 0) ? "-" : "", ltar_abs / 10, ltar_abs % 10,
	               (rtar < 0) ? "-" : "", rtar_abs / 10, rtar_abs % 10,
	               moto_chassis[LEFT_WHEEL_SLOT].speed_rpm,
	               moto_chassis[RIGHT_WHEEL_SLOT].speed_rpm,
	               left_current,
	               right_current,
	               left_motor_online,
	               right_motor_online,
	               drive_reason_text(reason));
	if(len >= (int)sizeof(line))
	{
		len = (int)sizeof(line) - 1;
	}
	uart2_tx_message(line, len);
	last_print_ms = now;
}

static void remote_drive_control_task(const remote_cmd_t *cmd,
                                      uint8_t left_motor_online,
                                      uint8_t right_motor_online)
{
	static uint32_t last_update_ms;
	uint32_t now = HAL_GetTick();
	uint32_t dt_ms = (last_update_ms == 0U) ? 10U : (uint32_t)(now - last_update_ms);
	uint8_t gate;
	drive_reason_t reason = DRIVE_REASON_NONE;
	float left_target = 0.0f;
	float right_target = 0.0f;
	s16 iq[4] = {0};

	if(dt_ms > 100U)
	{
		dt_ms = 100U;
	}
	last_update_ms = now;

	gate = remote_drive_gate_is_open(cmd);

	if(gate == 0U)
	{
		if(REMOTE_DRIVE_ENABLE == 0 || IBUS_DIAGNOSTIC_ONLY != 0)
		{
			reason = DRIVE_REASON_REMOTE_DISABLED;
		}
		else if(cmd->arm_state != REMOTE_ARMED || cmd->command_valid == 0U)
		{
			reason = drive_reason_from_remote(cmd);
		}
		else if(REMOTE_DRIVE_ENABLE == 0)
		{
			reason = DRIVE_REASON_REMOTE_DISABLED;
		}
		remote_drive_force_safe_stop(iq);
		remote_drive_debug_update(cmd, 0U, left_motor_online, right_motor_online);
		remote_drive_led_task();
		remote_drive_print_status(0U, cmd, left_motor_online, right_motor_online,
		                          0.0f, 0.0f, 0, 0, reason);
		remote_drive_print_led_status();
		return;
	}

	{
		float speed_limit = wheel_speed_limit(cmd->speed_mode);
		float left_mix = clamp_float(cmd->forward + cmd->turn, -1.0f, 1.0f);
		float right_mix = clamp_float(cmd->forward - cmd->turn, -1.0f, 1.0f);
		float step = REMOTE_RPM_RAMP_PER_SEC * ((float)dt_ms / 1000.0f);
		float left_ref = LEFT_WHEEL_DIR * left_mix * speed_limit;
		float right_ref = RIGHT_WHEEL_DIR * right_mix * speed_limit;

		wheel_target_rpm[LEFT_WHEEL_SLOT] =
			ramp_float(wheel_target_rpm[LEFT_WHEEL_SLOT], left_ref, step);
		wheel_target_rpm[RIGHT_WHEEL_SLOT] =
			ramp_float(wheel_target_rpm[RIGHT_WHEEL_SLOT], right_ref, step);

		left_target = wheel_target_rpm[LEFT_WHEEL_SLOT];
		right_target = wheel_target_rpm[RIGHT_WHEEL_SLOT];
		set_spd[LEFT_WHEEL_SLOT] = (int)left_target;
		set_spd[RIGHT_WHEEL_SLOT] = (int)right_target;
		set_spd[1] = 0;
		set_spd[3] = 0;

		pid_calc(&pid_spd[LEFT_WHEEL_SLOT],
		         moto_chassis[LEFT_WHEEL_SLOT].speed_rpm,
		         left_target);
		pid_calc(&pid_spd[RIGHT_WHEEL_SLOT],
		         moto_chassis[RIGHT_WHEEL_SLOT].speed_rpm,
		         right_target);

		iq[LEFT_WHEEL_SLOT] = clamp_s16((int)pid_spd[LEFT_WHEEL_SLOT].pos_out,
		                                -REMOTE_BENCH_CURRENT_LIMIT,
		                                REMOTE_BENCH_CURRENT_LIMIT);
		iq[RIGHT_WHEEL_SLOT] = clamp_s16((int)pid_spd[RIGHT_WHEEL_SLOT].pos_out,
		                                 -REMOTE_BENCH_CURRENT_LIMIT,
		                                 REMOTE_BENCH_CURRENT_LIMIT);
	}

	remote_drive_send_currents(iq);
	remote_drive_debug_update(cmd, 1U, left_motor_online, right_motor_online);
	remote_drive_led_task();
	remote_drive_print_status(1U, cmd, left_motor_online, right_motor_online,
	                          left_target, right_target,
	                          iq[LEFT_WHEEL_SLOT], iq[RIGHT_WHEEL_SLOT],
	                          DRIVE_REASON_NONE);
	remote_drive_print_led_status();
}
#endif

#if (IBUS_DIAGNOSTIC_ONLY != 0)
static const char *remote_arm_state_text(remote_arm_state_t state)
{
	return (state == REMOTE_ARMED) ? "ARMED" : "DISARM";
}

static const char *remote_speed_mode_text(remote_speed_mode_t mode)
{
	if(mode == REMOTE_SPEED_HIGH)
	{
		return "HIGH";
	}
	if(mode == REMOTE_SPEED_MID)
	{
		return "MID";
	}
	return "LOW";
}

static const char *remote_disarm_reason_text(const remote_cmd_t *cmd)
{
	if(cmd->arm_state == REMOTE_ARMED && cmd->command_valid != 0U)
	{
		return "NONE";
	}

	if(cmd->disarm_reason == REMOTE_DISARM_REASON_SA_SAFE)
	{
		return "SA_SAFE";
	}
	if(cmd->disarm_reason == REMOTE_DISARM_REASON_IBUS_SERIAL_TIMEOUT)
	{
		return "IBUS_TIMEOUT";
	}
	if(cmd->disarm_reason == REMOTE_DISARM_REASON_INVALID_CHANNEL)
	{
		return "INVALID_CHANNEL";
	}
	if(cmd->disarm_reason == REMOTE_DISARM_REASON_FAILSAFE_PATTERN)
	{
		return "FAILSAFE_PATTERN";
	}
	if(cmd->disarm_reason == REMOTE_DISARM_REASON_ARM_SEQUENCE)
	{
		return "ARM_SEQUENCE";
	}
	return "BOOT";
}

static int remote_float_to_centi(float value)
{
	if(value >= 0.0f)
	{
		return (int)(value * 100.0f + 0.5f);
	}
	return (int)(value * 100.0f - 0.5f);
}

static int remote_abs_int(int value)
{
	return (value < 0) ? -value : value;
}

static uint8_t remote_cmd_sticks_not_centered(const remote_cmd_t *cmd)
{
	const uint16_t deadzone_count = (uint16_t)(((REMOTE_CHANNEL_MAX - REMOTE_CHANNEL_CENTER) *
	                                            REMOTE_STICK_DEADZONE) + 0.5f);
	const uint16_t low = REMOTE_CHANNEL_CENTER - deadzone_count;
	const uint16_t high = REMOTE_CHANNEL_CENTER + deadzone_count;

	return (cmd->ch_raw[REMOTE_CH_TURN_INDEX] < low ||
	        cmd->ch_raw[REMOTE_CH_TURN_INDEX] > high ||
	        cmd->ch_raw[REMOTE_CH_FORWARD_INDEX] < low ||
	        cmd->ch_raw[REMOTE_CH_FORWARD_INDEX] > high) ? 1U : 0U;
}

static uint8_t remote_raw_changed(const remote_cmd_t *cmd, const uint16_t last_raw[REMOTE_CHANNEL_NUM])
{
	uint8_t i;

	for(i = 0U; i < REMOTE_CHANNEL_NUM; i++)
	{
		if(cmd->ch_raw[i] != last_raw[i])
		{
			return 1U;
		}
	}
	return 0U;
}

static void remote_copy_raw(uint16_t dst[REMOTE_CHANNEL_NUM], const remote_cmd_t *cmd)
{
	uint8_t i;

	for(i = 0U; i < REMOTE_CHANNEL_NUM; i++)
	{
		dst[i] = cmd->ch_raw[i];
	}
}

static void uart2_tx_message(const char *text, int len)
{
	if(text == 0 || len <= 0)
	{
		return;
	}

	if(len > 320)
	{
		len = 320;
	}

	(void)HAL_UART_Transmit(&huart2, (uint8_t *)text, (uint16_t)len, UART2_TX_TIMEOUT_MS);
}

static void remote_cmd_debug_print_task(void)
{
	static uint32_t last_cmd_print_ms;
	static uint32_t last_reason_print_ms;
	static uint32_t last_probe_print_ms;
	static uint32_t last_raw_change_ms;
	static uint16_t last_probe_raw[REMOTE_CHANNEL_NUM];
	static uint8_t last_probe_raw_ready;
	static char line[320];
	remote_cmd_t cmd;
	uint32_t now = HAL_GetTick();
	int len;

	ibus_poll_link_state();
	remote_control_update();
	if(remote_control_get(&cmd) == 0U)
	{
		return;
	}

	if(last_probe_raw_ready == 0U || remote_raw_changed(&cmd, last_probe_raw) != 0U)
	{
		remote_copy_raw(last_probe_raw, &cmd);
		last_probe_raw_ready = 1U;
		last_raw_change_ms = now;
	}

	if(ibus_take_serial_link_lost_event() != 0U)
	{
		const char *msg = "REMOTE DISARM: iBUS serial timeout\r\n";
		uart2_tx_message(msg, (int)strlen(msg));
	}

	if((uint32_t)(now - last_cmd_print_ms) >= REMOTE_CMD_PRINT_PERIOD_MS)
	{
		int forward = remote_float_to_centi(cmd.forward);
		int turn = remote_float_to_centi(cmd.turn);
		int aux1 = remote_float_to_centi(cmd.aux1);
		int aux2 = remote_float_to_centi(cmd.aux2);
		int forward_abs = remote_abs_int(forward);
		int turn_abs = remote_abs_int(turn);
		int aux1_abs = remote_abs_int(aux1);
		int aux2_abs = remote_abs_int(aux2);

		len = snprintf(line, sizeof(line),
		               "CMD arm=%s valid=%u serial_online=%u failsafe=%u reason=%s mode=%s forward=%s%d.%02d turn=%s%d.%02d aux1=%s%d.%02d aux2=%s%d.%02d raw=[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u]\r\n",
		               remote_arm_state_text(cmd.arm_state),
		               cmd.command_valid,
		               cmd.serial_online,
		               cmd.failsafe_detected,
		               remote_disarm_reason_text(&cmd),
		               remote_speed_mode_text(cmd.speed_mode),
		               (forward < 0) ? "-" : "", forward_abs / 100, forward_abs % 100,
		               (turn < 0) ? "-" : "", turn_abs / 100, turn_abs % 100,
		               (aux1 < 0) ? "-" : "", aux1_abs / 100, aux1_abs % 100,
		               (aux2 < 0) ? "-" : "", aux2_abs / 100, aux2_abs % 100,
		               cmd.ch_raw[0], cmd.ch_raw[1], cmd.ch_raw[2], cmd.ch_raw[3], cmd.ch_raw[4],
		               cmd.ch_raw[5], cmd.ch_raw[6], cmd.ch_raw[7], cmd.ch_raw[8], cmd.ch_raw[9]);
		if(len >= (int)sizeof(line))
		{
			len = (int)sizeof(line) - 1;
		}
		uart2_tx_message(line, len);
		last_cmd_print_ms = now;
	}

	if(cmd.serial_online != 0U &&
	   cmd.ch_raw[REMOTE_CH_ARM_INDEX] > REMOTE_SWITCH_HIGH_THRESHOLD &&
	   (uint32_t)(now - last_raw_change_ms) >= REMOTE_LINK_STILL_MS &&
	   (uint32_t)(now - last_probe_print_ms) >= REMOTE_LINK_PROBE_MS)
	{
		len = snprintf(line, sizeof(line),
		               "RC LINK PROBE: serial_online=1 raw=[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u]\r\n",
		               cmd.ch_raw[0], cmd.ch_raw[1], cmd.ch_raw[2], cmd.ch_raw[3], cmd.ch_raw[4],
		               cmd.ch_raw[5], cmd.ch_raw[6], cmd.ch_raw[7], cmd.ch_raw[8], cmd.ch_raw[9]);
		if(len >= (int)sizeof(line))
		{
			len = (int)sizeof(line) - 1;
		}
		uart2_tx_message(line, len);
		last_probe_print_ms = now;
	}

	if((uint32_t)(now - last_reason_print_ms) >= REMOTE_REASON_PRINT_MS)
	{
		if(cmd.serial_online != 0U &&
		        cmd.ch_raw[REMOTE_CH_ARM_INDEX] > REMOTE_SWITCH_HIGH_THRESHOLD &&
		        cmd.arm_state == REMOTE_DISARM &&
		        cmd.channels_valid != 0U &&
		        cmd.failsafe_detected == 0U)
		{
			const char *msg;

			if(remote_cmd_sticks_not_centered(&cmd) != 0U)
			{
				msg = "ARM BLOCKED: center CH1/CH2 before SA down\r\n";
			}
			else
			{
				msg = "ARM BLOCKED: wait iBUS stable 500ms\r\n";
			}
			uart2_tx_message(msg, (int)strlen(msg));
			last_reason_print_ms = now;
		}
	}
}
#endif

/* StartDefaultTask function */
void StartDefaultTask(void const * argument)
{

  /* USER CODE BEGIN StartDefaultTask */
#if (IBUS_DIAGNOSTIC_ONLY != 0)
	ibus_init();
	remote_control_init();
  /* Infinite loop */
  for(;;)
  {
#if UART2_TX_SELF_TEST
		static uint32_t uart2_tx_self_test_ms;
		const char *msg = "HEARTBEAT UART2 TX OK\r\n";
		uint32_t now = HAL_GetTick();

		if((uint32_t)(now - uart2_tx_self_test_ms) >= UART2_TX_HEARTBEAT_MS)
		{
			uart2_tx_self_test_ms = now;
			(void)HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 100);
		}
#endif
		remote_cmd_debug_print_task();
		ibus_debug_print_task();
		osDelay(10);
  }
#else
	HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_4);
	
	my_can_filter_init_recv_all(&hcan1);
	my_can_filter_init_recv_all(&hcan2);
	HAL_CAN_Receive_IT(&hcan1, CAN_FIFO0);
	HAL_CAN_Receive_IT(&hcan2, CAN_FIFO0);
//	PID_struct_init(&pid_omg, POSITION_PID, 20000, 20000,
//									1.5f,	0.1f,	0.0f	);  //angular rate closeloop.
	for(int i=0; i<4; i++)
	{
		PID_struct_init(&pid_spd[i], POSITION_PID, WHEEL_PID_MAX_OUTPUT, WHEEL_PID_I_LIMIT,
									WHEEL_PID_KP,	WHEEL_PID_KI,	WHEEL_PID_KD	);  // Real car PID must be tuned again.
	}
	
	__HAL_TIM_SetCompare(&htim5, TIM_CHANNEL_1, 1000);
	__HAL_TIM_SetCompare(&htim5, TIM_CHANNEL_2, 1000);
	__HAL_TIM_SetCompare(&htim5, TIM_CHANNEL_3, 1000);
	__HAL_TIM_SetCompare(&htim5, TIM_CHANNEL_4, 1000);	//ppm must be 1000 at first time.
	HAL_Delay(100);
	ibus_init();
	remote_control_init();
  /* Infinite loop */
  for(;;)
  {
		remote_cmd_t cmd;
		uint32_t now = HAL_GetTick();
		uint8_t left_motor_online = motor_feedback_online(LEFT_WHEEL_SLOT, now);
		uint8_t right_motor_online = motor_feedback_online(RIGHT_WHEEL_SLOT, now);

		remote_control_set_motor_feedback_online(left_motor_online, right_motor_online);
		ibus_poll_link_state();
		remote_control_update();
		(void)remote_control_get(&cmd);

#if UART2_TX_SELF_TEST
		{
			static uint32_t uart2_tx_self_test_ms;
			const char *msg = "HEARTBEAT UART2 TX OK\r\n";

			if((uint32_t)(now - uart2_tx_self_test_ms) >= UART2_TX_HEARTBEAT_MS)
			{
				uart2_tx_self_test_ms = now;
				(void)HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 100);
			}
		}
#endif

#if defined CAN_CONTROL
		remote_drive_control_task(&cmd, left_motor_online, right_motor_online);
#else
		{
			s16 iq[4] = {0};
			remote_drive_force_safe_stop(iq);
			remote_drive_debug_update(&cmd, 0U, left_motor_online, right_motor_online);
			remote_drive_led_task();
			remote_drive_print_led_status();
		}
#endif
	
	osDelay(10);
  }
#endif
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Application */
     
/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
