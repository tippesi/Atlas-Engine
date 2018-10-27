#ifndef SHADERSOURCE_H
#define SHADERSOURCE_H

#include "../system.h"

#define VERTEX_SHADER GL_VERTEX_SHADER
#define FRAGMENT_SHADER GL_FRAGMENT_SHADER

class ShaderSource {

public:
	ShaderSource(const char* filename, int32_t type);

	int32_t ID;
	int32_t type;

	char* code;

private:

};


#endif