#ifndef AE_CONTROLLER_H
#define AE_CONTROLLER_H

#include "../System.h"
#include "../Camera.h"
#include "events/EventManager.h"

namespace Atlas {

	namespace Input {

		class ControllerHandler {

		public:
			ControllerHandler();

			ControllerHandler(const ControllerHandler& that);

			ControllerHandler(Camera* camera, float sensibility, float speed, float reactivity,
					float threshold, int32_t device = -1);

			~ControllerHandler();

			ControllerHandler& operator=(const ControllerHandler& that);

			void Update(Camera* camera, float deltaTime);

			void Reset(Camera* camera);

			float sensibility = 1.5f;
			float speed = 7.0f;
			float reactivity = 6.0f;
			float threshold = 5000.0f;

		private:
			void RegisterEvents();

			void ControllerAxisEventHandler(Events::ControllerAxisEvent event);
			void ControllerButtonEventHandler(Events::ControllerButtonEvent event);
			void ControllerDeviceEventHandler(Events::ControllerDeviceEvent event);

			void DeepCopy(const ControllerHandler& that);

			vec2 leftStick = vec2(0.0f);
			vec2 rightStick = vec2(0.0f);

			float speedIncrease = 0.0f;

			vec3 location = vec3(0.0f);
			vec2 rotation = vec3(0.0f);

			int32_t controllerDevice = -1;

			int32_t controllerAxisEventHandle;
			int32_t controllerButtonEventHandle;
			int32_t controllerDeviceEventHandle;

		};

	}

}

#endif