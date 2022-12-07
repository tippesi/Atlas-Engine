#include "Surface.h"
#include "../EngineInstance.h"

#include <SDL_vulkan.h>
#include <cassert>


namespace Atlas {

    namespace Graphics {

        Surface::Surface(SDL_Window* window, bool& success) {

            auto nativeInstance = EngineInstance::GetGraphicsInstance()->GetNativeInstance();
            success = SDL_Vulkan_CreateSurface(window, nativeInstance, &surface);
            assert(success && "Error creating surface for window");

        }

        Surface::~Surface() {

            auto nativeInstance = EngineInstance::GetGraphicsInstance()->GetNativeInstance();
            vkDestroySurfaceKHR(nativeInstance, surface, nullptr);

        }

        VkSurfaceKHR Surface::GetNativeSurface() const {

            return surface;

        }

    }

}