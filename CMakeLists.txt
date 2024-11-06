# cmake_minimum_required(VERSION <specify CMake version here>)
project(gps C)

set(CMAKE_C_STANDARD 99)

add_executable(gps main.c gps.h gps.c)