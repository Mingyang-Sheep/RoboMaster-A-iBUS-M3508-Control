# FS-iA10B iBUS monitor

This stage is only for observing FS-i6X / FS-iA10B iBUS data through the RoboMaster A board BTUART. It does not control any C620 or M3508 motor.

## Wiring

| Signal | Connect to |
| --- | --- |
| FS-iA10B iBUS signal | A board `R / PD6 / USART2_RX` |
| FS-iA10B GND | A board `PGND` |
| FS-iA10B VCC | A board `5V_U` |
| A board `T / PD5 / USART2_TX` | USB-TTL module `RX` |
| A board `PGND` | USB-TTL module `GND` |
| USB-TTL VCC | Do not connect |

Do not connect iBUS to the DBUS port or to `T / PD5`. The USB-TTL adapter must use 3.3 V logic. Leave USB-TTL VCC disconnected to avoid back-powering the board. After BTUART is used for iBUS monitoring, do not connect the Bluetooth module to the same BTUART port.

If the measured iBUS high level is close to 5 V, add level shifting or a resistor divider before connecting it to `PD6`.

## Receiver and UART setup

Set the receiver output mode to iBUS output, not sensor or telemetry output.

The current project maps BTUART as:

```text
PD5 = USART2_TX
PD6 = USART2_RX
USART2 = 115200 baud, 8 data bits, no parity, 1 stop bit, full duplex
USART2 RX DMA = enabled
```

Serial assistant settings:

```text
115200
8N1
No flow control
```

If no checksum-valid frame is ever received, do not change everything at once. First check wiring and receiver mode, then later compare by changing only the USART2 stop bit from 1 to 2.

## Firmware mode

`IBUS_DIAGNOSTIC_ONLY` is set in `MDK-ARM/bsp/remote_drive_config.h`. When set to `1`, the FreeRTOS default task only starts iBUS receiving, remote command mapping, and USART2 debug printing. It does not run motor PID control and does not send nonzero CAN current.

Debug print macros:

```c
#define IBUS_DIAGNOSTIC_ONLY   0
#define IBUS_PRINT_RAW_ENABLE  1
#define IBUS_PRINT_RATE_HZ     5
```

## VSCode watch variables

- `ibus_state.channel[0]` through `ibus_state.channel[9]`
- `ibus_state.valid_frame_count`
- `ibus_state.checksum_error_count`
- `ibus_state.frame_error_count`
- `ibus_state.serial_online`
- `ibus_state.serial_link_lost_event`
- `ibus_state.serial_link_recovered_event`
- `ibus_state.online`
- `remote_cmd.forward`
- `remote_cmd.turn`
- `remote_cmd.aux1`
- `remote_cmd.aux2`
- `remote_cmd.arm_state`
- `remote_cmd.speed_mode`
- `remote_cmd.disarm_reason`
- `remote_cmd.serial_online`
- `remote_cmd.failsafe_detected`
- `remote_cmd.command_valid`

`serial_online` only means the A board is still receiving checksum-valid iBUS serial frames from the receiver. It does not prove the wireless link between the transmitter and receiver is online. If the receiver keeps outputting valid iBUS frames after the transmitter is turned off, `serial_online` can remain `1`.

## Remote command mapping

| iBUS channel | Transmitter control | Command field |
| --- | --- | --- |
| CH1 / `channel[0]` | Right stick left/right | `turn` |
| CH2 / `channel[1]` | Right stick up/down | `forward` |
| CH3 / `channel[2]` | Left stick up/down | `aux1` reserved |
| CH4 / `channel[3]` | Left stick left/right | `aux2` reserved |
| CH7 / `channel[6]` | SA switch | `arm_state` |
| CH9 / `channel[8]` | SC switch | `speed_mode` |

CH5, CH6, CH8, and CH10 are kept in `ch_raw[]` for observation only.

Stick normalization:

```text
1000 -> -1.0
1500 ->  0.0
2000 -> +1.0
```

`SA` up is about 1000 and means `DISARM`; `SA` down is about 2000 and requests `ARMED`. The firmware only enters `ARMED` when iBUS serial frames are online, valid frames have been stable for at least 500 ms, and CH1 / CH2 are centered.

Failsafe pattern detection is reserved but disabled by default:

```c
#define REMOTE_FAILSAFE_PATTERN_ENABLE       0
#define REMOTE_FAILSAFE_CH7_SAFE_MAX         1300U
#define REMOTE_FAILSAFE_STABLE_FRAMES        5U
```

Regardless of that switch, `CH7 < 1300` always forces `DISARM`. Keep `REMOTE_FAILSAFE_PATTERN_ENABLE` at `0` until the receiver's real transmitter-off output is observed through `RC LINK PROBE`.

`SC` maps to speed mode only for printing:

```text
CH9 < 1300 -> LOW
CH9 > 1700 -> HIGH
otherwise -> MID
```

## Expected serial output

Heartbeat output every 3 seconds:

```text
HEARTBEAT UART2 TX OK
```

Command mapping output at 5 Hz:

```text
CMD arm=DISARM valid=0 serial_online=1 failsafe=0 reason=SA_SAFE mode=LOW forward=0.00 turn=0.00 aux1=0.00 aux2=0.00 raw=[1500,1500,1500,1500,2000,2000,1000,1000,1000,1000]
CMD arm=ARMED valid=1 serial_online=1 failsafe=0 reason=NONE mode=HIGH forward=0.82 turn=-0.35 aux1=0.00 aux2=0.00 raw=[1325,1910,1500,1500,2000,2000,2000,1000,2000,1000]
```

ARM block and failsafe outputs are rate-limited to at most once per second:

```text
ARM BLOCKED: center sticks before SA down
ARM BLOCKED: wait iBUS stable 500ms
REMOTE DISARM: iBUS serial timeout
RC LINK PROBE: serial_online=1 raw=[1500,1500,1500,1500,2000,2000,2000,1000,2000,1000]
```

iBUS parsed output at 5 Hz:

```text
IBUS OK cnt=154 ch1=1500 ch2=1498 ch3=1000 ch4=2000 ch5=1000 ch6=1500 ch7=1500 ch8=1500 ch9=1500 ch10=1500 csum_err=0 frame_err=0
```

Raw valid frame output, at most once per second when enabled:

```text
RAW 20 40 DC 05 DC 05 E8 03 D0 07 E8 03 DC 05 DC 05 DC 05 DC 05 DC 05 DC 05 DC 05 DC 05 00 00 00 00 5A F3
```

Error output is rate-limited to at most once per second:

```text
IBUS ERR csum_err=3 frame_err=1 last_len=32
```

## Validation actions

1. Power on with `SA` in the up position; serial output must show `arm=DISARM`.
2. Center all sticks.
3. Put `SC` up and confirm `mode=LOW`.
4. Move `SA` down and confirm `arm=ARMED` and `valid=1` after the 500 ms stable-frame gate.
5. Move the right stick up/down and confirm `forward` changes between negative, zero, and positive values.
6. Move the right stick left/right and confirm `turn` changes between negative, zero, and positive values.
7. Move `SA` up and confirm the state immediately returns to `DISARM` and `valid=0`.
8. Unplug the iBUS signal or power off the receiver and confirm `REMOTE DISARM: iBUS serial timeout` appears after about 120 ms, followed by `serial_online=0`, `reason=IBUS_TIMEOUT`, and `valid=0`.
9. Turn off only the transmitter while keeping the receiver powered. Watch `RC LINK PROBE` and copy the full raw line. If the receiver still sends valid iBUS frames, `serial_online` may stay `1`; configure receiver FailSafe so CH7 goes to about 1000 on radio loss, then the firmware will DISARM through `SA_SAFE`.

## Link loss tests

Test 1: unplug iBUS signal or turn off receiver power.

Expected result:

```text
REMOTE DISARM: iBUS serial timeout
CMD arm=DISARM valid=0 serial_online=0 failsafe=0 reason=IBUS_TIMEOUT mode=HIGH forward=0.00 turn=0.00 raw=[...]
```

Test 2: turn off the transmitter but keep the receiver powered.

Expected result: observe `RC LINK PROBE`. If it keeps printing `serial_online=1`, the receiver is still sending legal iBUS frames. In that case serial timeout cannot detect radio loss; set the receiver's FailSafe so `CH7` becomes about `1000` when the transmitter is off.

## Troubleshooting order

1. Confirm the iBUS signal is connected to `R / PD6`, not `T / PD5`.
2. Confirm the receiver is actually in iBUS output mode.
3. Confirm receiver and A board share ground.
4. Confirm the iBUS voltage level is safe for the MCU RX pin.
5. Confirm USART2 is 115200 baud with no parity.
6. Only after the above checks, try changing USART2 from 1 stop bit to 2 stop bits for comparison.

## Next stage

After `forward`, `turn`, `SA`, and `SC` are confirmed stable and correctly oriented, the next stage can map `forward / turn` into the existing left and right wheel target slots through the current PID and CAN path. Keep `IBUS_DIAGNOSTIC_ONLY` enabled until direction, failsafe behavior, and switch mapping are verified.
