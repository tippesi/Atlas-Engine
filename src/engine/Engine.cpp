#include "Engine.h"
#include "Extensions.h"
#include "Profiler.h"
#include "EngineInstance.h"

#include <volk.h>
#include <SDL_vulkan.h>

extern Atlas::EngineInstance* GetEngineInstance();

namespace Atlas {

	void Engine::Init(std::string assetDirectory, std::string shaderDirectory) {

#ifdef AE_NO_APP
        SDL_SetMainReady();
#endif
		if (SDL_WasInit(SDL_INIT_EVERYTHING) != SDL_INIT_EVERYTHING) {
			SDL_Init(SDL_INIT_EVERYTHING);
		}

        auto engineInstance = GetEngineInstance();
		EngineInstance::instance = engineInstance;

        // Create the surface to render to.
        engineInstance->window->CreateSurface();
        // Initialize the graphics device
        engineInstance->graphicInstance->IntitializeGraphicsDevice(engineInstance->window->surface);

		// Do the setup for all the classes that need static setup
		// Extensions::Process();
		// Texture::Texture::GetMaxAnisotropyLevel();

		Loader::AssetLoader::Init();
		Common::Random::Init();

		Audio::AudioManager::Configure(48000, 2, 1024);

		Loader::AssetLoader::SetAssetDirectory(assetDirectory);
		Shader::ShaderStage::SetSourceDirectory(shaderDirectory);

		// Renderer::OpaqueRenderer::InitShaderBatch();
		// Renderer::ShadowRenderer::InitShaderBatch();

		Clock::Update();

	}

    void Engine::Shutdown() {

        Shader::ShaderManager::Clear();

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