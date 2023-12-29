#pragma once

#include <volk.h>
#include <SDL.h>

namespace Atlas {

    namespace Graphics {

        class Instance;

        class Surface {
        public:
            Surface(Instance* instance, SDL_Window* window, bool& success);

            Surface(Instance* instance, bool& success);

            ~Surface();

            VkSurfaceKHR GetNativeSurface() const;

            void SetExtent(int32_t width, int32_t height);

            void GetExtent(int32_t& width, int32_t& height);

        private:
            Instance* instance = nullptr;

            VkSurfaceKHR surface;
            SDL_Window* window = nullptr;

            int32_t width = -1;
            int32_t height = -1;

        };

    }

}