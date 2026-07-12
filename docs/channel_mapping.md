# Channel Mapping

The code array index starts at zero, so `channel[0]` is CH1.

| iBUS channel | Code index | Current use |
| --- | ---: | --- |
| CH1 | `channel[0]` | right stick horizontal, `turn` |
| CH2 | `channel[1]` | right stick vertical, `forward` |
| CH3 | `channel[2]` | parsed as `aux1`, not used by the wheel drive task |
| CH4 | `channel[3]` | parsed as `aux2`, not used by the wheel drive task |
| CH5 | `channel[4]` | raw observation only |
| CH6 | `channel[5]` | raw observation only |
| CH7 | `channel[6]` | SA switch, arm/disarm request |
| CH8 | `channel[7]` | raw observation only |
| CH9 | `channel[8]` | SC switch, speed mode |
| CH10 | `channel[9]` | raw observation only |

Stick channels are clamped to `1000..2000` and normalized around `1500`.

```text
1000 -> -1.0
1500 ->  0.0
2000 -> +1.0
```

`REMOTE_STICK_DEADZONE` is `0.05f`, so values close to the center are forced to zero. Direction signs are controlled by `REMOTE_FORWARD_DIR`, `REMOTE_TURN_DIR`, `REMOTE_AUX1_DIR`, and `REMOTE_AUX2_DIR`.

SC speed mode thresholds:

```text
CH9 < 1300  -> LOW
CH9 > 1700  -> HIGH
otherwise   -> MID
```

Current target speed limits:

| Mode | Target speed value |
| --- | ---: |
| LOW | `160.0f` |
| MID | `320.0f` |
| HIGH | `1600.0f` through `REMOTE_FULL_THROTTLE_MAX_SPEED` |
