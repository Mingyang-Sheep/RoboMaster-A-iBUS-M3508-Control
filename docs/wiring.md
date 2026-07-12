# Wiring Notes

This file records only wiring that is supported by the current CubeMX and source configuration.

## iBUS and serial diagnostics

| Signal | Current code/configuration |
| --- | --- |
| FS-iA10B iBUS signal | RoboMaster A board BTUART RX, `PD6 / USART2_RX` |
| RoboMaster A board debug TX | `PD5 / USART2_TX`, connect to USB-TTL RX when serial diagnostics are needed |
| UART instance | `huart2` / `USART2` |
| UART settings | `115200`, 8 data bits, no parity, 1 stop bit |
| RX DMA | `DMA1_Stream5`, `DMA_CHANNEL_4`, normal mode |
| IDLE interrupt | handled in `USART2_IRQHandler()` before `HAL_UART_IRQHandler()` |

The receiver, A board, C620 ESCs, and USB-TTL adapter must share ground. Leave the USB-TTL VCC pin disconnected unless the adapter and board power design explicitly require otherwise.

The receiver supply must match the receiver and board connector specification. Verify the RoboMaster A board pinout before using a 5 V rail.

If the iBUS signal high level is close to 5 V, use level shifting or a resistor divider before connecting it to `PD6`.

## CAN wiring

| Bus | RX pin | TX pin | Use in current firmware |
| --- | --- | --- | --- |
| CAN1 | `PD0` | `PD1` | Sends C620 current commands and receives motor feedback |
| CAN2 | `PB12` | `PB13` | Same filter and current-command path is also initialized |

Connect CAN_H to CAN_H and CAN_L to CAN_L. Use proper CAN bus termination and keep C620 motor power wiring separate from signal wiring. Do not power the C620 from the A board.
