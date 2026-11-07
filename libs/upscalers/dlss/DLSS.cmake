option(ATLAS_DLSS "Build the DLSS upscaler when its SDK is found" ON)
set(ATLAS_DLSS_SDK "${CMAKE_CURRENT_LIST_DIR}/sdk" CACHE PATH "DLSS SDK with the include and lib folders of github.com/NVIDIA/DLSS")

if (ATLAS_DLSS AND EXISTS "${ATLAS_DLSS_SDK}/include/nvsdk_ngx_vk.h" AND (WIN32 OR (UNIX AND NOT APPLE AND NOT ANDROID)))
    message(STATUS "Building the DLSS upscaler with ${ATLAS_DLSS_SDK}")

    target_sources(AtlasEngine PRIVATE ${CMAKE_CURRENT_LIST_DIR}/DLSSRenderer.cpp ${CMAKE_CURRENT_LIST_DIR}/DLSSRenderer.h)
    target_include_directories(AtlasEngine PRIVATE ${ATLAS_DLSS_SDK}/include)

    # NGX loads the DLSS library from the executable folder, other than in release builds also from this one
    if (WIN32)
        target_link_libraries(AtlasEngine LINK_PRIVATE
            "${ATLAS_DLSS_SDK}/lib/Windows_x86_64/x64/nvsdk_ngx_d$<$<CONFIG:Debug>:_dbg>.lib")
        set(ATLAS_DLSS_RUNTIME_DIR "${ATLAS_DLSS_SDK}/lib/Windows_x86_64/rel")
    else()
        target_link_libraries(AtlasEngine LINK_PRIVATE "${ATLAS_DLSS_SDK}/lib/Linux_x86_64/libnvsdk_ngx.a" dl)
        set(ATLAS_DLSS_RUNTIME_DIR "${ATLAS_DLSS_SDK}/lib/Linux_x86_64/rel")
    endif()

    # Only for the files which use it, the precompiled header of the engine is shared with the applications
    set_property(SOURCE ${CMAKE_CURRENT_LIST_DIR}/DLSSRenderer.cpp ${ATLAS_UPSCALER_ADDONS_SOURCE} APPEND PROPERTY
        COMPILE_DEFINITIONS AE_UPSCALER_DLSS "$<$<NOT:$<CONFIG:Release>>:AE_DLSS_RUNTIME_DIR=\"${ATLAS_DLSS_RUNTIME_DIR}\">")
endif()
