#ifndef AE_GRAPHICSSURFACE_H
#define AE_GRAPHICSSURFACE_H

#include <volk.h>
#include <SDL.h>

namespace Atlas {

    namespace Graphics {

        class Instance;

        class Surface {
        public:
            Surface(Instance* instance, SDL_Window* window, bool& success);

            ~Surface();

            VkSurfaceKHR GetNativeSurface() const;

            SDL_Window* GetNativeWindow() const;

        private:
            Instance* instance = nullptr;

            VkSurfaceKHR surface;
            SDL_Window* window = nullptr;

        };

    }

}

#endif
