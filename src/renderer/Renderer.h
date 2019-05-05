#ifndef AE_RENDERER_H
#define AE_RENDERER_H

#include "../System.h"
#include "../RenderTarget.h"
#include "../Camera.h"
#include "../scene/Scene.h"
#include "../Viewport.h"

namespace Atlas {

	namespace Renderer {

		class Renderer {

		public:
			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) = 0;
			virtual ~Renderer() {}

		};

	}

}

#endif