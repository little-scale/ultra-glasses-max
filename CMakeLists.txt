cmake_minimum_required(VERSION 3.25)

# VITURE's macOS SDK is arm64-only and requires macOS 13.
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "macOS architectures" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0 CACHE STRING "Minimum macOS version" FORCE)
set(MAX_SDK_PACKAGE_OUT_OF_TREE ON CACHE BOOL "Build an assembled Max package" FORCE)

set(MAX_SDK_BASE_PATH "" CACHE PATH "Optional local checkout of Cycling '74 max-sdk-base")
if(MAX_SDK_BASE_PATH AND EXISTS "${MAX_SDK_BASE_PATH}/CMakeLists.txt")
    add_subdirectory("${MAX_SDK_BASE_PATH}" "${CMAKE_BINARY_DIR}/max-sdk-base")
else()
    include(FetchContent)
    FetchContent_Declare(
        max-sdk-base
        GIT_REPOSITORY https://github.com/Cycling74/max-sdk-base.git
        GIT_TAG c03a2922a2a8ff149165a1e2ea134e9321d6e202
    )
    FetchContent_MakeAvailable(max-sdk-base)
endif()

project(UltraGlassesMax VERSION 0.4.1 LANGUAGES C CXX)
include(CTest)

# The proprietary VITURE SDK is intentionally not stored in this repository.
# Point CMake at an SDK 2.4.0 download with either the environment variable or
# an explicit -DVITURE_SDK_ROOT=/absolute/path argument.
set(VITURE_SDK_ROOT "$ENV{VITURE_SDK_ROOT}" CACHE PATH
    "Path to VITURE_XR_Glasses_SDK_for_MacOS_arm64")
if(NOT VITURE_SDK_ROOT)
    message(FATAL_ERROR
        "VITURE_SDK_ROOT is required. Download the macOS arm64 Glasses SDK "
        "from VITURE's developer portal and point this build at its root.")
endif()

add_subdirectory(source/frame-hub)
add_subdirectory(source/viture.ultra)
add_subdirectory(source/jit.viture.stereo)

# These notices travel with local package builds. Public binary distribution
# additionally requires an appropriate end-user agreement; see docs/RELEASING.md.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/package")
configure_file("${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
               "${CMAKE_BINARY_DIR}/package/THIRD_PARTY_NOTICES.md" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/docs/BINARY_DISTRIBUTION.md"
               "${CMAKE_BINARY_DIR}/package/BINARY_DISTRIBUTION.md" COPYONLY)

# max-sdk-base refreshes bundle metadata during configuration. Always perform a
# final signing pass, even when the external binaries did not need relinking.
add_custom_target(viture_package_finalize ALL
    COMMAND /usr/bin/codesign --force --sign - "$<TARGET_FILE:viture_frame_hub>"
    COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:viture.ultra>"
    COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:jit.viture.stereo>"
    DEPENDS viture_frame_hub viture.ultra jit.viture.stereo
    VERBATIM)
