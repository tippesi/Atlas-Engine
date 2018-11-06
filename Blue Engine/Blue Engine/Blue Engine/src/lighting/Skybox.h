#ifndef SKYBOX_H
#define SKYBOX_H

#include "../System.h"
#include "../Cubemap.h"

class Skybox {

public:
	Skybox(Cubemap* cubemap, mat4 matrix = mat4(1.0f)) : cubemap(cubemap), matrix(matrix) {};

	Cubemap * cubemap;
	mat4 matrix;

};


#endif