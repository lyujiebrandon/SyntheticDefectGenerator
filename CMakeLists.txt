cmake_minimum_required(VERSION 3.24)

project(SyntheticDefectGenerator
    VERSION 0.3.0
    DESCRIPTION "Automated 3D Synthetic Defect Generator via Depth from Focus"
    LANGUAGES CXX
)

# ─── C++ Standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Qt Automation ───────────────────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/src/ui)

# ─── Qt6 ─────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Concurrent)

# ─── OpenCV ──────────────────────────────────────────────────────────────────
set(OpenCV_ARCH x64)
set(OpenCV_RUNTIME vc16)
find_package(OpenCV REQUIRED)

# ─── CUDA (Phase 3 — uncomment when ready) ────────────────────────────────────
# enable_language(CUDA)
# set(CMAKE_CUDA_STANDARD 17)
# find_package(CUDAToolkit REQUIRED)

# ─── Suppress Windows min/max macros (conflict with OpenCV templates) ────────
add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

# ─── Core pipeline library (no UI, linkable by tests) ────────────────────────
add_library(SDG_Core STATIC
    src/core/focal_stack/FocalStackProcessor.cpp
    src/core/image_registration/ImageRegistrator.cpp
    src/core/depth_map/DepthMapReconstructor.cpp
    src/core/defect_gen/DefectGenerator.cpp
)

target_include_directories(SDG_Core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(SDG_Core PUBLIC
    Qt6::Core
    ${OpenCV_LIBS}
)

# ─── Main executable ──────────────────────────────────────────────────────────
set(SDG_HEADERS
    src/ui/MainWindow.h
    src/ui/ZoomableImageLabel.h
    src/core/focal_stack/FocalStackProcessor.h
    src/core/image_registration/ImageRegistrator.h
    src/core/depth_map/DepthMapReconstructor.h
    src/core/defect_gen/DefectGenerator.h
    src/core/defect_gen/DefectTypes.h
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/ZoomableImageLabel.cpp
    ${SDG_HEADERS}
    src/ui/MainWindow.ui
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDG_Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Concurrent
    # CUDA::cudart   # Phase 3
)

# ─── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)

# ─── Deploy runtime DLLs (Windows) ───────────────────────────────────────────
if(WIN32)
    # Step 1: copy all imported DLLs (OpenCV, etc.) into the output directory.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs..."
    )

    # Step 2: windeployqt adds Qt DLLs and platform plugins.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe"
            --no-translations
            $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Deploying Qt runtime..."
    )
endif()
