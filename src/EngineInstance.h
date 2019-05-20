#ifndef AE_ENGINEINSTANCE_H
#define AE_ENGINEINSTANCE_H

#include "Engine.h"
#include "Context.h"

namespace Atlas {

	class EngineInstance {

	public:
		EngineInstance(std::string windowTitle, int32_t windowWidth,
			int32_t windowHeight, int32_t flags = AE_WINDOW_RESIZABLE);

		virtual void LoadContent() = 0;

		virtual void UnloadContent() = 0;

		virtual void Update(float deltaTime) = 0;

		virtual void Render(float deltaTime) = 0;

		void Update();

		static std::string assetDirectory;
		static std::string shaderDirectory;

		Window window;
		Context context;

	protected:
		void Exit();

		Renderer::MasterRenderer masterRenderer;

	};

}


#endif