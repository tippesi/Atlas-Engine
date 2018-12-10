#ifndef SYSTEMKEYBOARDEVENT_H
#define SYSTEMKEYBOARDEVENT_H

#include "../System.h"
#include "../libraries/SDL/include/SDL.h"
#include "EventDelegate.h"
#include "EngineKeycodes.h"

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

	uint32_t windowID;
	Keycode keycode;
	uint8_t state;
	bool repeat;

};


#endif