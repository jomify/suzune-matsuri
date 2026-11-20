#!/usr/bin/env bash
# Compile the SuzuneMatsuri editor target. Usage: tools/build.sh <logname>
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
DOTNET="/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe"
UBT="/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll"
export DOTNET_ROOT="$(dirname "$DOTNET")"
LOG="$HERE/logs/${1:-build}.log"
mkdir -p "$HERE/logs"
"$DOTNET" "$UBT" SuzuneMatsuriEditor Win64 Development "-Project=$(cygpath -w "$HERE/SuzuneMatsuri/SuzuneMatsuri.uproject")" -WaitMutex > "$LOG" 2>&1
code=$?
grep -E "error|warning C|Result:|Total execution" "$LOG" | tail -40
echo "exit $code (log: $LOG)"
exit $code
