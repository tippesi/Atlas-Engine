#include "Engine.h"
#include "Extensions.h"

namespace Atlas {

	SDL_Window* Engine::defaultWindow = nullptr;
	Context* Engine::defaultContext = nullptr;

	Context* Engine::Init(std::string assetDirectory, std::string shaderDirectory) {

#ifdef AE_NO_APP
        SDL_SetMainReady();
#endif
		if (SDL_WasInit(SDL_INIT_EVERYTHING) != SDL_INIT_EVERYTHING) {
			SDL_Init(SDL_INIT_EVERYTHING);
		}

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

		defaultWindow = SDL_CreateWindow("", 0, 0, 1, 1, AE_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);

#ifdef AE_API_GL
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
#elif AE_API_GLES
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    
        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
#endif
#ifdef AE_SHOW_API_DEBUG_LOG
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

		auto setupContext = SDL_GL_CreateContext(defaultWindow);

#if defined(AE_OS_WINDOWS) || defined(AE_OS_LINUX) || defined(AE_OS_MACOS)
#ifdef AE_API_GL
		if (!gladLoadGL()) {
			Log::Error("Error initializing OpenGL");
			return nullptr;
		}
#elif AE_API_GLES
        if (SDL_GL_LoadLibrary(nullptr) != 0) {
			Log::Error("Error initializing OpenGL ES");
			return nullptr;
        }
        gladLoadGLES2Loader(SDL_GL_GetProcAddress);
        SDL_GL_UnloadLibrary();
#endif
#endif

		SDL_GL_DeleteContext(setupContext);
		defaultContext = new Context(defaultWindow);

		int value;

#ifdef AE_SHOW_LOG
		Log::Message("OpenGL Version: " + std::string((const char*)glGetString(GL_VERSION)));
		SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &value);
		Log::Message("Native colorbuffer red component precision " + std::to_string(value) + " bits");
		SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &value);
		Log::Message("Native colorbuffer green component precision " + std::to_string(value) + " bits");
		SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &value);
		Log::Message("Native colorbuffer blue component precision " + std::to_string(value) + " bits");
		SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &value);
		Log::Message("Native depthbuffer precision " + std::to_string(value) + " bits");
#endif

		// Do the setup for all the classes that need static setup
		Extensions::Process();
		Texture::Texture::GetMaxAnisotropyLevel();

		Loader::AssetLoader::Init();
		Common::Random::Init();

		Audio::AudioManager::Configure(48000, 2, 1024);

		Loader::AssetLoader::SetAssetDirectory(assetDirectory);
		Shader::ShaderStage::SetSourceDirectory(shaderDirectory);

		Renderer::OpaqueRenderer::InitShaderBatch();
		Renderer::ShadowRenderer::InitShaderBatch();

		Clock::Update();

		return defaultContext;

	}

    void Engine::Shutdown() {

        Shader::ShaderManager::Clear();

		if (defaultWindow)
			SDL_DestroyWindow(defaultWindow);

#ifdef AE_NO_APP
        SDL_Quit();
#endif
    }

    void Engine::Update() {

        Clock::Update();
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