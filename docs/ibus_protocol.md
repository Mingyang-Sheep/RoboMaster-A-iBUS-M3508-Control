# iBUS Protocol Notes

The current parser is implemented in `MDK-ARM/bsp/bsp_ibus.c` and `MDK-ARM/bsp/bsp_ibus.h`.

| Item | Current implementation |
| --- | --- |
| UART | `USART2` / `huart2` |
| Baud rate and format | `115200`, 8N1 |
| RX method | DMA receive buffer plus UART idle-line detection |
| DMA receive buffer | 64 bytes |
| iBUS frame length | 32 bytes |
| Frame header | byte 0 = `0x20`, byte 1 = `0x40` |
| Checksum bytes | byte 30 low byte, byte 31 high byte |
| Checksum calculation | `0xFFFF - sum(frame[0]..frame[29])` |
| Parsed channels | 10 channels |
| Channel encoding | little-endian 16-bit values from byte 2 onward |
| Channel center | 1500 |
| Serial timeout | 120 ms after the last checksum-valid frame |

`serial_online` means checksum-valid iBUS serial frames are still arriving from the receiver. It does not prove that the wireless link between the transmitter and receiver is still healthy.

The parser prints `IBUS OK`, `IBUS WAIT`, `IBUS ERR`, and optional `RAW` lines through `USART2_TX`.
