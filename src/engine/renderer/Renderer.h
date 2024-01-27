#pragma once

#include "../System.h"
#include "../RenderTarget.h"
#include "../scene/Scene.h"
#include "../Viewport.h"
#include "../pipeline/PipelineManager.h"
#include "../buffer/UniformBuffer.h"

#include "../graphics/Profiler.h"
#include "../graphics/GraphicsDevice.h"

#include "../common/ColorConverter.h"

#include "helper/CommonStructures.h"

namespace Atlas {

    namespace Renderer {

        using namespace Scene::Components;

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