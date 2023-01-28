#include "Surface.h"
#include "Instance.h"

#include <SDL_vulkan.h>
#include <cassert>


namespace Atlas {

    namespace Graphics {

        Surface::Surface(SDL_Window* window, bool& success) : window(window) {

            auto nativeInstance = Instance::DefaultInstance->GetNativeInstance();
            success = SDL_Vulkan_CreateSurface(window, nativeInstance, &surface);
            assert(success && "Error creating surface for window");

        }

        Surface::~Surface() {

            auto nativeInstance = Instance::DefaultInstance->GetNativeInstance();
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