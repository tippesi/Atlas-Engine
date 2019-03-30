#ifndef AE_MOUSEBUTTONEVENT_H
#define AE_MOUSEBUTTONEVENT_H

#include "../System.h"
#include "EventDelegate.h"

#include <SDL/include/SDL.h>

#define AE_MOUSEBUTTON_LEFT 	SDL_BUTTON_LEFT
#define AE_MOUSEBUTTON_MIDDLE 	SDL_BUTTON_MIDDLE
#define AE_MOUSEBUTTON_RIGHT 	SDL_BUTTON_RIGHT
#define AE_MOUSEBUTTON_X1 		SDL_BUTTON_X1
#define AE_MOUSEBUTTON_X2 		SDL_BUTTON_X2

namespace Atlas {

	namespace Events {

		/**
         * A class to distribute mouse button events.
         */
		class MouseButtonEvent {

		public:
			MouseButtonEvent(SDL_MouseButtonEvent event) {

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
             * The button which was pressed. See {@link MouseButtonEvent.h} for more
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

	}

}

#endif