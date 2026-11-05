#!/usr/bin/env bash
# Builds and runs the 32x8 preview on the PC (needs g++ and the ArduinoJson
# library that PlatformIO downloads for the native env: run `pio test -e native`
# once first). HTML output: tools/preview/out/preview.html
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/tools/preview/out"
AJ="$ROOT/.pio/libdeps/native/ArduinoJson/src"
if [ ! -d "$AJ" ]; then
  echo "ArduinoJson not found. Run: pio pkg install -e native   (or pio test -e native)" >&2
  exit 1
fi
mkdir -p "$OUT"
g++ -std=gnu++17 -O1 -Wall -Wextra -I"$ROOT/lib/GoldCore/src" -I"$AJ" \
  "$ROOT/tools/preview/preview.cpp" "$ROOT"/lib/GoldCore/src/*.cpp -o "$OUT/preview"
"$OUT/preview" --html "$OUT/preview.html" "$@"
