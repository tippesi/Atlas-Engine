#include "Window.h"
#include "graphics/Instance.h"

#include <SDL3/SDL_vulkan.h>

namespace Atlas {

    Window::Window(const std::string& title, int32_t x, int32_t y, int32_t width, int32_t height,
            int32_t flags) : title(title), width(width), height(height) {

#ifndef AE_HEADLESS
        auto properties = SDL_CreateProperties();
        SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, x);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, flags | SDL_WINDOW_VULKAN);

        sdlWindow = SDL_CreateWindowWithProperties(properties);
        SDL_DestroyProperties(properties);
        if (!sdlWindow) {
            auto error = SDL_GetError();
            Log::Error("Error initializing window: " + std::string(error));
            return;
        }

        ID = SDL_GetWindowID(sdlWindow);
#endif

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

    uint64_t Window::GetFlags() const {

        return SDL_GetWindowFlags(sdlWindow);

    }

    void Window::SetTitle(std::string title) {

        SDL_SetWindowTitle(sdlWindow, title.c_str());

    }

    void Window::SetIcon(Texture::Texture2D *icon) {

#ifndef AE_OS_APPLE_MOBILE
        auto data = icon->GetData<uint8_t>();

        auto format = SDL_GetPixelFormatForMasks(icon->channels * 8,
            0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
        SDL_Surface *surface = SDL_CreateSurfaceFrom(icon->width, icon->height, format,
            data.data(), icon->channels * icon->width);

        SDL_SetWindowIcon(sdlWindow, surface);

        SDL_DestroySurface(surface);
#endif

    }

    void Window::SetPosition(int32_t x, int32_t y) {

        SDL_SetWindowPosition(sdlWindow, x, y);

    }

    ivec2 Window::GetPosition() {

        ivec2 position(0);
        SDL_GetWindowPosition(sdlWindow, &position.x, &position.y);

        return position;

    }

    void Window::SetSize(int32_t width, int32_t height) {

        if (sdlWindow == nullptr) {
            this->width = width;
            this->height = height;
            return;
        }

        SDL_SetWindowSize(sdlWindow, width, height);

    }

    int32_t Window::GetWidth() {

        if (sdlWindow == nullptr)
            return width;

        int32_t windowWidth = 0;
        SDL_GetWindowSize(sdlWindow, &windowWidth, nullptr);

        return windowWidth;

    }

    int32_t Window::GetHeight() {

        if (sdlWindow == nullptr)
            return height;

        int32_t windowHeight = 0;
        SDL_GetWindowSize(sdlWindow, nullptr, &windowHeight);

        return windowHeight;

    }

    ivec2 Window::GetDrawableSize() {

        ivec2 size;

        SDL_GetWindowSizeInPixels(sdlWindow, &size.x, &size.y);

        return size;

    }

    bool Window::IsFocused() const {

        auto flags = GetFlags();
        return flags & SDL_WINDOW_INPUT_FOCUS && flags & SDL_WINDOW_MOUSE_FOCUS
               && !(flags & SDL_WINDOW_MINIMIZED);

    }

    bool Window::IsOccluded() const {

        return GetFlags() & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_OCCLUDED);

    }

    void Window::Show() {

        SDL_ShowWindow(sdlWindow);

    }

    void Window::Hide() {

        SDL_HideWindow(sdlWindow);

    }

    void Window::Maximize() {

        SDL_MaximizeWindow(sdlWindow);

    }

    void Window::Minimize() {

        SDL_MinimizeWindow(sdlWindow);

    }

    void Window::SetFullscreen(bool fullscreen) {

        SDL_SetWindowFullscreen(sdlWindow, fullscreen);

    }

    void Window::SetBordered(bool border) {

        SDL_SetWindowBordered(sdlWindow, border);

    }

    void Window::SetDisplayMode(const DisplayMode& mode) {

        int32_t displayModeCount = 0;
        auto displayModes = SDL_GetFullscreenDisplayModes(mode.displayID, &displayModeCount);
        if (!displayModes) {
            auto error = SDL_GetError();
            Log::Warning("Getting display modes failed: " + std::string(error));
            return;
        }

        // Look for the exact mode, there might be several with the same size and refresh rate
        const SDL_DisplayMode* displayMode = nullptr;
        for (int32_t i = 0; i < displayModeCount; i++) {
            auto candidate = displayModes[i];
            if (candidate->w == mode.width && candidate->h == mode.height && candidate->format == mode.rawFormat &&
                candidate->refresh_rate == mode.refreshRate && candidate->pixel_density == mode.pixelDensity) {
                displayMode = candidate;
                break;
            }
        }

        if (displayMode)
            SDL_SetWindowFullscreenMode(sdlWindow, displayMode);
        else
            Log::Warning("Display mode isn't available on the display");

        SDL_free(displayModes);

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
