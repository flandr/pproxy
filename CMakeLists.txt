project(pproxy C CXX)

# Required CMake version
cmake_minimum_required(VERSION 2.8)

if (WIN32)
  # Windows library searches for libevent
  set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
endif (WIN32)


# Module search path
set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${CMAKE_CURRENT_LIST_DIR}/cmake
)

# Some CMake configuration
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

# Default to release builds w/ debug info
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif (NOT CMAKE_BUILD_TYPE)

# Add third-party dependencies
include(ExternalProject)
include(External_gtest)
include(External_http-parser)
include(External_LibEvent)

# Use C++11 when building C++ code
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Recurse
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(example)
