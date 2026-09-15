#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode.app/Contents/Developer ]]; then export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer; fi
MODE="${1:-run}"
pkill -x YachtApp >/dev/null 2>&1 || true
xcodebuild -project Yacht.xcodeproj -scheme Yacht -configuration Debug -derivedDataPath build -destination 'platform=macOS' build -quiet
mkdir -p dist
rm -rf -- "$ROOT_DIR/dist/Y.A.C.H.T..app"
ditto 'build/Build/Products/Debug/Y.A.C.H.T..app' 'dist/Y.A.C.H.T..app'
case "$MODE" in
  --build-only) ;;
  run) open -n 'dist/Y.A.C.H.T..app' ;;
  --verify) open -n 'dist/Y.A.C.H.T..app'; sleep 2; pgrep -x YachtApp >/dev/null ;;
  --debug) lldb -- 'dist/Y.A.C.H.T..app/Contents/MacOS/YachtApp' ;;
  --logs|--telemetry) open -n 'dist/Y.A.C.H.T..app'; /usr/bin/log stream --info --style compact --predicate 'process == "YachtApp"' ;;
  *) echo 'Usage: build_and_run.sh [--build-only|--verify|--debug|--logs|--telemetry]' >&2; exit 2 ;;
esac
