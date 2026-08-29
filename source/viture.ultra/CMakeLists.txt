set_property(DIRECTORY PROPERTY max::external YES)
set(AUTHOR_DOMAIN "com.sebastiantomczak")
set(BUNDLE_IDENTIFIER "ultra-glasses-max-tracking")
set(GIT_VERSION_TAG "0.4.1")
set(COPYRIGHT_STRING "2026 Sebastian Tomczak")
project(viture.ultra)

foreach(required_file
        "${VITURE_SDK_ROOT}/include/viture_glasses_provider.h"
        "${VITURE_SDK_ROOT}/aarch64/libglasses.dylib"
        "${VITURE_SDK_ROOT}/aarch64/libcarina_vio.dylib")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Missing VITURE SDK file: ${required_file}")
    endif()
endforeach()

target_sources(viture.ultra PRIVATE viture.ultra.cpp)
target_include_directories(viture.ultra PRIVATE
    "${VITURE_SDK_ROOT}/include"
    "${CMAKE_SOURCE_DIR}/source/frame-hub")

add_library(viture_glasses SHARED IMPORTED)
set_target_properties(viture_glasses PROPERTIES
    IMPORTED_LOCATION "${VITURE_SDK_ROOT}/aarch64/libglasses.dylib")

target_link_libraries(viture.ultra PRIVATE
    viture_frame_hub
    viture_glasses
    "-framework CoreFoundation"
    "-framework IOKit")

set_target_properties(viture.ultra PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    BUILD_WITH_INSTALL_RPATH ON
    BUILD_RPATH "@loader_path/../Frameworks;@loader_path/../../../../support"
    INSTALL_RPATH "@loader_path/../Frameworks;@loader_path/../../../../support")

# Bundle private SDK copies inside the external. SDK 2.4.0's libglasses.dylib
# has a malformed linker signature, so sign only these build copies before
# re-signing the complete .mxo bundle. The downloaded SDK is never modified.
add_custom_command(TARGET viture.ultra POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_BUNDLE_DIR:viture.ultra>/Contents/Frameworks"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${VITURE_SDK_ROOT}/aarch64/libglasses.dylib"
            "$<TARGET_BUNDLE_DIR:viture.ultra>/Contents/Frameworks/libglasses.dylib"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${VITURE_SDK_ROOT}/aarch64/libcarina_vio.dylib"
            "$<TARGET_BUNDLE_DIR:viture.ultra>/Contents/Frameworks/libcarina_vio.dylib"
    COMMAND /usr/bin/codesign --force --sign -
            "$<TARGET_BUNDLE_DIR:viture.ultra>/Contents/Frameworks/libglasses.dylib"
    COMMAND /usr/bin/codesign --force --sign -
            "$<TARGET_BUNDLE_DIR:viture.ultra>/Contents/Frameworks/libcarina_vio.dylib"
    COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:viture.ultra>"
    VERBATIM)
