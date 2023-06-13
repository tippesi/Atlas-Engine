#include "EngineInstance.h"

namespace Atlas {

    EngineInstance* EngineInstance::instance;

    EngineInstance::EngineInstance(const std::string& instanceName, int32_t windowWidth,
        int32_t windowHeight, int32_t flags, bool createMainRenderer) : window(instanceName, AE_WINDOWPOSITION_UNDEFINED,
        AE_WINDOWPOSITION_UNDEFINED, windowWidth, windowHeight, flags) {

        graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

        // Only create swap chain after the engine instance window was created and
        // it's surface was assigned to the default graphics device
        graphicsDevice->surface = window.surface;
        graphicsDevice->CreateSwapChain();

        // Clean up old invisible default window and assign this one
        delete Engine::DefaultWindow;
        Engine::DefaultWindow = &window;

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

    EngineInstance* EngineInstance::GetInstance() {

        return instance;

    }

}