#!/usr/bin/env bash
#
# mayhem/build.sh — builds the fuzz harnesses, their standalone reproducers and the oracle's KAT probe.
#
# Arduino-FOC (SimpleFOC) is an Arduino library. Its hardware-independent core (BLDCMotor, the FOC
# base classes, PID/LPF, foc_utils, the Commander text protocol) only needs the Arduino core API for
# Print/Stream, timing and a few macros, so the harnesses compile it on the host against the small
# stand-in in mayhem/arduino_mock/ (output discarded, deterministic virtual clock). No hardware
# driver, sensor or MCU-specific source is built.
#
# Outputs: /mayhem/<target> and /mayhem/<target>-standalone per harness, and the oracle's probe at
# mayhem/build-kat/kat_probe. All intermediate objects live in gitignored dirs under mayhem/, and the
# script rebuilds from scratch every run, so it is idempotent and needs no network.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

M="$SRC/mayhem"
FUZZ_OBJ="$M/build-fuzz"
KAT_OBJ="$M/build-kat"
INC=(-I"$M" -I"$M/arduino_mock" -I"$SRC/src")

# The hardware-independent library sources the harnesses exercise, plus the Arduino stand-in.
LIB_SRCS=(
  src/communication/Commander.cpp
  src/communication/SimpleFOCDebug.cpp
  src/common/pid.cpp
  src/common/lowpass_filter.cpp
  src/common/foc_utils.cpp
  src/common/time_utils.cpp
  src/common/base_classes/FOCMotor.cpp
  src/common/base_classes/Sensor.cpp
  src/common/base_classes/CurrentSense.cpp
  src/BLDCMotor.cpp
  mayhem/arduino_mock/arduino_mock.cpp
)
TARGETS=(fuzz_commander_stream fuzz_commander_string fuzz_motor_loop)

rm -rf "$FUZZ_OBJ" "$KAT_OBJ"
mkdir -p "$FUZZ_OBJ" "$KAT_OBJ"

# Fuzz build: the library carries ASan/UBSan, DWARF 3 and SanCov (fuzzer-no-link, unconditionally, so
# the target records edges even with an empty $SANITIZER_FLAGS).
FUZZ_CXXFLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -O1 -std=c++17 -w"
# Oracle build: the project's normal flags (no sanitizer, no -gdwarf-3).
KAT_CXXFLAGS="-O2 -std=c++17 -w $COVERAGE_FLAGS"

obj_name() { local s="${1//\//_}"; echo "${s%.*}.o"; }

# Commander parses user text with atof() and stores it in narrow integer members and enum fields with a plain C
# cast, relying on the MCU's own float-to-int conversion (saturating or truncating, never memory-unsafe) when the
# value is out of range. float-cast-overflow reports each of those casts as its own defect, so it is the one UBSan
# check relaxed, on Commander.cpp only; ASan and every other UBSan check stay on and halting, and foc_utils.cpp keeps
# the check. The reproducers for those casts are in mayhem/fuzz_commander_*/known-findings/.
relax_flags() {
  case "$1" in
    src/communication/Commander.cpp) echo "-fno-sanitize=float-cast-overflow" ;;
  esac
}

pids=()
for s in "${LIB_SRCS[@]}"; do
  o="$(obj_name "$s")"
  # shellcheck disable=SC2086
  $CXX $FUZZ_CXXFLAGS $(relax_flags "$s") "${INC[@]}" -c "$SRC/$s" -o "$FUZZ_OBJ/$o" &
  pids+=($!)
  # shellcheck disable=SC2086
  $CXX $KAT_CXXFLAGS "${INC[@]}" -c "$SRC/$s" -o "$KAT_OBJ/$o" &
  pids+=($!)
  while [ "$(jobs -rp | wc -l)" -ge "$MAYHEM_JOBS" ]; do wait -n || true; done
done
for p in "${pids[@]}"; do wait "$p"; done

LIB_FUZZ_OBJS=(); LIB_KAT_OBJS=()
for s in "${LIB_SRCS[@]}"; do
  LIB_FUZZ_OBJS+=("$FUZZ_OBJ/$(obj_name "$s")")
  LIB_KAT_OBJS+=("$KAT_OBJ/$(obj_name "$s")")
done

# LSan is off at build time: leak reports are noise for these harnesses.
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -c "$M/lsan_off.cc" -o "$FUZZ_OBJ/lsan_off.o"
# The standalone driver is C; compile it as C so the harness's extern "C" entry point resolves.
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$FUZZ_OBJ/standalone_main.o"

for t in "${TARGETS[@]}"; do
  # shellcheck disable=SC2086
  $CXX $FUZZ_CXXFLAGS "${INC[@]}" -c "$M/$t.cpp" -o "$FUZZ_OBJ/$t.o"
  # shellcheck disable=SC2086
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE \
      "$FUZZ_OBJ/$t.o" "${LIB_FUZZ_OBJS[@]}" "$FUZZ_OBJ/lsan_off.o" -o "$SRC/$t"
  # shellcheck disable=SC2086
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS \
      "$FUZZ_OBJ/$t.o" "$FUZZ_OBJ/standalone_main.o" "${LIB_FUZZ_OBJS[@]}" "$FUZZ_OBJ/lsan_off.o" \
      -o "$SRC/$t-standalone"
done

# Oracle probe: normal flags, dynamically linked (the sabotage check neuters non-system executables).
# shellcheck disable=SC2086
$CXX $KAT_CXXFLAGS "${INC[@]}" "$M/kat/kat_probe.cpp" "${LIB_KAT_OBJS[@]}" -o "$KAT_OBJ/kat_probe"
if ! readelf -l "$KAT_OBJ/kat_probe" | grep -q 'Requesting program interpreter'; then
  echo "build.sh: kat_probe must be dynamically linked" >&2
  exit 1
fi

for t in "${TARGETS[@]}"; do
  [ -x "$SRC/$t" ] && [ -x "$SRC/$t-standalone" ] || { echo "build.sh: missing binary for $t" >&2; exit 1; }
done
echo "build.sh: built ${TARGETS[*]} and the KAT probe"
