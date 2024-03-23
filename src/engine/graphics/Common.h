#pragma once

#include "Initializers.h"
#include "../Log.h"

#define VK_ENABLE_BETA_EXTENSIONS
#include <volk.h>

#include <string>
#include <cassert>

#define FRAME_DATA_COUNT 2
#define DESCRIPTOR_POOL_SIZE 1024u
#define DESCRIPTOR_SET_COUNT 4
#define BINDINGS_PER_DESCRIPTOR_SET 32
#define MAX_COLOR_ATTACHMENTS 8
#define MAX_VERTEX_BUFFER_BINDINGS 16

#define VK_CHECK(x) {                                                               \
                VkResult err = x;                                                   \
                if (err)                                                            \
                {                                                                   \
                    Atlas::Log::Error("Detected Vulkan error: " +                   \
                        Atlas::Graphics::VkResultToString(err));                    \
                    AE_ASSERT(err == VK_SUCCESS);                                      \
                }                                                                   \
            }

#define VK_CHECK_MESSAGE(x,y) {                                                     \
                VkResult err = x;                                                   \
                if (err)                                                            \
                {                                                                   \
                    Atlas::Log::Error("Detected Vulkan error: " +                   \
                        Atlas::Graphics::VkResultToString(err));                    \
                    AE_ASSERT(err == VK_SUCCESS && y);                                 \
                }                                                                   \
            }

namespace Atlas {

    namespace Graphics {

        inline std::string VkResultToString(VkResult result) {
            switch (result) {
#define ENUM_TO_STR(x) \
            case x:    \
                return #x
                ENUM_TO_STR(VK_SUCCESS);
                ENUM_TO_STR(VK_NOT_READY);
                ENUM_TO_STR(VK_TIMEOUT);
                ENUM_TO_STR(VK_EVENT_SET);
                ENUM_TO_STR(VK_EVENT_RESET);
                ENUM_TO_STR(VK_INCOMPLETE);
                ENUM_TO_STR(VK_ERROR_OUT_OF_HOST_MEMORY);
                ENUM_TO_STR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
                ENUM_TO_STR(VK_ERROR_INITIALIZATION_FAILED);
                ENUM_TO_STR(VK_ERROR_DEVICE_LOST);
                ENUM_TO_STR(VK_ERROR_MEMORY_MAP_FAILED);
                ENUM_TO_STR(VK_ERROR_LAYER_NOT_PRESENT);
                ENUM_TO_STR(VK_ERROR_EXTENSION_NOT_PRESENT);
                ENUM_TO_STR(VK_ERROR_FEATURE_NOT_PRESENT);
                ENUM_TO_STR(VK_ERROR_INCOMPATIBLE_DRIVER);
                ENUM_TO_STR(VK_ERROR_TOO_MANY_OBJECTS);
                ENUM_TO_STR(VK_ERROR_FORMAT_NOT_SUPPORTED);
                ENUM_TO_STR(VK_ERROR_FRAGMENTED_POOL);
                ENUM_TO_STR(VK_ERROR_OUT_OF_POOL_MEMORY);
                ENUM_TO_STR(VK_ERROR_INVALID_EXTERNAL_HANDLE);
                ENUM_TO_STR(VK_ERROR_SURFACE_LOST_KHR);
                ENUM_TO_STR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
                ENUM_TO_STR(VK_SUBOPTIMAL_KHR);
                ENUM_TO_STR(VK_ERROR_OUT_OF_DATE_KHR);
                ENUM_TO_STR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
                ENUM_TO_STR(VK_ERROR_VALIDATION_FAILED_EXT);
                ENUM_TO_STR(VK_ERROR_INVALID_SHADER_NV);
                ENUM_TO_STR(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
                ENUM_TO_STR(VK_ERROR_FRAGMENTATION_EXT);
                ENUM_TO_STR(VK_ERROR_NOT_PERMITTED_EXT);
#undef ENUM_TO_STR
                default:
                    return "Unknown error";
            }

        }

    }

}