add_library(viture_frame_hub SHARED viture_frame_hub.cpp)

target_include_directories(viture_frame_hub PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(viture_frame_hub PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME "VitureFrameHub"
    MACOSX_RPATH ON
    INSTALL_NAME_DIR "@rpath"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/package/support"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/package/support"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/package/support")

add_custom_command(TARGET viture_frame_hub POST_BUILD
    COMMAND /usr/bin/codesign --force --sign - "$<TARGET_FILE:viture_frame_hub>"
    VERBATIM)

if(BUILD_TESTING)
    add_executable(viture_frame_hub_test viture_frame_hub_test.cpp)
    target_link_libraries(viture_frame_hub_test PRIVATE viture_frame_hub)
    set_target_properties(viture_frame_hub_test PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        BUILD_RPATH "${CMAKE_BINARY_DIR}/package/support")
    add_test(NAME viture_frame_hub COMMAND viture_frame_hub_test)
endif()
