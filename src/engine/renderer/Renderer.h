#ifndef AE_RENDERER_H
#define AE_RENDERER_H

#include "../System.h"
#include "../RenderTarget.h"
#include "../Camera.h"
#include "../scene/Scene.h"
#include "../Viewport.h"
#include "../pipeline/PipelineManager.h"
#include "../buffer/UniformBuffer.h"

#include "../graphics/Profiler.h"
#include "../graphics/GraphicsDevice.h"

#include "helper/CommonStructures.h"

namespace Atlas {

    namespace Renderer {

        class Renderer {

        public:
            Renderer() {}

            Renderer(const Renderer&) = delete;

            virtual ~Renderer() {}

            Renderer& operator=(const Renderer&) = delete;

        protected:
            Graphics::GraphicsDevice* device = nullptr;

        };

    }

}

#endif