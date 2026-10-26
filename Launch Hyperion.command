#!/bin/zsh
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
if [ -x "$ROOT/Hyperion.app/Contents/MacOS/Hyperion" ]; then
  open "$ROOT/Hyperion.app"
else
  cd "$ROOT"
  CMAKE_BIN="$(command -v cmake || true)"
  if [ -z "$CMAKE_BIN" ] && [ -x "$HOME/.local/bin/cmake" ]; then CMAKE_BIN="$HOME/.local/bin/cmake"; fi
  if [ -z "$CMAKE_BIN" ]; then print 'CMake is required to build this source copy.'; read; exit 1; fi
  "$CMAKE_BIN" -S . -B build -DCMAKE_BUILD_TYPE=Release
  "$CMAKE_BIN" --build build -j 4
  open "$ROOT/build/Hyperion.app"
fi
