if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the source code and call cmake from there")
endif()

project(AtlasEngine)
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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (CYGWIN OR MINGW)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -O3 -std=gnu++11" )
endif()

# Disable some assimp stuff #######################################################################
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "Override Assimp settings" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Override Assimp tools settings" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Override Assimp tools settings" FORCE) # Doesn't work with C++17

# Set SDL options #################################################################################
set(SDL_SHARED ON CACHE BOOL "Override SDL settings" FORCE)
set(SDL_STATIC ON CACHE BOOL "Override SDL settings" FORCE)

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

# Workaround for SDL issue in Visual Studio
if (MSVC)
list(APPEND EXTRA_LIBS vcruntime)
endif()
if (ANDROID)
    list(APPEND EXTRA_LIBS dl)
endif()

# Workaround for assimp std::iterator deprecation warnings
if (MSVC)
add_compile_definitions(_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
endif()

if (ATLAS_DEMO)
set(ATLAS_EXPORT_MAIN ON CACHE BOOL "Override engine settings" FORCE)
endif()

# Set dependencies location #######################################################################
set (ASSIMP_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Assimp)
set (SDL_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/SDL)
set (HIDAPI_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Hidapi)
set (GLAD_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Glad)
set (IMGUI_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Imgui)
set (ATLAS_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/src)
set (DEMO_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/demo)

# Add dependencies ################################################################################
if(ATLAS_ASSIMP)
add_subdirectory(${ASSIMP_LOCATION})
endif()
add_subdirectory(${SDL_LOCATION})
if (ANDROID)
add_subdirectory(${HIDAPI_LOCATION})
else()
add_subdirectory(${GLAD_LOCATION})
endif()
add_subdirectory(${ATLAS_LOCATION})

if (ATLAS_IMGUI)
add_subdirectory(${IMGUI_LOCATION})
endif()
if (ATLAS_DEMO)
add_subdirectory(${DEMO_LOCATION})
endif()
