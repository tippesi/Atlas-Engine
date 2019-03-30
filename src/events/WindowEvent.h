#ifndef AE_WINDOWEVENT_H
#define AE_WINDOWEVENT_H

#include "../System.h"
#include "EventDelegate.h"

#include <SDL/include/SDL.h>

#define AE_WINDOWEVENT_SHOWN 			SDL_WINDOWEVENT_SHOWN
#define AE_WINDOWEVENT_HIDDEN 			SDL_WINDOWEVENT_HIDDEN
#define AE_WINDOWEVENT_EXPOSED 			SDL_WINDOWEVENT_EXPOSED
#define AE_WINDOWEVENT_MOVED 			SDL_WINDOWEVENT_MOVED
#define AE_WINDOWEVENT_RESIZED 			SDL_WINDOWEVENT_RESIZED
#define AE_WINDOWEVENT_MINIMIZED 		SDL_WINDOWEVENT_MINIMIZED
#define AE_WINDOWEVENT_MAXIMIZED 		SDL_WINDOWEVENT_MAXIMIZED
#define AE_WINDOWEVENT_RESTORED 		SDL_WINDOWEVENT_RESTORED
#define AE_WINDOWEVENT_MOUSE_ENTERED 	SDL_WINDOWEVENT_ENTER
#define AE_WINDOWEVENT_MOUSE_LEAVED 	SDL_WINDOWEVENT_LEAVE
#define AE_WINDOWEVENT_FOCUS_GAINED 	SDL_WINDOWEVENT_FOCUS_GAINED
#define AE_WINDOWEVENT_FOCUS_LOST 		SDL_WINDOWEVENT_FOCUS_LOST
#define AE_WINDOWEVENT_CLOSE 			SDL_WINDOWEVENT_CLOSE

namespace Atlas {

	namespace Events {

		/**
         * A class to distribute window events.
         */
		class WindowEvent {

		public:
			WindowEvent(SDL_WindowEvent event) {
				windowID = event.windowID;
				type = event.event;
				data = ivec2(event.data1, event.data2);
			}

			/**
             * The ID of the window the event occurred in
             */
			uint32_t windowID;

			/**
             * The type of the window event. See {@link WindowEvent.h} for more
             */
			uint8_t type;

			/**
             * The data specific to the event. Represents screen space coordinates
             */
			ivec2 data;

		};

	}

}

#endif