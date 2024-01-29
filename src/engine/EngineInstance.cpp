#include "EngineInstance.h"
#include "graphics/Instance.h"

namespace Atlas {

    EngineInstance::EngineInstance(const std::string& instanceName, int32_t windowWidth,
        int32_t windowHeight, int32_t flags, bool createMainRenderer) : window(instanceName, AE_WINDOWPOSITION_UNDEFINED,
        AE_WINDOWPOSITION_UNDEFINED, windowWidth, windowHeight, flags) {

        graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

        // If we're in headless mode, we already have a valid surface
#ifndef AE_HEADLESS
        // Only create swap chain after the engine instance window was created and
        // it's surface was assigned to the default graphics device
        auto graphicsInstance = Graphics::Instance::DefaultInstance;
        graphicsDevice->surface = graphicsInstance->CreateSurface(window.GetSDLWindow());
#endif
        graphicsDevice->CreateSwapChain();

        if(createMainRenderer) {
            mainRenderer = std::make_unique<Renderer::MainRenderer>();
            mainRenderer->Init(graphicsDevice);
        }

        auto displayCount = SDL_GetNumVideoDisplays();
        for (int32_t i = 0; i < displayCount; i++) {
            displays.push_back(Display(i));
        }

    }

    EngineInstance::~EngineInstance() {



    }

    void EngineInstance::Update() {

        if(mainRenderer)
            mainRenderer->Update();

    }

    ivec2 EngineInstance::GetScreenSize() {

        return Engine::GetScreenSize();

    }

    void EngineInstance::Exit() {

        Events::EventManager::QuitEventDelegate.Fire();

    }

}