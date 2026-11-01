#include "EngineInstance.h"
#include "graphics/Instance.h"
#include "profiler/CPUProfiler.h"

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

        int32_t displayCount = 0;
        auto displayIDs = SDL_GetDisplays(&displayCount);
        for (int32_t i = 0; i < displayCount; i++) {
            displays.push_back(Display(displayIDs[i]));
        }
        SDL_free(displayIDs);

    }

    EngineInstance::~EngineInstance() {

        if (renderer.joinable()) {
            WaitForRender();
            stopRendering = true;
            renderStart.release();
            renderer.join();
        }

    }

    void EngineInstance::Update() {

        // The render thread updates the main renderer before each frame
        if(mainRenderer && !UsesRenderThread())
            mainRenderer->Update();

    }

    bool EngineInstance::RenderFrame(float deltaTime) {

        if (!UsesRenderThread()) {
            {
                CPUProfiler::Scope profilerScope("Render");
                Render(deltaTime);
            }
            graphicsDevice->SubmitFrame();
            return !graphicsDevice->isDeviceLost;
        }

        WaitForRender();
        if (graphicsDevice->isDeviceLost)
            return false;

        if (!renderer.joinable())
            renderer = std::thread([this]() { RenderLoop(); });

        renderDeltaTime = deltaTime;
        rendering = true;
        renderFrameBegun = false;
        renderStart.release();

        return true;

    }

    void EngineInstance::WaitForRender() {

        if (!UsesRenderThread() || renderFrameBegun)
            return;

        if (rendering) {
            CPUProfiler::Scope profilerScope("Wait for render");
            renderDone.acquire();
            rendering = false;
        }

        renderFrameBegun = true;
        Engine::BeginRenderFrame();

    }

    bool EngineInstance::UsesRenderThread() {

        if (!renderThreadKnown) {
            usesRenderThread = renderThread;
            renderThreadKnown = true;
        }

        return usesRenderThread;

    }

    void EngineInstance::RenderLoop() {

        CPUProfiler::SetThreadName("Render");

        while (true) {
            renderStart.acquire();
            if (stopRendering)
                return;

            {
                CPUProfiler::Scope profilerScope("Render");
                if (mainRenderer)
                    mainRenderer->Update();

                Render(renderDeltaTime);
            }
            graphicsDevice->SubmitFrame();

            renderDone.release();
        }

    }

    ivec2 EngineInstance::GetScreenSize() {

        return Engine::GetScreenSize();

    }

    void EngineInstance::Exit(int32_t exitCode) {

        this->exitCode = exitCode;
        Events::EventManager::QuitEventDelegate.Fire();

    }

}