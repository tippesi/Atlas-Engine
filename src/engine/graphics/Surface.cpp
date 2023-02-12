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