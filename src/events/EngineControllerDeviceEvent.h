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

	uint32_t type;
	int32_t device;

};


#endif