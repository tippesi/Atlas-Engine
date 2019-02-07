#ifndef AE_TEXTINPUTEVENT_H
#define AE_TEXTINPUTEVENT_H


#include "../System.h"
#include "EventDelegate.h"
#include "Keycodes.h"

#include <SDL/include/SDL.h>

namespace Atlas {

	namespace Events {

		/**
         * A class to distribute text input events.
         */
		class TextInputEvent {

		public:
			TextInputEvent(SDL_TextInputEvent event) {

				windowID = event.windowID;

				char *text = event.text;

				while (*text != '\0') {
					character.push_back(*text);
					text++;
				}

			}

			/**
             * The ID of the window the event occurred in.
             */
			uint32_t windowID;

			/**
             * The character that was typed in UTF-8.
             */
			std::string character;

		};

	}

}

#endif