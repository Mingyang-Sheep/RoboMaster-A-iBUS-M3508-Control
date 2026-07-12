# FS-iA10B iBUS first-stage input

This stage only adds FS-i6X + FS-iA10B iBUS input and maps it to the existing two C620/M3508 wheel motors. It does not add the third or fourth motor, does not change motor IDs, and does not change the current CAN feedback slots.

## Hardware wiring

| FS-iA10B | RoboMaster A board BTUART |
| --- | --- |
| iBUS signal | `R / PD6 / USART2_RX` |
| GND | `PGND` |
| VCC | `5V_U` |

Leave `T / PD5 / USART2_TX` unconnected. Do not use the DBUS port for this iBUS receiver.

`demo.ioc` and `Src/usart.c` show that BTUART is currently mapped as:

```text
PD5 = USART2_TX
PD6 = USART2_RX
huart2 = 115200 baud, 8 data bits, no parity, 1 stop bit
```

After using BTUART for iBUS, do not connect the Bluetooth module to the same BTUART port at the same time.

If the measured iBUS signal high level is close to 5 V, add level shifting or a resistor divider before connecting it to MCU RX. Do not connect a 5 V signal directly to `PD6`.

## Receiver setup

In the FS-i6X/FS-iA10B setup, confirm that the receiver output mode is iBUS. First power-up testing should be done with the motors disconnected or otherwise unable to drive the chassis. Confirm channel values before enabling wheel current.

If the receiver can produce UART bytes but the firmware never gets a valid checksum, keep this first-stage firmware at 8N1 and later compare by changing only this UART stop bit to 2.

## Debug watch items

Recommended watch expressions in VSCode:

- `ibus_state.channel[0]` through `ibus_state.channel[5]`
- `ibus_state.valid_frame_count`
- `ibus_state.checksum_error_count`
- `remote_cmd`
- `set_spd[0]` for the existing left wheel target
- `set_spd[2]` for the existing right wheel target
- `moto_chassis[0].speed_rpm`
- `moto_chassis[2].speed_rpm`
- `pid_spd[0].pos_out`
- `pid_spd[2].pos_out`

## Acceptance order

1. After binding, confirm CH1, CH2, CH5, and CH6 values are correct.
2. While DISARMED, confirm the two active wheel slots continuously receive zero current.
3. Confirm ARM is only accepted after CH1 and CH2 return to center and valid iBUS frames have been stable for at least 500 ms.
4. Confirm forward, reverse, left turn, and right turn behavior.
5. Turn off the transmitter, unplug iBUS, or interrupt motor feedback and confirm both wheels stop.

## Channel mapping

| Channel | Use in this stage |
| --- | --- |
| CH1 / `channel[0]` | turn |
| CH2 / `channel[1]` | forward |
| CH3 / `channel[2]` | parsed and kept for debug only |
| CH4 / `channel[3]` | parsed and kept for debug only |
| CH5 / `channel[4]` | ARM / DISARM |
| CH6 / `channel[5]` | low / mid / high speed mode |

The second stage can reuse the parsed CH3 and CH4 values for the third and fourth motors. That expansion is intentionally not implemented here.
