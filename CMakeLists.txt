cmake_minimum_required(VERSION 3.14)
project(provafinaleapi C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}" )

add_executable(provafinaleapi main.c)