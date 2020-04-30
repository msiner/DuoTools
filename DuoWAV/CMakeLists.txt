cmake_minimum_required(VERSION 2.8)

include_directories(${PROJECT_SOURCE_DIR}/DuoEngine)

link_libraries(DuoEngineStatic)

add_executable(
    DuoWAV
    DuoWAV.c
    wav.h
    ${PROJECT_SOURCE_DIR}/DuoEngine/DuoEngine.h
    ${PROJECT_SOURCE_DIR}/DuoEngine/DuoParse.h
    ${PROJECT_SOURCE_DIR}/DuoEngine/getopt.h)