cmake_minimum_required(VERSION 3.25)
project(fsr2)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ffx-fsr2-api)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

file(GLOB_RECURSE FSR_SOURCE_FILES
        "Main.cpp"
        "*.h"
        "*.hpp"
        )

foreach(SOURCE_FILE IN ITEMS ${FSR_SOURCE_FILES})
    if (IS_ABSOLUTE "${SOURCE_FILE}")
        file(RELATIVE_PATH SOURCE_FILE_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${SOURCE_FILE}")
    else()
        set(SOURCE_FILE_REL "${SOURCE_FILE}")
    endif()
    get_filename_component(SOURCE_PATH "${SOURCE_FILE_REL}" PATH)
    string(REPLACE "/" "\\" SOURCE_PATH_CONVERTED "${SOURCE_PATH}")
    source_group("${SOURCE_PATH_CONVERTED}" FILES "${SOURCE_FILE}")
endforeach()

find_path(VULKAN_HEADERS_INCLUDE_DIRS "vk_video/vulkan_video_codec_h264std.h")

# We cam do this here, since the ImguiExtension dependency uses the engine and therefore is always included
# after the engine is added
add_subdirectory(ffx-fsr2-api)

add_library(${PROJECT_NAME} STATIC ${FSR_SOURCE_FILES})
target_include_directories(${PROJECT_NAME} 
    PUBLIC
    ${VULKAN_HEADERS_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR})

# target_compile_definitions(${PROJECT_NAME} PUBLIC ${ATLAS_ENGINE_COMPILE_DEFINITIONS})
target_link_libraries(${PROJECT_NAME} ffx_fsr2_api)