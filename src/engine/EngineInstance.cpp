#include "EngineInstance.h"

namespace Atlas {

    EngineInstance* EngineInstance::instance;

	EngineInstance::EngineInstance(const std::string& instanceName, int32_t windowWidth,
		int32_t windowHeight, int32_t flags) : window(instanceName, AE_WINDOWPOSITION_UNDEFINED,
        AE_WINDOWPOSITION_UNDEFINED, windowWidth, windowHeight, flags) {

		LockFramerate();

		// Only create swap chain after the engine instance window was created and
		// it's surface was assigned to the default graphics device
		Graphics::GraphicsDevice::DefaultDevice->surface = window.surface;
		Graphics::GraphicsDevice::DefaultDevice->CreateSwapChain();

		// Clean up old invisible default window and assign this one
		delete Engine::DefaultWindow;
		Engine::DefaultWindow = &window;

        mainRenderer.Init(Graphics::GraphicsDevice::DefaultDevice);

	}

    EngineInstance::~EngineInstance() {



    }

	void EngineInstance::Update() {

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

    EngineInstance* EngineInstance::GetInstance() {

        return instance;

    }

}