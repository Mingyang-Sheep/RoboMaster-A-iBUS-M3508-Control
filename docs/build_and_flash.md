# Build and Flash

This project is a STM32CubeMX / Keil MDK-ARM V5 project.

| Item | Current location/value |
| --- | --- |
| CubeMX file | `demo.ioc` |
| Keil project | `MDK-ARM/demo.uvprojx` |
| Keil target | `demo` |
| Device in Keil target | `STM32F427IIHx` |
| Toolchain recorded by project | ARMCC 5.06 update 7 |
| Startup file | `MDK-ARM/startup_stm32f427xx.s` |
| Build output directory | `OBJ/` |

Open `MDK-ARM/demo.uvprojx` in Keil MDK-ARM V5, select the `demo` target, then build and flash with the debugger used by your board setup.

Serial diagnostics are printed through `USART2_TX` at `115200` 8N1. Because `USART2_RX` is used for iBUS, make sure the debug adapter is wired only to the TX side unless you intentionally need bidirectional UART.

This cleanup only performed static project consistency checks in the current environment. Keil was not run here, so this document does not claim a fresh successful build.
