cmake_minimum_required(VERSION 2.8)

include_directories(${CMAKE_SOURCE_DIR}/DuoEngine)

link_libraries(DuoEngineStatic)

add_executable(
    DuoWAV
    DuoWAV.c
    ${CMAKE_SOURCE_DIR}/DuoEngine/DuoEngine.h
    ${CMAKE_SOURCE_DIR}/DuoEngine/DuoParse.h)