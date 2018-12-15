#ifndef ENGINECONTROLLERBUTTONEVENT_H
#define ENGINECONTROLLERBUTTONEVENT_H

#include "../System.h"
#include "../libraries/SDL/include/SDL.h"
#include "EventDelegate.h"

#define CONTROLLERBUTTON_A SDL_CONTROLLER_BUTTON_A
#define CONTROLLERBUTTON_B SDL_CONTROLLER_BUTTON_B
#define CONTROLLERBUTTON_X SDL_CONTROLLER_BUTTON_X
#define CONTROLLERBUTTON_Y SDL_CONTROLLER_BUTTON_Y
#define CONTROLLERBUTTON_UP SDL_CONTROLLER_BUTTON_DPAD_UP
#define CONTROLLERBUTTON_DOWN SDL_CONTROLLER_BUTTON_DPAD_DOWN
#define CONTROLLERBUTTON_LEFT SDL_CONTROLLER_BUTTON_DPAD_LEFT
#define CONTROLLERBUTTON_RIGHT SDL_CONTROLLER_BUTTON_DPAD_RIGHT
#define CONTROLLERBUTTON_LEFTSTICK SDL_CONTROLLER_BUTTON_LEFTSTICK
#define CONTROLLERBUTTON_RIGHTSTICK SDL_CONTROLLER_BUTTON_RIGHTSTICK
#define CONTROLLERBUTTON_LEFTSHOULDER SDL_CONTROLLER_BUTTON_LEFTSHOULDER
#define CONTROLLERBUTTON_RIGHTSHOULDER SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
#define CONTROLLERBUTTON_BACK SDL_CONTROLLER_BUTTON_BACK
#define CONTROLLERBUTTON_GUIDE SDL_CONTROLLER_BUTTON_GUIDE
#define CONTROLLERBUTTON_START SDL_CONTROLLER_BUTTON_START

/**
 * A class to distribute controller button events.
 */
class EngineControllerButtonEvent {

public:
	EngineControllerButtonEvent(SDL_ControllerButtonEvent event) {

		button = event.button;
		state = event.state;
		device = event.which;

	}

	/**
	 * The button which was pressed on the controller. See {@link EngineControllerButtonEvent.h} for more.
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

#endif