#ifndef AE_ENGINEINSTANCE_H
#define AE_ENGINEINSTANCE_H

#include "Engine.h"

namespace Atlas {

	class EngineInstance {

	public:
		virtual void LoadContent() = 0;

		virtual void UnloadContent() = 0;

		virtual void Update(float deltaTime) = 0;

		virtual void Render(float deltaTime) = 0;

		void Update();

	protected:
		void Exit();

		Renderer::MasterRenderer masterRenderer;

	};

}


#endif