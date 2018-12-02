#ifndef SYSTEMMOUSEMOTIONEVENT_H
#define SYSTEMMOUSEMOTIONEVENT_H

#include "../System.h"
#include "../libraries/SDL/include/SDL.h"
#include "EventDelegate.h"

/**
 * A class to distribute mouse motion events.
 */
class SystemMouseMotionEvent {

public:
	SystemMouseMotionEvent(SDL_MouseMotionEvent event) {

		windowID = event.windowID;
		x = event.x;
		y = event.y;
		xDelta = event.xrel;
		yDelta = event.yrel;

	}

	/**
	* The ID of the window the event occurred in
	*/
	uint32_t windowID;

	/**
	 * The x coordinate relative to the window where the button event occurred
	 */
	int32_t x;

	/**
	 * The y coordinate relative to the window where the button event occurred
	 */
	int32_t y;

	/**
	 * The relative motion in x direction
	 */
	int32_t xDelta;

	/**
	 * The relative motion in y direction
	 */
	int32_t yDelta;

};

#endif