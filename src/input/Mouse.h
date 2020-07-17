#ifndef AE_MOUSE_H
#define AE_MOUSE_H

#include "../System.h"
#include "../Camera.h"
#include "events/EventManager.h"

namespace Atlas {

	namespace Input {

		class MouseHandler {

		public:
			MouseHandler();

			MouseHandler(const MouseHandler& that);

			MouseHandler(Camera* camera, float sensibility, float reactivity, bool hideMouse = false);

			~MouseHandler();

			MouseHandler& operator=(const MouseHandler& that);

			void Update(Camera* camera, float deltaTime);

			void Reset(Camera* camera);

			void SetActivationButton(uint8_t mouseButton);

			void HideMouse();

			void ShowMouse();

			float sensibility = 1.5f;
			float reactivity = 6.0f;

			bool lock = false;
			bool hideMouse = false;

		private:
			void RegisterEvents();

			void MouseMotionEventHandler(Events::MouseMotionEvent event);
			void MouseButtonEventHandler(Events::MouseButtonEvent event);

			void DeepCopy(const MouseHandler& that);

			bool activationButtonDown = false;
			uint8_t activationButton = AE_MOUSEBUTTON_LEFT;

			vec2 rotation = vec2(0.0f);

			int32_t mouseMotionEventHandle;
			int32_t mouseButtonEventHandle;

		};

	}

}

#endif