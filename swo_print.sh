#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP="${APP:-vibe}"
APP_DIR="$SCRIPT_DIR/apps/$APP"

SWO_LOG="${SWO_LOG:-/tmp/${APP}_swo.log}"
OPENOCD_LOG="${OPENOCD_LOG:-/tmp/${APP}_openocd.log}"
TRACE_MAP="${TRACE_MAP:-$APP_DIR/build/swo/trace_map.json}"
BOOT_TRACE_MAP="${BOOT_TRACE_MAP:-$SCRIPT_DIR/bootloader/build/trace_map.json}"
TRACE_PID=""

cleanup() {
    if [[ -n "$TRACE_PID" ]]; then
        kill -TERM "-$TRACE_PID" 2>/dev/null || true
        wait "$TRACE_PID" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

for map in "$BOOT_TRACE_MAP" "$TRACE_MAP"; do
    if [[ ! -f "$map" ]]; then
        echo "swo_print.sh: missing $map; run 'make flash-swo' first" >&2
        exit 1
    fi
done

rm -f "$SWO_LOG"

setsid make -C "$APP_DIR" swo-trace SWO_LOG="$SWO_LOG" >"$OPENOCD_LOG" 2>&1 &
TRACE_PID=$!

for _ in {1..50}; do
    if [[ -e "$SWO_LOG" ]]; then
        break
    fi
    if ! kill -0 "$TRACE_PID" 2>/dev/null; then
        echo "swo_print.sh: OpenOCD exited early. Log:" >&2
        sed -n '1,120p' "$OPENOCD_LOG" >&2
        exit 1
    fi
    sleep 0.1
done

if [[ ! -e "$SWO_LOG" ]]; then
    echo "swo_print.sh: timed out waiting for $SWO_LOG. OpenOCD log:" >&2
    sed -n '1,120p' "$OPENOCD_LOG" >&2
    exit 1
fi

echo "SWO trace running. Press Ctrl-C to stop." >&2
python3 "$SCRIPT_DIR/tools/decode_trace.py" \
    --map "$BOOT_TRACE_MAP" \
    --map "$TRACE_MAP" \
    --input "$SWO_LOG" \
    --follow
