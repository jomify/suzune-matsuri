#!/usr/bin/env bash
# Run UnrealEditor-Cmd against the project. Usage: tools/ue_cmd.sh <logname> [editor args...]
# Paths are passed with forward slashes: UE unescapes "\t" etc. inside -script= values.
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
UE="/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
LOG="$HERE/logs/$1.log"; shift
mkdir -p "$HERE/logs"
"$UE" "$(cygpath -m "$HERE/SuzuneMatsuri/SuzuneMatsuri.uproject")" "$@" -unattended -nosplash -NoSound > "$LOG" 2>&1
code=$?
echo "exit $code (log: $LOG)"
exit $code
