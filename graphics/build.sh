#!/usr/bin/env bash
set -euo pipefail
mkdir -p artifacts/logs
cp -R graphics upstream/
cp -R graphics-overlay/aurora-main/. upstream/aurora-main/
cmake -S upstream/graphics -B build-graphics -G Ninja \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DCMAKE_BUILD_TYPE=Release 2>&1 | tee artifacts/logs/configure.txt
# Exercise the actual lifecycle bridge on the host without a graphics device.
clang++ -std=c++20 -pthread -Iupstream/aurora-main/include \
  -Ibuild-graphics/_deps/sdl-src/include graphics/callback-event-tests.cpp \
  -o callback-event-tests
./callback-event-tests | tee artifacts/logs/callback-tests.txt
cmake --build build-graphics --target WiiCompiledGraphicsTests -j 3 2>&1 | tee artifacts/logs/build.txt
ninja -C build-graphics -t commands > artifacts/logs/commands.txt
python3 graphics/audit-libraries.py build-graphics artifacts/logs/commands.txt | tee artifacts/logs/library-audit.txt
app=build-graphics/WiiCompiledGraphicsTests.app
/usr/libexec/PlistBuddy -c Print "$app/Info.plist" > artifacts/logs/plist.txt
otool -l "$app/WiiCompiledGraphicsTests" > artifacts/logs/macho.txt
mkdir -p artifacts/package/Payload
cp -R "$app" artifacts/package/Payload/
(cd artifacts/package && zip -q -r ../WiiCompiledGraphicsTests-unsigned.ipa Payload)
python3 graphics/verify-ipa.py artifacts/WiiCompiledGraphicsTests-unsigned.ipa | tee artifacts/logs/ipa-validation.txt
