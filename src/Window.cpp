#include "Window.h"
#include "Engine.h"

namespace Atlas {

	Window::Window(std::string title, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags) :
			x(x == AE_WINDOWPOSITION_UNDEFINED ? 0 : x), y(y == AE_WINDOWPOSITION_UNDEFINED ? 0 : y),
			width(width), height(height) {

	    // The engine creates a default SDL window.
	    // We destroy it here in case we need room on some
	    // platforms which only support one window at a time.
	    if (Engine::defaultWindow) {
            Engine::defaultContext->Unbind();
            SDL_DestroyWindow(Engine::defaultWindow);
        }

		sdlWindow = SDL_CreateWindow(title.c_str(), x, y, width, height, flags | SDL_WINDOW_OPENGL);

		if (sdlWindow == nullptr) {
			throw AtlasException("Error initializing window");
		}

        if (Engine::defaultWindow) {
            Engine::defaultContext->AttachTo(this);
            Engine::defaultWindow = nullptr;
        }

		ID = SDL_GetWindowID(sdlWindow);

		auto windowEventHandler = std::bind(&Window::WindowEventHandler, this, std::placeholders::_1);
		windowEventSubcriberID = Events::EventManager::WindowEventDelegate.Subscribe(windowEventHandler);

		auto keyboardEventHandler = std::bind(&Window::KeyboardEventHandler, this, std::placeholders::_1);
		keyboardEventSubscriberID = Events::EventManager::KeyboardEventDelegate.Subscribe(keyboardEventHandler);

		auto mouseButtonEventHandler = std::bind(&Window::MouseButtonEventHandler, this, std::placeholders::_1);
		mouseButtonEventSubscriberID = Events::EventManager::MouseButtonEventDelegate.Subscribe(mouseButtonEventHandler);

		auto mouseMotionEventHandler = std::bind(&Window::MouseMotionEventHandler, this, std::placeholders::_1);
		mouseMotionEventSubscriberID = Events::EventManager::MouseMotionEventDelegate.Subscribe(mouseMotionEventHandler);

		auto mouseWheelEventHandler = std::bind(&Window::MouseWheelEventHandler, this, std::placeholders::_1);
		mouseWheelEventSubscriberID = Events::EventManager::MouseWheelEventDelegate.Subscribe(mouseWheelEventHandler);

		auto controllerAxisEventHandler = std::bind(&Window::ControllerAxisEventHandler, this, std::placeholders::_1);
		controllerAxisEventSubscriberID = Events::EventManager::ControllerAxisEventDelegate.Subscribe(controllerAxisEventHandler);

		auto controllerButtonEventHandler = std::bind(&Window::ControllerButtonEventHandler, this, std::placeholders::_1);
		controllerButtonEventSubscriberID = Events::EventManager::ControllerButtonEventDelegate.Subscribe(controllerButtonEventHandler);

		auto dropEventHandler = std::bind(&Window::DropEventHandler, this, std::placeholders::_1);
		dropEventSubscriberID = Events::EventManager::DropEventDelegate.Subscribe(dropEventHandler);

	}

	Window::~Window() {

		SDL_DestroyWindow(sdlWindow);

		Events::EventManager::WindowEventDelegate.Unsubscribe(windowEventSubcriberID);
		Events::EventManager::KeyboardEventDelegate.Unsubscribe(keyboardEventSubscriberID);
		Events::EventManager::MouseButtonEventDelegate.Unsubscribe(mouseButtonEventSubscriberID);
		Events::EventManager::MouseMotionEventDelegate.Unsubscribe(mouseMotionEventSubscriberID);
		Events::EventManager::MouseWheelEventDelegate.Unsubscribe(mouseWheelEventSubscriberID);
		Events::EventManager::ControllerAxisEventDelegate.Unsubscribe(controllerAxisEventSubscriberID);
		Events::EventManager::ControllerButtonEventDelegate.Unsubscribe(controllerButtonEventSubscriberID);
		Events::EventManager::DropEventDelegate.Unsubscribe(dropEventSubscriberID);

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

	ivec2 Window::GetPosition() {

		return ivec2(x, y);

	}

	void Window::SetSize(int32_t width, int32_t height) {

		SDL_SetWindowSize(sdlWindow, width, height);

		this->width = width;
		this->height = height;

	}

	int32_t Window::GetWidth() {

		return width;

	}

	int32_t Window::GetHeight() {

		return height;

	}

	void Window::Show() {

		SDL_ShowWindow(sdlWindow);

	}

	void Window::Hide() {

		SDL_HideWindow(sdlWindow);

	}

	void Window::Maximize() {

		SDL_MaximizeWindow(sdlWindow);
		SDL_GL_GetDrawableSize(sdlWindow, &width, &height);

	}

	void Window::Minimize() {

		SDL_MinimizeWindow(sdlWindow);
		SDL_GL_GetDrawableSize(sdlWindow, &width, &height);

	}

	void Window::SetFullscreen(bool fullscreen) {

		SDL_SetWindowFullscreen(sdlWindow, (fullscreen ? AE_WINDOW_FULLSCREEN : 0));

	}

	void Window::SetBordered(bool border) {

	    SDL_SetWindowBordered(sdlWindow, border ? SDL_TRUE : SDL_FALSE);

	}

	void Window::Update() {

		SDL_GL_SwapWindow(sdlWindow);

	}

	void Window::Clear(vec3 color) {

		glClearColor(color.r, color.g, color.b, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	}

	SDL_Window* Window::GetSDLWindow() {

		return sdlWindow;

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

		if (event.type == AE_WINDOWEVENT_RESIZED) {
			width = event.data.x;
			height = event.data.y;
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

}