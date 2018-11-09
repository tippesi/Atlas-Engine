#ifndef MOUSE_H
#define MOUSE_H

#include "../System.h"
#include "../Camera.h"

#include "../libraries/SDL/include/SDL.h"

/**

*/
typedef struct MouseHandler {

	float sensibility;
	float reactivity;

	bool lock;
	bool relative;

	//If the camera gets changed, the rotation should be updated by the user
	glm::vec2 rotation;

	glm::vec2 lastMousePosition;
	glm::vec2 mousePosition;

}MouseHandler;

/**

*/
MouseHandler* CreateMouseHandler(Camera* camera, float sensibility, float reactivity);

/**

*/
void DeleteMouseHandler(MouseHandler* mouseHandler);

/**

*/
void CalculateMouseHandler(MouseHandler*, Camera* camera, uint32_t deltatime);


#endif