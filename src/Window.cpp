#include "Window.h"

Window::Window(string title, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags) : 
	x(x), y(y), width(width), height(height) {

	sdlWindow = SDL_CreateWindow(title.c_str(), x, y, width, height, flags | SDL_WINDOW_OPENGL);

	if (sdlWindow == NULL) {
		throw EngineException("Error initializing window");
	}

	context = SDL_GL_CreateContext(sdlWindow);

	ID = SDL_GetWindowID(sdlWindow);

	this->viewport = new Viewport(0, 0, width, height);

	auto windowEventHandler = std::bind(&Window::WindowEventHandler, this, std::placeholders::_1);
	EngineEventHandler::WindowEventDelegate.Subscribe(windowEventHandler);

	auto keyboardEventHandler = std::bind(&Window::KeyboardEventHandler, this, std::placeholders::_1);
	EngineEventHandler::KeyboardEventDelegate.Subscribe(keyboardEventHandler);

	auto mouseButtonEventHandler = std::bind(&Window::MouseButtonEventHandler, this, std::placeholders::_1);
	EngineEventHandler::MouseButtonEventDelegate.Subscribe(mouseButtonEventHandler);

	auto mouseMotionEventHandler = std::bind(&Window::MouseMotionEventHandler, this, std::placeholders::_1);
	EngineEventHandler::MouseMotionEventDelegate.Subscribe(mouseMotionEventHandler);

	auto mouseWheelEventHandler = std::bind(&Window::MouseWheelEventHandler, this, std::placeholders::_1);
	EngineEventHandler::MouseWheelEventDelegate.Subscribe(mouseWheelEventHandler);

}

uint32_t Window::GetID() {

	return ID;

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

void Window::WindowEventHandler(EngineWindowEvent event) {

	if (event.windowID != ID)
		return;

	if (event.type == WINDOWEVENT_FOCUS_GAINED) {
		hasFocus = true;
	}

	if (event.type == WINDOWEVENT_FOCUS_LOST) {
		hasFocus = false;
	}

	windowEventDelegate.Fire(event);

}

void Window::KeyboardEventHandler(EngineKeyboardEvent event) {

	if (event.windowID != ID)
		return;

	keyboardEventDelegate.Fire(event);

}

void Window::MouseButtonEventHandler(EngineMouseButtonEvent event) {

	if (event.windowID != ID)
		return;

	mouseButtonEventDelegate.Fire(event);

}

void Window::MouseMotionEventHandler(EngineMouseMotionEvent event) {

	if (event.windowID != ID)
		return;

	mouseMotionEventDelegate.Fire(event);

}

void Window::MouseWheelEventHandler(EngineMouseWheelEvent event) {

	if (event.windowID != ID)
		return;

	mouseWheelEventDelegate.Fire(event);

}

void Window::ControllerAxisEventHandler(EngineControllerAxisEvent event) {

	if (!hasFocus)
		return;

	controllerAxisEventDelegate.Fire(event);

}

void Window::ControllerButtonEventHandler(EngineControllerButtonEvent event) {

	if (!hasFocus)
		return;

	controllerButtonEventDelegate.Fire(event);

}

Window::~Window() {

	SDL_DestroyWindow(sdlWindow);

	SDL_GL_DeleteContext(context);

	delete this->viewport;

}