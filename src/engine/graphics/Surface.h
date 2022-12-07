#ifndef AE_GRAPHICSSURFACE_H
#define AE_GRAPHICSSURFACE_H

#include <volk.h>
#include <SDL.h>

namespace Atlas {

    namespace Graphics {

        class Instance;

        class Surface {
        public:
            Surface(SDL_Window* window, bool& success);

            ~Surface();

            VkSurfaceKHR GetNativeSurface() const;

        private:
            VkSurfaceKHR surface;

        };

    }

}

#endif //ATLASENGINE_SURFACE_H
