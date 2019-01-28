cmake_minimum_required(VERSION 3.7)
project(GLAD)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)
add_library(GLAD STATIC "glad.c")