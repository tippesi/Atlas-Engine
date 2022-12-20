#include "Engine.h"
#include "graphics/Extensions.h"
#include "Profiler.h"
#include "EngineInstance.h"
#include "loader/ShaderLoader.h"
#include "graphics/Instance.h"

#include "graphics/ShaderCompiler.h"

#include <SDL_vulkan.h>

extern Atlas::EngineInstance* GetEngineInstance();
extern Atlas::Window* GetWindow();

namespace Atlas {

    Window* Engine::defaultWindow = nullptr;

	void Engine::Init(std::string assetDirectory, std::string shaderDirectory) {

#ifdef AE_NO_APP
        SDL_SetMainReady();
#endif
		if (SDL_WasInit(SDL_INIT_EVERYTHING) != SDL_INIT_EVERYTHING) {
			SDL_Init(SDL_INIT_EVERYTHING);
		}

        Loader::AssetLoader::SetAssetDirectory(assetDirectory);
        Loader::ShaderLoader::SetSourceDirectory(shaderDirectory);

        Graphics::ShaderCompiler::Init();

        // First need to get a window to retrieve the title
        defaultWindow = new Window("Default window", AE_WINDOWPOSITION_UNDEFINED,
            AE_WINDOWPOSITION_UNDEFINED, 100, 100,
            SDL_WINDOW_VULKAN, false);
        // Then create graphics instance
        auto graphicsInstance = new Graphics::Instance("AtlasEngineInstance", true);
        Graphics::Instance::defaultInstance = graphicsInstance;
        // Initialize window surface
        defaultWindow->CreateSurface();
        // Initialize device
        graphicsInstance->InitializeGraphicsDevice(defaultWindow->surface);
        Graphics::GraphicsDevice::defaultDevice = graphicsInstance->GetGraphicsDevice();

        Graphics::Extensions::Process();

        // Do the setup for all the classes that need static setup
		Loader::AssetLoader::Init();
		Common::Random::Init();

		Audio::AudioManager::Configure(48000, 2, 1024);

		// Renderer::OpaqueRenderer::InitShaderBatch();
		// Renderer::ShadowRenderer::InitShaderBatch();

		Clock::Update();

        // Only then create engine instance. This makes sure that the engine instance already
        // has access to all graphics functionality and all other functionality on construction
        auto engineInstance = GetEngineInstance();
        EngineInstance::instance = engineInstance;


    }

    void Engine::Shutdown() {

        Shader::ShaderManager::Clear();
        Graphics::ShaderCompiler::Shutdown();

        delete defaultWindow;

#ifdef AE_NO_APP
        SDL_Quit();
#endif
    }

    void Engine::Update() {

        Clock::Update();
		Profiler::Update();
        Events::EventManager::Update();

    }

    ivec2 Engine::GetScreenSize() {

        SDL_DisplayMode displayMode;
        SDL_GetCurrentDisplayMode(0, &displayMode);

        return ivec2(displayMode.w, displayMode.h);

    }

    void Engine::LockFramerate() {

        SDL_GL_SetSwapInterval(1);

    }

    void Engine::UnlockFramerate() {

        SDL_GL_SetSwapInterval(0);

    }

}