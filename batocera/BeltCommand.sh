#!/bin/bash
# Belt Command - Batocera port launcher
DIR="$(dirname "$(readlink -f "$0")")/beltcommand"
BIN="$DIR/beltcommand.x86_64"
LOG=/userdata/system/logs/beltcommand.log
mkdir -p /userdata/system/logs
cd "$DIR" || { echo "missing $DIR" > "$LOG"; exit 1; }
export SDL_JOYSTICK_HIDAPI_WII=0
{
  echo "=== Belt Command launch $(date) ==="
  # copying over the network share (SMB) can drop the executable bit: restore it
  [ -x "$BIN" ] || chmod +x "$BIN" 2>&1
} > "$LOG" 2>&1
if [ -x "$BIN" ]; then
  BC_HOME="$DIR" "$BIN" "$@" >> "$LOG" 2>&1
else
  BC_HOME="$DIR" /lib64/ld-linux-x86-64.so.2 "$BIN" "$@" >> "$LOG" 2>&1
fi
rc=$?
echo "=== exit code $rc ===" >> "$LOG"
exit $rc
