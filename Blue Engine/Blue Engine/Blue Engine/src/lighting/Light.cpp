#include "light.h"

Light::Light(int32_t type) : type(type) {

	direction = vec3(0.0f, -1.0f, 0.0f);
	location = vec3(0.0f, 3.0f, 0.0f);

	diffuseColor = vec3(1.0f);
	ambient = 0.1f;

	shadow = nullptr;

}