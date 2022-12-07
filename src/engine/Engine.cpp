#include "Engine.h"
#include "Extensions.h"
#include "Profiler.h"

#include <volk.h>
#include <SDL_vulkan.h>

namespace Atlas {

    Graphics::Instance* Engine::instance = nullptr;

	Graphics::Instance* Engine::Init(std::string assetDirectory, std::string shaderDirectory) {

#ifdef AE_NO_APP
        SDL_SetMainReady();
#endif
		if (SDL_WasInit(SDL_INIT_EVERYTHING) != SDL_INIT_EVERYTHING) {
			SDL_Init(SDL_INIT_EVERYTHING);
		}

#if defined(AE_OS_WINDOWS) || defined(AE_OS_LINUX) || defined(AE_OS_MACOS)
		bool success = false;
        instance = new Graphics::Instance("My application name", success, true);
        if (!success) {
			Log::Error("Error initializing Vulkan");
			return nullptr;
		}
#endif

		int value;

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

		return instance;

	}

    void Engine::Shutdown() {

        Shader::ShaderManager::Clear();

        delete instance;

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