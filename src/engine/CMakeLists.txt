if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the source code and call cmake from there")
endif()

project(AtlasEngine)
cmake_minimum_required(VERSION 3.24)

# Validate options ################################################################################
if (ATLAS_NO_APP AND ATLAS_EXPORT_MAIN)
    set (ATLAS_EXPORT_MAIN OFF)
endif()

# Message options #################################################################################
if (ATLAS_EXPORT_MAIN)
    message(STATUS "Exporting main function")
endif()

if (ATLAS_NO_APP)
    message(STATUS "Building without app class")
    set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_NO_APP)
else()
    message(STATUS "Building with app class")
endif()

# Set built type macros ###########################################################################
if (DEFINED CMAKE_BUILD_TYPE)
    # We want the better/safer usertypes, such that we don't crash everything
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_BUILDTYPE_RELEASE SOL_ALL_SAFETIES_ON)
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_BUILDTYPE_RELWITHDEBINFO SOL_ALL_SAFETIES_ON)
    else()
        set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_BUILDTYPE_DEBUG SOL_ALL_SAFETIES_ON)
        # We need additional defines to match what Jolt compiled with as a vcpkg package (ignore this for now for other build types)
        set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} JPH_PROFILE_ENABLED JPH_DEBUG_RENDERER)
    endif()
else()
    # This is for non-single config generators like XCode
    # Release compile definitions
    set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} "$<$<CONFIG:Release>:AE_BUILDTYPE_RELEASE>" 
        "$<$<CONFIG:Release>:SOL_ALL_SAFETIES_ON>")
    # RelWithDebInfo compile definitions
    set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} "$<$<CONFIG:RelWithDebInfo>:AE_BUILDTYPE_RELWITHDEBINFO>" 
        "$<$<CONFIG:RelWithDebInfo>:SOL_ALL_SAFETIES_ON>")
    # Debug compile definitions
    set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} "$<$<CONFIG:Debug>:AE_BUILDTYPE_DEBUG>" 
        "$<$<CONFIG:Debug>:JPH_PROFILE_ENABLED>" "$<$<CONFIG:Debug>:JPH_DEBUG_RENDERER>" "$<$<CONFIG:Debug>:SOL_ALL_SAFETIES_ON>")
endif()

# Configure platform specific stuff ###############################################################
if(WIN32)
    set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_OS_WINDOWS)

    set(ATLAS_ENGINE_LIBS assimp::assimp SDL2::SDL2 SDL2::SDL2main SPIRV-Tools-opt
            volk::volk volk::volk_headers GPUOpen::VulkanMemoryAllocator
            unofficial::spirv-reflect glslang::SPIRV Jolt::Jolt
            ${LUA_LIBRARIES} sol2 fsr2)
endif()

if (MINGW)
    set(ATLAS_ENGINE_LIBS mingw32 ${ATLAS_ENGINE_LIBS})
endif()

if(APPLE)
    set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_OS_MACOS)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_MACOS_MVK -DVK_EXAMPLE_XCODE_GENERATED")

    set(ATLAS_ENGINE_LIBS assimp::assimp SDL2::SDL2 SDL2::SDL2main SDL2::SDL2-static
            volk::volk SPIRV-Tools-opt Jolt::Jolt fsr2
            GPUOpen::VulkanMemoryAllocator Vulkan::Vulkan Vulkan::Headers
            unofficial::spirv-reflect glslang::SPIRV ${LUA_LIBRARIES} sol2)
endif()

if(ANDROID)
    set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_OS_ANDROID)

    set(ATLAS_ENGINE_LIBS assimp::assimp SDL2::SDL2 SDL2::SDL2main SDL2::SDL2-static hidapi
            ${LUA_LIBRARIES} sol2 fsr2)
    set(ATLAS_ENGINE_SYSTEM_LIBS android m)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
    set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_OS_LINUX)
    
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
    set (CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -ldl")

    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "./")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    
    set(ATLAS_ENGINE_LIBS assimp::assimp SDL2::SDL2 SDL2::SDL2main SDL2::SDL2-static fsr2
            volk::volk volk::volk_headers GPUOpen::VulkanMemoryAllocator Jolt::Jolt
            unofficial::spirv-reflect SPIRV-Tools-opt glslang::SPIRV ${LUA_LIBRARIES} sol2)
endif()

# Due to lua bindings we need more addressable functions (but not for clang)
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/bigobj)
endif()

# Add source files ################################################################################
file(GLOB_RECURSE ATLAS_ENGINE_SOURCE_FILES
        "*.cpp"
        "*.c"
        "*.h"
        "*.hpp"
        )

# Grouping source files into tree structure #######################################################
foreach(SOURCE_FILE IN ITEMS ${ATLAS_ENGINE_SOURCE_FILES})
    if (IS_ABSOLUTE "${SOURCE_FILE}")
        file(RELATIVE_PATH SOURCE_FILE_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${SOURCE_FILE}")
    else()
        set(SOURCE_FILE_REL "${SOURCE_FILE}")
    endif()
    get_filename_component(SOURCE_PATH "${SOURCE_FILE_REL}" PATH)
    string(REPLACE "/" "\\" SOURCE_PATH_CONVERTED "${SOURCE_PATH}")
    source_group("${SOURCE_PATH_CONVERTED}" FILES "${SOURCE_FILE}")
endforeach()

if (ATLAS_EXPORT_MAIN)
    set(ATLAS_ENGINE_MAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp CACHE INTERNAL "Make main global")
    list(FILTER ATLAS_ENGINE_SOURCE_FILES EXCLUDE REGEX "(^.*\/|^)Main\.cpp$")
endif()

if (NOT ATLAS_ASSIMP)
    list(FILTER ATLAS_ENGINE_SOURCE_FILES EXCLUDE REGEX "(^.*\/|^)ModelLoader\.(c(pp)?|h(pp)?)$")
endif()

# Configure linking ###############################################################################
if(ANDROID)    
    add_library(${PROJECT_NAME} SHARED ${ATLAS_ENGINE_SOURCE_FILES})
else()
    list(FILTER ATLAS_ENGINE_SOURCE_FILES EXCLUDE REGEX "(^.*\/|^)[Aa]pp\.(c(pp)?|h(pp)?)$")
if (ATLAS_NO_APP)
    list(FILTER ATLAS_ENGINE_SOURCE_FILES EXCLUDE REGEX "(^.*\/|^)Main\.(c(pp)?|h(pp)?)$")
endif()
if (ATLAS_BUILD_SHARED)
    add_library(${PROJECT_NAME} SHARED ${ATLAS_ENGINE_SOURCE_FILES})
else()
    add_library(${PROJECT_NAME} STATIC ${ATLAS_ENGINE_SOURCE_FILES})
endif()
endif()

find_path(GLM_INCLUDE_DIRS "glm/common.hpp")
find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")
find_path(VULKAN_HEADERS_INCLUDE_DIRS "vk_video/vulkan_video_codec_h264std.h")

if (ATLAS_HEADLESS)
set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_HEADLESS)
endif()

if (ATLAS_BINDLESS)
set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_BINDLESS)
endif()

# Include directories and definitions #############################################################
target_compile_definitions(${PROJECT_NAME} PUBLIC ${ATLAS_ENGINE_COMPILE_DEFINITIONS})
target_include_directories(${PROJECT_NAME} 
    PUBLIC
    ${VULKAN_HEADERS_INCLUDE_DIRS}
    ${STB_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libs)

message(${VULKAN_HEADERS_INCLUDE_DIRS})
    
# Link libraries ##################################################################################
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ${ATLAS_ENGINE_LIBS} LINK_PRIVATE ${ATLAS_ENGINE_SYSTEM_LIBS})
