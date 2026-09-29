#!/bin/zsh
# Runs one Tools/Editor/*.py editor script headless and stops the editor when the script logs DONE/FAILED.
# UE 5.8.3 either crashes or hangs in quit_editor after these scripts; the work is saved before that point.
#   Tools/Editor/run_headless.sh Tools/Editor/<script>.py "<log tag>"      e.g. "LFF look"
set -u
SCRIPT="$PWD/$1"; TAG="$2"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
LOG="$HOME/Library/Logs/Unreal Engine/VirtualStadiumEditor/VirtualStadium.log"
START=$(date -u +%Y.%m.%d-%H.%M.%S)
"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/VirtualStadium.uproject" \
  -ExecutePythonScript="$SCRIPT" -ModelContextProtocolPort=18000 -unattended -nullrhi -nopause -nosplash -log \
  > "Saved/Logs/$(basename "$1" .py).log" 2>&1 &
UEPID=$!
until grep -qE "\[$TAG\] (DONE|FAILED)" "$LOG" 2>/dev/null && [[ "$(grep -E "\[$TAG\] (DONE|FAILED)" "$LOG" | tail -1 | cut -c2-20)" > "$START" ]]; do
  kill -0 $UEPID 2>/dev/null || break
  sleep 5
done
grep -E "\[$TAG\]" "$LOG" | awk -v s="$START" 'substr($0,2,19) >= s' | cut -c1-400
sleep 10
kill -TERM $UEPID 2>/dev/null; sleep 5; kill -KILL $UEPID 2>/dev/null
pkill -f CrashReportClient 2>/dev/null
grep -qE "\[$TAG\] DONE" "$LOG" && [[ "$(grep -E "\[$TAG\] DONE" "$LOG" | tail -1 | cut -c2-20)" > "$START" ]]
