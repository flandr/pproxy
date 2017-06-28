set(_source "${PROJECT_SOURCE_DIR}/third_party/gtest")
set(_build "${CMAKE_CURRENT_BINARY_DIR}/gtest")

ExternalProject_Add(gtest_ext
    SOURCE_DIR ${_source}
    BINARY_DIR ${_build}
    # googletest doesn't provide an install target
    INSTALL_COMMAND ""
    CMAKE_ARGS
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
)

include_directories("${_source}/include")
link_directories("${_build}")
