#include "engine.h"

namespace Engine {

	Window* Init(const char* title, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags) {

		SDL_Init(SDL_INIT_EVERYTHING);

#ifdef ENGINE_OGL
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

		SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
#else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

		SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
#endif	

		Window* window = new Window(title, x, y, width, height, flags);

#ifdef ENGINE_OGL
		glewExperimental = GL_TRUE;
		GLenum err = glewInit();

		if (err != GLEW_OK)
			throw new EngineException("Error initializing GLEW");
#endif

		int value;

#ifdef ENGINE_SHOW_LOG
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
		// glEnable(GL_CULL_FACE);

		glEnable(GL_TEXTURE_2D);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifdef ENGINE_OGL
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif

		int32_t maxAnisotropy = Texture::GetMaxAnisotropyLevel();
		Texture::SetAnisotropyLevel(maxAnisotropy);

		LockFramerate();

		return window;

	}

	void LockFramerate() {

		SDL_GL_SetSwapInterval(1);

	}

	void UnlockFramerate() {

		SDL_GL_SetSwapInterval(0);

	}

}