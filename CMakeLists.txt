cmake_minimum_required(VERSION 3.24)

project(SyntheticDefectGenerator
    VERSION 0.1.0
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

# ─── Dependencies ─────────────────────────────────────────────────────────────
# Qt6: set Qt6_DIR or CMAKE_PREFIX_PATH to your Qt install, e.g.:
#   -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Concurrent)

# OpenCV: set OpenCV_DIR to your OpenCV build dir, e.g.:
#   -DOpenCV_DIR="C:/opencv/build"
find_package(OpenCV REQUIRED)

# ─── CUDA (Phase 3 — uncomment when ready) ────────────────────────────────────
# enable_language(CUDA)
# set(CMAKE_CUDA_STANDARD 17)
# find_package(CUDAToolkit REQUIRED)

# ─── Source Files ─────────────────────────────────────────────────────────────
set(SDG_SOURCES
    src/main.cpp
    src/ui/MainWindow.cpp
    src/camera/MockCameraModule.cpp
    src/core/focal_stack/FocalStackProcessor.cpp
    src/core/depth_map/DepthMapReconstructor.cpp
    src/core/defect_gen/DefectGenerator.cpp
)

set(SDG_HEADERS
    src/ui/MainWindow.h
    src/camera/ICameraModule.h
    src/camera/MockCameraModule.h
    src/core/focal_stack/FocalStackProcessor.h
    src/core/depth_map/DepthMapReconstructor.h
    src/core/defect_gen/DefectGenerator.h
    src/core/defect_gen/DefectTypes.h
)

set(SDG_UI_FILES
    src/ui/MainWindow.ui
)

# ─── Executable ───────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}
    ${SDG_SOURCES}
    ${SDG_HEADERS}
    ${SDG_UI_FILES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Concurrent
    ${OpenCV_LIBS}
    # CUDA::cudart        # Phase 3
)

# ─── Install Qt DLLs on Windows (Release builds) ──────────────────────────────
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe"
            --no-translations
            $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Deploying Qt runtime..."
    )
endif()
