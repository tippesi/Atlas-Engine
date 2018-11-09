#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../System.h"
#include "../Camera.h"

#include "../libraries/SDL/include/SDL.h"

/**

*/
typedef struct KeyboardHandler {

	float speed;
	float reactivity;

	bool lock;

	glm::vec3 location;

}KeyboardHandler;

/**

*/
KeyboardHandler* CreateKeyboardHandler(Camera* camera, float speed, float reactivity);

/**

*/
void DeleteKeyboardHandler(KeyboardHandler* handler);

/**

*/
void CalculateKeyboardHandler(KeyboardHandler* handler, Camera* camera, uint32_t deltatime);

#endif