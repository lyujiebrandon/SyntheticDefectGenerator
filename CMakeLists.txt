cmake_minimum_required(VERSION 3.24)

project(SyntheticDefectGenerator
    VERSION 0.2.0
    DESCRIPTION "Automated 3D Synthetic Defect Generator via Depth from Focus (Liquid Lens)"
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
find_package(Qt6 OPTIONAL_COMPONENTS SerialPort)   # needed for Optotune lens control

# ─── OpenCV (force vc16 runtime for pre-built binaries on VS 2022) ────────────
set(OpenCV_ARCH x64)
set(OpenCV_RUNTIME vc16)
find_package(OpenCV REQUIRED)

# ─── SVS-VISTEK SVCam Kit SDK (optional) ─────────────────────────────────────
set(SVS_SDK_ROOT "C:/Program Files/SVS-VISTEK GmbH/SVCam Kit/SDK")
set(SVS_SDK_INCLUDE "${SVS_SDK_ROOT}/include")
set(SVS_SDK_LIB     "${SVS_SDK_ROOT}/lib/SVGenSDK64.lib")

if(EXISTS "${SVS_SDK_INCLUDE}/sv_gen_sdk.h" AND EXISTS "${SVS_SDK_LIB}")
    set(SDG_HAVE_SVS_SDK TRUE)
    message(STATUS "SVS-VISTEK SDK found — real camera support enabled")
else()
    set(SDG_HAVE_SVS_SDK FALSE)
    message(STATUS "SVS-VISTEK SDK NOT found at ${SVS_SDK_ROOT} — real camera support disabled (install SDK to enable)")
endif()

# ─── CUDA (Phase 3 — uncomment when ready) ────────────────────────────────────
# enable_language(CUDA)
# set(CMAKE_CUDA_STANDARD 17)
# find_package(CUDAToolkit REQUIRED)

# ─── Source Files ─────────────────────────────────────────────────────────────
set(SDG_SOURCES
    src/main.cpp

    # UI
    src/ui/MainWindow.cpp

    # Camera
    src/camera/MockCameraModule.cpp
    src/camera/FileCameraModule.cpp

    # Core pipeline
    src/core/focal_stack/FocalStackProcessor.cpp
    src/core/image_registration/ImageRegistrator.cpp
    src/core/depth_map/DepthMapReconstructor.cpp
    src/core/defect_gen/DefectGenerator.cpp
)

set(SDG_HEADERS
    src/ui/MainWindow.h

    # Camera
    src/camera/ICameraModule.h
    src/camera/ILiquidLensController.h
    src/camera/MockCameraModule.h
    src/camera/MockLiquidLensController.h
    src/camera/FileCameraModule.h

    # Core pipeline
    src/core/focal_stack/FocalStackProcessor.h
    src/core/image_registration/ImageRegistrator.h
    src/core/depth_map/DepthMapReconstructor.h
    src/core/defect_gen/DefectGenerator.h
    src/core/defect_gen/DefectTypes.h
)

set(SDG_UI_FILES
    src/ui/MainWindow.ui
)

if(SDG_HAVE_SVS_SDK)
    list(APPEND SDG_SOURCES src/camera/RealCameraModule.cpp)
    list(APPEND SDG_HEADERS src/camera/RealCameraModule.h)
endif()

# ─── Suppress Windows min/max macros (conflict with OpenCV templates) ────────
add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

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
    # CUDA::cudart   # Phase 3
)

if(SDG_HAVE_SVS_SDK)
    target_include_directories(${PROJECT_NAME} PRIVATE ${SVS_SDK_INCLUDE})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SVS_SDK_LIB})
    target_compile_definitions(${PROJECT_NAME} PRIVATE SDG_HAVE_SVS_SDK)
endif()

# ─── Qt SerialPort (optional — enables Optotune lens control) ─────────────────
if(Qt6SerialPort_FOUND)
    target_sources(${PROJECT_NAME} PRIVATE
        src/camera/RealLiquidLensController.cpp
        src/camera/RealLiquidLensController.h
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::SerialPort)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SDG_HAVE_SERIAL_PORT)
    message(STATUS "Qt6 SerialPort found — Optotune lens control enabled")
else()
    message(STATUS "Qt6 SerialPort NOT found — lens will use mock control (install via Qt Maintenance Tool to enable)")
endif()

# ─── Deploy Qt DLLs (Windows Release) ────────────────────────────────────────
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe"
            --no-translations
            $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Deploying Qt runtime..."
    )
endif()
