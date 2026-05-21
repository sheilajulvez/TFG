cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# Incluye bootstrap si usas, define _name y _version ahí o aquí:
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

# Si no defines _name y _version en bootstrap.cmake, hazlo aquí:
# set(_name "SheilaJosePluginTest")
# set(_version "1.0.0")

project(${_name} VERSION ${_version})
add_library(${_name} MODULE)

# Forzar UTF-8 en MSVC para que los literales de texto y los logs/strings (OBS usa UTF-8)
# no salgan corruptos (mojibake) al mostrarse en OBS.
if(MSVC)
    target_compile_options(${_name} PRIVATE /utf-8)
endif()

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT            "Use Qt functionality"          ON)

# DISABLE auto-copy to Program Files to avoid LNK1104 while OBS is running
set(OBS_BUILD_DIR "" CACHE PATH "Disable auto-install" FORCE)
unset(OBS_BUILD_DIR CACHE)

include(compilerconfig)
include(defaults)
include(helpers)

# Rutas para libobs y obs-frontend-api (OBS Studio compilado localmente)
set(libobs_DIR           "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs")
set(obs-frontend-api_DIR "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/UI/obs-frontend-api")
set(w32-pthreads_DIR     "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/deps/w32-pthreads")

include_directories("${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs-opengl/include")
link_directories("${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs-opengl/lib")

find_package(libobs REQUIRED)
find_package(OpenGL REQUIRED)

if(ENABLE_FRONTEND_API)
    find_package(obs-frontend-api REQUIRED)
endif()

if(ENABLE_QT)
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/.deps/obs-deps-qt6-2024-09-12-x64")
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)
    set_target_properties(${_name} PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
    )
endif()

# ==== Assimp ====
set(ASSIMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/assimp-6.0.2")
include_directories("${ASSIMP_DIR}/include")

message(STATUS "Assimp Debug lib path: ${ASSIMP_DIR}/lib/Debug/assimp-vc143-mtd.lib")
message(STATUS "Assimp Release lib path: ${ASSIMP_DIR}/lib/Release/assimp-vc143-mt.lib")

# ==== OpenCV ====
set(OpenCV_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/Opencv")

target_include_directories(${_name} PRIVATE
    "${OpenCV_ROOT_DIR}/include"
)

link_directories("${OpenCV_ROOT_DIR}/lib")

set(OPENCV_LIBS_RELEASE
    "${OpenCV_ROOT_DIR}/lib/opencv_aruco460.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_core460.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_imgproc460.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_highgui460.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_videoio460.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_imgcodecs460.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_calib3d460.lib"
)

set(OPENCV_LIBS_DEBUG
    "${OpenCV_ROOT_DIR}/lib/opencv_aruco460d.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_core460d.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_imgproc460d.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_highgui460d.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_videoio460d.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_imgcodecs460d.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_calib3d460d.lib"
)

# ==== CURL ====
set(CURL_ROOT "${CMAKE_SOURCE_DIR}/.deps/obs-deps-2024-09-12-x64")

target_include_directories(${_name} PRIVATE
    "${CURL_ROOT}/include"
)

set(CURL_LIB_DEBUG   "${CURL_ROOT}/lib/libcurl_imp.lib")
set(CURL_LIB_RELEASE "${CURL_ROOT}/lib/libcurl_imp.lib")

# ==== Target link libraries ====
target_link_libraries(${_name} PRIVATE
    OBS::libobs
    opengl32
    $<$<BOOL:${ENABLE_FRONTEND_API}>:OBS::obs-frontend-api>
    $<$<BOOL:${ENABLE_QT}>:Qt6::Core>
    $<$<BOOL:${ENABLE_QT}>:Qt6::Widgets>
    $<$<BOOL:${ENABLE_QT}>:Qt6::OpenGL>

    $<$<CONFIG:Debug>:${ASSIMP_DIR}/lib/Debug/assimp-vc143-mtd.lib>
    $<$<CONFIG:Release>:${ASSIMP_DIR}/lib/Release/assimp-vc143-mt.lib>

    $<$<CONFIG:Debug>:${OPENCV_LIBS_DEBUG}>
    $<$<CONFIG:Release>:${OPENCV_LIBS_RELEASE}>

    $<$<CONFIG:Debug>:${CURL_LIB_DEBUG}>
    $<$<CONFIG:Release>:${CURL_LIB_RELEASE}>
)

target_include_directories(${_name} PRIVATE
    "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs/include"
)

target_sources(${_name} PRIVATE
    src/main_filter.c
    src/SJ_3DModel.c
    src/aruco_detector.cpp
    src/yuv2bgra.cpp
    src/web_sync.c
    src/countdown_clock.c
    src/json_utils.c
)


set_target_properties_plugin(${_name} PROPERTIES OUTPUT_NAME ${_name})


add_custom_command(TARGET ${_name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${_name}> "${CMAKE_CURRENT_BINARY_DIR}/Release/$<TARGET_FILE_NAME:${_name}>"
    COMMAND ${CMAKE_COMMAND} -E copy "${CURL_ROOT}/bin/libcurl.dll" "${CMAKE_CURRENT_BINARY_DIR}/Release/libcurl.dll"
    COMMENT "Copying plugin and dependencies to local build directory..."
)
