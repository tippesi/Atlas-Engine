#ifndef SYSTEMKEYBOARDEVENT_H
#define SYSTEMKEYBOARDEVENT_H

#include "../System.h"
#include "EventDelegate.h"
#include "EngineKeycodes.h"

#include <SDL/include/SDL.h>

/**
 * A class to distribute keyboard events.
 */
class EngineKeyboardEvent {

public:
	EngineKeyboardEvent(SDL_KeyboardEvent event) {

		windowID = event.windowID;
		keycode = event.keysym.sym;
		state = event.state;
		repeat = event.repeat > 0 ? true : false;

	}

	/**
	 * The ID of the window the event occurred in.
	 */
	uint32_t windowID;

	/**
	 * The code of the key. See {@link EngineKeycodes.h} for more.
	 */
	Keycode keycode;

	/**
	 * The state of the button. Might be BUTTON_PRESSED or BUTTON_RELEASED.
	 */
	uint8_t state;

	/**
	 * True if the key was pressed for longer. False otherwise.
	 */
	bool repeat;

};


#endif