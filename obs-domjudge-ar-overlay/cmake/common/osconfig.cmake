# CMake operating system bootstrap module

include_guard(GLOBAL)

# Windows-only project bootstrap.
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
  message(FATAL_ERROR "This project only supports Windows builds.")
endif()

set(CMAKE_C_EXTENSIONS FALSE)
set(CMAKE_CXX_EXTENSIONS FALSE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows")
set(OS_WINDOWS TRUE)
