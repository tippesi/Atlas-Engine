cmake_minimum_required(VERSION 3.5)
project(Blue-Engine)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

set(OpenGL_GL_PREFERENCE GLVND)

include_directories(${OPENGL_INCLUDE_DIR})

add_definitions(-DENGINE_CMAKE)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(SEND_ERROR "32 bit isn't supported")
endif()

# Setting linker flags and library stuff for the different platforms

if(MSVC)
    # for detecting Windows compiler MSVC
    include_directories(lib/windows)
	set(SDL_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/lib/Windows/lib/SDL2.lib)
	set(SDLMAIN_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/lib/Windows/lib/SDL2main.lib)
    set(ASSIMP_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/lib/Windows/lib/assimp-vc140-mt.lib)
    set(GLEW_LIBRARY  ${CMAKE_CURRENT_SOURCE_DIR}/lib/Windows/lib/glew32.lib)
    set(LIBS ${GLEW_LIBRARY} ${SDL_LIBRARY} ${SDLMAIN_LIBRARY} opengl32 ${ASSIMP_LIBRARY})
endif()

if(APPLE)
    # for MacOS X or iOS, watchOS, tvOS (since 3.10.3)
    include_directories(lib/mac)
    set(ASSIMP_LIBRARY assimp.4.1.0)
    set(GLEW_LIBRARY libGLEW)
    link_directories(lib/mac/)

    INCLUDE_DIRECTORIES(/System/Library/Frameworks)
    FIND_LIBRARY(COCOA_LIBRARY Cocoa)
    FIND_LIBRARY(OpenGL_LIBRARY OpenGL)
    FIND_LIBRARY(IOKit_LIBRARY IOKit)
    FIND_LIBRARY(CoreVideo_LIBRARY CoreVideo)
    MARK_AS_ADVANCED(COCOA_LIBRARY OpenGL_LIBRARY)
    SET(APPLE_LIBS ${COCOA_LIBRARY} ${IOKit_LIBRARY} ${OpenGL_LIBRARY} ${CoreVideo_LIBRARY})
    SET(APPLE_LIBS ${APPLE_LIBS} ${GLEW_LIBRARY} ${ASSIMP_LIBRARY})
    set(LIBS ${LIBS} ${APPLE_LIBS})
endif()

if(UNIX AND NOT APPLE)
    # for Linux, BSD, Solaris, Minix
    include_directories(lib/Linux)
    set(ASSIMP_LIBRARY assimp.4.1.0)
    set(GLEW_LIBRARY GLEW)
    set(SDLMAIN_LIBRARY SDL2main)
    set(SDL_LIBRARY SDL2)
    link_directories(lib/Linux/)

    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
    find_package(OpenGL REQUIRED)
    add_definitions(${OPENGL_DEFINITIONS})
    find_package(X11 REQUIRED)
    # note that the order is important for setting the libs
    # use pkg-config --libs $(pkg-config --print-requires --print-requires-private glfw3) in a terminal to confirm
    set(LIBS ${GLEW_LIBRARY} ${SDL_LIBRARY} ${SDLMAIN_LIBRARY} X11 Xrandr Xinerama Xi Xxf86vm Xcursor GL dl pthread ${ASSIMP_LIBRARY})
    set (CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -ldl")

        # rpath settings
    SET(CMAKE_SKIP_BUILD_RPATH FALSE)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    SET(CMAKE_INSTALL_RPATH "./")
    SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# include directories

include_directories(src)
include_directories(src/libraries)

file(GLOB_RECURSE BLUE_ENGINE_SOURCE_FILES
        "src/*.cpp"
        "src/*.c"
		"src/*.h"
		"src/*.hpp"
        )

list(FILTER BLUE_ENGINE_SOURCE_FILES EXCLUDE REGEX ".*src/libraries/.*")

add_executable(${PROJECT_NAME}
${BLUE_ENGINE_SOURCE_FILES}
        )

target_link_libraries(${PROJECT_NAME} ${LIBS})


# Postbuild stuff

if(APPLE)
    # for MacOS X or iOS, watchOS, tvOS (since 3.10.3)
    add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND install_name_tool -change /Users/corbiniangruber/Downloads/assimp-4.1.0/lib/libassimp.4.dylib @executable_path/libassimp.4.1.0.dylib ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}
            )
    add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND cp -rf ../lib/mac/libassimp.4.1.0.dylib ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libassimp.4.1.0.dylib
            )
endif()

if(UNIX AND NOT APPLE)
    # for Linux, BSD, Solaris, Minix
    add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND cp -rf lib/Linux/libassimp.4.1.0.so ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libassimp.so.4
            )
    add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND cp -rf lib/Linux/libGLEW.so ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libGLEW.so.2.1
            )
endif()

if(MSVC)
    # for detecting Windows compiler MSVC
    add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND ${CMAKE_COMMAND}
            ARGS -E copy ../lib/Windows/dll/assimp-vc140-mt.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
            )
    add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND ${CMAKE_COMMAND}
            ARGS -E copy ../lib/Windows/dll/glew32.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
            )
	add_custom_command(TARGET ${PROJECT_NAME}
            COMMAND ${CMAKE_COMMAND}
            ARGS -E copy ../lib/Windows/dll/SDL2.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
            )
endif()
