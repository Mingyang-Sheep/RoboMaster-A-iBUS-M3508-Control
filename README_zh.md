# RoboMaster A 型板 iBUS M3508 遥控闭环控制

[English](README.md) | 简体中文

## 1. 项目简介

本仓库整理的是一条面向学习和复现的遥控闭环控制链路：

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

当前代码重点演示 FS-i6X/iA10B 的 iBUS 输入如何进入 STM32，经过通道映射、安全解锁、差速混控、目标转速斜坡、M3508 速度 PID，再通过 CAN 给 C620 发送电流指令。它不是完整 RoboMaster 比赛底盘框架。

## 2. 功能特性

- `USART2 + DMA + IDLE` 接收 iBUS 数据。
- 32 字节 iBUS 帧头检查、校验和验证和 10 通道解析。
- CH1/CH2/CH7/CH9 遥控映射，CH3/CH4 作为保留辅助量解析。
- SA 安全解锁流程和 SC 三档速度模式。
- 摇杆归一化、死区、左右轮差速混控和目标转速斜坡。
- M3508 速度 PID 闭环，C620 CAN 电流指令输出。
- 串口诊断输出和 LED 状态提示。
- 串口超时、通道非法、SA 安全位和接收机 FailSafe 配置提示。

## 3. 硬件组成

| 硬件 | 型号 | 用途 |
| --- | --- | --- |
| 开发板 | DJI RoboMaster A 型开发板 | 主控板 |
| MCU | `STM32F427IIHx` | Keil 工程目标芯片 |
| 遥控器 | FlySky FS-i6X | 遥控发射端 |
| 接收机 | FlySky FS-iA10B | AFHDS 2A 接收机，输出 iBUS |
| 电调 | DJI C620 | M3508 电机控制器 |
| 电机 | DJI M3508 | 左右轮驱动电机 |
| 调试适配器 | USB-TTL | 串口诊断 |

## 4. 系统架构

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

## 5. 仓库结构

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

`OBJ/` 和 `MDK-ARM/demo/` 是现有编译产物目录，已在 `.gitignore` 中为后续提交忽略。仓库中的 C620 PDF 是否适合公开分发需要用户自行确认。

## 6. 硬件接线

关键接线见 [docs/wiring.md](docs/wiring.md)。

| 信号 | 当前代码配置 |
| --- | --- |
| iA10B iBUS Signal | `PD6 / USART2_RX` |
| A 型板串口调试 TX | `PD5 / USART2_TX` |
| iBUS UART | `USART2`, `115200 8N1` |
| USART2 RX DMA | `DMA1_Stream5`, `DMA_CHANNEL_4` |
| CAN1 | `PD0 / CAN1_RX`, `PD1 / CAN1_TX` |
| CAN2 | `PB12 / CAN2_RX`, `PB13 / CAN2_TX` |

接收机、A 型板、C620 和 USB-TTL 必须共地。iA10B 供电请按接收机和 A 型板接口定义确认；不要把未经确认的 5 V 信号直接接到 MCU RX。

## 7. iBUS 配置

详细说明见 [docs/ibus_protocol.md](docs/ibus_protocol.md)。

| 项目 | 当前实现 |
| --- | --- |
| UART | `USART2 / huart2` |
| 串口格式 | `115200`, 8 数据位，无校验，1 停止位 |
| 帧长度 | 32 字节 |
| 帧头 | `0x20 0x40` |
| 校验 | `0xFFFF - sum(frame[0]..frame[29])` |
| 通道数 | 10 |
| 串口超时 | 120 ms |

## 8. 通道映射

代码数组下标从 0 开始，因此 `channel[0]` 对应 CH1。详细表格见 [docs/channel_mapping.md](docs/channel_mapping.md)。

| 通道 | 输入 | 当前用途 |
| --- | --- | --- |
| CH1 / `channel[0]` | 右摇杆左右 | 转向 `turn` |
| CH2 / `channel[1]` | 右摇杆上下 | 前后 `forward` |
| CH3 / `channel[2]` | 左摇杆上下 | `aux1`，当前驱动任务未使用 |
| CH4 / `channel[3]` | 左摇杆左右 | `aux2`，当前驱动任务未使用 |
| CH7 / `channel[6]` | SA | ARM / DISARM |
| CH9 / `channel[8]` | SC | LOW / MID / HIGH |

## 9. 安全解锁流程

安全流程见 [docs/safety_and_failsafe.md](docs/safety_and_failsafe.md)。

当前代码要求：iBUS 串口在线、通道有效、SA 先进入安全位并保持 300 ms、CH1/CH2 回中、有效帧稳定 500 ms，然后 SA 切到运行位才允许 `REMOTE_ARMED`。任何未解锁、串口超时、通道非法或 FailSafe 命中都会清零目标、复位 PID 动态状态并发送零电流。

## 10. 接收机失控保护

> iBUS 串口在线不等于遥控无线链路在线。某些接收机在遥控器关闭后仍可能继续输出合法 iBUS 帧。

必须在 FS-i6X/iA10B 中配置 FailSafe，建议失联后让 CH7/SA 回到停止值。首次测试必须架空车轮或断开机械负载。当前 `REMOTE_FAILSAFE_PATTERN_ENABLE` 为 `0`，可选的模式检测默认未启用。

## 11. 差速混控

当前混控公式：

```text
left_mix  = clamp(forward + turn, -1.0, 1.0)
right_mix = clamp(forward - turn, -1.0, 1.0)
```

左右方向由 `LEFT_WHEEL_DIR = -1.0f` 和 `RIGHT_WHEEL_DIR = +1.0f` 修正。若实测方向相反，优先按方向宏修正，不要交换 CAN ID、槽位或 C620 接线。

## 12. C620 与 M3508 控制

详细说明见 [docs/c620_m3508_control.md](docs/c620_m3508_control.md)。

| 项目 | 当前实现 |
| --- | --- |
| CAN 波特率 | 1 Mbps |
| C620 1-4 号电机电流指令 | 标准 ID `0x200` |
| C620 5-8 号电机电流指令 | 标准 ID `0x1FF`，当前发送零 |
| M3508 反馈 ID | `0x201` 到 `0x204` |
| 左轮槽位 | `0`，反馈 ID `0x201` |
| 右轮槽位 | `2`，反馈 ID `0x203` |
| PID | 位置式速度 PID |
| 电流限幅 | `+/-14500` C620 协议原始值 |

电流指令是 C620 协议数值，不应直接写成安培值。

## 13. 编译与烧录

编译说明见 [docs/build_and_flash.md](docs/build_and_flash.md)。

- Keil 工程：`MDK-ARM/demo.uvprojx`
- Target：`demo`
- CubeMX 文件：`demo.ioc`
- 启动文件：`MDK-ARM/startup_stm32f427xx.s`
- 工程记录工具链：MDK-ARM V5 / ARMCC 5.06 update 7

本次仅完成工程结构检查，未在当前环境重新执行 Keil 编译。

## 14. 首次测试流程

1. 暂时断开电机动力电源或确保车轮架空。
2. 检查 iBUS 接收和 `IBUS OK` 输出。
3. 检查 CH1、CH2、CH7、CH9 是否符合预期。
4. 检查 SA 安全解锁状态。
5. 配置接收机 FailSafe，让失联后 CH7 回到停止值。
6. 架空车轮并固定线束。
7. 使用 LOW 档进行第一次电机测试。
8. 检查左右轮方向。
9. 检查 CAN 反馈 ID 和实际转速。
10. 最后再进行带负载测试。

## 15. 串口诊断

当前通过 `USART2_TX` 输出：

- `HEARTBEAT UART2 TX OK`
- `IBUS OK` / `IBUS WAIT` / `IBUS ERR`
- 可选 `RAW` iBUS 帧
- `CMD` 遥控命令状态
- `ARM BLOCKED` 解锁阻塞原因
- `REMOTE DISARM: iBUS serial timeout`
- `RC LINK PROBE`
- `DRIVE` 驱动状态、目标转速、实际转速和 PID 输出电流
- `LED_STATUS`

## 16. 参数配置

主要配置位置：

| 参数 | 位置 |
| --- | --- |
| iBUS 和遥控驱动开关 | `MDK-ARM/bsp/remote_drive_config.h` |
| 通道索引、阈值、死区、FailSafe 宏 | `MDK-ARM/bsp/remote_control.h` |
| 左右轮槽位、方向、PID、电流限幅、斜坡 | `Src/freertos.c` |
| C620/M3508 CAN ID | `MDK-ARM/bsp/bsp_can.h` |

先用低速、低负载和架空车轮调试。PID 参数必须根据实际机械负载重新确认。

## 17. 常见问题

排查清单见 [docs/troubleshooting.md](docs/troubleshooting.md)。

优先检查 iBUS 接线、接收机输出模式、共地、串口格式、SA 解锁条件、CAN_H/CAN_L、C620 供电、反馈 ID 和 Keil 工程路径是否仍保持原结构。

## 18. 当前限制

- 当前主要面向 FS-i6X/iA10B 的特定通道映射。
- 当前只驱动左/右两路，固定使用反馈 ID `0x201` 和 `0x203`。
- PID 参数和电流限幅仍需根据实际机械结构调试。
- 接收机无线失联依赖用户正确配置 FailSafe。
- `REMOTE_FAILSAFE_PATTERN_ENABLE` 默认关闭。
- `REMOTE_STICK_EXPO` 仅定义，当前运行逻辑未使用 Expo 曲线。
- 电机反馈在线状态只用于诊断打印，不参与驱动门控。
- 当前不是完整底盘运动控制框架。

## 19. 后续计划

- 增加电机反馈掉线保护到驱动门控。
- 增加 CAN Bus-Off 保护和恢复策略。
- 将单 CAN / 双 CAN、左右轮 ID 和方向集中配置化。
- 改造非阻塞串口日志。
- 增加遥控器 Expo 曲线。
- 拆分 FreeRTOS 控制任务。
- 增加单元测试或离线解析测试。
- 补充示波器、逻辑分析仪波形和实机演示 GIF。

以上均为计划，不代表当前已实现。

## 20. 开源许可

仓库当前没有顶层 `LICENSE` 文件。公开前需要用户自行选择许可证；本次整理不代替用户决定授权方式。

## 21. 致谢

本工程使用或参考了 DJI RoboMaster 生态硬件、FlySky 遥控/接收设备、STM32 HAL 和 FreeRTOS。本文档不表示获得上述厂商的官方授权或合作背书。
