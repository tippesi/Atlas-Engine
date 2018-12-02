#include "Mouse.h"

#include <stdio.h>
#include <stdlib.h>

MouseHandler::MouseHandler(Camera* camera, float sensibility, float reactivity, bool hideMouse) :
	sensibility(sensibility), reactivity(reactivity), hideMouse(hideMouse) {

	activationButtonDown = false;
	hideMouse = false;
	lock = false;
	activationButton = MOUSEBUTTON_LEFT;
	rotation = camera->rotation;

	auto mouseMotionEventHandler = std::bind(&MouseHandler::MouseMotionEventHandler, this, std::placeholders::_1);
	SystemEventHandler::mouseMotionEventDelegate.Subscribe(mouseMotionEventHandler);

	auto mouseButtonEventHandler = std::bind(&MouseHandler::MouseButtonEventHandler, this, std::placeholders::_1);
	SystemEventHandler::mouseButtonEventDelegate.Subscribe(mouseButtonEventHandler);

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

void MouseHandler::MouseMotionEventHandler(SystemMouseMotionEvent event) {

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

void MouseHandler::MouseButtonEventHandler(SystemMouseButtonEvent event) {

	if (event.windowID == 0 || lock)
		return;

	if (event.button == activationButton && event.buttonDown)
		activationButtonDown = true;

	if (event.button == activationButton && !event.buttonDown)
		activationButtonDown = false;

}

/*
void CalculateMouseHandler(MouseHandler* handler, Camera* camera, uint32_t deltatime) {

	if (handler != NULL && camera != NULL && deltatime > 0) {

		if (handler->lock == false) {

			int x, y;

			SDL_GetGlobalMouseState(&x, &y);

			glm::vec2 mousePosition = glm::vec2((float)x, (float)y);

			if (handler->relative) {

				// First time in relative mode, reset the mouse position and return
				if (handler->lastMousePosition == glm::vec2(-1.0f)) {
					SDL_ShowCursor(0);
					SDL_WarpMouseGlobal((int)handler->mousePosition.x, (int)handler->mousePosition.y);
					handler->lastMousePosition = glm::vec2(1.0f);
					return;
				}

				handler->rotation += glm::vec2(-(handler->mousePosition.x - mousePosition.x), handler->mousePosition.y - mousePosition.y) * handler->sensibility * 0.001f;

				SDL_WarpMouseGlobal((int)handler->mousePosition.x, (int)handler->mousePosition.y);
			}
			else {
				if (handler->lastMousePosition == glm::vec2(-1.0f)) {

					handler->lastMousePosition.x = mousePosition.x;
					handler->lastMousePosition.y = mousePosition.y;

				}

				handler->rotation += glm::vec2(-(handler->lastMousePosition.x - mousePosition.x), -(handler->lastMousePosition.y - mousePosition.y)) * handler->sensibility * 0.001f;

				handler->lastMousePosition.x = mousePosition.x;
				handler->lastMousePosition.y = mousePosition.y;
			}

		}
		else {

			if (handler->relative) {
				SDL_WarpMouseGlobal((int)handler->mousePosition.x, (int)handler->mousePosition.y);
			}
			else {
				if (handler->lastMousePosition != glm::vec2(-1.0f))
					handler->lastMousePosition = glm::vec2(-1.0f);
			}

		}



	}

}
*/