#include "EngineInstance.h"

namespace Atlas {

    EngineInstance* EngineInstance::instance;

	EngineInstance::EngineInstance(std::string instanceName, int32_t windowWidth,
		int32_t windowHeight, int32_t flags) : window() {

		LockFramerate();

        window = Engine::defaultWindow;
        window->SetTitle(instanceName);
        window->SetPosition(AE_WINDOWPOSITION_UNDEFINED, AE_WINDOWPOSITION_UNDEFINED);
        window->SetSize(windowWidth, windowHeight);
        if (flags & AE_WINDOW_BORDERLESS) window->SetBordered(false);
        if (flags & AE_WINDOW_FULLSCREEN) window->SetFullscreen(true);

	}

    EngineInstance::~EngineInstance() {



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

}