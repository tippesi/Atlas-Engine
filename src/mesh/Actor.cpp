#include "Actor.h"

Actor::Actor(Mesh* mesh) : mesh(mesh) {

	modelMatrix = mat4(1.0f);

	castShadow = true;

	visible = true;

}