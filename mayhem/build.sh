#!/usr/bin/env bash
#
# mayhem/build.sh — ExpressLRS host build: fuzz harnesses + upstream's Unity unit-test suite.
#
# ExpressLRS is ESP32/ESP8266/STM32 firmware built with PlatformIO. Upstream's own host build is the
# PlatformIO `[env:native]` environment (src/platformio.ini): the libraries under src/lib compiled
# for the host with `-DUNIT_TEST=1 -DTARGET_NATIVE -DPROGMEM=""` (Arduino shims in
# src/include/native.h), linked with each src/test/test_* directory and Unity. PlatformIO is not
# available offline, so this script does the same compile directly with clang:
#   1. the native-buildable libraries (LIBS below: every src/lib directory except the ones
#      [env:native] lib_ignore's and the ones that need Arduino/ESP-only headers) -> a static archive,
#      once with $SANITIZER_FLAGS $DEBUG_FLAGS (fuzz) and once with normal flags (tests);
#   2. each harness in mayhem/harnesses -> /mayhem/fuzz_<name> (libFuzzer) and
#      /mayhem/fuzz_<name>-standalone (run-once reproducer);
#   3. each upstream unit test (src/test/test_*, except test_embedded which needs hardware) linked
#      with the Unity test framework prebuilt in /opt/toolchains/deps/unity-<v> -> mayhem/build/test/bin/<test> for test.sh.
# Objects go under mayhem/build/ (gitignored by mayhem/.gitignore). Everything is rebuilt on each
# run, so a re-run is idempotent; nothing is fetched.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${UNITY_DIR:=/opt/toolchains/deps/unity-2.6.1}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
S="$SRC/src"
OUT="$SRC/mayhem/build"

LIBS="ADC Backpack BLE CONFIG CRC CRSF2MSP CrsfProtocol DEVICE elrs_eeprom FEC FHSS FIFO Handset
      HWTIMER logging MDNS MeanAccumulator MSP MSPVTX OPTIONS OTA PFD POWER_DETECT POWERMGNT RFAMP
      rx-crsf ServoOutput StubbornReceiver StubbornSender SX1280Driver SX12xxDriverCommon VTXSPI"
HARNESSES="crsf_parser mdns msp msp2crsf crsf2msp gps hott scorpion ota_uplink"

INCS=(-I"$S/include")
for d in "$S"/lib/*/; do INCS+=(-I"${d%/}"); done
INCS+=(-I"$S/src" -I"$UNITY_DIR/include")
# [env:native] build_flags. LATEST_COMMIT is normally injected by src/python/build_flags.py as the
# ASCII bytes of the git sha; a fixed value ("mayhem") keeps the build independent of .git.
DEFS=(-std=c++17 -DPROGMEM= -DUNIT_TEST=1 -DTARGET_NATIVE
      -DLATEST_COMMIT=0x6d,0x61,0x79,0x68,0x65,0x6d -include "$S/include/targets.h")

# build_lib <objdir> <flags...>: compile every LIBS source into <objdir>/libelrs.a
build_lib() {
  local dir="$1"; shift
  rm -rf "$dir"; mkdir -p "$dir"
  local srcs=()
  for l in $LIBS; do for f in "$S/lib/$l"/*.cpp; do [ -e "$f" ] && srcs+=("$f"); done; done
  printf '%s\n' "${srcs[@]}" | xargs -P "$MAYHEM_JOBS" -I{} bash -c \
    'f="$1"; shift; o="$0/$(basename "$(dirname "$f")")-$(basename "$f" .cpp).o"; exec "$CXX" "$@" -c "$f" -o "$o"' \
    "$dir" {} "$@" "${DEFS[@]}" "${INCS[@]}"
  ar rcs "$dir/libelrs.a" "$dir"/*.o
}

# 1+2) Sanitized, instrumented library + harnesses.
FUZZFLAGS=($SANITIZER_FLAGS $DEBUG_FLAGS -O1)
build_lib "$OUT/fuzz" "${FUZZFLAGS[@]}" -fsanitize=fuzzer-no-link
"$CC" "${FUZZFLAGS[@]}" -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/fuzz/standalone_main.o"
"$CXX" "${FUZZFLAGS[@]}" -c "$SRC/mayhem/lsan_off.cc" -o "$OUT/fuzz/lsan_off.o"
for h in $HARNESSES; do
  "$CXX" "${FUZZFLAGS[@]}" -fsanitize=fuzzer-no-link "${DEFS[@]}" "${INCS[@]}" \
      -c "$SRC/mayhem/harnesses/fuzz_$h.cpp" -o "$OUT/fuzz/fuzz_$h.o"
  "$CXX" "${FUZZFLAGS[@]}" $LIB_FUZZING_ENGINE "$OUT/fuzz/fuzz_$h.o" "$OUT/fuzz/lsan_off.o" \
      "$OUT/fuzz/libelrs.a" -o "$SRC/fuzz_$h"
  "$CXX" "${FUZZFLAGS[@]}" "$OUT/fuzz/fuzz_$h.o" "$OUT/fuzz/standalone_main.o" "$OUT/fuzz/lsan_off.o" \
      "$OUT/fuzz/libelrs.a" -o "$SRC/fuzz_$h-standalone"
done

# 3) Upstream unit tests, normal flags (no sanitizers), against the prebuilt Unity (mayhem/Dockerfile).
TESTFLAGS=(-O1 $COVERAGE_FLAGS)
build_lib "$OUT/test" "${TESTFLAGS[@]}"
mkdir -p "$OUT/test/bin"
for t in "$S"/test/test_*/; do
  n="$(basename "$t")"
  [ "$n" = test_embedded ] && continue
  "$CXX" "${TESTFLAGS[@]}" -DUNITY_INCLUDE_DOUBLE -DUNITY_SUPPORT_64 "${DEFS[@]}" "${INCS[@]}" \
      "$t"/*.cpp "$OUT/test/libelrs.a" "$UNITY_DIR/lib/libunity.a" -o "$OUT/test/bin/$n"
done
echo "build.sh: built $(echo $HARNESSES | wc -w) harnesses and $(ls "$OUT/test/bin" | wc -l) test runners"
