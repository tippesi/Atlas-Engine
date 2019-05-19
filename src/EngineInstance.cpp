#include "EngineInstance.h"

namespace Atlas {

	EngineInstance::EngineInstance(std::string windowTitle, int32_t windowWidth, 
		int32_t windowHeight, int32_t flags) : window(windowTitle, 
		AE_WINDOWPOSITION_UNDEFINED, AE_WINDOWPOSITION_UNDEFINED, windowWidth, 
		windowHeight, flags) {



	}

	void EngineInstance::Update() {

		masterRenderer.Update();

	}

	void EngineInstance::Exit() {

		Events::EventManager::QuitEventDelegate.Fire();

	}

}