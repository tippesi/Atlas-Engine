#ifndef AE_CONTROLLERAXISEVENT_H
#define AE_CONTROLLERAXISEVENT_H

#include "../System.h"
#include "EventDelegate.h"

#include <SDL/include/SDL.h>

#define AE_CONTROLLERAXIS_LEFTX 		SDL_CONTROLLER_AXIS_LEFTX
#define AE_CONTROLLERAXIS_LEFTY 		SDL_CONTROLLER_AXIS_LEFTY
#define AE_CONTROLLERAXIS_RIGHTX 		SDL_CONTROLLER_AXIS_RIGHTX
#define AE_CONTROLLERAXIS_RIGHTY 		SDL_CONTROLLER_AXIS_RIGHTY
#define AE_CONTROLLERAXIS_LEFTTRIGGER 	SDL_CONTROLLER_AXIS_TRIGGERLEFT
#define AE_CONTROLLERAXIS_RIGHTTRIGGER SDL_CONTROLLER_AXIS_TRIGGERRIGHT

namespace Atlas {

	namespace Events {

		/**
          * A class to distribute controller axis events.
         */
		class ControllerAxisEvent {

		public:
			ControllerAxisEvent(SDL_ControllerAxisEvent event) {

				axis = event.axis;
				value = event.value;
				device = event.which;

			}

			/**
             * The axis which was moved on the controller. See {@link ControllerAxisEvent.h} for more.
             */
			uint8_t axis;

			/**
             * The value of the axis. Ranges from -32767-32767.
             */
			int16_t value;

			/**
             * The device ID of the game controller.
             */
			int32_t device;

		};

	}

}


#endif