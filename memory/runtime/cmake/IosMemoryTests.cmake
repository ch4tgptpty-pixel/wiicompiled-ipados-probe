enable_language(OBJCXX ASM)
add_executable(WiiCompiledMemoryTests MACOSX_BUNDLE
    tests/ios_memory_app.mm tests/ios_memory_tests.cpp tests/ios_memory_access_tests.cpp
    tests/host_context_tests.cpp src/guest_flat_memory_ios.cpp
    src/host_context.cpp src/platform/macos/co_switch.S)
target_include_directories(WiiCompiledMemoryTests PRIVATE include tests)
target_compile_features(WiiCompiledMemoryTests PRIVATE cxx_std_17)
target_compile_definitions(WiiCompiledMemoryTests PRIVATE MKW_IOS_MEMORY_TESTING)
set_source_files_properties(tests/host_context_tests.cpp PROPERTIES
    COMPILE_DEFINITIONS "main=MkwContextTestMain")
set_source_files_properties(tests/ios_memory_app.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
target_link_libraries(WiiCompiledMemoryTests PRIVATE "-framework UIKit" "-framework Foundation")
set_target_properties(WiiCompiledMemoryTests PROPERTIES
    OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED YES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/../tests/ios_memory_Info.plist"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.local.wiicompiled.memorytests"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "2"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
