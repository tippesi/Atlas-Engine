if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the source code and call cmake from there")
endif()

project(BlueEngine)
cmake_minimum_required(VERSION 3.6)

option(BUILD_LIBRARY "Build as library" OFF)
option(OPENGL_ES "Use OpenGL ES instead of OpenGL" OFF)

if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT ANDROID)
	message(FATAL_ERROR "32 bit isn't supported")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# Override Assimp settings
unset(CMAKE_DEBUG_POSTFIX CACHE)
# Override SDL settings
unset(SDL_CMAKE_DEBUG_POSTFIX CACHE)

set(OpenGL_GL_PREFERENCE GLVND)

include_directories(${OPENGL_INCLUDE_DIR})

set(ENGINE_COMPILE_DEFINITIONS ENGINE_CMAKE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Assimp EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/SDL EXCLUDE_FROM_ALL)
if (ANDROID)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Hidapi EXCLUDE_FROM_ALL)
endif()
if (NOT ANDROID)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/libraries/glad)
endif()

# Setting linker flags and library stuff for the different platforms

if(MSVC)
	set(ENGINE_COMPILE_DEFINITIONS ${ENGINE_COMPILE_DEFINITIONS} ENGINE_WINDOWS)
    # for detecting Windows compiler MSVC
	set(BLUE_ENGINE_LIBS assimp SDL2main SDL2)
    # find_library(ASSIMP_LIBRARY assimp-vc140-mt ${CMAKE_CURRENT_SOURCE_DIR}/lib/Windows/lib)
    set(BLUE_ENGINE_LIBS ${BLUE_ENGINE_LIBS} opengl32)
endif()

if(APPLE)
	set(ENGINE_COMPILE_DEFINITIONS ${ENGINE_COMPILE_DEFINITIONS} ENGINE_OSX)
    # for MacOS X or iOS, watchOS, tvOS (since 3.10.3)
    find_library(ASSIMP_LIBRARY assimp.4.1.0 ${CMAKE_CURRENT_SOURCE_DIR}/lib/Mac)
    #find_library(SDLMAIN_LIBRARY SDL2main ${CMAKE_CURRENT_SOURCE_DIR}/lib/Mac)
    find_library(SDL_LIBRARY SDL2 ${CMAKE_CURRENT_SOURCE_DIR}/lib/Mac)

    include_directories(/System/Library/Frameworks)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(OpenGL_LIBRARY OpenGL)
    find_library(IOKit_LIBRARY IOKit)
    find_library(CoreVideo_LIBRARY CoreVideo)
    mark_as_advanced(COCOA_LIBRARY OpenGL_LIBRARY)
    set(APPLE_LIBS ${COCOA_LIBRARY} ${IOKit_LIBRARY} ${OpenGL_LIBRARY} ${CoreVideo_LIBRARY})
    set(APPLE_LIBS ${APPLE_LIBS} ${ASSIMP_LIBRARY} ${SDL_LIBRARY})
    set(BLUE_ENGINE_LIBS ${APPLE_LIBS})
endif()

if(ANDROID)
	set(ENGINE_COMPILE_DEFINITIONS ${ENGINE_COMPILE_DEFINITIONS} ENGINE_ANDROID)
	set(BLUE_ENGINE_LIBS assimp SDL2main SDL2 hidapi)
	set(BLUE_ENGINE_LIBS ${BLUE_ENGINE_LIBS} android EGL GLESv3 m)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
	set(ENGINE_COMPILE_DEFINITIONS ${ENGINE_COMPILE_DEFINITIONS} ENGINE_LINUX)
    # for Linux, BSD, Solaris, Minix
	
    set(BLUE_ENGINE_LIBS assimp SDL2main SDL2)

    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
    find_package(OpenGL REQUIRED)
    add_definitions(${OPENGL_DEFINITIONS})
    find_package(X11 REQUIRED)
    # note that the order is important for setting the libs
    # use pkg-config --libs $(pkg-config --print-requires --print-requires-private glfw3) in a terminal to confirm
    set(BLUE_ENGINE_LIBS ${BLUE_ENGINE_LIBS} X11 Xxf86vm GL dl pthread)
    set (CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -ldl")

        # rpath settings
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "./")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# include directories

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/libraries)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/libraries/glad)

file(GLOB_RECURSE BLUE_ENGINE_SOURCE_FILES
        "src/*.cpp"
        "src/*.c"
		"src/*.h"
		"src/*.hpp"
        )
		
list(FILTER BLUE_ENGINE_SOURCE_FILES EXCLUDE REGEX ".*src/libraries/.*")

if (OPENGL_ES OR ANDROID)
set(ENGINE_COMPILE_DEFINITIONS ${ENGINE_COMPILE_DEFINITIONS} ENGINE_GLES)
else()
set(ENGINE_COMPILE_DEFINITIONS ${ENGINE_COMPILE_DEFINITIONS} ENGINE_GL)
endif()

# For Android we have to always generate a shared library
if(ANDROID)	
    add_library(${PROJECT_NAME} SHARED ${BLUE_ENGINE_SOURCE_FILES})
	target_compile_definitions(${PROJECT_NAME} PUBLIC ${ENGINE_COMPILE_DEFINITIONS})
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(BLUE_ENGINE_LIBS  ${BLUE_ENGINE_LIBS} PARENT_SCOPE)
endif()
    target_link_libraries(${PROJECT_NAME} ${BLUE_ENGINE_LIBS})
else()
# When the engine is just a subproject we dont want to produce an executable
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
if (BUILD_LIBRARY)
    list(FILTER BLUE_ENGINE_SOURCE_FILES EXCLUDE REGEX "./src/main.cpp")
    list(FILTER BLUE_ENGINE_SOURCE_FILES EXCLUDE REGEX "./src/main.c")
    add_library(${PROJECT_NAME} ${BLUE_ENGINE_SOURCE_FILES})
	target_compile_definitions(${PROJECT_NAME} PUBLIC ${ENGINE_COMPILE_DEFINITIONS})
    target_link_libraries(${PROJECT_NAME} GLAD ${BLUE_ENGINE_LIBS})
    set(CMAKE_BUILD_TYPE Release ... FORCE)
else()
    add_executable(${PROJECT_NAME} ${BLUE_ENGINE_SOURCE_FILES})
	target_compile_definitions(${PROJECT_NAME} PUBLIC ${ENGINE_COMPILE_DEFINITIONS})
    target_link_libraries(${PROJECT_NAME} GLAD ${BLUE_ENGINE_LIBS})
endif()
else()
    list(FILTER BLUE_ENGINE_SOURCE_FILES EXCLUDE REGEX "./src/main.cpp")
    list(FILTER BLUE_ENGINE_SOURCE_FILES EXCLUDE REGEX "./src/main.c")
    add_library(${PROJECT_NAME} ${BLUE_ENGINE_SOURCE_FILES})
	target_compile_definitions(${PROJECT_NAME} PUBLIC ${ENGINE_COMPILE_DEFINITIONS})
    set(BLUE_ENGINE_LIBS GLAD ${BLUE_ENGINE_LIBS} PARENT_SCOPE)
    target_link_libraries(${PROJECT_NAME} ${BLUE_ENGINE_LIBS})
endif()
endif()