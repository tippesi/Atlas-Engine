cmake_minimum_required(VERSION 3.25)

project(AtlasBridge VERSION 0.2.1)

# The bridge is a library: game hosts link AtlasBridgeLibrary to run jobs with their own interface
# (HostedJob.h). The executable adds the standalone app with its own renderer (App.cpp), for it the root
# CMakeLists.txt turns the ATLAS_EXPORT_MAIN option on. Neither needs ImGui.
set(ATLAS_BRIDGE_OUTPUT_NAME "AtlasBridge" CACHE STRING "File name of the bridge executable")

file(GLOB_RECURSE BRIDGE_SOURCE_FILES "*.cpp" "*.h")
set(BRIDGE_APP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/App.cpp ${CMAKE_CURRENT_SOURCE_DIR}/App.h)
list(REMOVE_ITEM BRIDGE_SOURCE_FILES ${BRIDGE_APP_FILES})

foreach(SOURCE_FILE IN ITEMS ${BRIDGE_SOURCE_FILES} ${BRIDGE_APP_FILES})
    file(RELATIVE_PATH SOURCE_FILE_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${SOURCE_FILE}")
    get_filename_component(SOURCE_PATH "${SOURCE_FILE_REL}" PATH)
    string(REPLACE "/" "\\" SOURCE_PATH_CONVERTED "${SOURCE_PATH}")
    source_group("${SOURCE_PATH_CONVERTED}" FILES "${SOURCE_FILE}")
endforeach()

add_library(AtlasBridgeLibrary STATIC ${BRIDGE_SOURCE_FILES})
target_include_directories(AtlasBridgeLibrary PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../engine ../../libs)
target_compile_definitions(AtlasBridgeLibrary PUBLIC ${ATLAS_ENGINE_COMPILE_DEFINITIONS})
target_link_libraries(AtlasBridgeLibrary PUBLIC AtlasEngine)

# Every program linking the library can offer the MCP server
find_package(httplib CONFIG REQUIRED)
target_link_libraries(AtlasBridgeLibrary PRIVATE httplib::httplib)
# Winsock has to come before windows.h, which the precompiled header of the engine can't promise
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/McpServer.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

if (NOT ATLAS_BRIDGE)
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

add_executable(${PROJECT_NAME} ${BRIDGE_APP_FILES} ${ATLAS_ENGINE_MAIN_FILE})
target_link_libraries(${PROJECT_NAME} AtlasBridgeLibrary)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${ATLAS_BRIDGE_OUTPUT_NAME})

# A running bridge can't be overwritten (LNK1168), but Windows lets it be renamed
if (WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
        COMMAND ${CMAKE_COMMAND} -DEXECUTABLE=$<TARGET_FILE:${PROJECT_NAME}> -P ${CMAKE_CURRENT_SOURCE_DIR}/MoveRunningExecutable.cmake)
endif()
