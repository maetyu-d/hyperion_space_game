#!/bin/zsh
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
APP="$ROOT/Hyperion.app"
if [ ! -x "$APP/Contents/MacOS/Hyperion" ]; then APP="$ROOT/build/Hyperion.app"; fi
open -n "$APP" --args --no-save --graphics high
