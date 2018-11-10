#include "Keyboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>


KeyboardHandler* CreateKeyboardHandler(Camera* camera, float speed, float reactivity) {

	KeyboardHandler* handler = (KeyboardHandler*)malloc(sizeof(KeyboardHandler));

	if (handler != NULL) {
		
		handler->speed = speed;
		handler->reactivity = reactivity;

		handler->lock = false;

		handler->location = camera->location;

		return handler;

	}

	return NULL;


}


void DeleteKeyboardHandler(KeyboardHandler* handler) {

	if (handler != NULL)
		free(handler);

}


void CalculateKeyboardHandler(KeyboardHandler* handler, Camera* camera, uint32_t deltatime) {

	if (handler != NULL && camera != NULL && deltatime > 0) {

		if (handler->lock == false) {

			float floatdelta = (float)deltatime / 1000.0f;

			const uint8_t *state = SDL_GetKeyboardState(NULL);

			float increasedSpeed = 0.0f;

			//You can use the left shift key for increased speed
			if (state[SDL_SCANCODE_LSHIFT] != 0)
				increasedSpeed = handler->speed * 4.0f;

			if (state[SDL_SCANCODE_W] != 0) {
				handler->location += camera->direction * floatdelta * (handler->speed + increasedSpeed);
			}
			if (state[SDL_SCANCODE_S] != 0) {
				handler->location -= camera->direction * floatdelta * (handler->speed + increasedSpeed);
			}
			if (state[SDL_SCANCODE_A] != 0) {
				handler->location -= camera->right * floatdelta * (handler->speed + increasedSpeed);
			}
			if (state[SDL_SCANCODE_D] != 0) {
				handler->location += camera->right * floatdelta * (handler->speed + increasedSpeed);
			}

		}

		float progress = glm::clamp(handler->reactivity * ((float)deltatime / 16.0f), 0.0f, 1.0f);

		camera->location = glm::mix(camera->location, handler->location, progress);

	}

}