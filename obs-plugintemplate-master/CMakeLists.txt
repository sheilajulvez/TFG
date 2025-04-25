cmake_minimum_required(VERSION 3.16...3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})
add_library(${_name} MODULE)

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

# Ruta directa a los archivos de configuración de CMake de libobs y frontend-api
set(libobs_DIR "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs")
set(obs-frontend-api_DIR "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/UI/obs-frontend-api")
set(w32-pthreads_DIR "${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/deps/w32-pthreads")
# Ruta a las cabeceras de libobs-opengl porque poor alguna razon libobs-opengl NO TIENE CMAKE PERO LOS DEMÁS SI

include_directories("${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs-opengl/include")
# Ruta a las bibliotecas de libobs-opengl (puede ser .lib o .a dependiendo de tu sistema)
link_directories("${CMAKE_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs-opengl/lib")

# Buscar libobs y frontend-api desde las rutas específicas
find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)
# OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE opengl32)


if(ENABLE_FRONTEND_API)
    find_package(obs-frontend-api REQUIRED)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

# Qt6 (si lo necesitas, ajusta el path a donde tengas las precompiladas de Qt)
if(ENABLE_QT)
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/.deps/obs-deps-qt6-2024-09-12-x64")
       find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        OpenGL
        OpenGLWidgets
    )
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::OpenGL )
    set_target_properties(
        ${CMAKE_PROJECT_NAME}
        PROPERTIES AUTOMOC ON
                   AUTOUIC ON
                   AUTORCC ON)
endif()

# Fuentes del plugin
target_sources(${CMAKE_PROJECT_NAME} PRIVATE 

 src/plugin-main.c
 src/model3D_OBS.c
  

)

# Nombre del plugin
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
