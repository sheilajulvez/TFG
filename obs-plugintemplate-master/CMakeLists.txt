cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})
add_library(${_name} MODULE)

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT           "Use Qt functionality"           ON)

include(compilerconfig)
include(defaults)
include(helpers)

# ==== libobs y frontend-api ====
set(libobs_DIR           "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs")
set(obs-frontend-api_DIR "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/UI/obs-frontend-api")
set(w32-pthreads_DIR     "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/deps/w32-pthreads")

# libobs-opengl (no tiene CMake package)
include_directories("${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs-opengl/include")
link_directories   ("${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs-opengl/lib")

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

# Mensajes de diagnóstico (opcional)
message(STATUS "Assimp Debug lib path: ${ASSIMP_DIR}/lib/Debug/assimp-vc143-mtd.lib")
message(STATUS "Assimp Release lib path: ${ASSIMP_DIR}/lib/Release/assimp-vc143-mt.lib")

# ==== Unificar linkeo ====
target_link_libraries(${_name} PRIVATE
    OBS::libobs
    opengl32

    # frontend-api si está activado
    $<$<BOOL:${ENABLE_FRONTEND_API}>:OBS::obs-frontend-api>

    # Qt6 si está activado
    $<$<BOOL:${ENABLE_QT}>:Qt6::Core>
    $<$<BOOL:${ENABLE_QT}>:Qt6::Widgets>
    $<$<BOOL:${ENABLE_QT}>:Qt6::OpenGL>

    # Assimp: rutas absolutas por configuración
    $<$<CONFIG:Debug>:${ASSIMP_DIR}/lib/Debug/assimp-vc143-mtd.lib>
    $<$<CONFIG:Release>:${ASSIMP_DIR}/lib/Release/assimp-vc143-mt.lib>
)

# ==== Incluir headers de OBS (obs-module.h, obs.h, etc.) ====
target_include_directories(${_name} PRIVATE
    "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs/include"
)

# ==== Archivos fuente del plugin ====
target_sources(${_name} PRIVATE
    src/Cubo-plugin.c
)

# ==== Nombre final del plugin ====
set_target_properties_plugin(${_name} PROPERTIES OUTPUT_NAME ${_name})
