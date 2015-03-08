find_package(LibEvent)

if(LibEvent_FOUND)
    include_directories(${LibEvent_INCLUDE_DIR})
endif()
