#ifndef ENGINECONTROLLERDEVICEEVENT_H
#define ENGINECONTROLLERDEVICEEVENT_H

#include "../System.h"
#include "../libraries/SDL/include/SDL.h"
#include "EventDelegate.h"

#define CONTROLLER_ADDED SDL_CONTROLLERDEVICEADDED
#define CONTROLLER_REMOVED SDL_CONTROLLERDEVICEREMOVED
#define CONTROLLER_MAPPED SDL_CONTROLLERDEVICEREMAPPED

class EngineControllerDeviceEvent {

public:
	EngineControllerDeviceEvent(SDL_ControllerDeviceEvent event) {

		type = event.type;
		device = event.which;

	}

	/**
	 * The type of the event. See {@link EngineControllerDeviceEvent.h} for more.
	 */
	uint32_t type;

	/**
	 * The device ID of the game controller.
	 */
	int32_t device;

};


#endif