#include "Keyboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

namespace Atlas {

	namespace Input {

		KeyboardHandler::KeyboardHandler(Camera* camera, float speed, float reactivity) :
				speed(speed), reactivity(reactivity) {

			location = camera->location;
			movement = vec2(0.0f);

			auto keyboardEventHandler = std::bind(&KeyboardHandler::KeyboardEventHandler, this, std::placeholders::_1);
			Events::EventManager::KeyboardEventDelegate.Subscribe(keyboardEventHandler);

		}

		void KeyboardHandler::Update(Camera* camera, uint32_t deltaTime) {

			float floatDelta = (float)deltaTime / 1000.0f;

			location += camera->direction * movement.x * floatDelta * speed;
			location += camera->right * movement.y * floatDelta * speed;

			float progress = glm::clamp(reactivity * ((float)deltaTime / 16.0f), 0.0f, 1.0f);

			camera->location = glm::mix(camera->location, location, progress);

		}

		void KeyboardHandler::KeyboardEventHandler(Events::KeyboardEvent event) {

			if (event.keycode == AE_KEY_W && event.state == AE_BUTTON_PRESSED && !event.repeat) {
				movement.x += 1.0f;
			}

			if (event.keycode == AE_KEY_W && event.state == AE_BUTTON_RELEASED) {
				movement.x -= 1.0f;
			}

			if (event.keycode == AE_KEY_S && event.state == AE_BUTTON_PRESSED && !event.repeat) {
				movement.x -= 1.0f;
			}

			if (event.keycode == AE_KEY_S && event.state == AE_BUTTON_RELEASED) {
				movement.x += 1.0f;
			}

			if (event.keycode == AE_KEY_D && event.state == AE_BUTTON_PRESSED && !event.repeat) {
				movement.y += 1.0f;
			}

			if (event.keycode == AE_KEY_D && event.state == AE_BUTTON_RELEASED) {
				movement.y -= 1.0f;
			}

			if (event.keycode == AE_KEY_A && event.state == AE_BUTTON_PRESSED && !event.repeat) {
				movement.y -= 1.0f;
			}

			if (event.keycode == AE_KEY_A && event.state == AE_BUTTON_RELEASED) {
				movement.y += 1.0f;
			}

		}

	}

}