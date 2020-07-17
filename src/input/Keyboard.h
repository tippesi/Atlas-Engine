#ifndef AE_KEYBOARD_H
#define AE_KEYBOARD_H

#include "../System.h"
#include "../Camera.h"
#include "events/EventManager.h"

namespace Atlas {

	namespace Input {

		class KeyboardHandler {

		public:
			KeyboardHandler();

			KeyboardHandler(const KeyboardHandler& that);

			KeyboardHandler(Camera* camera, float speed, float reactivity);

			~KeyboardHandler();

			KeyboardHandler& operator=(const KeyboardHandler& that);

			void Update(Camera* camera, float deltaTime);

			void Reset(Camera* camera);

			float speed = 7.0f;
			float reactivity = 6.0f;

		private:
			void RegisterEvent();

			void KeyboardEventHandler(Events::KeyboardEvent event);

			void DeepCopy(const KeyboardHandler& that);

			vec3 location = vec3(0.0f);
			vec2 movement = vec2(0.0f);

			int32_t eventHandle;

		};

	}

}

#endif