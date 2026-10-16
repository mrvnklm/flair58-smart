#!/usr/bin/env bash
# Query the Fellow Stagg EKG Pro's local HTTP CLI (firmware 1.1.x, see docs/kettle.md).
# Usage:
#   tools/kettle_cli.sh <kettle-ip> state
#   tools/kettle_cli.sh <kettle-ip> help
#   tools/kettle_cli.sh <kettle-ip> setstate S_Off
# Spaces are sent as "+": the kettle does not decode %20.
set -euo pipefail
if [ $# -lt 2 ]; then
  echo "usage: $0 <kettle-ip> <command...>" >&2
  exit 1
fi
ip="$1"; shift
enc=$(python3 -c 'import sys, urllib.parse; print(urllib.parse.quote_plus(sys.argv[1]))' "$*")
curl -sS --max-time 5 "http://${ip}/cli?cmd=${enc}"
echo
