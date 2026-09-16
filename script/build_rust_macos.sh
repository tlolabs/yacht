#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="$HOME/.cargo/bin:$PATH"
export MACOSX_DEPLOYMENT_TARGET=14.0
mkdir -p target/swift
mode="${CONFIGURATION:-Debug}"
profile=debug
flag=""
if [[ "$mode" == Release ]]; then profile=release; flag=--release; fi
libraries=()
cli=()
for arch in ${ARCHS:-$(uname -m)}; do
  case "$arch" in arm64) triple=aarch64-apple-darwin;; x86_64) triple=x86_64-apple-darwin;; *) exit 1;; esac
  cargo build --locked --workspace --target "$triple" ${flag}
  libraries+=("target/$triple/$profile/libyacht_ffi.a")
  cli+=("target/$triple/$profile/yacht")
done
lipo -create "${libraries[@]}" -output target/swift/libyacht_ffi.a
lipo -create "${cli[@]}" -output target/swift/yacht
