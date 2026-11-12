# Known findings: `fuzz_commander_stream`

Every file here is a crashing input found by this target. Replay one with the standalone reproducer:
`/mayhem/fuzz_commander_stream-standalone mayhem/fuzz_commander_stream/known-findings/<file>`. They are kept out of
`testsuite/` on purpose: seeds are replayed on every run. All of them are bugs in `src/communication/Commander.cpp`
reached through ordinary command text; none involves the harness.

## 1. Missing NULL check on `strtok()` in `Commander::target` (`T<value>`)

`target()` does `atof(strtok(user_cmd, separator))`. A value made only of separator characters (for example `T ` with
a trailing space, or `T` followed by a NUL) makes `strtok()` return NULL, which is passed to `atof()`. The same
unchecked call is copied into each motion-control case, so each case is its own line; it is one root cause.
Impact: a stray space after `T` crashes the MCU's command handler (NULL dereference in `strtod`).
Upstream fix: `char *tok = strtok(user_cmd, separator); if (!tok) return;` in each case.

| input | line | motion-control case |
|---|---|---|
| `strtok_null_torque_L582` | 582 | `MotionControlType::torque` / `angle_nocascade` |
| `strtok_null_velocity_L587` | 587 | `velocity` |
| `strtok_null_angle_L601` | 601 | `angle` |
| `strtok_null_velocity_openloop_L622` | 622 | `velocity_openloop` |
| `strtok_null_angle_openloop_L635` | 635 | `angle_openloop` |

## 2. Negative left shift in the `MMS` monitor-variables parser

`monitor_variables |= (user_command[value_index+i] - '0') << (6-i)` shifts a negative `int` whenever a character below
`'0'` is sent (`MMS,` or a byte >= 0x80), which is undefined behaviour in C++.
Impact: wrong `monitor_variables` bits on the MCU; no memory corruption. Upstream fix: reject characters outside
`'0'..'1'` (or cast to `uint8_t` after the subtraction check).

| input | line |
|---|---|
| `shift_negative_monitor_set_L397` | 397 |

## 3. Out-of-range float-to-integer casts of parsed values (not checked by the fuzz build)

`Commander` converts `atof()` results to narrow integer, `bool` and enum members with plain casts. When the user sends
an out-of-range value (`3131`, `1e38`, `inf`, `nan`) the cast is undefined behaviour in C++, though on the
library's real targets (ARM, ESP32, AVR) it saturates or truncates and corrupts nothing. Because every such
cast would be reported as its own defect, `mayhem/build.sh` compiles `Commander.cpp` with
`-fno-sanitize=float-cast-overflow`; the check stays on for the rest of the library. These inputs therefore only
crash a build without that flag: delete `relax_flags`'s `-fno-sanitize=float-cast-overflow` in `mayhem/build.sh` to
reproduce them. Upstream fix: clamp the value to the member's range before casting.

| input | line | cast |
|---|---|---|
| `cast_modulation_enum_L191` | 191 | `MWT3131`: `(FOCModulationType)value` of an out-of-range float |
| `cast_modulation_centered_L209` | 209 | `modulation_centered = value` (float to `bool`) in `MWC` |
| `cast_monitor_get_L327` | 327 | `(uint8_t)value` of an out-of-range float in the `MMG` monitor getter |
| `cast_monitor_downsample_L379` | 379 | `monitor_downsample = value` (float to `uint32_t`) with `inf`/`nan` |
| `cast_motion_type_L439` | 439 | `(int)value` of `1e38` in the `MC` motion-type setter |
| `cast_torque_type_L476` | 476 | `(int8_t)value` in the `MT` torque-type setter |
