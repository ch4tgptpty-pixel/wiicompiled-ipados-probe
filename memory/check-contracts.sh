#!/usr/bin/env bash
set -euo pipefail
mkdir -p artifacts/mutants
cp -R upstream/runtime/include artifacts/mutants/include
python3 - <<'PY'
from pathlib import Path
s=Path('upstream/runtime/src/guest_flat_memory_ios.cpp').read_text()
needle='VM_FAIL(Remap)?KERN_RESOURCE_SHORTAGE:\n                vm_remap('
assert s.count(needle)==1
s=s.replace(needle,'VM_FAIL(Remap)?KERN_RESOURCE_SHORTAGE:\n                (r.base==0xc0000000u || r.base==0xd0000000u)?KERN_SUCCESS:\n                vm_remap(')
Path('artifacts/mutants/no-uncached.cpp').write_text(s)
p=Path('artifacts/mutants/include/isa/ppc_isa_quantized.h')
s=p.read_text()
needle='MKW_PPC_FORCE_INLINE uint8_t* PpcTryGetPsqWritableHostInline(uint32_t addr)\n{\n    if (GuestFlat::RequiresCheckedAccess()) [[unlikely]]\n        return nullptr;'
assert s.count(needle)==1
p.write_text(s.replace(needle,'MKW_PPC_FORCE_INLINE uint8_t* PpcTryGetPsqWritableHostInline(uint32_t addr)\n{'))
PY
common=(-std=c++17 -O2 -DMKW_IOS_MEMORY_TESTING -DMKW_IOS_MEMORY_HOST_TEST -I upstream/runtime/tests)
tests=(upstream/runtime/tests/ios_memory_tests.cpp upstream/runtime/tests/ios_memory_access_tests.cpp upstream/runtime/tests/ios_memory_host_main.cpp)
# Preserve the actual pre-review tests to demonstrate that these omissions
# previously escaped coverage. Mutations stay in artifacts, never in the IPA.
xcrun --sdk macosx clang++ "${common[@]}" -I upstream/runtime/include \
  artifacts/mutants/no-uncached.cpp memory/baseline-alias-tests.cpp \
  upstream/runtime/tests/ios_memory_access_tests.cpp upstream/runtime/tests/ios_memory_host_main.cpp \
  -o artifacts/mutants/baseline-no-uncached
artifacts/mutants/baseline-no-uncached > artifacts/logs/baseline-no-uncached.txt
printf 'Baseline uncached omission: reproduced (old tests passed broken guest mapping)\n'
xcrun --sdk macosx clang++ "${common[@]}" -I artifacts/mutants/include -I artifacts/mutants/include/isa \
  upstream/runtime/src/guest_flat_memory_ios.cpp upstream/runtime/tests/ios_memory_tests.cpp \
  memory/baseline-access-tests.cpp upstream/runtime/tests/ios_memory_host_main.cpp \
  -o artifacts/mutants/baseline-no-psq-check
artifacts/mutants/baseline-no-psq-check > artifacts/logs/baseline-no-psq-check.txt
printf 'Baseline PSQ omission: reproduced (old tests passed broken checked routing)\n'
xcrun --sdk macosx clang++ "${common[@]}" -I upstream/runtime/include \
  artifacts/mutants/no-uncached.cpp "${tests[@]}" -o artifacts/mutants/no-uncached
xcrun --sdk macosx clang++ "${common[@]}" -I artifacts/mutants/include -I artifacts/mutants/include/isa \
  upstream/runtime/src/guest_flat_memory_ios.cpp "${tests[@]}" -o artifacts/mutants/no-psq-check
missing=0
for name in no-uncached no-psq-check; do
  if artifacts/mutants/"$name" > "artifacts/logs/mutant-$name.txt" 2>&1; then
    printf '%s: FAIL (broken implementation escaped tests)\n' "$name"
    missing=1
  else
    printf '%s: PASS (broken implementation rejected)\n' "$name"
  fi
done
if cmake -S upstream/runtime -B artifacts/simulator-rejection -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 -DCMAKE_BUILD_TYPE=Release \
  -DMKW_BUILD_PRODUCTS=OFF -DMKW_BUILD_IOS_MEMORY_TESTS=ON > artifacts/logs/simulator-rejection.txt 2>&1; then
  printf 'Simulator rejection: FAIL (unsupported SDK accepted)\n'; missing=1
elif grep -q 'iOS memory tests require the physical iphoneos SDK' artifacts/logs/simulator-rejection.txt; then
  printf 'Simulator rejection: PASS\n'
else
  cat artifacts/logs/simulator-rejection.txt
  printf 'Simulator rejection: FAIL (unrelated configure failure)\n'; missing=1
fi
exit "$missing"
