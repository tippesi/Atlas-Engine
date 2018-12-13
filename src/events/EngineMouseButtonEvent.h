#ifndef ENGINEMOUSEBUTTONEVENT_H
#define ENGINEMOUSEBUTTONEVENT_H

#include "../System.h"
#include "../libraries/SDL/include/SDL.h"
#include "EventDelegate.h"

#define MOUSEBUTTON_LEFT SDL_BUTTON_LEFT
#define MOUSEBUTTON_MIDDLE SDL_BUTTON_MIDDLE
#define MOUSEBUTTON_RIGHT SDL_BUTTON_RIGHT
#define MOUSEBUTTON_X1 SDL_BUTTON_X1
#define MOUSEBUTTON_X2 SDL_BUTTON_X2

/**
 * A class to distribute mouse button events.
 */
class EngineMouseButtonEvent {

public:
	EngineMouseButtonEvent(SDL_MouseButtonEvent event) {

		windowID = event.windowID;
		state = event.state;
		button = event.button;
		x = event.x;
		y = event.y;

	}

	/**
	 * The ID of the window the event occurred in
	 */
	uint32_t windowID;

	/**
	 * The state of the button. Might be BUTTON_PRESSED or BUTTON_RELEASED.
	 */
	uint8_t state;

	/**
	 * The button which was pressed. See {@link EngineMouseButtonEvent.h} for more
	 */
	uint8_t button;

	/**
	 * The x coordinate relative to the window where the button event occurred
	 */
	int32_t x;

	/**
	 * The y coordinate relative to the window where the button event occurred
	 */
	int32_t y;

};

#endif