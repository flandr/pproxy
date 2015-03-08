set(_source "${CMAKE_CURRENT_SOURCE_DIR}/third_party/http-parser")
set(_build "${CMAKE_CURRENT_BINARY_DIR}/http-parser")

set(_cmd make clean && CFLAGS="-fPIC" make package && mkdir -p ${_build} && cp ${_source}/libhttp_parser.a ${_build})

ExternalProject_Add(http_parser_ext
    SOURCE_DIR ${_source}
    CONFIGURE_COMMAND ""
    UPDATE_COMMAND ""
    BUILD_COMMAND "${_cmd}"
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE 1
)

include_directories("${_source}")
link_directories(${_build})
