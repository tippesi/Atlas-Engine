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

		mainRenderer.Update();

	}

	ivec2 EngineInstance::GetScreenSize() {

		return Engine::GetScreenSize();

	}

	void EngineInstance::LockFramerate() {

		Engine::LockFramerate();

	}

	void EngineInstance::UnlockFramerate() {

		Engine::UnlockFramerate();

	}

	void EngineInstance::Exit() {

		Events::EventManager::QuitEventDelegate.Fire();

	}

}