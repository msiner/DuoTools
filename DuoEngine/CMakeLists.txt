cmake_minimum_required(VERSION 2.8.12)

if(WIN32)
    include_directories("C:\\Program Files\\SDRPlay\\API\\inc")
    find_library(
        SDRPLAY_API
        sdrplay_api
        PATHS "C:\\Program Files\\SDRPlay\\API\\x64")
else()
    find_library(
        SDRPLAY_API
        sdrplay_api)
endif()

link_libraries(${SDRPLAY_API})

add_library(DuoEngine SHARED DuoEngine.c DuoEngine.h)
add_library(DuoEngineStatic STATIC DuoEngine.c DuoEngine.h)