#include "Mouse.h"

#include <stdio.h>
#include <stdlib.h>


MouseHandler* CreateMouseHandler(Camera* camera, float sensibility, float reactivity) {

	MouseHandler* handler = (MouseHandler*)malloc(sizeof(MouseHandler));

	if (handler != NULL) {

		handler->rotation = camera->rotation;

		handler->sensibility = sensibility;
		handler->reactivity = reactivity;

		handler->lock = false;
		handler->relative = false;

		handler->lastMousePosition = glm::vec2(-1.0f);
		handler->mousePosition = glm::vec2(100.0f);

		return handler;
	
	}

	return NULL;

}


void DeleteMouseHandler(MouseHandler* handler) {

	if (handler != NULL)
		free(handler);

}


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

		float progress = glm::clamp(handler->reactivity * ((float)deltatime / 16.0f), 0.0f, 1.0f);

		camera->rotation = glm::mix(camera->rotation, handler->rotation, progress);

	}

}