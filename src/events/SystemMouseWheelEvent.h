#ifndef SYSTEMMOUSEWHEELEVENT_H
#define SYSTEMMOUSEWHEELEVENT_H

#include "../System.h"
#include "../libraries/SDL/include/SDL.h"
#include "EventDelegate.h"

class SystemMouseWheelEvent {

public:
	SystemMouseWheelEvent(SDL_MouseWheelEvent event) {

		windowID = event.windowID;
		x = event.x;
		y = event.y;

	}

	/**
	 * The ID of the window the event occurred in
	 */
	uint32_t windowID;

	/**
	 * Horizontal scrolling, is positive when scrolling to
	 * the right and negative otherwise
	 */
	int32_t x;

	/**
	 * Vertical scrolling, is positive when scrolling to
	 * the up and negative otherwise
	 */
	int32_t y;

};

#endif