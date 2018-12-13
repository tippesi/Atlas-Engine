#include "Keyboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

KeyboardHandler::KeyboardHandler(Camera* camera, float speed, float reactivity) : 
	speed(speed), reactivity(reactivity) {

	location = camera->location;
	movement = vec2(0.0f);

	auto keyboardEventHandler = std::bind(&KeyboardHandler::KeyboardEventHandler, this, std::placeholders::_1);
	EngineEventHandler::KeyboardEventDelegate.Subscribe(keyboardEventHandler);

}

void KeyboardHandler::Update(Camera* camera, uint32_t deltaTime) {

	const float increasedSpeed = 4.0f;
	float floatDelta = (float)deltaTime / 1000.0f;

	location += camera->direction * movement.x * floatDelta * (speed + increasedSpeed);
	location += camera->right * movement.y * floatDelta * (speed + increasedSpeed);

	float progress = glm::clamp(reactivity * ((float)deltaTime / 16.0f), 0.0f, 1.0f);

	camera->location = glm::mix(camera->location, location, progress);

}

void KeyboardHandler::KeyboardEventHandler(EngineKeyboardEvent event) {

	if (event.keycode == KEY_W && event.state == BUTTON_PRESSED && !event.repeat) {
		movement.x += 1.0f;
	}

	if (event.keycode == KEY_W && event.state == BUTTON_RELEASED) {
		movement.x -= 1.0f;
	}

	if (event.keycode == KEY_S && event.state == BUTTON_PRESSED && !event.repeat) {
		movement.x -= 1.0f;
	}

	if (event.keycode == KEY_S && event.state == BUTTON_RELEASED) {
		movement.x += 1.0f;
	}

	if (event.keycode == KEY_D && event.state == BUTTON_PRESSED && !event.repeat) {
		movement.y += 1.0f;
	}

	if (event.keycode == KEY_D && event.state == BUTTON_RELEASED) {
		movement.y -= 1.0f;
	}

	if (event.keycode == KEY_A && event.state == BUTTON_PRESSED && !event.repeat) {
		movement.y -= 1.0f;
	}

	if (event.keycode == KEY_A && event.state == BUTTON_RELEASED) {
		movement.y += 1.0f;
	}

}