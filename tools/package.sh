#!/usr/bin/env bash
# Build, cook and package the game for Windows. Usage: tools/package.sh [Shipping|Development] [logname]
# Output: Package/Windows/SuzuneMatsuri.exe (starts on the title screen). Maps and always-cooked folders are
# listed in SuzuneMatsuri/Config/DefaultGame.ini ([/Script/UnrealEd.ProjectPackagingSettings]).
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG="${1:-Shipping}"
LOG="$HERE/logs/${2:-package}.log"
# cmd splits an unquoted "Program Files" when Git Bash runs a .bat: call it by its 8.3 short path.
UAT="$(cygpath -d "/c/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat")"
mkdir -p "$HERE/logs" "$HERE/Package"
"$UAT" BuildCookRun \
	"-project=$(cygpath -w "$HERE/SuzuneMatsuri/SuzuneMatsuri.uproject")" \
	-noP4 -platform=Win64 "-clientconfig=$CONFIG" \
	-build -cook -stage -pak -iostore -prereqs -archive \
	"-archivedirectory=$(cygpath -w "$HERE/Package")" \
	-utf8output -unattended -nocompileeditor > "$LOG" 2>&1
code=$?
grep -E "BUILD SUCCESSFUL|BUILD FAILED|AutomationTool exiting|Error:|error C" "$LOG" | tail -20
echo "exit $code (log: $LOG)"
exit $code
