#include "Window.h"

namespace Atlas {

	Window::Window(std::string title, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags) :
			x(x), y(y), width(width), height(height) {

		sdlWindow = SDL_CreateWindow(title.c_str(), x, y, width, height, flags | SDL_WINDOW_OPENGL);

		if (sdlWindow == NULL) {
			throw AtlasException("Error initializing window");
		}

		context = SDL_GL_CreateContext(sdlWindow);

		ID = SDL_GetWindowID(sdlWindow);

		this->viewport = new Viewport(0, 0, width, height);

		auto windowEventHandler = std::bind(&Window::WindowEventHandler, this, std::placeholders::_1);
		Events::EventManager::WindowEventDelegate.Subscribe(windowEventHandler);

		auto keyboardEventHandler = std::bind(&Window::KeyboardEventHandler, this, std::placeholders::_1);
		Events::EventManager::KeyboardEventDelegate.Subscribe(keyboardEventHandler);

		auto mouseButtonEventHandler = std::bind(&Window::MouseButtonEventHandler, this, std::placeholders::_1);
		Events::EventManager::MouseButtonEventDelegate.Subscribe(mouseButtonEventHandler);

		auto mouseMotionEventHandler = std::bind(&Window::MouseMotionEventHandler, this, std::placeholders::_1);
		Events::EventManager::MouseMotionEventDelegate.Subscribe(mouseMotionEventHandler);

		auto mouseWheelEventHandler = std::bind(&Window::MouseWheelEventHandler, this, std::placeholders::_1);
		Events::EventManager::MouseWheelEventDelegate.Subscribe(mouseWheelEventHandler);

		auto controllerAxisEventHandler = std::bind(&Window::ControllerAxisEventHandler, this, std::placeholders::_1);
		Events::EventManager::ControllerAxisEventDelegate.Subscribe(controllerAxisEventHandler);

		auto controllerButtonEventHandler = std::bind(&Window::ControllerButtonEventHandler, this, std::placeholders::_1);
		Events::EventManager::ControllerButtonEventDelegate.Subscribe(controllerButtonEventHandler);

		auto dropEventHandler = std::bind(&Window::DropEventHandler, this, std::placeholders::_1);
		Events::EventManager::DropEventDelegate.Subscribe(dropEventHandler);

	}

	uint32_t Window::GetID() {

		return ID;

	}

	void Window::SetTitle(std::string title) {

		SDL_SetWindowTitle(sdlWindow, title.c_str());

	}

	void Window::SetIcon(Texture::Texture2D *icon) {

		auto data = icon->GetData();

		SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(data.data(), icon->width, icon->height, icon->channels * 8,
														icon->channels * icon->width,
														0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);

		SDL_SetWindowIcon(sdlWindow, surface);

		SDL_FreeSurface(surface);

	}

	void Window::SetPosition(int32_t x, int32_t y) {

		SDL_SetWindowPosition(sdlWindow, x, y);

		this->x = x;
		this->y = y;

	}

	void Window::GetPosition(int32_t *x, int32_t *y) {

		*x = this->x;
		*y = this->y;

	}

	void Window::SetSize(int32_t width, int32_t height) {

		SDL_SetWindowSize(sdlWindow, width, height);

		this->width = width;
		this->height = height;

	}

	void Window::GetSize(int32_t *width, int32_t *height) {

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

	void Window::Clear(vec3 color) {

		glClearColor(color.r, color.g, color.b, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	}

	void Window::WindowEventHandler(Events::WindowEvent event) {

		if (event.windowID != ID)
			return;

		if (event.type == AE_WINDOWEVENT_FOCUS_GAINED) {
			hasFocus = true;
		}

		if (event.type == AE_WINDOWEVENT_FOCUS_LOST) {
			hasFocus = false;
		}

		windowEventDelegate.Fire(event);

	}

	void Window::KeyboardEventHandler(Events::KeyboardEvent event) {

		if (event.windowID != ID)
			return;

		keyboardEventDelegate.Fire(event);

	}

	void Window::MouseButtonEventHandler(Events::MouseButtonEvent event) {

		if (event.windowID != ID)
			return;

		mouseButtonEventDelegate.Fire(event);

	}

	void Window::MouseMotionEventHandler(Events::MouseMotionEvent event) {

		if (event.windowID != ID)
			return;

		mouseMotionEventDelegate.Fire(event);

	}

	void Window::MouseWheelEventHandler(Events::MouseWheelEvent event) {

		if (event.windowID != ID)
			return;

		mouseWheelEventDelegate.Fire(event);

	}

	void Window::ControllerAxisEventHandler(Events::ControllerAxisEvent event) {

		if (!hasFocus)
			return;

		controllerAxisEventDelegate.Fire(event);

	}

	void Window::ControllerButtonEventHandler(Events::ControllerButtonEvent event) {

		if (!hasFocus)
			return;

		controllerButtonEventDelegate.Fire(event);

	}

	void Window::DropEventHandler(Atlas::Events::DropEvent event) {

		if (event.windowID != ID)
			return;

		dropEventDelegate.Fire(event);

	}

	Window::~Window() {

		SDL_DestroyWindow(sdlWindow);

		SDL_GL_DeleteContext(context);

		delete this->viewport;

	}

}