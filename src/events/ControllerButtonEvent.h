#ifndef AE_CONTROLLERBUTTONEVENT_H
#define AE_CONTROLLERBUTTONEVENT_H

#include "../System.h"
#include "EventDelegate.h"

#include <SDL/include/SDL.h>

#define AE_CONTROLLERBUTTON_A 				SDL_CONTROLLER_BUTTON_A
#define AE_CONTROLLERBUTTON_B 				SDL_CONTROLLER_BUTTON_B
#define AE_CONTROLLERBUTTON_X 				SDL_CONTROLLER_BUTTON_X
#define AE_CONTROLLERBUTTON_Y 				SDL_CONTROLLER_BUTTON_Y
#define AE_CONTROLLERBUTTON_UP 				SDL_CONTROLLER_BUTTON_DPAD_UP
#define AE_CONTROLLERBUTTON_DOWN 			SDL_CONTROLLER_BUTTON_DPAD_DOWN
#define AE_CONTROLLERBUTTON_LEFT 			SDL_CONTROLLER_BUTTON_DPAD_LEFT
#define AE_CONTROLLERBUTTON_RIGHT 			SDL_CONTROLLER_BUTTON_DPAD_RIGHT
#define AE_CONTROLLERBUTTON_LEFTSTICK 		SDL_CONTROLLER_BUTTON_LEFTSTICK
#define AE_CONTROLLERBUTTON_RIGHTSTICK 		SDL_CONTROLLER_BUTTON_RIGHTSTICK
#define AE_CONTROLLERBUTTON_LEFTSHOULDER 	SDL_CONTROLLER_BUTTON_LEFTSHOULDER
#define AE_CONTROLLERBUTTON_RIGHTSHOULDER 	SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
#define AE_CONTROLLERBUTTON_BACK 			SDL_CONTROLLER_BUTTON_BACK
#define AE_CONTROLLERBUTTON_GUIDE 			SDL_CONTROLLER_BUTTON_GUIDE
#define AE_CONTROLLERBUTTON_START 			SDL_CONTROLLER_BUTTON_START

namespace Atlas {

	namespace Events {

		/**
         * A class to distribute controller button events.
         */
		class ControllerButtonEvent {

		public:
			ControllerButtonEvent(SDL_ControllerButtonEvent event) {

				button = event.button;
				state = event.state;
				device = event.which;

			}

			/**
             * The button which was pressed on the controller. See {@link ControllerButtonEvent.h} for more.
             */
			uint8_t button;

			/**
             * The state of the button. Might be BUTTON_PRESSED or BUTTON_RELEASED.
             */
			uint8_t state;

			/**
             * The device ID of the game controller.
             */
			int32_t device;

		};

	}

}

#endif