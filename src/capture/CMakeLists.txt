cmake_minimum_required(VERSION 3.25)

project(AtlasEngineCapture VERSION 0.2.1)

# Note: For this project, the root CMakeLists.txt turns the ATLAS_EXPORT_MAIN option on.
# The capture tool only uses the public engine API, it doesn't need ImGui.

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

file(GLOB_RECURSE CAPTURE_SOURCE_FILES
        "*.cpp"
        "*.c"
        "*.h"
        "*.hpp"
        )

# Required: Set both the source and dependency directories
# as include directories
include_directories(../engine)
include_directories(../../libs)

foreach(SOURCE_FILE IN ITEMS ${CAPTURE_SOURCE_FILES})
    if (IS_ABSOLUTE "${SOURCE_FILE}")
        file(RELATIVE_PATH SOURCE_FILE_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${SOURCE_FILE}")
    else()
        set(SOURCE_FILE_REL "${SOURCE_FILE}")
    endif()
    get_filename_component(SOURCE_PATH "${SOURCE_FILE_REL}" PATH)
    string(REPLACE "/" "\\" SOURCE_PATH_CONVERTED "${SOURCE_PATH}")
    source_group("${SOURCE_PATH_CONVERTED}" FILES "${SOURCE_FILE}")
endforeach()

# We want to make sure that the linker searches for local libraries first
if (UNIX AND NOT APPLE AND NOT ANDROID)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath='$ORIGIN'")
endif()
# Need to reference the vulkan SDK for MacOS to search at runtime
if (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,'/usr/local/lib'")
endif()

add_executable(${PROJECT_NAME} ${CAPTURE_SOURCE_FILES} ${ATLAS_ENGINE_MAIN_FILE})
target_compile_definitions(${PROJECT_NAME} PUBLIC ${ATLAS_ENGINE_COMPILE_DEFINITIONS})
target_link_libraries(${PROJECT_NAME} AtlasEngine)
