#include "Engine.h"

namespace Atlas {

	SDL_Window* Engine::defaultWindow = nullptr;
	Context* Engine::defaultContext = nullptr;

	Context* Engine::Init(std::string assetDirectory, std::string shaderDirectory) {

		if (SDL_WasInit(SDL_INIT_EVERYTHING) != SDL_INIT_EVERYTHING) {
			SDL_Init(SDL_INIT_EVERYTHING);
		}

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
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    
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
			throw AtlasException("Error initializing OpenGL");
		}
#elif AE_API_GLES
        if (SDL_GL_LoadLibrary(nullptr) != 0) {
            throw AtlasException("Error initializing OpenGL ES");
        }
        gladLoadGLES2Loader(SDL_GL_GetProcAddress);
        SDL_GL_UnloadLibrary();
#endif
#endif

		SDL_GL_DeleteContext(setupContext);
		defaultContext = new Context(defaultWindow);

		int value;

#ifdef AE_SHOW_LOG
		AtlasLog("OpenGL Version: %s", glGetString(GL_VERSION));
		SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &value);
		AtlasLog("Native colorbuffer red component precision %d bits", value);
		SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &value);
		AtlasLog("Native colorbuffer green component precision %d bits", value);
		SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &value);
		AtlasLog("Native colorbuffer blue component precision %d bits", value);
		SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &value);
		AtlasLog("Native depthbuffer precision %d bits", value);
#endif

		// Do the setup for all the classes that need static setup
		Texture::Texture::CheckExtensions();
		Buffer::Buffer::CheckExtensions();
		Texture::Texture::GetMaxAnisotropyLevel();

		Loader::AssetLoader::Init();

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

	}

	void Engine::Update() {

		Clock::Update();
		Events::EventManager::Update();

	}

}