#ifndef WINDOW_H
#define WINDOW_H

#include "System.h"
#include "Viewport.h"
#include "Display.h"
#include "graphics/Surface.h"
#include "texture/Texture2D.h"

#include "events/EventDelegate.h"
#include "events/EventManager.h"

#include <SDL.h>

#define AE_WINDOWPOSITION_UNDEFINED SDL_WINDOWPOS_UNDEFINED

#define AE_WINDOW_FULLSCREEN    SDL_WINDOW_FULLSCREEN
#define AE_WINDOW_SHOWN            SDL_WINDOW_SHOWN
#define AE_WINDOW_HIDDEN        SDL_WINDOW_HIDDEN
#define AE_WINDOW_BORDERLESS    SDL_WINDOW_BORDERLESS
#define AE_WINDOW_RESIZABLE        SDL_WINDOW_RESIZABLE
#define AE_WINDOW_MINIMIZED        SDL_WINDOW_MINIMIZED
#define AE_WINDOW_MAXIMIZED        SDL_WINDOW_MAXIMIZED
#define AE_WINDOW_HIGH_DPI        SDL_WINDOW_ALLOW_HIGHDPI

namespace Atlas {

    /**
      * A simple window class.
     */
    class Window {

        friend class Engine;

    public:
        /**
         * Creates a window object
         * @param title The title of the window
         * @param x The x position of the window on the screen in pixels
         * @param y The y position of the window on the screen in pixels
         * @param width The width of the window in pixels
         * @param height The height of the window in pixels
         * @param flags Window flags. See {@link Window.h} for more.
         * @param createSurface Whether or not to immediately create a surface
         */
        Window(const std::string& title, int32_t x, int32_t y, int32_t width, int32_t height,
               int32_t flags = AE_WINDOW_FULLSCREEN, bool createSurface = true);

        ~Window();

        /**
         * Returns the ID of the window.
         * @return
         * @note The window ID is important to filter the SystemEvents which
         * are just generated for a specific window
         */
        uint32_t GetID();

        /**
         * Returns the window flags as a bitfield
         * @return An integer with the corresponding bits set
         */
        uint32_t GetFlags() const;

        /**
         * Sets the title of the window.
         * @param title The title string
         */
        void SetTitle(std::string title);

        /**
         * Sets the icon of the window.
         * @param icon A pointer to a Texture object
         */
        void SetIcon(Texture::Texture2D *icon);

        /**
         * Resets the position of the window to the given screen coordinates.
         * @param x The x position of the window on the screen in pixels
         * @param y The y position of the window on the screen in pixels
         */
        void SetPosition(int32_t x, int32_t y);

        /**
         * Returns the position of the window.
         * @return An 2-component integer vector.
         */
        ivec2 GetPosition();

        /**
         * Resets the size of the window to the given values.
         * @param width The width of the window in pixels
         * @param height The height of the window in pixels
         */
        void SetSize(int32_t width, int32_t height);

        /**
         * Returns the width of the window.
         * @return The width of the window.
         */
        int32_t GetWidth();

        /**
         * Returns the height of the window.
         * @return The height of the window.
         */
        int32_t GetHeight();

        /**
         * Returns the drawable size of the window.
         * @return The drawable size of the window.
         */
        ivec2 GetDrawableSize();

        /**
         * Shows the window if the window was hidden
         */
        void Show();

        /**
         * Hides the window if the window was shown.
         */
        void Hide();

        /**
         * Maximizes the window.
         */
        void Maximize();

        /**
         * Minimizes the window.
         */
        void Minimize();

        /**
         * Changes the window mode depending on the parameter.
         * @param fullscreen True to make the window fullscreen, false
         * to change into windowed mode.
         */
        void SetFullscreen(bool fullscreen);

        /**
         * Changes the window border depending on the parameter.
         * @param border True to adds a border, false to remove it.
         */
        void SetBordered(bool border);

        /**
         * Sets the windows display mode
         * @param mode The display mode to be set
         */
        void SetDisplayMode(const DisplayMode& mode);

        /**
         * Creates a surface for the window to render to.
         */
        bool CreateSurface();

        /**
         * Returns the pointer to the SDL window structure
         */
        SDL_Window* GetSDLWindow();

        const std::string title;

        Graphics::Surface* surface;

        Events::EventDelegate<Events::WindowEvent> windowEventDelegate;
        Events::EventDelegate<Events::KeyboardEvent> keyboardEventDelegate;
        Events::EventDelegate<Events::MouseButtonEvent> mouseButtonEventDelegate;
        Events::EventDelegate<Events::MouseMotionEvent> mouseMotionEventDelegate;
        Events::EventDelegate<Events::MouseWheelEvent> mouseWheelEventDelegate;
        Events::EventDelegate<Events::ControllerAxisEvent> controllerAxisEventDelegate;
        Events::EventDelegate<Events::ControllerButtonEvent> controllerButtonEventDelegate;
        Events::EventDelegate<Events::DropEvent> dropEventDelegate;

    private:
        void WindowEventHandler(Events::WindowEvent event);
        void KeyboardEventHandler(Events::KeyboardEvent event);
        void MouseButtonEventHandler(Events::MouseButtonEvent event);
        void MouseMotionEventHandler(Events::MouseMotionEvent event);
        void MouseWheelEventHandler(Events::MouseWheelEvent event);
        void ControllerAxisEventHandler(Events::ControllerAxisEvent event);
        void ControllerButtonEventHandler(Events::ControllerButtonEvent event);
        void DropEventHandler(Events::DropEvent event);

        uint32_t ID;

        SDL_Window* sdlWindow = nullptr;

        int32_t x;
        int32_t y;

        int32_t width;
        int32_t height;

        bool hasFocus = false;

        int32_t windowEventSubcriberID = -1;
        int32_t keyboardEventSubscriberID = -1;
        int32_t mouseButtonEventSubscriberID = -1;
        int32_t mouseMotionEventSubscriberID = -1;
        int32_t mouseWheelEventSubscriberID = -1;
        int32_t controllerAxisEventSubscriberID = -1;
        int32_t controllerButtonEventSubscriberID = -1;
        int32_t dropEventSubscriberID = -1;

    };

}

#endif