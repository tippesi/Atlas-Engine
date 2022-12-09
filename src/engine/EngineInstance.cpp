#include "EngineInstance.h"

namespace Atlas {

    EngineInstance* EngineInstance::instance;

	EngineInstance::EngineInstance(std::string instanceName, int32_t windowWidth,
		int32_t windowHeight, int32_t flags) : window() {

		LockFramerate();

        graphicInstance = new Graphics::Instance(instanceName, true);

        if (!graphicInstance->isComplete) {
            Atlas::Log::Error("Couldn't initialize engine instance");
        }

        // Create window after creation of graphics instance
        window = new Window(instanceName,AE_WINDOWPOSITION_UNDEFINED,
            AE_WINDOWPOSITION_UNDEFINED, windowWidth,windowHeight,
            flags, false);

	}

    EngineInstance::~EngineInstance() {

        delete graphicInstance;
        // Surface needs to be deleted after swap chain
        delete window;

    }

	void EngineInstance::Update() {

		window->Clear();

		// mainRenderer.Update();

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

    EngineInstance* EngineInstance::GetInstance() {

        return instance;

    }

    Graphics::Instance *EngineInstance::GetGraphicsInstance() {

        return instance->graphicInstance;

    }

}