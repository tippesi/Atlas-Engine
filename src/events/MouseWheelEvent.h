#ifndef AE_MOUSEWHEELEVENT_H
#define AE_MOUSEWHEELEVENT_H

#include "../System.h"
#include "EventDelegate.h"

#include <SDL/include/SDL.h>

namespace Atlas {

	namespace Events {

		/**
         * A class to distribute mouse wheel events.
         */
		class MouseWheelEvent {

		public:
			MouseWheelEvent(SDL_MouseWheelEvent event) {

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

	}

}

#endif