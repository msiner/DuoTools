cmake_minimum_required(VERSION 2.8.12)

include_directories(${PROJECT_SOURCE_DIR}/DuoEngine)

link_libraries(DuoEngineStatic)

if(WIN32)
    add_executable(
        DuoWAV
        DuoWAV.c
        wav.h
        ${PROJECT_SOURCE_DIR}/DuoEngine/DuoEngine.h
        ${PROJECT_SOURCE_DIR}/DuoEngine/DuoParse.h
        ${PROJECT_SOURCE_DIR}/DuoEngine/windows_getopt.h)
else()
    add_executable(
        DuoWAV
        DuoWAV.c
        wav.h
        ${PROJECT_SOURCE_DIR}/DuoEngine/DuoEngine.h
        ${PROJECT_SOURCE_DIR}/DuoEngine/DuoParse.h
        ${PROJECT_SOURCE_DIR}/DuoEngine/posix_conio.h)
endif()
