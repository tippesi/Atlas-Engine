#ifndef AE_VULKANRENDERER_H
#define AE_VULKANRENDERER_H

#include "../RenderTarget.h"
#include "../scene/Scene.h"

#include "../graphics/Profiler.h"
#include "../graphics/GraphicsDevice.h"

namespace Atlas {

    namespace Renderer {

        using namespace Graphics;

        class VulkanRenderer {

            virtual void Init(GraphicsDevice* device) = 0;

            // virtual void Render(Ref<RenderTarget>& target, Ref<Scene>& scene, ) = 0;

        private:
            GraphicsDevice* device = nullptr;

        };

    }

}

#endif
