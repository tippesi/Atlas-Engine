#include "Touch.h"

namespace Atlas {

    namespace Input {

		TouchHandler::TouchHandler() {

			RegisterEvent();

		}

		TouchHandler::TouchHandler(const TouchHandler& that) {

			RegisterEvent();

			DeepCopy(that);

		}

		TouchHandler::TouchHandler(Camera* camera, float sensibility, float speed, float reactivity)
			: sensibility(sensibility), speed(speed), reactivity(reactivity) {

			RegisterEvent();

			location = camera->location;
			rotation = camera->rotation;

		}

		TouchHandler::~TouchHandler() {

			Atlas::Events::EventManager::TouchEventDelegate.Unsubscribe(eventHandle);

		}

		TouchHandler& TouchHandler::operator=(const TouchHandler& that) {

			if (this != &that) {

				DeepCopy(that);

			}

			return *this;

		}

		void TouchHandler::Update(Camera* camera, float deltaTime) {

			location += 2.0f * camera->direction * -leftFinger.position.y * deltaTime * speed;
			location += 2.0f * camera->right * leftFinger.position.x * deltaTime * speed;

			rotation += 60.0f * -rightFinger.position * deltaTime * sensibility;

			float progress = glm::clamp(reactivity * deltaTime, 0.0f, 1.0f);

			camera->location = glm::mix(camera->location, location, progress);
			camera->rotation = glm::mix(camera->rotation, rotation, progress);

			// We want to reset the accumulated position to zero for the right finger
			rightFinger.position = vec2(0.0f);
			
		}

		void TouchHandler::Reset(Camera* camera) {

			rotation = camera->rotation;
			location = camera->location;

		}

		void TouchHandler::RegisterEvent() {

			auto touchEventHandler = std::bind(&TouchHandler::TouchEventHandler, this, std::placeholders::_1);
			eventHandle = Events::EventManager::TouchEventDelegate.Subscribe(touchEventHandler);

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

		void TouchHandler::DeepCopy(const TouchHandler& that) {

			sensibility = that.sensibility;
			speed = that.speed;
			reactivity = that.reactivity;

			leftFinger = that.leftFinger;
			rightFinger = that.rightFinger;

			location = that.location;
			rotation = that.rotation;

		}

    }

}