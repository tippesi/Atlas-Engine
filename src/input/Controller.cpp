#include "Controller.h"
#include <math.h>

namespace Atlas {

	namespace Input {

		ControllerHandler::ControllerHandler(Camera* camera, float sensibility, float speed, float reactivity,
				float threshold, int32_t device) :
				sensibility(sensibility), speed(speed), reactivity(reactivity),
				threshold(threshold), controllerDevice(device) {

			auto controllerAxisEventHandler = std::bind(&ControllerHandler::ControllerAxisEventHandler,
														this, std::placeholders::_1);
			Events::EventManager::ControllerAxisEventDelegate.Subscribe(controllerAxisEventHandler);

			auto controllerButtonEventHandler = std::bind(&ControllerHandler::ControllerButtonEventHandler,
														  this, std::placeholders::_1);
			Events::EventManager::ControllerButtonEventDelegate.Subscribe(controllerButtonEventHandler);

			auto controllerDeviceEventHandler = std::bind(&ControllerHandler::ControllerDeviceEventHandler,
														  this, std::placeholders::_1);
			Events::EventManager::ControllerDeviceEventDelegate.Subscribe(controllerDeviceEventHandler);

			location = camera->location;
			rotation = camera->rotation;

			leftStick = vec2(0.0f);
			rightStick = vec2(0.0f);
			speedIncrease = 0.0f;

		}

		void ControllerHandler::Update(Camera* camera, uint32_t deltaTime) {

			if (controllerDevice > -1) {

				float floatdelta = (float)deltaTime / 1000.0f;

				location += camera->direction * -leftStick.y * floatdelta * (speed + speedIncrease);
				location += camera->right * leftStick.x * floatdelta * (speed + speedIncrease);

				rotation += -rightStick * floatdelta * sensibility;

				float progress = glm::clamp(reactivity * ((float)deltaTime / 16.0f), 0.0f, 1.0f);

				camera->location = glm::mix(camera->location, location, progress);
				camera->rotation = glm::mix(camera->rotation, rotation, progress);

			}
			else {

				location = camera->location;
				rotation = camera->rotation;

			}

		}

		void ControllerHandler::ControllerAxisEventHandler(Events::ControllerAxisEvent event) {

			if (event.device != controllerDevice)
				return;

			if (event.axis == AE_CONTROLLERAXIS_LEFTX) {
				leftStick.x = abs(event.value) > threshold ? (float)event.value / 32766.0f : 0.0f;
			}
			else if (event.axis == AE_CONTROLLERAXIS_LEFTY) {
				leftStick.y = abs(event.value) > threshold ? (float)event.value / 32766.0f : 0.0f;
			}
			if (event.axis == AE_CONTROLLERAXIS_RIGHTX) {
				rightStick.x = abs(event.value) > threshold ? (float)event.value / 32766.0f : 0.0f;
			}
			else if (event.axis == AE_CONTROLLERAXIS_RIGHTY) {
				rightStick.y = abs(event.value) > threshold ? (float)event.value / 32766.0f : 0.0f;
			}
			else if (event.axis == AE_CONTROLLERAXIS_RIGHTTRIGGER) {
				speedIncrease = 100.0f * (float)event.value / 32766.0f;
			}

		}

		void ControllerHandler::ControllerButtonEventHandler(Events::ControllerButtonEvent event) {

			if (event.device != controllerDevice)
				return;



		}

		void ControllerHandler::ControllerDeviceEventHandler(Events::ControllerDeviceEvent event) {

			if (event.type == AE_CONTROLLER_ADDED) {

				if (controllerDevice == -1) {
					controllerDevice = event.device;
				}

			}
			else if (event.type == AE_CONTROLLER_REMOVED) {

				if (controllerDevice == event.device) {
					controllerDevice = -1;
				}

			}

		}

	}

}