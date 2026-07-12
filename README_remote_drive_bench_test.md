# Remote drive bench test

This stage is the first suspended low-speed closed-loop test for the existing left and right C620/M3508 wheel pair.

Do not run this test with the chassis on the ground. Lift the chassis, keep both drive wheels free in the air, keep people clear of the wheels, and secure all wiring before enabling drive output.

## Safety switches

The drive gate is configured in `MDK-ARM/bsp/remote_drive_config.h`:

```c
#define REMOTE_DRIVE_ENABLE               1
#define REMOTE_DRIVE_BENCH_TEST_ENABLE    1
```

`REMOTE_DRIVE_ENABLE = 0` keeps the firmware in zero-current mode. CAN and PID can be initialized, but the drive gate stays closed and all C620 current commands are zero.

Only after the chassis is lifted, `SC` is in `LOW`, and `SA` is in `DISARM`, change:

```c
#define REMOTE_DRIVE_ENABLE               1
```

## Existing wheel mapping

The current project keeps the existing left/right wheel slots:

```c
#define LEFT_WHEEL_SLOT                   0U
#define RIGHT_WHEEL_SLOT                  2U
#define LEFT_WHEEL_DIR                    (-1.0f)
#define RIGHT_WHEEL_DIR                   (+1.0f)
```

These signs preserve the old fixed-speed test relationship: the old left slot used negative rpm and the old right slot used positive rpm. If the suspended test shows the forward direction is wrong, only change `LEFT_WHEEL_DIR` or `RIGHT_WHEEL_DIR`. Do not swap motor IDs, CAN IDs, slots, or wiring.

## Remote speed limits

```c
#define REMOTE_RPM_LIMIT_LOW              120.0f
#define REMOTE_RPM_LIMIT_MID              240.0f
#define REMOTE_RPM_LIMIT_HIGH             360.0f
#define REMOTE_BENCH_CURRENT_LIMIT        1500
#define REMOTE_RPM_RAMP_PER_SEC           300.0f
```

In `HIGH`, full forward stick now restores the original fixed-speed test magnitude of about 360 rpm, while keeping the current clamp and all drive gates. PID output is clamped again before CAN current is sent.

## Drive gate

The firmware only drives when all conditions are true:

```text
REMOTE_DRIVE_ENABLE == 1
remote_cmd.command_valid == 1
remote_cmd.arm_state == REMOTE_ARMED
remote_cmd.serial_online == 1
```

Any DISARM, serial timeout, FailSafe, invalid channel, `REMOTE_DRIVE_ENABLE = 0`, or `IBUS_DIAGNOSTIC_ONLY != 0` immediately clears targets, resets dynamic PID state, and sends zero current. C620 feedback online flags are printed only as diagnostics and do not close the drive gate.

## LED drive status

The firmware uses the existing board LED `LED1` on `PF14`. The LED is driven by a non-blocking state machine in the FreeRTOS control loop. `LED1` is written as active-low by `REMOTE_STATUS_LED_ON_LEVEL`.

Priority is:

```text
drive disabled
iBUS fault
boot wait for SA safe
ARM blocked by CH1/CH2
normal DISARM
drive gate open
```

| LED pattern | Status | Field action |
| --- | --- | --- |
| Off | `REMOTE_DRIVE_ENABLE == 0` or `IBUS_DIAGNOSTIC_ONLY != 0` | Firmware is not allowed to drive. Keep checking software macros. |
| Fast blink, about 4 Hz | iBUS serial fault | Check receiver power, iBUS signal wire on PD6, and common ground. |
| Double flash, then pause | boot wait for SA safe | Move `SA` up and hold it for at least 300 ms. |
| Triple short flash, then pause | ARM blocked by CH1/CH2 | Center the right stick left/right and up/down, then redo `SA` up to down. |
| Slow blink, about 1 Hz | normal DISARM | Safe lock is active; motors must not move. |
| Solid on | `drive_gate_open == 1` | PID and CAN output are actually allowed for the left and right wheel pair. |

With USB-TTL connected, the matching low-rate line is:

```text
LED_STATUS status=ARMED_GATE_OPEN gate=1 arm=1 block=NONE sa_seen=1 ch12_center=1 serial=1 motorL_diag=0 motorR_diag=0 enable=1 diag=0
```

## Expected serial output

With `REMOTE_DRIVE_ENABLE = 0`:

```text
DRIVE gate=0 arm=DISARM serial=1 mode=LOW fwd=0.00 turn=0.00 Ltar=0.0 Rtar=0.0 Lrpm=0 Rrpm=0 Liq=0 Riq=0 motorL_diag=0 motorR_diag=0 reason=SA_SAFE
```

With drive enabled, armed, and sticks centered:

```text
DRIVE gate=1 arm=ARMED serial=1 mode=LOW fwd=0.00 turn=0.00 Ltar=0.0 Rtar=0.0 Lrpm=0 Rrpm=0 Liq=0 Riq=0 motorL_diag=0 motorR_diag=0 reason=NONE
```

With full forward command in `HIGH`:

```text
DRIVE gate=1 arm=ARMED serial=1 mode=HIGH fwd=1.00 turn=0.00 Ltar=-360.0 Rtar=360.0 Lrpm=... Rrpm=... Liq=... Riq=... motorL_diag=0 motorR_diag=0 reason=NONE
```

## Test order

1. Lift the chassis so both drive wheels are off the ground.
2. Before power-on, put `SA` up. `SC` may stay up for `LOW`.
3. Power on and observe the LED first.
4. Double flash means move `SA` up and hold it.
5. Slow blink means normal `DISARM`.
6. Triple short flash means CH1/CH2 are not centered.
7. Fast blink means iBUS data is abnormal.
8. When the LED is solid on, move `SC` down for `HIGH`, then gently push the right stick up/down.
9. Move `SA` up again; the LED must return to slow blink and the motors must stop immediately.

If forward direction is wrong, adjust only `LEFT_WHEEL_DIR` or `RIGHT_WHEEL_DIR`.
