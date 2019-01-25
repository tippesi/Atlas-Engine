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

set(OpenGL_GL_PREFERENCE GLVND)

include_directories(${OPENGL_INCLUDE_DIR})

set(ENGINE_COMPILE_DEFINITIONS ENGINE_CMAKE)

if (NOT ANDROID)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/libraries/glad)
endif()

# Setting linker flags and library stuff for the different platforms

if(MSVC)
	set(ENGINE_COMPILE_DEFINITIONS ${ENGINE_COMPILE_DEFINITIONS} ENGINE_WINDOWS)
    # for detecting Windows compiler MSVC
	find_library(SDL_LIBRARY SDL2 ${CMAKE_CURRENT_SOURCE_DIR}/lib/Windows/lib)
	find_library(SDLMAIN_LIBRARY SDL2main ${CMAKE_CURRENT_SOURCE_DIR}/lib/Windows/lib)
    find_library(ASSIMP_LIBRARY assimp-vc140-mt ${CMAKE_CURRENT_SOURCE_DIR}/lib/Windows/lib)
    set(BLUE_ENGINE_LIBS ${SDL_LIBRARY} ${SDLMAIN_LIBRARY} opengl32 ${ASSIMP_LIBRARY})
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
	find_library(ASSIMP_LIBRARY assimp ${CMAKE_CURRENT_SOURCE_DIR}/platform/android)
    find_library(SDLMAIN_LIBRARY SDL2main ${CMAKE_CURRENT_SOURCE_DIR}/platform/android)
    find_library(SDL_LIBRARY SDL2 ${CMAKE_CURRENT_SOURCE_DIR}/platform/android)
	set(BLUE_ENGINE_LIBS ${SDL_LIBRARY} ${SDLMAIN_LIBRARY} ${OpenGL_LIBRARY} ${ASSIMP_LIBRARY})
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
	set(ENGINE_COMPILE_DEFINITIONS ${ENGINE_COMPILE_DEFINITIONS} ENGINE_LINUX)
    # for Linux, BSD, Solaris, Minix
    find_library(ASSIMP_LIBRARY assimp.4.1.0 ${CMAKE_CURRENT_SOURCE_DIR}/lib/Linux)
    find_library(SDLMAIN_LIBRARY SDL2main ${CMAKE_CURRENT_SOURCE_DIR}/lib/Linux)
    find_library(SDL_LIBRARY SDL2 ${CMAKE_CURRENT_SOURCE_DIR}/lib/Linux)

    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
    find_package(OpenGL REQUIRED)
    add_definitions(${OPENGL_DEFINITIONS})
    find_package(X11 REQUIRED)
    # note that the order is important for setting the libs
    # use pkg-config --libs $(pkg-config --print-requires --print-requires-private glfw3) in a terminal to confirm
    set(BLUE_ENGINE_LIBS ${SDL_LIBRARY} ${SDLMAIN_LIBRARY} X11 Xxf86vm GL dl pthread ${ASSIMP_LIBRARY})
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
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/SDL)
	set(BLUE_ENGINE_LIBS  ${BLUE_ENGINE_LIBS} SDL2main SDL2)
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

# Postbuild stuff (we should probably force users to download dependencies on their own)
if(APPLE)
    # for MacOS X or iOS, watchOS, tvOS (since 3.10.3)
	# RPath stuff
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        if (NOT BUILD_LIBRARY)

    add_custom_command(TARGET ${PROJECT_NAME}
            POST_BUILD COMMAND
            ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/"
            $<TARGET_FILE:${PROJECT_NAME}>) # this is for the framework stuff

    add_custom_command(TARGET ${PROJECT_NAME}
            POST_BUILD COMMAND
            ${CMAKE_INSTALL_NAME_TOOL} -change /Users/corbiniangruber/Downloads/assimp-4.1.0/lib/libassimp.4.dylib @executable_path/libassimp.4.1.0.dylib
            $<TARGET_FILE:${PROJECT_NAME}>)

        endif()
    endif()

    # copy libraries
    add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND cp -rf ${CMAKE_CURRENT_SOURCE_DIR}/lib/mac/libassimp.4.1.0.dylib ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libassimp.4.1.0.dylib
            )
    add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND cp -rf ${CMAKE_CURRENT_SOURCE_DIR}/lib/mac/SDL2.framework ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/SDL2.framework
            )
endif()

if(UNIX AND NOT APPLE)
    # for Linux, BSD, Solaris, Minix
    add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND cp -rf ${CMAKE_CURRENT_SOURCE_DIR}/lib/Linux/libassimp.4.1.0.so ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libassimp.so.4
            )
    add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND cp -rf ${CMAKE_CURRENT_SOURCE_DIR}/lib/Linux/libSDL2-2.0.so.0.9.0 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libSDL2-2.0.so
            )
endif()

if(MSVC)
    # for detecting Windows compiler MSVC
    add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND ${CMAKE_COMMAND}
            ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/lib/Windows/dll/assimp-vc140-mt.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
            )
	add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND ${CMAKE_COMMAND}
            ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/lib/Windows/dll/SDL2.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
            )
endif()
