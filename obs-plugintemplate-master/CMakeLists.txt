cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# bootstrap.cmake carga metadatos del plugin desde buildspec.json
# y configura módulos/plataforma comunes.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})
add_library(${_name} MODULE)

# Fuerza UTF-8 en MSVC para evitar texto corrupto en logs y propiedades de OBS.
if(MSVC)
    target_compile_options(${_name} PRIVATE /utf-8)
endif()

# Desactiva la copia automática al árbol de OBS; este proyecto copia al build local.
unset(OBS_BUILD_DIR CACHE)

include(compilerconfig)
include(defaults)
include(helpers)

# libobs compilado localmente desde las dependencias de OBS.
set(libobs_DIR           "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs")
# libobsConfig.cmake resuelve w32-pthreads como dependencia transitiva en Windows.
set(w32-pthreads_DIR     "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/deps/w32-pthreads")

find_package(libobs REQUIRED)

# ==== Dependencias externas ====
# Assimp: carga de modelos 3D.
set(ASSIMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/assimp-6.0.2")
include_directories("${ASSIMP_DIR}/include")

message(STATUS "Assimp Debug lib path: ${ASSIMP_DIR}/lib/Debug/assimp-vc143-mtd.lib")
message(STATUS "Assimp Release lib path: ${ASSIMP_DIR}/lib/Release/assimp-vc143-mt.lib")

# OpenCV: ArUco y matemáticas/transformaciones de imagen necesarias para detección.
set(OpenCV_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/Opencv")

target_include_directories(${_name} PRIVATE
    "${OpenCV_ROOT_DIR}/include"
)

link_directories("${OpenCV_ROOT_DIR}/lib")

set(OPENCV_LIBS_RELEASE
    "${OpenCV_ROOT_DIR}/lib/opencv_aruco460.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_core460.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_imgproc460.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_calib3d460.lib"
)

set(OPENCV_LIBS_DEBUG
    "${OpenCV_ROOT_DIR}/lib/opencv_aruco460d.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_core460d.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_imgproc460d.lib"
    "${OpenCV_ROOT_DIR}/lib/opencv_calib3d460d.lib"
)

# libcurl: sincronización HTTP con DOMjudge.
set(CURL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/curl-8.18.0_1-win64-mingw")

target_include_directories(${_name} PRIVATE
    "${CURL_ROOT}/include"
)

set(CURL_LIB_DEBUG   "${CURL_ROOT}/lib/Debug/libcurl-d_imp.lib")
set(CURL_LIB_RELEASE "${CURL_ROOT}/lib/Release/libcurl_imp.lib")
set(CURL_DLL_DEBUG   "${CURL_ROOT}/lib/Debug/libcurl-d.dll")
set(CURL_DLL_RELEASE "${CURL_ROOT}/lib/Release/libcurl.dll")

# ==== Enlace del plugin ====
target_link_libraries(${_name} PRIVATE
    OBS::libobs
    opengl32

    $<$<CONFIG:Debug>:${ASSIMP_DIR}/lib/Debug/assimp-vc143-mtd.lib>
    $<$<NOT:$<CONFIG:Debug>>:${ASSIMP_DIR}/lib/Release/assimp-vc143-mt.lib>

    $<$<CONFIG:Debug>:${OPENCV_LIBS_DEBUG}>
    $<$<NOT:$<CONFIG:Debug>>:${OPENCV_LIBS_RELEASE}>

    $<$<CONFIG:Debug>:${CURL_LIB_DEBUG}>
    $<$<NOT:$<CONFIG:Debug>>:${CURL_LIB_RELEASE}>
)

target_include_directories(${_name} PRIVATE
    "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs/include"
)

# Fuentes del plugin.
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

# Copia el binario generado y libcurl al directorio Release local del build.
add_custom_command(TARGET ${_name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${_name}> "${CMAKE_CURRENT_BINARY_DIR}/Release/$<TARGET_FILE_NAME:${_name}>"
    COMMAND ${CMAKE_COMMAND} -E copy "$<$<CONFIG:Debug>:${CURL_DLL_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${CURL_DLL_RELEASE}>" "${CMAKE_CURRENT_BINARY_DIR}/Release/$<$<CONFIG:Debug>:libcurl-d.dll>$<$<NOT:$<CONFIG:Debug>>:libcurl.dll>"
    COMMENT "Copying plugin and dependencies to local build directory..."
)
