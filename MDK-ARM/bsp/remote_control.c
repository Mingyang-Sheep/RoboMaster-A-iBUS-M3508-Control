#include "remote_control.h"
#include "bsp_ibus.h"
#include "remote_drive_config.h"

volatile remote_cmd_t remote_cmd;

static remote_arm_state_t remote_arm_state;
static remote_disarm_reason_t remote_disarm_reason;
static uint8_t remote_valid_tracking;
static uint32_t remote_valid_since_ms;
static uint32_t remote_failsafe_last_valid_frame_count;
static uint16_t remote_failsafe_stable_frames;
static uint8_t remote_sa_safe_ready;
static uint8_t remote_saw_sa_safe_after_boot;
static uint8_t remote_sa_safe_timing;
static uint8_t remote_last_arm_request;
static uint32_t remote_sa_safe_since_ms;

static float remote_clamp_float(float value, float min_value, float max_value)
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

static uint16_t remote_clamp_channel(uint16_t value, uint16_t min_value, uint16_t max_value)
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

static uint8_t remote_channel_in_range(uint16_t value)
{
    return (value >= REMOTE_CHANNEL_VALID_MIN && value <= REMOTE_CHANNEL_VALID_MAX) ? 1U : 0U;
}

static float remote_map_channel(uint16_t value, float direction)
{
    uint16_t clipped = remote_clamp_channel(value, REMOTE_CHANNEL_MIN, REMOTE_CHANNEL_MAX);
    float mapped = ((float)clipped - (float)REMOTE_CHANNEL_CENTER) /
                   ((float)REMOTE_CHANNEL_MAX - (float)REMOTE_CHANNEL_CENTER);

    mapped = remote_clamp_float(mapped, -1.0f, 1.0f);
    if(mapped > -REMOTE_STICK_DEADZONE && mapped < REMOTE_STICK_DEADZONE)
    {
        mapped = 0.0f;
    }

    mapped *= direction;
    return remote_clamp_float(mapped, -1.0f, 1.0f);
}

static uint8_t remote_stick_is_centered(uint16_t value)
{
    return (remote_map_channel(value, +1.0f) == 0.0f) ? 1U : 0U;
}

static remote_speed_mode_t remote_speed_mode_from_channel(uint16_t value)
{
    if(value < REMOTE_SWITCH_LOW_THRESHOLD)
    {
        return REMOTE_SPEED_LOW;
    }
    if(value > REMOTE_SWITCH_HIGH_THRESHOLD)
    {
        return REMOTE_SPEED_HIGH;
    }
    return REMOTE_SPEED_MID;
}

static void remote_copy_raw_channels(remote_cmd_t *cmd, const ibus_state_t *state)
{
    uint8_t i;

    for(i = 0U; i < REMOTE_CHANNEL_NUM; i++)
    {
        cmd->ch_raw[i] = state->channel[i];
    }
}

static uint8_t remote_channels_are_valid(const ibus_state_t *state)
{
    if(remote_channel_in_range(state->channel[REMOTE_CH_TURN_INDEX]) == 0U)
    {
        return 0U;
    }
    if(remote_channel_in_range(state->channel[REMOTE_CH_FORWARD_INDEX]) == 0U)
    {
        return 0U;
    }
    if(remote_channel_in_range(state->channel[REMOTE_CH_AUX1_INDEX]) == 0U)
    {
        return 0U;
    }
    if(remote_channel_in_range(state->channel[REMOTE_CH_AUX2_INDEX]) == 0U)
    {
        return 0U;
    }
    if(remote_channel_in_range(state->channel[REMOTE_CH_ARM_INDEX]) == 0U)
    {
        return 0U;
    }
    if(remote_channel_in_range(state->channel[REMOTE_CH_SPEED_INDEX]) == 0U)
    {
        return 0U;
    }
    return 1U;
}

static uint8_t remote_sticks_are_centered(const ibus_state_t *state)
{
    return (remote_stick_is_centered(state->channel[REMOTE_CH_TURN_INDEX]) != 0U &&
            remote_stick_is_centered(state->channel[REMOTE_CH_FORWARD_INDEX]) != 0U) ? 1U : 0U;
}

static uint8_t remote_failsafe_pattern_update(const ibus_state_t *state)
{
#if REMOTE_FAILSAFE_PATTERN_ENABLE
    if(state->valid_frame_count != remote_failsafe_last_valid_frame_count)
    {
        remote_failsafe_last_valid_frame_count = state->valid_frame_count;
        if(state->channel[REMOTE_CH_ARM_INDEX] < REMOTE_FAILSAFE_CH7_SAFE_MAX)
        {
            if(remote_failsafe_stable_frames < REMOTE_FAILSAFE_STABLE_FRAMES)
            {
                remote_failsafe_stable_frames++;
            }
        }
        else
        {
            remote_failsafe_stable_frames = 0U;
        }
    }
    return (remote_failsafe_stable_frames >= REMOTE_FAILSAFE_STABLE_FRAMES) ? 1U : 0U;
#else
    remote_failsafe_last_valid_frame_count = state->valid_frame_count;
    remote_failsafe_stable_frames = 0U;
    return 0U;
#endif
}

static void remote_write_cmd(const remote_cmd_t *cmd)
{
    uint8_t i;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    remote_cmd.forward = cmd->forward;
    remote_cmd.turn = cmd->turn;
    remote_cmd.aux1 = cmd->aux1;
    remote_cmd.aux2 = cmd->aux2;
    for(i = 0U; i < REMOTE_CHANNEL_NUM; i++)
    {
        remote_cmd.ch_raw[i] = cmd->ch_raw[i];
    }
    remote_cmd.arm_state = cmd->arm_state;
    remote_cmd.speed_mode = cmd->speed_mode;
    remote_cmd.disarm_reason = cmd->disarm_reason;
    remote_cmd.serial_online = cmd->serial_online;
    remote_cmd.failsafe_detected = cmd->failsafe_detected;
    remote_cmd.channels_valid = cmd->channels_valid;
    remote_cmd.command_valid = cmd->command_valid;
    remote_cmd.saw_sa_safe_after_boot = cmd->saw_sa_safe_after_boot;
    remote_cmd.ch1_ch2_centered = cmd->ch1_ch2_centered;
    if(primask == 0U)
    {
        __enable_irq();
    }
}

void remote_control_init(void)
{
    uint8_t i;
    remote_cmd_t init_cmd;

    remote_arm_state = REMOTE_DISARM;
    remote_disarm_reason = REMOTE_DISARM_REASON_BOOT;
    remote_valid_tracking = 0U;
    remote_valid_since_ms = 0U;
    remote_failsafe_last_valid_frame_count = 0U;
    remote_failsafe_stable_frames = 0U;
    remote_sa_safe_ready = 0U;
    remote_saw_sa_safe_after_boot = 0U;
    remote_sa_safe_timing = 0U;
    remote_last_arm_request = 0U;
    remote_sa_safe_since_ms = 0U;

    init_cmd.forward = 0.0f;
    init_cmd.turn = 0.0f;
    init_cmd.aux1 = 0.0f;
    init_cmd.aux2 = 0.0f;
    for(i = 0U; i < REMOTE_CHANNEL_NUM; i++)
    {
        init_cmd.ch_raw[i] = REMOTE_CHANNEL_CENTER;
    }
    init_cmd.arm_state = REMOTE_DISARM;
    init_cmd.speed_mode = REMOTE_SPEED_LOW;
    init_cmd.disarm_reason = REMOTE_DISARM_REASON_BOOT;
    init_cmd.serial_online = 0U;
    init_cmd.failsafe_detected = 0U;
    init_cmd.channels_valid = 0U;
    init_cmd.command_valid = 0U;
    init_cmd.saw_sa_safe_after_boot = 0U;
    init_cmd.ch1_ch2_centered = 0U;
    remote_write_cmd(&init_cmd);
}

void remote_control_set_motor_feedback_online(uint8_t left_online, uint8_t right_online)
{
    (void)left_online;
    (void)right_online;
}

void remote_control_update(void)
{
    ibus_state_t ibus_snapshot;
    remote_cmd_t next_cmd;
    uint32_t now = HAL_GetTick();
    uint8_t stream_ok;
    uint8_t sticks_centered = 0U;
    uint8_t arm_request;
    float mapped_forward;
    float mapped_turn;
    float mapped_aux1;
    float mapped_aux2;

    ibus_poll_link_state();
    (void)ibus_get_state(&ibus_snapshot);
    arm_request = (ibus_snapshot.channel[REMOTE_CH_ARM_INDEX] > REMOTE_SWITCH_HIGH_THRESHOLD) ? 1U : 0U;

    next_cmd.serial_online = ibus_snapshot.serial_online;
    next_cmd.channels_valid = remote_channels_are_valid(&ibus_snapshot);
    next_cmd.failsafe_detected = 0U;
    remote_copy_raw_channels(&next_cmd, &ibus_snapshot);
    if(next_cmd.channels_valid != 0U)
    {
        sticks_centered = remote_sticks_are_centered(&ibus_snapshot);
    }

    mapped_forward = remote_map_channel(ibus_snapshot.channel[REMOTE_CH_FORWARD_INDEX],
                                        REMOTE_FORWARD_DIR);
    mapped_turn = remote_map_channel(ibus_snapshot.channel[REMOTE_CH_TURN_INDEX],
                                     REMOTE_TURN_DIR);
    mapped_aux1 = remote_map_channel(ibus_snapshot.channel[REMOTE_CH_AUX1_INDEX],
                                     REMOTE_AUX1_DIR);
    mapped_aux2 = remote_map_channel(ibus_snapshot.channel[REMOTE_CH_AUX2_INDEX],
                                     REMOTE_AUX2_DIR);
    next_cmd.speed_mode = (next_cmd.channels_valid != 0U) ?
        remote_speed_mode_from_channel(ibus_snapshot.channel[REMOTE_CH_SPEED_INDEX]) :
        REMOTE_SPEED_LOW;

    stream_ok = (next_cmd.serial_online != 0U &&
                 next_cmd.channels_valid != 0U &&
                 ibus_snapshot.valid_frame_count != 0U) ? 1U : 0U;

    if(stream_ok != 0U)
    {
        if(remote_valid_tracking == 0U)
        {
            remote_valid_tracking = 1U;
            remote_valid_since_ms = now;
        }
    }
    else
    {
        remote_valid_tracking = 0U;
        remote_valid_since_ms = 0U;
        remote_arm_state = REMOTE_DISARM;
        remote_sa_safe_ready = 0U;
        remote_sa_safe_timing = 0U;
        remote_last_arm_request = 0U;
    }

    if(next_cmd.serial_online == 0U)
    {
        remote_valid_tracking = 0U;
        remote_valid_since_ms = 0U;
        remote_arm_state = REMOTE_DISARM;
        remote_disarm_reason = REMOTE_DISARM_REASON_IBUS_SERIAL_TIMEOUT;
        remote_sa_safe_ready = 0U;
        remote_sa_safe_timing = 0U;
        remote_last_arm_request = 0U;
    }
    else if(next_cmd.channels_valid == 0U)
    {
        remote_valid_tracking = 0U;
        remote_valid_since_ms = 0U;
        remote_arm_state = REMOTE_DISARM;
        remote_disarm_reason = REMOTE_DISARM_REASON_INVALID_CHANNEL;
        remote_sa_safe_ready = 0U;
        remote_sa_safe_timing = 0U;
        remote_last_arm_request = 0U;
    }
    else
    {
        next_cmd.failsafe_detected = remote_failsafe_pattern_update(&ibus_snapshot);

        if(next_cmd.failsafe_detected != 0U)
        {
            remote_arm_state = REMOTE_DISARM;
            remote_disarm_reason = REMOTE_DISARM_REASON_FAILSAFE_PATTERN;
            remote_sa_safe_ready = 0U;
            remote_sa_safe_timing = 0U;
            remote_last_arm_request = 0U;
        }
        else if(ibus_snapshot.channel[REMOTE_CH_ARM_INDEX] < REMOTE_SWITCH_LOW_THRESHOLD)
        {
            if(remote_last_arm_request != 0U || remote_sa_safe_timing == 0U)
            {
                remote_sa_safe_since_ms = now;
                remote_sa_safe_timing = 1U;
                remote_sa_safe_ready = 0U;
            }
            if(remote_sa_safe_ready == 0U &&
               (uint32_t)(now - remote_sa_safe_since_ms) >= REMOTE_SA_SAFE_REQUIRED_MS)
            {
                remote_sa_safe_ready = 1U;
                remote_saw_sa_safe_after_boot = 1U;
            }
            remote_last_arm_request = 0U;
            remote_arm_state = REMOTE_DISARM;
            remote_disarm_reason = REMOTE_DISARM_REASON_SA_SAFE;
        }
        else if(arm_request != 0U)
        {
            remote_sa_safe_timing = 0U;
            if(remote_arm_state != REMOTE_ARMED)
            {
                remote_disarm_reason = REMOTE_DISARM_REASON_ARM_SEQUENCE;
            }
            if(remote_arm_state == REMOTE_ARMED)
            {
                remote_arm_state = REMOTE_ARMED;
            }
            else if(remote_last_arm_request == 0U &&
                    remote_sa_safe_ready != 0U &&
                    sticks_centered != 0U &&
                    remote_valid_tracking != 0U &&
                    (uint32_t)(now - remote_valid_since_ms) >= REMOTE_ARM_STABLE_MS)
            {
                remote_arm_state = REMOTE_ARMED;
                remote_sa_safe_ready = 0U;
            }
            remote_last_arm_request = 1U;
        }
        else if(remote_arm_state != REMOTE_ARMED)
        {
            remote_sa_safe_timing = 0U;
            remote_last_arm_request = 0U;
            remote_disarm_reason = REMOTE_DISARM_REASON_ARM_SEQUENCE;
        }
    }

    next_cmd.arm_state = remote_arm_state;
    next_cmd.disarm_reason = remote_disarm_reason;
    next_cmd.saw_sa_safe_after_boot = remote_saw_sa_safe_after_boot;
    next_cmd.ch1_ch2_centered = sticks_centered;
    next_cmd.command_valid = (next_cmd.arm_state == REMOTE_ARMED &&
                              next_cmd.serial_online != 0U &&
                              next_cmd.channels_valid != 0U &&
                              next_cmd.failsafe_detected == 0U) ? 1U : 0U;

    if(next_cmd.command_valid != 0U)
    {
        next_cmd.forward = mapped_forward;
        next_cmd.turn = mapped_turn;
        next_cmd.aux1 = mapped_aux1;
        next_cmd.aux2 = mapped_aux2;
    }
    else
    {
        next_cmd.forward = 0.0f;
        next_cmd.turn = 0.0f;
        next_cmd.aux1 = 0.0f;
        next_cmd.aux2 = 0.0f;
    }

    remote_write_cmd(&next_cmd);
}

uint8_t remote_control_get(remote_cmd_t *out)
{
    uint8_t i;
    uint32_t primask;

    if(out == 0)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    out->forward = remote_cmd.forward;
    out->turn = remote_cmd.turn;
    out->aux1 = remote_cmd.aux1;
    out->aux2 = remote_cmd.aux2;
    for(i = 0U; i < REMOTE_CHANNEL_NUM; i++)
    {
        out->ch_raw[i] = remote_cmd.ch_raw[i];
    }
    out->arm_state = remote_cmd.arm_state;
    out->speed_mode = remote_cmd.speed_mode;
    out->disarm_reason = remote_cmd.disarm_reason;
    out->serial_online = remote_cmd.serial_online;
    out->failsafe_detected = remote_cmd.failsafe_detected;
    out->channels_valid = remote_cmd.channels_valid;
    out->command_valid = remote_cmd.command_valid;
    out->saw_sa_safe_after_boot = remote_cmd.saw_sa_safe_after_boot;
    out->ch1_ch2_centered = remote_cmd.ch1_ch2_centered;
    if(primask == 0U)
    {
        __enable_irq();
    }

    return 1U;
}
