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
)

# Main library targets
add_library(${pproxy_SHARED_LIBRARY} SHARED ${libpproxy_SRCS})

# Apple frameworks
#IF(APPLE)
#INCLUDE_DIRECTORIES ( /Developer/Headers/FlatCarbon )
#FIND_LIBRARY(CARBON_LIBRARY Carbon)
#FIND_LIBRARY(APP_SERVICES_LIBRARY ApplicationServices )
#MARK_AS_ADVANCED (CARBON_LIBRARY
#APP_SERVICES_LIBRARY)
#SET(EXTRA_LIBS
#${CARBON_LIBRARY}
#${APP_SERVICES_LIBRARY})
#ENDIF (APPLE)

# Link dependencies
target_link_libraries(${pproxy_SHARED_LIBRARY}
    ${LibEvent_LIBRARY}
    # Uh oh, win32?
    ${LibEvent_PTHREADS_LIBRARY}
    http_parser
    ${EXTRA_LIBS}
)

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
