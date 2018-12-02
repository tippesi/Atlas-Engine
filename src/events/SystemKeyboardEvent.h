#ifndef SYSTEMKEYBOARDEVENT_H
#define SYSTEMKEYBOARDEVENT_H

#include "../System.h"
#include "../libraries/SDL/include/SDL.h"
#include "EventDelegate.h"

class SystemKeyboardEvent {

public:
	SystemKeyboardEvent(SDL_KeyboardEvent event) {



	}

	uint32_t windowID;
	bool keyDown;
	bool keyRepeat;

};


#endif