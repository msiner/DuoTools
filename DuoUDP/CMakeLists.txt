cmake_minimum_required(VERSION 2.8)

include_directories(${PROJECT_SOURCE_DIR}/DuoEngine)

link_libraries(ws2_32)
link_libraries(DuoEngineStatic)

add_executable(
    DuoUDP
    DuoUDP.c
    ${PROJECT_SOURCE_DIR}/DuoEngine/DuoEngine.h
    ${PROJECT_SOURCE_DIR}/DuoEngine/DuoParse.h
    ${PROJECT_SOURCE_DIR}/DuoEngine/getopt.h)