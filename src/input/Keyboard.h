#ifndef AE_KEYBOARD_H
#define AE_KEYBOARD_H

#include "../System.h"
#include "../Camera.h"
#include "../events/EngineEventHandler.h"


class KeyboardHandler {

public:
	KeyboardHandler(Camera* camera, float speed, float reactivity);

	void Update(Camera* camera, uint32_t deltaTime);

	float speed;
	float reactivity;

private:
	void KeyboardEventHandler(EngineKeyboardEvent event);

	vec3 location;
	vec2 movement;

};

#endif