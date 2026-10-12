cmake_minimum_required(VERSION 3.25)
project(WiiCompiledProbe LANGUAGES C CXX OBJCXX ASM)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  message(FATAL_ERROR "This disposable probe requires the iOS SDK and an arm64 device target")
endif()
if(NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
  message(FATAL_ERROR "Build for a physical arm64 iPad, not a simulator")
endif()
set(UPSTREAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../upstream" CACHE PATH "Pinned WiiCompiled checkout")
if(NOT EXISTS "${UPSTREAM_DIR}/runtime/src/host_context.cpp")
  message(FATAL_ERROR "Missing upstream checkout; use the supplied GitHub Actions workflow")
endif()

add_executable(WiiCompiledProbe MACOSX_BUNDLE
  main.mm
  "${UPSTREAM_DIR}/runtime/src/host_context.cpp"
  "${UPSTREAM_DIR}/runtime/src/platform/macos/co_switch.S")
target_include_directories(WiiCompiledProbe PRIVATE "${UPSTREAM_DIR}/runtime/include")
target_compile_features(WiiCompiledProbe PRIVATE cxx_std_17)
set_source_files_properties(main.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
target_link_libraries(WiiCompiledProbe PRIVATE
  "-framework UIKit" "-framework Foundation" "-framework Metal" "-framework MetalKit"
  "-framework CoreGraphics")
set_target_properties(WiiCompiledProbe PROPERTIES
  OBJCXX_STANDARD 17
  OBJCXX_STANDARD_REQUIRED YES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.local.wiicompiled.probe"
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "2"
  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
  XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
  XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
