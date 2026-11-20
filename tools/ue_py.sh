#!/usr/bin/env bash
# Run an editor Python script headless. Usage: tools/ue_py.sh <logname> <script.py>
# The project lives under "Group project": UE's -script= cannot take a path with a space (quoted or not),
# so the script runs from a space-free temp copy. The scripts find the project through unreal.Paths, not
# their own location, so the copy behaves the same.
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
COPY="$(cygpath -m "${TEMP:-/tmp}")/matsuri_$(basename "$2")"
cp "$2" "$COPY"
"$HERE/tools/ue_cmd.sh" "$1" -run=pythonscript "-script=$COPY" -nullrhi
code=$?
rm -f "$COPY"
exit $code
