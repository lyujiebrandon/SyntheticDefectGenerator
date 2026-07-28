include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(SDG_Tests
    test_focal_stack_processor.cpp
    test_image_registrator.cpp
    test_depth_map_reconstructor.cpp
    test_defect_generator.cpp
)

target_link_libraries(SDG_Tests PRIVATE
    GTest::gtest_main
    SDG_Core
)

if(WIN32)
    # Step 1: copy all imported DLLs (Qt Core, OpenCV, etc.) into the output dir.
    add_custom_command(TARGET SDG_Tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:SDG_Tests>
            $<TARGET_FILE_DIR:SDG_Tests>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs for SDG_Tests..."
    )

    # Step 2: windeployqt adds Qt platform plugins not covered by the copy above.
    add_custom_command(TARGET SDG_Tests POST_BUILD
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe"
            --no-translations
            $<TARGET_FILE:SDG_Tests>
        COMMENT "Deploying Qt plugins for SDG_Tests..."
    )
endif()

include(GoogleTest)
# PRE_TEST: discover tests when ctest runs (after build + windeployqt are done).
gtest_discover_tests(SDG_Tests DISCOVERY_MODE PRE_TEST)
