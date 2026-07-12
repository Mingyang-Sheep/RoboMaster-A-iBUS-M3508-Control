# RoboMaster A Board iBUS M3508 Control

English | [简体中文](README_zh.md)

## 1. Project Overview

This repository documents a conservative, reproducible remote-control chain:

```text
FS-i6X
  -> AFHDS 2A
FS-iA10B
  -> iBUS
DJI RoboMaster A Board
  -> CAN
C620 ESC
  -> M3508 Motor
```

The firmware receives FS-i6X/iA10B iBUS data, maps remote-control channels, applies a safety arming sequence, calculates differential left/right wheel targets, ramps the target RPM, closes a M3508 speed PID loop, and sends C620 CAN current commands. It is not a full RoboMaster competition chassis framework.

## 2. Features

- `USART2 + DMA + IDLE` iBUS reception.
- 32-byte iBUS frame header check, checksum verification, and 10-channel parsing.
- CH1/CH2/CH7/CH9 remote mapping, with CH3/CH4 parsed as reserved auxiliary values.
- SA safety arming and SC three-position speed mode.
- Stick normalization, deadzone handling, differential drive mixing, and RPM ramping.
- M3508 speed PID loop and C620 CAN current command output.
- Serial diagnostics and LED status indication.
- Serial timeout, invalid-channel handling, SA-safe disarming, and receiver failsafe guidance.

## 3. Hardware

| Hardware | Model | Purpose |
| --- | --- | --- |
| Development board | DJI RoboMaster A Board | Main controller |
| MCU | `STM32F427IIHx` | Keil target device |
| Transmitter | FlySky FS-i6X | Remote transmitter |
| Receiver | FlySky FS-iA10B | AFHDS 2A receiver with iBUS output |
| ESC | DJI C620 | M3508 motor controller |
| Motor | DJI M3508 | Left/right drive motor |
| Debug adapter | USB-TTL | Serial diagnostics |

## 4. System Architecture

```text
FS-i6X
  -> AFHDS 2A
FS-iA10B
  -> iBUS
USART2 + DMA + IDLE
  -> iBUS parser
  -> channel mapping
  -> safety arming state machine
  -> differential drive mixing
  -> RPM ramp
  -> speed PID loop
  -> CAN current command
  -> C620 + M3508
```

## 5. Repository Structure

```text
.
├── README.md
├── README_zh.md
├── demo.ioc
├── MDK-ARM/
│   ├── demo.uvprojx
│   ├── demo.uvoptx
│   ├── startup_stm32f427xx.s
│   └── bsp/
├── Inc/
├── Src/
├── Drivers/
├── Middlewares/
├── OBJ/
├── docs/
│   ├── wiring.md
│   ├── ibus_protocol.md
│   ├── channel_mapping.md
│   ├── safety_and_failsafe.md
│   ├── c620_m3508_control.md
│   ├── build_and_flash.md
│   ├── troubleshooting.md
│   └── archive/
└── c620电调使用程序说明书.pdf
```

`OBJ/` and `MDK-ARM/demo/` are existing build-output directories and are ignored for future changes. The C620 PDF at the repository root should be reviewed before public redistribution.

## 6. Wiring

See [docs/wiring.md](docs/wiring.md) for the detailed wiring notes.

| Signal | Current code configuration |
| --- | --- |
| iA10B iBUS signal | `PD6 / USART2_RX` |
| A board serial diagnostic TX | `PD5 / USART2_TX` |
| iBUS UART | `USART2`, `115200 8N1` |
| USART2 RX DMA | `DMA1_Stream5`, `DMA_CHANNEL_4` |
| CAN1 | `PD0 / CAN1_RX`, `PD1 / CAN1_TX` |
| CAN2 | `PB12 / CAN2_RX`, `PB13 / CAN2_TX` |

The receiver, A board, C620 ESCs, and USB-TTL adapter must share ground. Confirm the receiver supply and board connector pinout before powering the receiver. Do not connect an unverified 5 V signal directly to the MCU RX pin.

## 7. iBUS Configuration

See [docs/ibus_protocol.md](docs/ibus_protocol.md).

| Item | Current implementation |
| --- | --- |
| UART | `USART2 / huart2` |
| Serial format | `115200`, 8 data bits, no parity, 1 stop bit |
| Frame length | 32 bytes |
| Header | `0x20 0x40` |
| Checksum | `0xFFFF - sum(frame[0]..frame[29])` |
| Parsed channels | 10 |
| Serial timeout | 120 ms |

## 8. Channel Mapping

Code array indices start at zero, so `channel[0]` is CH1. See [docs/channel_mapping.md](docs/channel_mapping.md).

| Channel | Input | Current use |
| --- | --- | --- |
| CH1 / `channel[0]` | right stick horizontal | `turn` |
| CH2 / `channel[1]` | right stick vertical | `forward` |
| CH3 / `channel[2]` | left stick vertical | `aux1`, not used by the drive task |
| CH4 / `channel[3]` | left stick horizontal | `aux2`, not used by the drive task |
| CH7 / `channel[6]` | SA | arm/disarm |
| CH9 / `channel[8]` | SC | low/mid/high speed mode |

## 9. Safety Arming Sequence

See [docs/safety_and_failsafe.md](docs/safety_and_failsafe.md).

The current code requires an online iBUS stream, valid channel values, SA first held in the safe position for 300 ms, CH1/CH2 centered, and valid frames stable for 500 ms before moving SA to the run position can produce `REMOTE_ARMED`. Any disarmed state, serial timeout, invalid channel, or failsafe hit clears targets, resets dynamic PID state, and sends zero current.

## 10. Receiver Failsafe

> iBUS serial online is not the same as transmitter-to-receiver radio-link online. Some receivers may continue sending valid iBUS frames after the transmitter is turned off.

Configure FS-i6X/iA10B failsafe so radio loss drives CH7/SA back to the safe value. First tests must be done with wheels lifted or the mechanical load removed. `REMOTE_FAILSAFE_PATTERN_ENABLE` is currently `0`, so the optional pattern detector is disabled by default.

## 11. Differential Drive Mixing

Current mixing formula:

```text
left_mix  = clamp(forward + turn, -1.0, 1.0)
right_mix = clamp(forward - turn, -1.0, 1.0)
```

Wheel directions are corrected by `LEFT_WHEEL_DIR = -1.0f` and `RIGHT_WHEEL_DIR = +1.0f`. If field direction is wrong, adjust direction macros after a lifted-wheel test. Do not swap CAN IDs, slots, or C620 wiring just to fix a software sign.

## 12. C620 and M3508 Control

See [docs/c620_m3508_control.md](docs/c620_m3508_control.md).

| Item | Current implementation |
| --- | --- |
| CAN bitrate | 1 Mbps |
| C620 current command for motors 1-4 | standard ID `0x200` |
| C620 current command for motors 5-8 | standard ID `0x1FF`, currently sent as zero |
| M3508 feedback IDs | `0x201` to `0x204` |
| Left wheel slot | `0`, feedback ID `0x201` |
| Right wheel slot | `2`, feedback ID `0x203` |
| PID | position speed PID |
| Current clamp | `+/-14500` raw C620 protocol units |

The CAN current command values are protocol values, not ampere values.

## 13. Build and Flash

See [docs/build_and_flash.md](docs/build_and_flash.md).

- Keil project: `MDK-ARM/demo.uvprojx`
- Target: `demo`
- CubeMX file: `demo.ioc`
- Startup file: `MDK-ARM/startup_stm32f427xx.s`
- Recorded toolchain: MDK-ARM V5 / ARMCC 5.06 update 7

This cleanup only performed static project checks. Keil was not run in the current environment.

## 14. First Test Procedure

1. Disconnect motor power or lift the wheels.
2. Check iBUS reception and `IBUS OK` output.
3. Verify CH1, CH2, CH7, and CH9.
4. Check the SA arming state.
5. Configure receiver failsafe so CH7 returns to the safe value on radio loss.
6. Lift the wheels and secure wiring.
7. Start with LOW mode.
8. Check left/right wheel direction.
9. Check CAN feedback IDs and actual RPM.
10. Only then continue with loaded testing.

## 15. Serial Diagnostics

The current firmware can print:

- `HEARTBEAT UART2 TX OK`
- `IBUS OK` / `IBUS WAIT` / `IBUS ERR`
- optional `RAW` iBUS frames
- `CMD` remote command state
- `ARM BLOCKED` arming-block reasons
- `REMOTE DISARM: iBUS serial timeout`
- `RC LINK PROBE`
- `DRIVE` gate state, targets, measured RPM, and PID current output
- `LED_STATUS`

## 16. Configuration

| Parameter group | Location |
| --- | --- |
| iBUS and drive mode switches | `MDK-ARM/bsp/remote_drive_config.h` |
| Channel indices, thresholds, deadzone, failsafe macros | `MDK-ARM/bsp/remote_control.h` |
| Wheel slots, directions, PID, current clamp, RPM ramp | `Src/freertos.c` |
| C620/M3508 CAN IDs | `MDK-ARM/bsp/bsp_can.h` |

Tune with low speed, low load, and lifted wheels first. PID values must be checked against the actual mechanical load.

## 17. Troubleshooting

See [docs/troubleshooting.md](docs/troubleshooting.md).

Start with iBUS wiring, receiver output mode, shared ground, UART format, SA arming conditions, CAN_H/CAN_L, C620 power, feedback IDs, and whether the Keil project still has the original repository structure.

## 18. Known Limitations

- The current code targets a specific FS-i6X/iA10B channel mapping.
- Only the left/right pair is driven, using fixed feedback IDs `0x201` and `0x203`.
- PID gains and current clamp still require mechanical-load tuning.
- Radio-link loss depends on correct receiver failsafe configuration.
- `REMOTE_FAILSAFE_PATTERN_ENABLE` is disabled by default.
- `REMOTE_STICK_EXPO` is defined but not used by the current runtime logic.
- Motor feedback online flags are diagnostic only and are not part of the drive gate.
- This is not a complete chassis motion-control framework.

## 19. Roadmap

- Add motor-feedback loss into the drive gate.
- Add CAN Bus-Off protection and recovery.
- Centralize single-CAN/dual-CAN, motor ID, and direction configuration.
- Convert serial logging to a non-blocking path.
- Add transmitter expo curve support.
- Split the FreeRTOS control task more clearly.
- Add unit tests or offline parser tests.
- Add oscilloscope/logic-analyzer captures and a real-machine demo GIF.

These items are planned work, not completed features.

## 20. License

There is no repository-level `LICENSE` file yet. Choose a license before public release; this cleanup does not choose one on the owner's behalf.

## 21. Acknowledgements

This project uses or references DJI RoboMaster ecosystem hardware, FlySky transmitter/receiver hardware, STM32 HAL, and FreeRTOS. This README does not claim official authorization or partnership with those vendors.
