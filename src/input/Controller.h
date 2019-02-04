#ifndef AE_CONTROLLER_H
#define AE_CONTROLLER_H

#include "../System.h"
#include "../Camera.h"
#include "../events/EventHandler.h"

class ControllerHandler {

public:
	ControllerHandler(Camera* camera, float sensibility, float speed, float reactivity, float threshold, int32_t device = -1);

	void Update(Camera* camera, uint32_t deltaTime);

	float sensibility;
	float speed;
	float reactivity;
	float threshold;

private:
	void ControllerAxisEventHandler(Events::ControllerAxisEvent event);
	void ControllerButtonEventHandler(Events::ControllerButtonEvent event);
	void ControllerDeviceEventHandler(Events::ControllerDeviceEvent event);

	vec2 leftStick;
	vec2 rightStick;

	float speedIncrease;

	vec3 location;
	vec2 rotation;

	int32_t controllerDevice;

};

#endif