#!/usr/bin/env bash
set -euo pipefail
mkdir -p artifacts/logs
cp -R memory/runtime/. upstream/runtime/
xcodebuild -version > artifacts/logs/toolchain.txt
xcrun --sdk iphoneos --show-sdk-version >> artifacts/logs/toolchain.txt
git -C upstream rev-parse HEAD >> artifacts/logs/toolchain.txt
uname -m >> artifacts/logs/toolchain.txt

# Execute the same vm_* backend and test cases on the host, separately from iPad evidence.
xcrun --sdk macosx clang++ -std=c++17 -O2 -DMKW_IOS_MEMORY_TESTING -DMKW_IOS_MEMORY_HOST_TEST \
  -I upstream/runtime/include -I upstream/runtime/tests \
  upstream/runtime/src/guest_flat_memory_ios.cpp upstream/runtime/tests/ios_memory_tests.cpp \
  upstream/runtime/tests/ios_memory_access_tests.cpp upstream/runtime/tests/ios_memory_host_main.cpp \
  -o artifacts/memory-host-tests 2>&1 | tee artifacts/logs/host-build.txt
artifacts/memory-host-tests 2>&1 | tee artifacts/logs/host-tests.txt
bash memory/check-contracts.sh 2>&1 | tee artifacts/logs/contract-regressions.txt

cmake -S upstream/runtime -B build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 -DCMAKE_BUILD_TYPE=Release \
  -DMKW_BUILD_PRODUCTS=OFF -DMKW_BUILD_IOS_MEMORY_TESTS=ON \
  2>&1 | tee artifacts/logs/ios-configure.txt
cmake --build build-ios --config Release --target WiiCompiledMemoryTests -- \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO 2>&1 | tee artifacts/logs/ios-build.txt
app="build-ios/Release-iphoneos/WiiCompiledMemoryTests.app"
test -f "$app/WiiCompiledMemoryTests"
lipo -info "$app/WiiCompiledMemoryTests" | tee artifacts/logs/architecture.txt
plutil -lint "$app/Info.plist" | tee artifacts/logs/plist.txt
mkdir -p artifacts/package/Payload
ditto "$app" artifacts/package/Payload/WiiCompiledMemoryTests.app
(cd artifacts/package && zip -q -r ../WiiCompiledMemoryTests-unsigned.ipa Payload)

cmake -S upstream/runtime -B build-macos -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" -DMKW_BUILD_PRODUCTS=OFF \
  -DMKW_PLATFORM_TESTS_ONLY=ON -DMKW_BUILD_PSQ_TESTS=ON \
  2>&1 | tee artifacts/logs/macos-configure.txt
cmake --build build-macos -j 4 2>&1 | tee artifacts/logs/macos-build.txt
ctest --test-dir build-macos --output-on-failure 2>&1 | tee artifacts/logs/macos-tests.txt
printf 'Host tests and device compilation passed; physical iPad execution pending.\n' | tee artifacts/logs/status.txt
