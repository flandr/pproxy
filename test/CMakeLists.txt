project(tests CXX)

# Set includes
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
    ${gtest_INCLUDE_DIRS}
)

add_executable(test
    basic.cc
    driver.cc
    echo_server.cc
)

target_link_libraries(test
    gtest
    pproxy
    pthread
)
