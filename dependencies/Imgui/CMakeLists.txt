cmake_minimum_required(VERSION 3.7)
project(Imgui)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../GLAD)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/imgui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

file(GLOB_RECURSE IMGUI_SOURCE_FILES
        "*.cpp"
        "*.c"
		"*.h"
		"*.hpp"
        )

list(FILTER IMGUI_SOURCE_FILES EXCLUDE REGEX "(^.*\/|^)examples\/.*$")
list(FILTER IMGUI_SOURCE_FILES EXCLUDE REGEX "(^.*\/|^)backends\/.*$")
list(FILTER IMGUI_SOURCE_FILES EXCLUDE REGEX "(^.*\/|^)misc\/.*$")

foreach(SOURCE_FILE IN ITEMS ${IMGUI_SOURCE_FILES})
    if (IS_ABSOLUTE "${SOURCE_FILE}")
        file(RELATIVE_PATH SOURCE_FILE_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${SOURCE_FILE}")
    else()
        set(SOURCE_FILE_REL "${SOURCE_FILE}")
    endif()
    get_filename_component(SOURCE_PATH "${SOURCE_FILE_REL}" PATH)
    string(REPLACE "/" "\\" SOURCE_PATH_CONVERTED "${SOURCE_PATH}")
    source_group("${SOURCE_PATH_CONVERTED}" FILES "${SOURCE_FILE}")
endforeach()  

# We cam do this here, since the Imgui dependency uses the engine and therefore is always included
# after the engine is added
add_library(${PROJECT_NAME} STATIC ${IMGUI_SOURCE_FILES})
target_compile_definitions(${PROJECT_NAME} PUBLIC ${ATLAS_ENGINE_COMPILE_DEFINITIONS})
target_link_libraries(${PROJECT_NAME})