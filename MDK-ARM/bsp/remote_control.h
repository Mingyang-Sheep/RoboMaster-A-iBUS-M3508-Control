#ifndef __REMOTE_CONTROL_H
#define __REMOTE_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "remote_drive_config.h"

#define REMOTE_CH_TURN_INDEX                 0U
#define REMOTE_CH_FORWARD_INDEX              1U
#define REMOTE_CH_AUX1_INDEX                 2U
#define REMOTE_CH_AUX2_INDEX                 3U
#define REMOTE_CH_ARM_INDEX                  6U
#define REMOTE_CH_SPEED_INDEX                8U
#define REMOTE_CHANNEL_NUM                   10U

#define REMOTE_CHANNEL_MIN                   1000U
#define REMOTE_CHANNEL_CENTER                1500U
#define REMOTE_CHANNEL_MAX                   2000U
#define REMOTE_CHANNEL_VALID_MIN             900U
#define REMOTE_CHANNEL_VALID_MAX             2100U

#define REMOTE_IBUS_TIMEOUT_MS               100U
#define REMOTE_ARM_STABLE_MS                 500U
#define REMOTE_SA_SAFE_REQUIRED_MS           300U

#define REMOTE_SWITCH_LOW_THRESHOLD          1300U
#define REMOTE_SWITCH_HIGH_THRESHOLD         1700U

/* Direction correction order after field checks:
 * 1. If right-stick up moves the robot backward, flip REMOTE_FORWARD_DIR.
 * 2. If right-stick right turns the robot left, flip REMOTE_TURN_DIR.
 * 3. If only one screw wheel is mismatched, verify mounting/handedness first,
 *    then adjust LEFT_WHEEL_DIR or RIGHT_WHEEL_DIR in the drive task.
 * 4. Do not fix software direction by swapping CAN IDs, slots, or C620 wiring.
 */
#define REMOTE_FORWARD_DIR                   (+1.0f)
#define REMOTE_TURN_DIR                      (+1.0f)
#define REMOTE_AUX1_DIR                      (+1.0f)
#define REMOTE_AUX2_DIR                      (+1.0f)

#define REMOTE_SPEED_SCALE_LOW               (0.33f)
#define REMOTE_SPEED_SCALE_MID               (0.66f)
#define REMOTE_SPEED_SCALE_HIGH              (1.00f)

#define REMOTE_FAILSAFE_PATTERN_ENABLE       0
#define REMOTE_FAILSAFE_CH7_SAFE_MAX         1300U
#define REMOTE_FAILSAFE_STABLE_FRAMES        5U

typedef enum
{
    REMOTE_DISARM = 0,
    REMOTE_ARMED  = 1
} remote_arm_state_t;

typedef enum
{
    REMOTE_SPEED_LOW = 0,
    REMOTE_SPEED_MID,
    REMOTE_SPEED_HIGH
} remote_speed_mode_t;

typedef enum
{
    REMOTE_DISARM_REASON_BOOT = 0,
    REMOTE_DISARM_REASON_SA_SAFE,
    REMOTE_DISARM_REASON_IBUS_SERIAL_TIMEOUT,
    REMOTE_DISARM_REASON_INVALID_CHANNEL,
    REMOTE_DISARM_REASON_FAILSAFE_PATTERN,
    REMOTE_DISARM_REASON_ARM_SEQUENCE
} remote_disarm_reason_t;

typedef struct
{
    float forward;
    float turn;
    float aux1;
    float aux2;

    uint16_t ch_raw[REMOTE_CHANNEL_NUM];

    remote_arm_state_t arm_state;
    remote_speed_mode_t speed_mode;
    remote_disarm_reason_t disarm_reason;

    uint8_t serial_online;
    uint8_t failsafe_detected;
    uint8_t channels_valid;
    uint8_t command_valid;
    uint8_t saw_sa_safe_after_boot;
    uint8_t ch1_ch2_centered;
} remote_cmd_t;

extern volatile remote_cmd_t remote_cmd;

void remote_control_init(void);
void remote_control_set_motor_feedback_online(uint8_t left_online, uint8_t right_online);
void remote_control_update(void);
uint8_t remote_control_get(remote_cmd_t *out);

#ifdef __cplusplus
}
#endif

#endif
