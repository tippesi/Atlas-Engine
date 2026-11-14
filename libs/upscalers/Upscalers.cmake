# Optional upscalers of vendor SDKs, compiled into the AtlasEngine target. An upscaler is only built when its SDK
# was found, without it or on GPUs it doesn't support the engine falls back to FSR2 (see README.md)
get_filename_component(ATLAS_UPSCALER_ADDONS_SOURCE ${CMAKE_CURRENT_LIST_DIR}/../../src/engine/renderer/UpscalerAddons.cpp ABSOLUTE)
include(${CMAKE_CURRENT_LIST_DIR}/dlss/DLSS.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/fsr4/FSR4.cmake)
