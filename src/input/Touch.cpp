#include "Touch.h"

namespace Atlas {

    namespace Input {

		TouchHandler::TouchHandler(Camera* camera, float sensibility, float speed, float reactivity)
			: sensibility(sensibility), speed(speed), reactivity(reactivity) {

			location = camera->location;
			rotation = camera->rotation;

			leftFinger.ID = -1;
			rightFinger.ID = -1;

			// The position will be the accumulated changes of positions until the finger
			// is no longer pressed.
			leftFinger.position = vec2(0.0f);
			rightFinger.position = vec2(0.0f);

			auto touchEventHandler = std::bind(&TouchHandler::TouchEventHandler, this, std::placeholders::_1);
			Events::EventManager::TouchEventDelegate.Subscribe(touchEventHandler);

		}

		void TouchHandler::Update(Camera* camera, uint32_t deltaTime) {

			float floatDelta = (float)deltaTime / 1000.0f;

			location += 2.0f * camera->direction * -leftFinger.position.y * floatDelta * speed;
			location += 2.0f * camera->right * leftFinger.position.x * floatDelta * speed;

			rotation += 60.0f * -rightFinger.position * floatDelta * sensibility;

			float progress = glm::clamp(reactivity * ((float)deltaTime / 16.0f), 0.0f, 1.0f);

			camera->location = glm::mix(camera->location, location, progress);
			camera->rotation = glm::mix(camera->rotation, rotation, progress);

			// We want to reset the accumulated position to zero for the right finger
			rightFinger.position = vec2(0.0f);
			
		}

		void TouchHandler::TouchEventHandler(Events::TouchEvent event) {

			if (event.finger == leftFinger.ID) {
				if (event.type == AE_FINGERUP) {
					leftFinger.ID = -1;
					leftFinger.position = vec2(0.0f);
					return;
				}
				leftFinger.position += vec2(event.dx, event.dy);
			}
			else if (event.finger == rightFinger.ID) {
				if (event.type == AE_FINGERUP) {
					rightFinger.ID = -1;
					return;
				}
				rightFinger.position += vec2(event.dx, event.dy);
			}
			else {
				if (event.x < 0.3f && leftFinger.ID < 0) {
					leftFinger.ID = event.finger;
					leftFinger.position += vec2(event.dx, event.dy);
				}
				else if (event.x >= 0.3f && rightFinger.ID < 0) {
					rightFinger.ID = event.finger;
					rightFinger.position += vec2(event.dx, event.dy);
				}
			}

		}

    }

}