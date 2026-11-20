option(ATLAS_FSR4 "Build the FSR 4 upscaler when its SDK is found" ON)
set(ATLAS_FSR4_SDK "${CMAKE_CURRENT_LIST_DIR}/sdk" CACHE PATH "Headers and the signed Vulkan upscaler library of the AMD FSR SDK")

# The SDK only has signed libraries for Windows
if (ATLAS_FSR4 AND EXISTS "${ATLAS_FSR4_SDK}/upscalers/include/ffx_upscale.h" AND WIN32)
    message(STATUS "Building the FSR 4 upscaler with ${ATLAS_FSR4_SDK}")

    target_sources(AtlasEngine PRIVATE ${CMAKE_CURRENT_LIST_DIR}/FSR4Renderer.cpp ${CMAKE_CURRENT_LIST_DIR}/FSR4Renderer.h)
    target_include_directories(AtlasEngine PRIVATE ${ATLAS_FSR4_SDK})

    # The upscaler library is loaded at runtime from the executable folder, other than in release builds also from this one
    set_property(SOURCE ${CMAKE_CURRENT_LIST_DIR}/FSR4Renderer.cpp ${ATLAS_UPSCALER_ADDONS_SOURCE} APPEND PROPERTY
        COMPILE_DEFINITIONS AE_UPSCALER_FSR4 "$<$<NOT:$<CONFIG:Release>>:AE_FSR4_RUNTIME_DIR=\"${ATLAS_FSR4_SDK}/bin\">")
endif()
