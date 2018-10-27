#include "window.h"

Window::Window(const char* title, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags) {

	sdlWindow = SDL_CreateWindow(title, x, y, width, height, flags | SDL_WINDOW_OPENGL);

	if (sdlWindow == NULL) {
		throw new EngineException("Error initializing window");
	}

	context = SDL_GL_CreateContext(sdlWindow);

	this->viewport = new Viewport(0, 0, width, height);	

}

void Window::SetIcon(Texture* icon) {

	uint8_t* data = icon->GetData();

	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(data, icon->width, icon->height, icon->channels * 8, icon->channels * icon->width,
		0x000000ff, 0x0000ff00, 0x00ff0000, 0);

	SDL_SetWindowIcon(sdlWindow, surface);

	SDL_FreeSurface(surface);
	delete data;

}

void Window::Update() {

	SDL_GL_SwapWindow(sdlWindow);

}

Window::~Window() {

	SDL_DestroyWindow(sdlWindow);

	SDL_GL_DeleteContext(context);

	delete this->viewport;

}