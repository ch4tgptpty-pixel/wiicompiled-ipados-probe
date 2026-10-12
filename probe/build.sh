#!/usr/bin/env bash
set -euo pipefail
script_dir="$(dirname "$0")"
root="$(cd "$script_dir/.." && pwd)"
cd "$root"
mkdir -p artifacts/logs
sdk="$(xcrun --sdk iphoneos --show-sdk-path)"
xcodebuild -version > artifacts/logs/toolchain.txt
xcrun --sdk iphoneos --show-sdk-version >> artifacts/logs/toolchain.txt
git -C upstream rev-parse HEAD >> artifacts/logs/toolchain.txt

# Audit original sources without editing them. Failures are recorded explicitly;
# this does not turn a failed audit into a claim that the full runtime compiles.
audit() {
  local name="$1" source="$2"
  if xcrun --sdk iphoneos clang++ -target arm64-apple-ios16.0 -isysroot "$sdk" \
      -std=c++17 -fsyntax-only -I upstream/runtime/include "$source" \
      > "artifacts/logs/$name.txt" 2>&1; then
    printf '%s: PASS (syntax only)\n' "$name" | tee -a artifacts/logs/audit-status.txt
  else
    printf '%s: FAIL (see log)\n' "$name" | tee -a artifacts/logs/audit-status.txt
  fi
}
audit guest-memory-original upstream/runtime/src/guest_flat_memory_macos.cpp
audit external-audio-original upstream/runtime/src/external_audio_macos.cpp
audit host-paths-original upstream/runtime/src/platform/host_platform.cpp

cmake -S probe -B build -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DUPSTREAM_DIR="$root/upstream" 2>&1 | tee artifacts/logs/configure.txt
cmake --build build --config Release --target WiiCompiledProbe -- \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO 2>&1 | tee artifacts/logs/build.txt
app="build/Release-iphoneos/WiiCompiledProbe.app"
test -f "$app/WiiCompiledProbe"
mkdir -p artifacts/package/Payload
ditto "$app" artifacts/package/Payload/WiiCompiledProbe.app
(cd artifacts/package && /usr/bin/zip -q -r ../WiiCompiledProbe-unsigned.ipa Payload)
printf 'Unsigned diagnostic IPA built. Full game not built or tested.\n' | tee artifacts/logs/package-status.txt
