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
option(ATLAS_BUILD_LIBRARY "Build as library" OFF)
option(ATLAS_BUILD_SHARED "Force build as shared library" OFF)
option(ATLAS_OPENGL_ES "Use OpenGL ES instead of OpenGL" OFF)
option(ATLAS_EXPORT_MAIN "Add main file in root of directory tree. Add ${ATLAS_ENGINE_MAIN_FILE} to your executable" OFF)
option(ATLAS_NO_APP "Disables the engines main function" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (CYGWIN OR MINGW)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -O3 -Wa,-mbig-obj -std=gnu++11" )
endif()

# Validate options ################################################################################
if(NOT (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR))
set (ATLAS_BUILD_LIBRARY ON)
endif()

if (ATLAS_BUILD_SHARED AND NOT ATLAS_BUILD_LIBRARY)
set (ATLAS_BUILD_LIBRARY ON)
endif()

if (ATLAS_NO_APP AND ATLAS_EXPORT_MAIN)
set (ATLAS_EXPORT_MAIN OFF)
endif()

# Disable some assimp stuff #######################################################################
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "Override Assimp settings" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Override Assimp tools settings" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Override Assimp tools settings" FORCE) # Doesn't work with C++17

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

# Message options #################################################################################
if (ATLAS_OPENGL_ES)
message(STATUS "Using OpenGL ES")
else()
message(STATUS "Using OpenGL")
endif()

if (ATLAS_BUILD_LIBRARY)
message(STATUS "Building as a library")
set(ATLAS_ENGINE_COMPILE_DEFINITIONS AE_BUILD_TYPE_LIBRARY)
else()
message(STATUS "Building as executable")
set(ATLAS_ENGINE_COMPILE_DEFINITIONS AE_BUILD_TYPE_APP)
endif()

if (ATLAS_EXPORT_MAIN)
message(STATUS "Exporting main function")
endif()

if (ATLAS_NO_APP)
message(STATUS "Building without app class")
set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_NO_APP)
else()
message(STATUS "Building with app class")
endif()

# Workaround for SDL issue in Visual Studio
if (MSVC)
list(APPEND EXTRA_LIBS vcruntime)
endif()

# Workaround for assimp std::iterator deprecation warnings
if (MSVC)
add_compile_definitions(_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
endif()

# Set dependencies location #######################################################################
set (ASSIMP_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Assimp)
set (SDL_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/SDL)
set (HIDAPI_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Hidapi)
set (GLAD_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Glad)

# Add dependencies ################################################################################
add_subdirectory(${ASSIMP_LOCATION})
add_subdirectory(${SDL_LOCATION})
if (ANDROID)
add_subdirectory(${HIDAPI_LOCATION})
else()
add_subdirectory(${GLAD_LOCATION})
endif()

# Configure platform specific stuff ###############################################################
if(WIN32)
	set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_OS_WINDOWS)

	set(ATLAS_ENGINE_LIBS assimp SDL2main SDL2 GLAD)
    set(ATLAS_ENGINE_SYSTEM_LIBS opengl32 user32 Shlwapi)
endif()

if (MINGW)
	set(ATLAS_ENGINE_LIBS mingw32 ${ATLAS_ENGINE_LIBS})
endif()

if(APPLE)
	set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_OS_MACOS)

    include_directories(/System/Library/Frameworks)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(OpenGL_LIBRARY OpenGL)
    find_library(IOKit_LIBRARY IOKit)
    find_library(CoreVideo_LIBRARY CoreVideo)
    mark_as_advanced(COCOA_LIBRARY OpenGL_LIBRARY)
	
	set(ATLAS_ENGINE_LIBS assimp SDL2main SDL2 GLAD)
    set(ATLAS_ENGINE_SYSTEM_LIBS ${COCOA_LIBRARY} ${IOKit_LIBRARY} ${OpenGL_LIBRARY} ${CoreVideo_LIBRARY})
endif()

if(ANDROID)
	set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_OS_ANDROID)

	set(ATLAS_ENGINE_LIBS assimp SDL2main SDL2 hidapi)
	set(ATLAS_ENGINE_SYSTEM_LIBS android EGL GLESv3 m)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
	set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_OS_LINUX)
	
    add_definitions(${OPENGL_DEFINITIONS})
    find_package(X11 REQUIRED)
	
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
    set (CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -ldl")

    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "./")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	
	set(ATLAS_ENGINE_LIBS assimp SDL2main SDL2 GLAD)
	set(ATLAS_ENGINE_SYSTEM_LIBS X11 Xxf86vm GL dl pthread)
endif()

# Add source files ################################################################################
file(GLOB_RECURSE ATLAS_ENGINE_SOURCE_FILES
        "src/*.cpp"
        "src/*.c"
		"src/*.h"
		"src/*.hpp"
        )
		
list(FILTER ATLAS_ENGINE_SOURCE_FILES EXCLUDE REGEX "^.*\/src\/libraries\/")

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
	
if (ATLAS_OPENGL_ES)
set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_API_GLES)
else()
set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_API_GL)
endif()

if (ATLAS_EXPORT_MAIN)
set(ATLAS_ENGINE_MAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/Main.cpp PARENT_SCOPE)
list(FILTER ATLAS_ENGINE_SOURCE_FILES EXCLUDE REGEX "(^.*\/|^)src\/Main\.cpp$")
endif()

# Configure linking ###############################################################################
if(ANDROID)	
    add_library(${PROJECT_NAME} SHARED ${ATLAS_ENGINE_SOURCE_FILES})
else()
if (ATLAS_BUILD_LIBRARY)
    list(FILTER ATLAS_ENGINE_SOURCE_FILES EXCLUDE REGEX "(^.*\/|^)src\/[Aa]pp\.(c(pp)?|h(pp)?)$")
if (ATLAS_NO_APP)
	list(FILTER ATLAS_ENGINE_SOURCE_FILES EXCLUDE REGEX "(^.*\/|^)src\/Main\.(c(pp)?|h(pp)?)$")
endif()
if (ATLAS_BUILD_SHARED)
    add_library(${PROJECT_NAME} SHARED ${ATLAS_ENGINE_SOURCE_FILES})
else()
	add_library(${PROJECT_NAME} ${ATLAS_ENGINE_SOURCE_FILES})
endif()
else()
    add_executable(${PROJECT_NAME} ${ATLAS_ENGINE_SOURCE_FILES})
endif()
endif()

# Include directories and definitions #############################################################
target_compile_definitions(${PROJECT_NAME} PUBLIC ${ATLAS_ENGINE_COMPILE_DEFINITIONS})
target_include_directories(${PROJECT_NAME} 
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/dependencies
	${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Assimp/include
	${CMAKE_CURRENT_BINARY_DIR}/dependencies/Assimp/include
	${CMAKE_CURRENT_SOURCE_DIR}/src
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src/libraries)
	
# Link libraries ##################################################################################
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ${ATLAS_ENGINE_LIBS} LINK_PRIVATE ${ATLAS_ENGINE_SYSTEM_LIBS})
