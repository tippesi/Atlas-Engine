cmake_minimum_required(VERSION 3.7)
project(hidapi)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../SDL/src/hidapi/android)
add_library(hidapi ${CMAKE_CURRENT_SOURCE_DIR}/../SDL/src/hidapi/android/hid.cpp)
target_link_libraries(hidapi log)