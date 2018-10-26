#include "window.h"

Window::Window(const char* title, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags) {

	sdlWindow = SDL_CreateWindow(title, x, y, width, height, flags | SDL_WINDOW_OPENGL);

	if (sdlWindow == NULL) {
		throw new EngineException("Error initializing window");
	}

	SDL_GLContext sdlContext = SDL_GL_CreateContext(sdlWindow);

	this->viewport = new Viewport(0, 0, width, height);	

}

void Window::Update() {

	SDL_GL_SwapWindow(sdlWindow);

}

Window::~Window() {

	SDL_DestroyWindow(sdlWindow);

	delete this->viewport;

}