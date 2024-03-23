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

cmake_minimum_required(VERSION 3.24)

project(AtlasEngine VERSION 0.2.0)

# Only 64 bit is supported
###################################################################
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "32 bit isn't supported")
endif()

# Options and compiler settings ###################################################################
option(ATLAS_BUILD_SHARED "Force build as shared library" OFF)
option(ATLAS_EXPORT_MAIN "Add main file in root of directory tree. Add ${ATLAS_ENGINE_MAIN_FILE} to your executable" OFF)
option(ATLAS_NO_APP "Disables the engines main function" OFF)
option(ATLAS_DEMO "Build demo executable" OFF)
option(ATLAS_EDITOR "Build editor executable" ON)
option(ATLAS_IMGUI "Activate ImGui integration" OFF)
option(ATLAS_ASSIMP "Activate Assimp integration" ON)
option(ATLAS_HEADLESS "Activate support for running the engine in headless mode" OFF)
option(ATLAS_BINDLESS "Activate support for running the engine with bindless resources turned on" ON)

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    option(ATLAS_TESTS "Activate support for running the engine with bindless resources turned on" ON)
else()
    option(ATLAS_TESTS "Activate support for running the engine with bindless resources turned on" OFF)
endif()


if (ATLAS_DEMO)
    set (ATLAS_IMGUI ON)
endif()
if (ATLAS_TESTS)
    set (ATLAS_IMGUI ON)
endif()
if (ATLAS_EDITOR)
    set (ATLAS_IMGUI ON)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (CYGWIN OR MINGW)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -O3 -std=gnu++20" )
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
set (TESTS_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/src/tests)
set (EDITOR_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/src/editor)
set (IMGUI_EXTENSION_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libs/ImguiExtension)

# Add dependencies ################################################################################
find_package(SDL2 CONFIG REQUIRED)

if(ATLAS_ASSIMP)
    find_package(assimp CONFIG REQUIRED)
endif()
if (ANDROID)
    add_subdirectory(${HIDAPI_LOCATION})
endif()

find_package(unofficial-spirv-reflect CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glslang CONFIG REQUIRED)
find_package(SPIRV-Tools-opt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-joltphysics CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)

if (ATLAS_TESTS)
find_package(GTest CONFIG REQUIRED)
endif()

add_subdirectory(libs/fsr2)
add_subdirectory(${ATLAS_LOCATION})

if (ATLAS_IMGUI)
    add_subdirectory(${IMGUI_EXTENSION_LOCATION})
endif()

if (ATLAS_DEMO)
    add_subdirectory(${DEMO_LOCATION})
endif()

if (ATLAS_EDITOR)
    find_package(imguizmo CONFIG REQUIRED)
    add_subdirectory(${EDITOR_LOCATION})
endif()

if (ATLAS_TESTS)
    add_subdirectory(${TESTS_LOCATION})
endif()
