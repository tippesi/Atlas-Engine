cmake_minimum_required(VERSION 3.25)

project(AtlasEngineCapture VERSION 0.2.1)

# The capture runner is a library: game hosts link AtlasEngineCaptureLibrary to run jobs with their own interface
# (HostedJob.h). The executable adds the standalone app with its own renderer (App.cpp), for it the root
# CMakeLists.txt turns the ATLAS_EXPORT_MAIN option on. Neither needs ImGui.
set(ATLAS_CAPTURE_OUTPUT_NAME "AtlasEngineCapture" CACHE STRING "File name of the capture tool executable")

file(GLOB_RECURSE CAPTURE_SOURCE_FILES "*.cpp" "*.h")
set(CAPTURE_APP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/App.cpp ${CMAKE_CURRENT_SOURCE_DIR}/App.h)
list(REMOVE_ITEM CAPTURE_SOURCE_FILES ${CAPTURE_APP_FILES})

foreach(SOURCE_FILE IN ITEMS ${CAPTURE_SOURCE_FILES} ${CAPTURE_APP_FILES})
    file(RELATIVE_PATH SOURCE_FILE_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${SOURCE_FILE}")
    get_filename_component(SOURCE_PATH "${SOURCE_FILE_REL}" PATH)
    string(REPLACE "/" "\\" SOURCE_PATH_CONVERTED "${SOURCE_PATH}")
    source_group("${SOURCE_PATH_CONVERTED}" FILES "${SOURCE_FILE}")
endforeach()

add_library(AtlasEngineCaptureLibrary STATIC ${CAPTURE_SOURCE_FILES})
target_include_directories(AtlasEngineCaptureLibrary PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../engine ../../libs)
target_compile_definitions(AtlasEngineCaptureLibrary PUBLIC ${ATLAS_ENGINE_COMPILE_DEFINITIONS})
target_link_libraries(AtlasEngineCaptureLibrary PUBLIC AtlasEngine)

if (NOT ATLAS_CAPTURE)
    return()
endif()

# We want to make sure that the linker searches for local libraries first
if (UNIX AND NOT APPLE AND NOT ANDROID)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath='$ORIGIN'")
endif()
# Need to reference the vulkan SDK for MacOS to search at runtime
if (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,'/usr/local/lib'")
endif()

add_executable(${PROJECT_NAME} ${CAPTURE_APP_FILES} ${ATLAS_ENGINE_MAIN_FILE})
target_link_libraries(${PROJECT_NAME} AtlasEngineCaptureLibrary)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${ATLAS_CAPTURE_OUTPUT_NAME})

# A running capture tool can't be overwritten (LNK1168), but Windows lets it be renamed
if (WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
        COMMAND ${CMAKE_COMMAND} -DEXECUTABLE=$<TARGET_FILE:${PROJECT_NAME}> -P ${CMAKE_CURRENT_SOURCE_DIR}/MoveRunningExecutable.cmake)
endif()
