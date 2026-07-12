# Safety and Failsafe

The current arming logic is implemented in `MDK-ARM/bsp/remote_control.c`.

## Arming sequence

The firmware starts in `REMOTE_DISARM`.

1. The iBUS stream must be online and channels must be valid.
2. CH1, CH2, CH3, CH4, CH7, and CH9 must be within `900..2100`.
3. SA / CH7 must first be below `1300`.
4. SA must stay in that safe position for at least `300 ms`.
5. CH1 and CH2 must be centered inside the configured deadzone.
6. Valid iBUS data must be stable for at least `500 ms`.
7. Moving SA / CH7 above `1700` requests arming.

If any required condition fails, the drive task sends zero current, clears wheel targets, and resets the dynamic PID state.

## Receiver failsafe warning

> iBUS serial online is not the same thing as radio-link online. Some receivers may keep sending valid iBUS frames after the transmitter is powered off.

Configure FS-i6X/FS-iA10B failsafe so that radio loss drives CH7/SA back to the safe/disarmed value. First motor tests should be done with wheels lifted or mechanical load removed.

`REMOTE_FAILSAFE_PATTERN_ENABLE` exists but is `0` in the current code, so the optional pattern detector is disabled by default. The active protection path is serial timeout, channel validation, SA safe/disarm behavior, and user-configured receiver failsafe.
