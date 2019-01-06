#include "Engine.h"

Window* Engine::Init(string shaderDirectory, string title, int32_t x, int32_t y,
	int32_t width, int32_t height, int32_t flags) {

	SDL_Init(SDL_INIT_EVERYTHING);

#ifdef ENGINE_GL
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
#elif ENGINE_GLES
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
#endif	

	Window* window = new Window(title, x, y, width, height, flags);

#if defined(ENGINE_WINDOWS) || defined(ENGINE_LINUX) || defined(ENGINE_OSX)
#ifdef ENGINE_GL
	if (!gladLoadGL()) {
		throw EngineException("Error initializing OpenGL");
	}
#elif ENGINE_GLES
	if (SDL_GL_LoadLibrary(nullptr) != 0) {
		throw EngineException("Error initializing OpenGL ES");
	}
	gladLoadGLES2Loader(SDL_GL_GetProcAddress);
	SDL_GL_UnloadLibrary();
#endif
#endif

	int value;

#ifdef ENGINE_SHOW_LOG
	EngineLog("OpenGL Version: %s", glGetString(GL_VERSION));
	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &value);
	EngineLog("Native colorbuffer red component precision %d bits", value);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &value);
	EngineLog("Native colorbuffer green component precision %d bits", value);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &value);
	EngineLog("Native colorbuffer blue component precision %d bits", value);
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &value);
	EngineLog("Native depthbuffer precision %d bits", value);
#endif		
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);

	// If the textures aren't working as expected this line should be changed
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifdef ENGINE_GL
	// Standard in OpenGL ES
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif		

	// Do the setup for all the classes that need static setup
	Texture::CheckExtensions();
	Buffer::CheckExtensions();
	Texture::GetMaxAnisotropyLevel();

	LockFramerate();

	ShaderStage::SetSourceDirectory(shaderDirectory);

	EngineEventHandler::Init();

	GeometryRenderer::InitShaderBatch();
	ShadowRenderer::InitShaderBatch();

	return window;

}

void Engine::LockFramerate() {

	SDL_GL_SetSwapInterval(1);

}

void Engine::UnlockFramerate() {

	SDL_GL_SetSwapInterval(0);

}

void Engine::Update() {

	EngineEventHandler::Update();

}