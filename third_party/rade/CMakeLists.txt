cmake_minimum_required(VERSION 3.16)
project(radae_nopy C)

set(CMAKE_C_STANDARD 11)

# Build opus with FARGAN/LPCNet support
include(cmake/BuildOpus.cmake)

add_subdirectory(src)
