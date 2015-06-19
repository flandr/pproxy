project(examples C)

# Set includes
include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

# Simple proxy example
add_executable(server
    server.c
)

target_link_libraries(server
    pproxy
)

add_executable(pausing-server
    pausing-server.c
)

target_link_libraries(pausing-server
    pproxy
)
