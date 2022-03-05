if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the source code and call cmake from there")
endif()

project(AtlasEngine)
cmake_minimum_required(VERSION 3.7)

# Options and compiler settings ###################################################################
option(ATLAS_BUILD_SHARED "Force build as shared library" OFF)
option(ATLAS_OPENGL_ES "Use OpenGL ES instead of OpenGL" OFF)
option(ATLAS_EXPORT_MAIN "Add main file in root of directory tree. Add ${ATLAS_ENGINE_MAIN_FILE} to your executable" OFF)
option(ATLAS_NO_APP "Disables the engines main function" OFF)

# Validate options ################################################################################
if (ATLAS_NO_APP AND ATLAS_EXPORT_MAIN)
set (ATLAS_EXPORT_MAIN OFF)
endif()

# Message options #################################################################################
if (ATLAS_OPENGL_ES)
message(STATUS "Using OpenGL ES")
else()
message(STATUS "Using OpenGL")
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

# Configure platform specific stuff ###############################################################
if(WIN32)
	set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_OS_WINDOWS)

	set(ATLAS_ENGINE_LIBS assimp SDL2main SDL2 GLAD)
    set(ATLAS_ENGINE_SYSTEM_LIBS opengl32 user32 Shlwapi)
endif()

if (MINGW)
	set(ATLAS_ENGINE_LIBS mingw32 ${ATLAS_ENGINE_LIBS})
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
	set(ATLAS_ENGINE_SYSTEM_LIBS X11 GL dl pthread)
endif()

# Add source files ################################################################################
file(GLOB_RECURSE ATLAS_ENGINE_SOURCE_FILES
        "*.cpp"
        "*.c"
		"*.h"
		"*.hpp"
        )
		
list(FILTER ATLAS_ENGINE_SOURCE_FILES EXCLUDE REGEX "^.*\/libraries\/")

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
set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_API_GLES CACHE INTERNAL "")
else()
set(ATLAS_ENGINE_COMPILE_DEFINITIONS ${ATLAS_ENGINE_COMPILE_DEFINITIONS} AE_API_GL CACHE INTERNAL "")
endif()

if (ATLAS_EXPORT_MAIN)
set(ATLAS_ENGINE_MAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp PARENT_SCOPE)
list(FILTER ATLAS_ENGINE_SOURCE_FILES EXCLUDE REGEX "(^.*\/|^)Main\.cpp$")
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

# Include directories and definitions #############################################################
target_compile_definitions(${PROJECT_NAME} PUBLIC ${ATLAS_ENGINE_COMPILE_DEFINITIONS})
target_include_directories(${PROJECT_NAME} 
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/../dependencies
	${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/Assimp/include
	${CMAKE_CURRENT_BINARY_DIR}/../dependencies/Assimp/include
	${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/libraries)
	
# Link libraries ##################################################################################
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ${ATLAS_ENGINE_LIBS} LINK_PRIVATE ${ATLAS_ENGINE_SYSTEM_LIBS})
