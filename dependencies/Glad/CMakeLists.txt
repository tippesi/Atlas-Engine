cmake_minimum_required(VERSION 3.7)
project(GLAD)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

# Problems with GLAD being compiled as shared but expected to be static
# https://stackoverflow.com/questions/53601141/cmake-generated-vs-project-expecting-lib-instead-of-dll
if(WIN32)
add_library(GLAD STATIC "glad.c")
else()
add_library(GLAD "glad.c")
endif()