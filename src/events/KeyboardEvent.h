#ifndef AE_KEYBOARDEVENT_H
#define AE_KEYBOARDEVENT_H

#include "../System.h"
#include "EventDelegate.h"
#include "Keycodes.h"

#include <SDL/include/SDL.h>

namespace Atlas {

	namespace Events {

		/**
         * A class to distribute keyboard events.
         */
		class KeyboardEvent {

		public:
			KeyboardEvent(SDL_KeyboardEvent event) {

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
             * The code of the key. See {@link Keycodes.h} for more.
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

	}

}


#endif