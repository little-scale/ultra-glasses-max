set_property(DIRECTORY PROPERTY max::external YES)
set(AUTHOR_DOMAIN "com.sebastiantomczak")
set(BUNDLE_IDENTIFIER "ultra-glasses-max-stereo")
set(GIT_VERSION_TAG "0.4.1")
set(COPYRIGHT_STRING "2026 Sebastian Tomczak")
project(jit.viture.stereo)

target_sources(jit.viture.stereo PRIVATE jit.viture.stereo.cpp)
target_include_directories(jit.viture.stereo PRIVATE
    "${CMAKE_SOURCE_DIR}/source/frame-hub")
target_link_libraries(jit.viture.stereo PRIVATE viture_frame_hub)

set_target_properties(jit.viture.stereo PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    BUILD_WITH_INSTALL_RPATH ON
    BUILD_RPATH "@loader_path/../../../../support"
    INSTALL_RPATH "@loader_path/../../../../support")

add_custom_command(TARGET jit.viture.stereo POST_BUILD
    COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:jit.viture.stereo>"
    VERBATIM)
