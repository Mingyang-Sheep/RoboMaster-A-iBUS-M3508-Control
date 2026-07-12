# C620 and M3508 Control

The current motor path uses C620 CAN current commands and M3508 speed feedback.

## CAN and motor IDs

| Item | Current implementation |
| --- | --- |
| CAN bitrate | 1 Mbps from the current CubeMX timing calculation |
| CAN1 pins | `PD0 / CAN1_RX`, `PD1 / CAN1_TX` |
| CAN2 pins | `PB12 / CAN2_RX`, `PB13 / CAN2_TX` |
| C620 current command for motors 1-4 | standard ID `0x200` |
| C620 current command for motors 5-8 | standard ID `0x1FF`, currently sent as zeros |
| M3508 feedback IDs handled | `0x201` through `0x204` |
| Left wheel feedback slot | slot `0`, feedback ID `0x201` |
| Right wheel feedback slot | slot `2`, feedback ID `0x203` |

The current command values are C620 protocol raw values, not ampere values.

## Differential mixing

The drive task uses the following current formula:

```text
left_mix  = clamp(forward + turn, -1.0, 1.0)
right_mix = clamp(forward - turn, -1.0, 1.0)
```

Wheel direction signs are:

```c
#define LEFT_WHEEL_DIR   (-1.0f)
#define RIGHT_WHEEL_DIR  (+1.0f)
```

The ramped wheel targets are calculated from the selected speed mode and `REMOTE_RPM_RAMP_PER_SEC = 300.0f`.

## PID and current limits

| Parameter | Current value |
| --- | ---: |
| PID mode | position PID |
| `WHEEL_PID_MAX_OUTPUT` | `14500` |
| `WHEEL_PID_I_LIMIT` | `4800` |
| `WHEEL_PID_KP` | `1.5f` |
| `WHEEL_PID_KI` | `0.1f` |
| `WHEEL_PID_KD` | `0.0f` |
| current command clamp | `+/-14500` raw C620 command units |
| motor feedback diagnostic timeout | `100 ms` |

Motor feedback online flags are printed as diagnostics. They are not part of the current drive gate.
