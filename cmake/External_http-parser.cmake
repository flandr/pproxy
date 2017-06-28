set(_source "${PROJECT_SOURCE_DIR}/third_party/http-parser")
set(_build "${CMAKE_CURRENT_BINARY_DIR}/http-parser")

ExternalProject_Add(http_parser_ext
    SOURCE_DIR ${_source}
    BINARY_DIR ${_build}
    INSTALL_COMMAND ""
    CMAKE_ARGS
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
)

include_directories("${_source}")
link_directories(${_build})
