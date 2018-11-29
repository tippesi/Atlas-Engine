#include "Window.h"

Window::Window(string title, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags) : 
	x(x), y(y), width(width), height(height) {

	sdlWindow = SDL_CreateWindow(title.c_str(), x, y, width, height, flags | SDL_WINDOW_OPENGL);

	if (sdlWindow == NULL) {
		throw EngineException("Error initializing window");
	}

	context = SDL_GL_CreateContext(sdlWindow);

	this->viewport = new Viewport(0, 0, width, height);	

}

void Window::SetTitle(string title) {

	SDL_SetWindowTitle(sdlWindow, title.c_str());

}

void Window::SetIcon(Texture* icon) {

	auto data = icon->GetData();

	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(data.data(), icon->width, icon->height, icon->channels * 8, icon->channels * icon->width,
		0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);

	SDL_SetWindowIcon(sdlWindow, surface);

	SDL_FreeSurface(surface);

}

void Window::SetPosition(int32_t x, int32_t y) {

	SDL_SetWindowPosition(sdlWindow, x, y);

	this->x = x;
	this->y = y;

}

void Window::GetPosition(int32_t* x, int32_t* y) {

	*x = this->x;
	*y = this->y;

}

void Window::SetSize(int32_t width, int32_t height) {

	SDL_SetWindowSize(sdlWindow, width, height);

	this->width = width;
	this->height = height;

}

void Window::GetSize(int32_t* width, int32_t* height) {

	*width = this->width;
	*height = this->height;

}

void Window::Show() {

	SDL_ShowWindow(sdlWindow);

}

void Window::Hide() {

	SDL_HideWindow(sdlWindow);

}

void Window::Update() {

	SDL_GL_SwapWindow(sdlWindow);

}

Window::~Window() {

	SDL_DestroyWindow(sdlWindow);

	SDL_GL_DeleteContext(context);

	delete this->viewport;

}