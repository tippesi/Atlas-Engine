#ifndef AE_RENDERER_H
#define AE_RENDERER_H

#include "../System.h"
#include "../RenderTarget.h"
#include "../Camera.h"
#include "../Scene.h"
#include "../Window.h"

namespace Atlas {

	namespace Renderer {

		class Renderer {

		public:
			virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene) = 0;
			virtual ~Renderer() {}

		};

	}

}

#endif