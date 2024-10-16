#include "Engine.h"
#include "graphics/Extensions.h"
#include "graphics/Profiler.h"
#include "EngineInstance.h"
#include "loader/ShaderLoader.h"
#include "graphics/Instance.h"
#include "pipeline/PipelineManager.h"
#include "physics/PhysicsManager.h"
#include "texture/Texture.h"
#include "input/KeyboardMap.h"
#include "events/EventManager.h"
#include "jobsystem/JobSystem.h"

#include "graphics/ShaderCompiler.h"

#include <SDL_vulkan.h>

extern Atlas::EngineInstance* GetEngineInstance();

namespace Atlas {

    Window* Engine::DefaultWindow = nullptr;

    void Engine::Init(EngineConfig config) {

#ifdef AE_NO_APP
        SDL_SetMainReady();
#endif
        if (SDL_WasInit(SDL_INIT_EVERYTHING) != SDL_INIT_EVERYTHING) {
            SDL_Init(SDL_INIT_EVERYTHING);
        }

        Loader::AssetLoader::SetAssetDirectory(config.assetDirectory);
        Loader::ShaderLoader::SetSourceDirectory(config.shaderDirectory);

        Graphics::ShaderCompiler::Init();

#ifndef AE_HEADLESS
        DefaultWindow = new Window("Default window", AE_WINDOWPOSITION_UNDEFINED,
            AE_WINDOWPOSITION_UNDEFINED, 100, 100, AE_WINDOW_HIDDEN);

        // Need to retrieve the extension names required to create an SDL surface
        uint32_t extensionCount;
        SDL_Vulkan_GetInstanceExtensions(DefaultWindow->sdlWindow, &extensionCount, nullptr);
        std::vector<const char*> requiredExtensions(extensionCount);
        SDL_Vulkan_GetInstanceExtensions(DefaultWindow->sdlWindow, &extensionCount, requiredExtensions.data());
#endif

        // Then create graphics instance
        auto instanceDesc = Graphics::InstanceDesc{
            .instanceName = "AtlasEngineInstance",
#ifdef AE_BUILDTYPE_DEBUG
            .enableValidationLayers = true,
#else
            .enableValidationLayers = false,
#endif
            .validationLayerSeverity = config.validationLayerSeverity,
#ifndef AE_HEADLESS
            .requiredExtensions = requiredExtensions
#else
            .requiredExtensions = { VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME }
#endif
        };

        Graphics::Instance::DefaultInstance = new Graphics::Instance(instanceDesc);
        Graphics::Surface* surface;
        // Initialize window surface
#ifndef AE_HEADLESS
        surface = Graphics::Instance::DefaultInstance->CreateSurface(DefaultWindow->GetSDLWindow());
#else
        surface = Graphics::Instance::DefaultInstance->CreateHeadlessSurface();
#endif
        // Initialize device
        Graphics::Instance::DefaultInstance->InitializeGraphicsDevice(surface);
        Graphics::GraphicsDevice::DefaultDevice = Graphics::Instance::DefaultInstance->GetGraphicsDevice();

#ifndef AE_HEADLESS
        delete Engine::DefaultWindow;
#endif

        Graphics::Extensions::Process();

        // Do the setup for all the classes that need static setup
        JobSystem::Init(config.jobSystemConfig);
        Loader::AssetLoader::Init();
        Common::Random::Init();
        PipelineManager::Init();
        Physics::PhysicsManager::Init();
        Input::KeyboardMap::Init();

        // We need lower sample size for smaller buffer on SDL side (e.g. 256 instead of 1024)
        Audio::AudioManager::Configure(48000, 2, 128);

        Clock::Update();

    }

    void Engine::Shutdown() {

        JobSystem::Shutdown();
        Graphics::ShaderCompiler::Shutdown();
        Graphics::Profiler::Shutdown();
        PipelineManager::Shutdown();
        Physics::PhysicsManager::Shutdown();
        Texture::Texture::Shutdown();
        Audio::AudioManager::Shutdown();
        Input::KeyboardMap::Shutdown();

        Events::EventManager::ShutdownEventDelegate.Fire();

#ifdef AE_NO_APP
        SDL_Quit();
#endif
    }

    void Engine::Update() {

        Clock::Update();
        Graphics::Profiler::BeginFrame();
        Events::EventManager::Update();
        PipelineManager::Update();
        Audio::AudioManager::Update();
        Physics::ShapesManager::Update();

    }

    ivec2 Engine::GetScreenSize() {

        SDL_DisplayMode displayMode;
        SDL_GetCurrentDisplayMode(0, &displayMode);

        return ivec2(displayMode.w, displayMode.h);

    }

}
