# Troubleshooting

## No iBUS data

Check that the receiver output mode is iBUS, the signal is connected to `PD6 / USART2_RX`, the board and receiver share ground, and the UART is still configured as `115200` 8N1.

## Channel values do not change

Confirm FS-i6X binding, receiver output mode, and that the expected switch/stick is mapped to the channel used by the code. CH1, CH2, CH7, and CH9 are the active drive controls.

## Many checksum errors

Check signal level, ground, cable quality, and receiver mode. Do not change multiple UART settings at once; compare wiring and receiver output first.

## SA cannot arm

Move SA/CH7 to the safe value below `1300` and hold for at least `300 ms`, center CH1 and CH2, wait for valid iBUS data to be stable for `500 ms`, then move SA above `1700`.

## Motor does not spin

Confirm `IBUS_DIAGNOSTIC_ONLY == 0`, `REMOTE_DRIVE_ENABLE == 1`, `CAN_CONTROL` is defined, SA is armed, and the drive log reports `gate=1`. Also check C620 power, CAN wiring, and feedback IDs.

## Motor direction is reversed

Adjust direction macros after a lifted-wheel test. Do not swap CAN IDs, motor slots, or wiring just to fix a software direction sign.

## M3508 feedback is missing

Check that feedback IDs `0x201` and `0x203` are present for the current left/right slots. Verify CAN_H/CAN_L, termination, ESC power, and CAN bitrate.

## CAN has no data

Check the CAN1 pins `PD0/PD1` and CAN2 pins `PB12/PB13`, C620 power, bus termination, and that the firmware initialized CAN receive interrupts.

## Transmitter off but motor does not stop

Configure receiver failsafe so CH7/SA returns to the safe value. Valid iBUS bytes can continue after radio loss on some receivers.

## Serial output is garbled

Use `115200` baud, 8 data bits, no parity, 1 stop bit, and no flow control on the USB-TTL side.

## Keil cannot find files

Open the project from the repository root layout without moving `Inc/`, `Src/`, `Drivers/`, `Middlewares/`, or `MDK-ARM/`. The current `.uvprojx` references relative paths.
