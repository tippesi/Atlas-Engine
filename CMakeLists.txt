if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the source code and call cmake from there")
endif()

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
    cmake_policy(SET CMP0048 NEW)
    cmake_policy(SET CMP0074 NEW)
    cmake_policy(SET CMP0011 NEW)
    cmake_policy(SET CMP0042 NEW)
endif(COMMAND cmake_policy)

project(AtlasEngine VERSION 0.1.8)
cmake_minimum_required(VERSION 3.7)

# Only 64 bit is supported
###################################################################
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "32 bit isn't supported")
endif()

# Options and compiler settings ###################################################################
# Options and compiler settings ###################################################################
option(ATLAS_BUILD_SHARED "Force build as shared library" OFF)
option(ATLAS_OPENGL_ES "Use OpenGL ES instead of OpenGL" OFF)
option(ATLAS_EXPORT_MAIN "Add main file in root of directory tree. Add ${ATLAS_ENGINE_MAIN_FILE} to your executable" OFF)
option(ATLAS_NO_APP "Disables the engines main function" OFF)
option(ATLAS_DEMO "Build demo executable" OFF)
option(ATLAS_IMGUI "Activate ImGui integration" OFF)
option(ATLAS_ASSIMP "Activate Assimp integration" ON)

if (ATLAS_DEMO)
    set (ATLAS_IMGUI ON)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (CYGWIN OR MINGW)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -O3 -std=gnu++11" )
endif()

# Find OpenGL #####################################################################################
if (ANDROID)
    set(ATLAS_OPENGL_ES ON)
    find_library(OpenGL REQUIRED)
    # Workaround for issue 6
    find_library(ANDROID_DL_LIBRARY NAMES libdl.so dl)
endif()
if (NOT ATLAS_OPENGL_ES)
    find_package(OpenGL REQUIRED)
    set(OpenGL_GL_PREFERENCE GLVND)
endif()

if (ANDROID)
    list(APPEND EXTRA_LIBS dl)
endif()

if (ATLAS_DEMO)
set(ATLAS_EXPORT_MAIN ON CACHE BOOL "Override engine settings" FORCE)
endif()

# Set dependencies location #######################################################################
set (ATLAS_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/src/engine)
set (DEMO_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/src/demo)
set (IMGUI_EXTENSION_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libs/ImguiExtension)

# Add dependencies ################################################################################
find_package(SDL2 CONFIG REQUIRED)

if(ATLAS_ASSIMP)
    find_package(assimp CONFIG REQUIRED)
endif()
if (ANDROID)
    add_subdirectory(${HIDAPI_LOCATION})
endif()

find_package(Threads REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
find_package(glslang CONFIG REQUIRED)
find_package(unofficial-spirv-reflect CONFIG REQUIRED)
find_package(SPIRV-Tools-opt CONFIG REQUIRED)

add_subdirectory(${ATLAS_LOCATION})

if (ATLAS_IMGUI)
    add_subdirectory(${IMGUI_EXTENSION_LOCATION})
endif()

if (ATLAS_DEMO)
    add_subdirectory(${DEMO_LOCATION})
endif()
