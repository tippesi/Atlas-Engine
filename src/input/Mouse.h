#ifndef AE_MOUSE_H
#define AE_MOUSE_H

#include "../System.h"
#include "../Camera.h"
#include "events/EventManager.h"

namespace Atlas {

	namespace Input {

		class MouseHandler {

		public:
			MouseHandler() {}

			MouseHandler(Camera* camera, float sensibility, float reactivity, bool hideMouse = false);

			void Update(Camera* camera, float deltaTime);

			void SetActivationButton(uint8_t mouseButton);

			void HideMouse();

			void ShowMouse();

			float sensibility;
			float reactivity;

			bool lock;
			bool hideMouse;

		private:
			void MouseMotionEventHandler(Events::MouseMotionEvent event);
			void MouseButtonEventHandler(Events::MouseButtonEvent event);

			bool activationButtonDown;
			uint8_t activationButton;

			//If the camera gets changed, the rotation should be updated by the user
			glm::vec2 rotation;

			glm::vec2 lastMousePosition;
			glm::vec2 mousePosition;

		};

	}

}

#endif