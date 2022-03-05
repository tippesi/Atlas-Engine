cmake_minimum_required(VERSION 3.7)
project(AtlasEngineDemo)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

file(GLOB_RECURSE DEMO_SOURCE_FILES
        "*.cpp"
        "*.c"
		"*.h"
		"*.hpp"
        )

include_directories(../src)
include_directories(../dependencies)

foreach(SOURCE_FILE IN ITEMS ${DEMO_SOURCE_FILES})
    if (IS_ABSOLUTE "${SOURCE_FILE}")
        file(RELATIVE_PATH SOURCE_FILE_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${SOURCE_FILE}")
    else()
        set(SOURCE_FILE_REL "${SOURCE_FILE}")
    endif()
    get_filename_component(SOURCE_PATH "${SOURCE_FILE_REL}" PATH)
    string(REPLACE "/" "\\" SOURCE_PATH_CONVERTED "${SOURCE_PATH}")
    source_group("${SOURCE_PATH_CONVERTED}" FILES "${SOURCE_FILE}")
endforeach()  

add_executable(${PROJECT_NAME} ${DEMO_SOURCE_FILES} ${ATLAS_ENGINE_MAIN_FILE})
target_compile_definitions(${PROJECT_NAME} PUBLIC ${ATLAS_ENGINE_COMPILE_DEFINITIONS})
target_link_libraries (${PROJECT_NAME} AtlasEngine Imgui)