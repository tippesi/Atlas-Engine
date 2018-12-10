#ifndef WINDOW_H
#define WINDOW_H

#include "System.h"
#include "Viewport.h"
#include "Texture.h"

#include "events/EventDelegate.h"
#include "events/EngineEventHandler.h"

#include "libraries/SDL/include/SDL.h"

#define WINDOWPOSITION_UNDEFINED SDL_WINDOWPOS_UNDEFINED

#define WINDOW_FULLSCREEN	SDL_WINDOW_FULLSCREEN
#define WINDOW_SHOWN		SDL_WINDOW_SHOWN
#define WINDOW_HIDDEN		SDL_WINDOW_HIDDEN
#define WINDOW_BORDERLESS	SDL_WINDOW_BORDERLESS
#define WINDOW_RESIZABLE	SDL_WINDOW_RESIZABLE
#define WINDOW_MINIMIZED	SDL_WINDOW_MINIMIZED
#define WINDOW_MAXIMIZED	SDL_WINDOW_MAXIMIZED

/**
 * A simple window class.
 */
class Window {

public:
	/**
	 * Creates a window object
	 * @param title The title of the window
	 * @param x The x position of the window on the screen in pixels
	 * @param y The y position of the window on the screen in pixels
	 * @param width The width of the window in pixels
	 * @param height The height of the window in pixels
	 * @param flags Window flags. See {@link Window.h} for more.
	 */
	Window(string title, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags = WINDOW_FULLSCREEN);

	/**
	 * Returns the ID of the window.
	 * @return
	 * @note The window ID is important to filter the SystemEvents which
	 * are just generated for a specific window
	 */
	uint32_t GetID();

	/**
	 * Sets the title of the window.
	 * @param title The title string
	 */
	void SetTitle(string title);

	/**
	 * Sets the icon of the window.
	 * @param icon A pointer to a Texture object
	 */
	void SetIcon(Texture* icon);

	/**
	 * Resets the position of the window to the given screen coordinates.
	 * @param x The x position of the window on the screen in pixels
	 * @param y The y position of the window on the screen in pixels
	 */
	void SetPosition(int32_t x, int32_t y);

	/**
	 * Returns the position of the window.
	 * @param x A pointer to an integer where the x value will be written into
	 * @param y A pointer to an integer where the y value will be written into
	 */
	void GetPosition(int32_t* x, int32_t* y);

	/**
	 * Resets the size of the window to the given values.
	 * @param width The width of the window in pixels
	 * @param height The height of the window in pixels
	 */
	void SetSize(int32_t width, int32_t height);

	/**
	 * Returns the size of the window.
	 * @param width A pointer to an integer where the width value will be written into
	 * @param height A pointer to an integer where the height value will be written into
	 */
	void GetSize(int32_t* width, int32_t* height);

	/**
	 * Shows the window if the window was hidden
	 */
	void Show();

	/**
	 * Hides the window if the window was shown.
	 */
	void Hide();

	/**
	 * Updates the window events and the rendering surface.
	 */
	void Update();

	/**
	 * Clears the rendering surface of the window.
	 */
	void Clear(vec3 color = vec3(0.0f));

	~Window();

	Viewport* viewport;

	EventDelegate<EngineWindowEvent> windowEventDelegate;
	EventDelegate<EngineKeyboardEvent> keyboardEventDelegate;
	EventDelegate<EngineMouseButtonEvent> mouseButtonEventDelegate;
	EventDelegate<EngineMouseMotionEvent> mouseMotionEventDelegate;
	EventDelegate<EngineMouseWheelEvent> mouseWheelEventDelegate;
	EventDelegate<EngineControllerAxisEvent> controllerAxisEventDelegate;
	EventDelegate<EngineControllerButtonEvent> controllerButtonEventDelegate;

private:
	void WindowEventHandler(EngineWindowEvent event);
	void KeyboardEventHandler(EngineKeyboardEvent event);
	void MouseButtonEventHandler(EngineMouseButtonEvent event);
	void MouseMotionEventHandler(EngineMouseMotionEvent event);
	void MouseWheelEventHandler(EngineMouseWheelEvent event);
	void ControllerAxisEventHandler(EngineControllerAxisEvent event);
	void ControllerButtonEventHandler(EngineControllerButtonEvent event);

	uint32_t ID;

	SDL_Window * sdlWindow;
	SDL_GLContext context;

	int32_t x;
	int32_t y;

	int32_t width;
	int32_t height;

	bool hasFocus;

};

#endif