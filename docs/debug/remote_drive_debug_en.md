# Remote Drive Debug Notes (English)

This file is a field debug note, not the final project README.

## 1. Debug Goal and Current Architecture

```text
FS-i6X transmitter
  -> FS-iA10B receiver
  -> iBUS
  -> Type-A board STM32 USART2 RX
  -> CH1/CH2/CH7/CH9 mapping
  -> left/right wheel speed targets
  -> speed PID
  -> CAN
  -> C620
  -> M3508
  -> left/right screw propulsion wheels
```

The current stage controls only the existing left and right C620/M3508 pair. The third and fourth motor targets and currents stay at 0.

## 2. Transmitter Channel Map

| Channel | Control | Current use |
| --- | --- | --- |
| CH1 | right stick horizontal | `turn` |
| CH2 | right stick vertical | `forward` |
| CH3 | left stick vertical | `aux1`, reserved |
| CH4 | left stick horizontal | `aux2`, reserved |
| CH5 | left knob | reserved |
| CH6 | right knob | reserved |
| CH7 | SA | ARM / DISARM |
| CH8 | SB | reserved |
| CH9 | SC | LOW / MID / HIGH |
| CH10 | SD | reserved |

```text
SA up, about 1000: DISARM
SA down, about 2000: ARM request

SC up, about 1000: LOW
SC middle, about 1500: MID
SC down, about 2000: HIGH
```

Stick channel normalization:

```text
1000 -> -1.0
1500 ->  0.0
2000 -> +1.0
```

`REMOTE_STICK_DEADZONE = 0.05f`. Values inside the deadzone are forced to zero; values outside it keep the linear stick scale. Full stick still reaches `-1.0` / `+1.0`.

## 3. Current Comparison Calibration and Speed Units

```text
Original zip fixed-speed test:
left wheel -360, right wheel +360.

Current remote HIGH at full stick:
left wheel -400, right wheel +400.

Current goal:
full-stick remote control should feel slightly stronger than the original fixed ±360 test,
not run at the motor nameplate maximum speed.

LOW  = 160
MID  = 280
HIGH = 400
```

At full stick, the command reaches the selected speed limit. The actual speed still depends on supply voltage, load, screw propulsion resistance, PID tuning, and current limiting.

The current speed numbers are the control units used by the original C620 feedback/PID chain. They are used to reproduce the original demo's feel and motion behavior, and must not be directly interpreted as final mechanical output-shaft rpm.

`set_spd[]`, `moto_chassis[].speed_rpm`, and the PID target all use this original control unit. The original demo's `-360 / +360` values were direct speed setpoints written to `set_spd[]`; there was no gear-ratio conversion.

## 4. Current Code Direction Table

Current direction macros:

```c
#define REMOTE_FORWARD_DIR  (+1.0f)
#define REMOTE_TURN_DIR     (+1.0f)
#define LEFT_WHEEL_DIR      (-1.0f)
#define RIGHT_WHEEL_DIR     (+1.0f)
```

Current code mixing formula:

```c
forward_cmd = REMOTE_FORWARD_DIR * forward;
turn_cmd    = REMOTE_TURN_DIR    * turn;

left_mix_raw  = forward_cmd + turn_cmd;
right_mix_raw = forward_cmd - turn_cmd;

mix_scale = max(1.0f, abs(left_mix_raw), abs(right_mix_raw));

left_mix  = left_mix_raw  / mix_scale;
right_mix = right_mix_raw / mix_scale;

left_target_pid_speed  = LEFT_WHEEL_DIR  * left_mix  * selected_pid_speed_limit;
right_target_pid_speed = RIGHT_WHEEL_DIR * right_mix * selected_pid_speed_limit;
```

| Operation | forward | turn | left target in HIGH | right target in HIGH | provisional code meaning | field result | confirmed |
| --- | ---: | ---: | ---: | ---: | --- | --- | --- |
| right stick up | +1 | 0 | -400 | +400 | provisional forward, pending field verification | TBD | no |
| right stick down | -1 | 0 | +400 | -400 | provisional reverse, pending field verification | TBD | no |
| right stick right | 0 | +1 | -400 | -400 | provisional right turn, pending field verification | TBD | no |
| right stick left | 0 | -1 | +400 | +400 | provisional left turn, pending field verification | TBD | no |

Direction correction order:

1. If pushing the right stick up makes the vehicle move backward, change the sign of `REMOTE_FORWARD_DIR` first.
2. If pushing the right stick right makes the vehicle turn the opposite way, change the sign of `REMOTE_TURN_DIR` first.
3. If only one screw propulsion wheel is inconsistent with the other side, inspect that wheel's mounting direction, screw handedness, and motor direction before changing `LEFT_WHEEL_DIR` or `RIGHT_WHEEL_DIR`.
4. Do not fix software direction by swapping CAN IDs, motor slots, or C620 wiring.

## 5. Screw Propulsion Direction Check Procedure

1. Define the vehicle forward direction.
2. Define the viewing angle used to observe each wheel rotation, for example from the outside of the vehicle looking toward the wheel center.
3. Record the left and right screw handedness and mounting direction.
4. In LOW mode, test right stick up, down, left, and right.
5. Record the actual rotation direction of both wheels.
6. Record whether the whole vehicle tends to move forward, reverse, left, right, or yaw unexpectedly.
7. Decide whether to change `REMOTE_FORWARD_DIR`, `REMOTE_TURN_DIR`, `LEFT_WHEEL_DIR`, or `RIGHT_WHEEL_DIR`.
8. Do not treat forward/reverse/left/right as final conclusions before field verification.

## 6. LED Status Table

| LED behavior | current code state | meaning | field action |
| --- | --- | --- | --- |
| off | `REMOTE_DRIVE_ENABLE == 0` or `IBUS_DIAGNOSTIC_ONLY != 0` | drive software disabled | check `REMOTE_DRIVE_ENABLE` / `IBUS_DIAGNOSTIC_ONLY` |
| fast blink | iBUS serial offline / RC link issue | receiver or iBUS link abnormal | check receiver, power, PD6, shared ground, and FailSafe |
| double blink with pause | boot SA-safe sequence not completed | SA-up safe position has not been observed | move SA up and hold |
| triple short blink | CH1/CH2 not centered, ARM rejected | stick not centered | center the right stick, then move SA up and down |
| slow blink | normal DISARM | drive not released | check SA state |
| solid on | `remote_cmd.arm_state == REMOTE_ARMED` and `drive_gate_open == 1` | left/right wheel drive allowed | proceed with LOW-mode checks |

C620 feedback online status is only reported as `motorL_diag` / `motorR_diag`. It is no longer an LED solid-on condition or a drive gate condition.

## 7. Debug Record Template

```text
Date:
Firmware configuration:
  IBUS_DIAGNOSTIC_ONLY =
  REMOTE_DRIVE_ENABLE =
  REMOTE_PID_SPEED_LOW =
  REMOTE_PID_SPEED_MID =
  REMOTE_PID_SPEED_HIGH =
  REMOTE_TARGET_RAMP_ENABLE =
  REMOTE_C620_CURRENT_RAW_LIMIT =

Direction macros:
  REMOTE_FORWARD_DIR =
  REMOTE_TURN_DIR =
  LEFT_WHEEL_DIR =
  RIGHT_WHEEL_DIR =

Test environment:
  Supply voltage:
  Wheels lifted:
  Screw propulsion wheels installed:
  Screw handedness / mounting direction:

Test action:
  SC mode:
  SA state:
  right stick action:
  DRIVE log line:
  left wheel rotation:
  right wheel rotation:
  whole-vehicle motion tendency:
  abnormal behavior:
  next action:
```
