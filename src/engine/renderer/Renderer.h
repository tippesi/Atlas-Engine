#ifndef AE_RENDERER_H
#define AE_RENDERER_H

#include "../System.h"
#include "../RenderTarget.h"
#include "../Camera.h"
#include "../scene/Scene.h"
#include "../Viewport.h"
#include "../shader/PipelineManager.h"

#include "../graphics/Profiler.h"
#include "../graphics/GraphicsDevice.h"

namespace Atlas {

	namespace Renderer {

        using namespace Graphics;

		class Renderer {

		public:
			Renderer() {}

			Renderer(const Renderer&) = delete;

			virtual ~Renderer() {}

			Renderer& operator=(const Renderer&) = delete;

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) = 0;

        protected:
            GraphicsDevice* device = nullptr;

		};

	}

}

#endif