#ifndef SHADER_H
#define SHADER_H

#include "../system.h"
#include "shadersource.h"
#include "uniform.h"

class Shader {

public:
	Shader();

	Uniform* GetUniform(const char* uniformName);

	ShaderSource* AddComponent(int32_t type, const char* filename);


};

#endif