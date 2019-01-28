if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the source code and call cmake from there")
endif()

project(BlueEngine)
cmake_minimum_required(VERSION 3.7)

# Options and compiler settings ###################################################################
option(BUILD_LIBRARY "Build as library" OFF)
option(OPENGL_ES "Use OpenGL ES instead of OpenGL" OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "Override Assimp settings")

# Find OpenGL #####################################################################################
if (ANDROID)
set(OPENGL_ES ON)
find_library(OpenGL REQUIRED)
endif()
if (NOT OPENGL_ES)
find_package(OpenGL REQUIRED)
set(OpenGL_GL_PREFERENCE GLVND)
endif()

# Add dependencies ################################################################################
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Assimp EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/SDL EXCLUDE_FROM_ALL)
if (ANDROID)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Hidapi EXCLUDE_FROM_ALL)
else()
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Glad EXCLUDE_FROM_ALL)
endif()

# Configure platform specific stuff ###############################################################
if(MSVC)
	set(ENGINE_COMPILE_DEFINITIONS ENGINE_WINDOWS)

	set(BLUE_ENGINE_LIBS assimp SDL2main SDL2 GLAD)
    set(BLUE_ENGINE_SYSTEM_LIBS opengl32)
endif()

if(APPLE)
	set(ENGINE_COMPILE_DEFINITIONS ENGINE_OSX)

    include_directories(/System/Library/Frameworks)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(OpenGL_LIBRARY OpenGL)
    find_library(IOKit_LIBRARY IOKit)
    find_library(CoreVideo_LIBRARY CoreVideo)
    mark_as_advanced(COCOA_LIBRARY OpenGL_LIBRARY)
	
	set(BLUE_ENGINE_LIBS assimp SDL2main SDL2 GLAD)
    set(BLUE_ENGINE_SYSTEM_LIBS ${COCOA_LIBRARY} ${IOKit_LIBRARY} ${OpenGL_LIBRARY} ${CoreVideo_LIBRARY})
endif()

if(ANDROID)
	set(ENGINE_COMPILE_DEFINITIONS ENGINE_ANDROID)
	
	set(BLUE_ENGINE_LIBS assimp SDL2main SDL2 hidapi)	
	set(BLUE_ENGINE_SYSTEM_LIBS android EGL GLESv3 m)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
	set(ENGINE_COMPILE_DEFINITIONS ENGINE_LINUX)
	
    add_definitions(${OPENGL_DEFINITIONS})
    find_package(X11 REQUIRED)
	
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
    set (CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -ldl")

    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "./")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	
	set(BLUE_ENGINE_LIBS assimp SDL2main SDL2 GLAD)
	set(BLUE_ENGINE_SYSTEM_LIBS X11 Xxf86vm GL dl pthread)
endif()

# Add source files ################################################################################
file(GLOB_RECURSE BLUE_ENGINE_SOURCE_FILES
        "src/*.cpp"
        "src/*.c"
		"src/*.h"
		"src/*.hpp"
        )
		
list(FILTER BLUE_ENGINE_SOURCE_FILES EXCLUDE REGEX "^.*\/src\/libraries\/")

if (OPENGL_ES)
set(ENGINE_COMPILE_DEFINITIONS ${ENGINE_COMPILE_DEFINITIONS} ENGINE_GLES)
else()
set(ENGINE_COMPILE_DEFINITIONS ${ENGINE_COMPILE_DEFINITIONS} ENGINE_GL)
endif()

# Configure linking ###############################################################################
if(ANDROID)	
    add_library(${PROJECT_NAME} SHARED ${BLUE_ENGINE_SOURCE_FILES})
else()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
if (BUILD_LIBRARY)
    list(FILTER BLUE_ENGINE_SOURCE_FILES EXCLUDE REGEX "^.*\/src\/[Mm]ain.c(pp)?")
    add_library(${PROJECT_NAME} STATIC ${BLUE_ENGINE_SOURCE_FILES})
else()
    add_executable(${PROJECT_NAME} ${BLUE_ENGINE_SOURCE_FILES})
endif()
else()
    list(FILTER BLUE_ENGINE_SOURCE_FILES EXCLUDE REGEX "^.*\/src\/[Mm]ain.c(pp)?")
    add_library(${PROJECT_NAME} STATIC ${BLUE_ENGINE_SOURCE_FILES})
endif()
endif()

# Include directories and definitions #############################################################
target_compile_definitions(${PROJECT_NAME} PUBLIC ${ENGINE_COMPILE_DEFINITIONS})
target_include_directories(${PROJECT_NAME} 
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/dependencies
	${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Assimp/include
	${CMAKE_CURRENT_BINARY_DIR}/dependencies/Assimp/include
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}/src/libraries)

# Link libraries ##################################################################################
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ${BLUE_ENGINE_LIBS} LINK_PRIVATE ${BLUE_ENGINE_SYSTEM_LIBS})