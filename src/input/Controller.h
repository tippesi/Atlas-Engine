#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "../System.h"
#include "../Camera.h"
#include "../events/EngineEventHandler.h"

class ControllerHandler {

public:
	ControllerHandler(Camera* camera, float sensibility, float speed, float reactivity, float threshold, int32_t device = -1);

	void Update(Camera* camera, uint32_t deltaTime);

	float sensibility;
	float speed;
	float reactivity;
	float threshold;

private:
	void ControllerAxisEventHandler(EngineControllerAxisEvent event);
	void ControllerButtonEventHandler(EngineControllerButtonEvent event);
	void ControllerDeviceEventHandler(EngineControllerDeviceEvent event);

	vec2 leftStick;
	vec2 rightStick;

	float speedIncrease;

	vec3 location;
	vec2 rotation;

	int32_t controllerDevice;

};

#endif