#!/usr/bin/env bash
#
# mayhem/test.sh — oracle for the Arduino-FOC port. Upstream ships no tests, so mayhem/build.sh builds
# a known-answer probe (mayhem/kat/kat_probe.cpp) that drives the real library (PID, low-pass filter,
# foc_utils, BLDCMotor PWM modulation, motor torque loop, the Commander text protocol) and prints one
# `KAT <name> <value>` line per result. Every expected value below is worked out from the library's
# documented formulas; a missing or wrong line fails, so a probe that exits 0 without computing
# anything cannot pass.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

PROBE="$SRC/mayhem/build-kat/kat_probe"
if [ ! -x "$PROBE" ]; then
  echo "test.sh: $PROBE is missing; mayhem/build.sh should have built it" >&2
  emit_ctrf "arduino-foc-kat" 0 1 0
  exit 1
fi

OUT="$("$PROBE" 2>/dev/null)" || true

# name expected-value (the probe prints %.4f)
EXPECTED='
pid_step1 2.5000
pid_step2 3.5000
pid_limit 4.0000
pid_ramp 1.0000
lpf_step1 0.5000
lpf_step2 0.7500
normalize_angle 0.7168
normalize_angle_neg 5.2832
sin_half_pi 1.0000
cos_pi -1.0000
electrical_angle 7.0000
sqrt_approx 0.4831
atan2_quadrant2 2.3564
sine_centered_a 6.0000
sine_centered_b 8.5981
sine_centered_c 3.4019
sine_edge_a 2.5981
sine_edge_b 5.1962
sine_edge_c 0.0000
svpwm_a 6.0000
cmd_pid_p 9.0000
cmd_pid_i 8.5000
cmd_lpf_tf 0.2500
cmd_scalar -2.5000
cmd_custom_sum 3.5000
cmd_motor_kv 120.0000
cmd_motor_voltage_limit 0.8000
cmd_motion_type 1.0000
cmd_target 3.0000
cmd_get_keeps_value 7.0000
cmd_pid_d -0.5000
cmd_pwm_mod_type 1.0000
move_torque_target 2.0000
move_torque_voltage_q 2.0000
'

passed=0 failed=0
while read -r name value; do
  [ -n "$name" ] || continue
  if printf '%s\n' "$OUT" | grep -Fxq "KAT $name $value"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    got="$(printf '%s\n' "$OUT" | grep -F "KAT $name " | head -1)"
    echo "FAIL $name: expected '$value', got '${got:-<no output>}'" >&2
  fi
done <<< "$EXPECTED"

emit_ctrf "arduino-foc-kat" "$passed" "$failed" 0
