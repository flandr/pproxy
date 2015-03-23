project(pproxy-library C)

# Define the public include directories
set(pproxy_PUBLIC_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/pproxy
    CACHE INTERNAL "pproxy public include directories"
)

# The shared library target
set(pproxy_SHARED_LIBRARY
    pproxy
    CACHE INTERNAL "pproxy shared library"
)

# Source translation units
set(libpproxy_SRCS
    pproxy.c
    pproxy_connection.c
)

# Set the include directories
include_directories(
    ${WhatTheEvent_PUBLIC_INCLUDE_DIRS}
    ${LibEvent_INCLUDE_DIR}
)

if (WIN32)
  include_directories(
    ${LibEvent_WIN32_INCLUDE_DIR}
  )
endif (WIN32)

# Main library targets
add_library(${pproxy_SHARED_LIBRARY} SHARED ${libpproxy_SRCS})

# Link dependencies
target_link_libraries(${pproxy_SHARED_LIBRARY}
    ${LibEvent_LIBRARY}
    http_parser
    ${EXTRA_LIBS}
)

if (NOT WIN32)
  target_link_libraries(${pproxy_SHARED_LIBRARY}
    ${LibEvent_PTHREADS_LIBRARY}
  )
endif (NOT WIN32)

# Installation
install(TARGETS
    ${pproxy_SHARED_LIBRARY}
    DESTINATION lib
    EXPORT pproxy-targets)
install(EXPORT pproxy-targets DESTINATION lib)

install(DIRECTORY
    ${CMAKE_SOURCE_DIR}/pproxy
    DESTINATION include/pproxy
    FILES_MATCHING PATTERN "*.h"
)
