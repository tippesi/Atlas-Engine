#include "EngineInstance.h"

namespace Atlas {

	EngineInstance::EngineInstance(std::string windowTitle, int32_t windowWidth, 
		int32_t windowHeight, int32_t flags) : window(windowTitle, 
		AE_WINDOWPOSITION_UNDEFINED, AE_WINDOWPOSITION_UNDEFINED, windowWidth, 
		windowHeight, flags) {

		LockFramerate();

	}

	void EngineInstance::Update() {

		window.Clear();

		masterRenderer.Update();

	}

	ivec2 EngineInstance::GetScreenSize() {

		SDL_DisplayMode displayMode;
		SDL_GetCurrentDisplayMode(0, &displayMode);

		return ivec2(displayMode.w, displayMode.h);

	}

	void EngineInstance::LockFramerate() {

		SDL_GL_SetSwapInterval(1);

	}

	void EngineInstance::UnlockFramerate() {

		SDL_GL_SetSwapInterval(0);

	}

	void EngineInstance::Exit() {

		Events::EventManager::QuitEventDelegate.Fire();

	}

}