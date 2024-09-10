cmake_minimum_required(VERSION 3.24)

project(AtlasEngineDemo VERSION 0.2.0)

# Note: For this project, the root CMakeLists.txt turns
# the ATLAS_IMGUI and ATLAS_EXPORT_MAIN options on.

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

file(GLOB_RECURSE DEMO_SOURCE_FILES
        "*.cpp"
        "*.c"
        "*.h"
        "*.hpp"
        )

# Required: Set both the source and dependency directories 
# as include directories
include_directories(../engine)
include_directories(../../libs)

foreach(SOURCE_FILE IN ITEMS ${DEMO_SOURCE_FILES})
    if (IS_ABSOLUTE "${SOURCE_FILE}")
        file(RELATIVE_PATH SOURCE_FILE_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${SOURCE_FILE}")
    else()
        set(SOURCE_FILE_REL "${SOURCE_FILE}")
    endif()
    get_filename_component(SOURCE_PATH "${SOURCE_FILE_REL}" PATH)
    string(REPLACE "/" "\\" SOURCE_PATH_CONVERTED "${SOURCE_PATH}")
    source_group("${SOURCE_PATH_CONVERTED}" FILES "${SOURCE_FILE}")
endforeach()  

# We want to make sure that the linker searches for local libraries first
if (UNIX AND NOT APPLE AND NOT ANDROID)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath='$ORIGIN'")
endif()
# Need to reference the vulkan SDK for MacOS to search at runtime
if (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,'/usr/local/lib'")
endif()

# We use the exported main file from the AtlasEngine library to able to use
# the app class. Alternatively, you can write a main function yourself.
# To export the main file the ATLAS_EXPORT_MAIN option has to be turned on.
if (APPLE AND ATLAS_BUNDLE)
add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${DEMO_SOURCE_FILES} ${ATLAS_ENGINE_MAIN_FILE})
target_compile_definitions(${PROJECT_NAME} PUBLIC AE_BUNDLE ${ATLAS_ENGINE_COMPILE_DEFINITIONS})
else()
add_executable(${PROJECT_NAME} ${DEMO_SOURCE_FILES} ${ATLAS_ENGINE_MAIN_FILE})
target_compile_definitions(${PROJECT_NAME} PUBLIC ${ATLAS_ENGINE_COMPILE_DEFINITIONS})
endif()
# We want to use both ImGui and the AtlasEngine. For ImGui, the ATLAS_IMGUI option
# needs to be turned on.
target_link_libraries (${PROJECT_NAME} AtlasEngine ImguiExtension)

if (APPLE AND ATLAS_BUNDLE)
    set(path ${Vulkan_LIBRARY})
    cmake_path(GET path PARENT_PATH VULKAN_SDK)
    file(GLOB FRAMEWORK_FILES
        "${VULKAN_SDK}/libvulkan.1.*"
        "${VULKAN_SDK}/libMoltenVK.dylib"
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.atlasengindemo"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE icon.png
    )
    install(
        FILES ${FRAMEWORK_FILES}
        DESTINATION "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app/Contents/Frameworks"
    )
    install(
        FILES "${CMAKE_SOURCE_DIR}/data/icon.png"
        DESTINATION "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app/Contents/Resources"
    )
    install(
        DIRECTORY "${CMAKE_SOURCE_DIR}/data"
        DESTINATION "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app/Contents/Resources"
    )
    install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION ${CMAKE_BINARY_DIR}
    )
endif()