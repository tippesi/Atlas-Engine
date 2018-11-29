#ifndef WINDOW_H
#define WINDOW_H

#include "System.h"
#include "Viewport.h"
#include "Texture.h"

#include "libraries/SDL/include/SDL.h"

#define WINDOWPOSITION_UNDEFINED SDL_WINDOWPOS_UNDEFINED

#define WINDOW_FULLSCREEN	SDL_WINDOW_FULLSCREEN
#define WINDOW_SHOWN		SDL_WINDOW_SHOWN
#define WINDOW_HIDDEN		SDL_WINDOW_HIDDEN
#define WINDOW_BORDERLESS	SDL_WINDOW_BORDERLESS
#define WINDOW_RESIZABLE	SDL_WINDOW_RESIZABLE
#define WINDOW_MINIMIZED	SDL_WINDOW_MINIMIZED
#define WINDOW_MAXIMIZED	SDL_WINDOW_MAXIMIZED

class Window {

public:
	///
	/// \param title
	/// \param x
	/// \param y
	/// \param width
	/// \param height
	/// \param flags
	Window(string title, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags = WINDOW_FULLSCREEN);

	///
	/// \param title
	void SetTitle(string title);

	///
	/// \param icon
	void SetIcon(Texture* icon);

	///
	/// \param x
	/// \param y
	void SetPosition(int32_t x, int32_t y);

	///
	void GetPosition(int32_t* x, int32_t* y);

	///
	/// \param width
	/// \param height
	void SetSize(int32_t width, int32_t height);

	///
	void GetSize(int32_t* width, int32_t* height);

	///
	void Show();

	///
	void Hide();

	///
	void Update();

	~Window();

	Viewport* viewport;


private:
	SDL_Window * sdlWindow;
	SDL_GLContext context;

	int32_t x;
	int32_t y;

	int32_t width;
	int32_t height;

};

#endif