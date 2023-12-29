#include "Surface.h"
#include "Instance.h"

#include <SDL_vulkan.h>
#include <cassert>


namespace Atlas {

    namespace Graphics {

        Surface::Surface(Instance* instance, SDL_Window* window, bool& success) :
            instance(instance), window(window) {

            auto nativeInstance = instance->GetNativeInstance();
            success = SDL_Vulkan_CreateSurface(window, nativeInstance, &surface);
            assert(success && "Error creating surface for window");

        }

        Surface::Surface(Instance* instance, bool& success) :
            instance(instance), window(nullptr) {

            auto nativeInstance = instance->GetNativeInstance();

            VkHeadlessSurfaceCreateInfoEXT createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
            createInfo.flags = 0;

            auto result = vkCreateHeadlessSurfaceEXT(nativeInstance, &createInfo, nullptr, &surface);
            VK_CHECK_MESSAGE(result, "Error creating headless surface");

            success = result == VK_SUCCESS;

        }

        Surface::~Surface() {

            auto nativeInstance = instance->GetNativeInstance();
            vkDestroySurfaceKHR(nativeInstance, surface, nullptr);

        }

        VkSurfaceKHR Surface::GetNativeSurface() const {

            return surface;

        }

        SDL_Window *Surface::GetNativeWindow() const {

            return window;

        }

    }

}