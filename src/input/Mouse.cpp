#include "Mouse.h"

#include <stdio.h>
#include <stdlib.h>

namespace Atlas {

	namespace Input {

		MouseHandler::MouseHandler(Camera* camera, float sensibility, float reactivity, bool hideMouse) :
				sensibility(sensibility), reactivity(reactivity), hideMouse(hideMouse) {

			activationButtonDown = false;
			hideMouse = false;
			lock = false;
			activationButton = AE_MOUSEBUTTON_LEFT;
			rotation = camera->rotation;

			auto mouseMotionEventHandler = std::bind(&MouseHandler::MouseMotionEventHandler, this, std::placeholders::_1);
			Events::EventManager::MouseMotionEventDelegate.Subscribe(mouseMotionEventHandler);

			auto mouseButtonEventHandler = std::bind(&MouseHandler::MouseButtonEventHandler, this, std::placeholders::_1);
			Events::EventManager::MouseButtonEventDelegate.Subscribe(mouseButtonEventHandler);

		}

		void MouseHandler::Update(Camera* camera, uint32_t deltaTime) {

			float progress = glm::clamp(reactivity * (float)deltaTime, 0.0f, 1.0f);

			camera->rotation = glm::mix(camera->rotation, rotation, progress);

		}

		void MouseHandler::SetActivationButton(uint8_t mouseButton) {

			activationButton = mouseButton;

		}

		void MouseHandler::HideMouse() {

			hideMouse = true;
			SDL_ShowCursor(0);

		}

		void MouseHandler::ShowMouse() {

			hideMouse = false;
			SDL_ShowCursor(1);

		}

		void MouseHandler::MouseMotionEventHandler(Events::MouseMotionEvent event) {

			if (event.windowID == 0 || lock)
				return;

			if (hideMouse) {



			}
			else {

				if (!activationButtonDown)
					return;

				rotation += glm::vec2(-((float)event.xDelta), -((float)event.yDelta)) * sensibility * 0.001f;

			}

		}

		void MouseHandler::MouseButtonEventHandler(Events::MouseButtonEvent event) {

			if (event.windowID == 0 || lock)
				return;

			if (event.button == activationButton && event.state == AE_BUTTON_PRESSED)
				activationButtonDown = true;

			if (event.button == activationButton && event.state == AE_BUTTON_RELEASED)
				activationButtonDown = false;

		}

	}

}