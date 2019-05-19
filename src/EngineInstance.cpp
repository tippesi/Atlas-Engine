#include "EngineInstance.h"

namespace Atlas {

	void EngineInstance::Update() {

		masterRenderer.Update();

	}

	void EngineInstance::Exit() {

		Events::EventManager::QuitEventDelegate.Fire();

	}

}