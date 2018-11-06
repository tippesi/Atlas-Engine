#ifndef UNIFORM_H
#define UNIFORM_H

#include "../system.h"

class Uniform {

public:
	Uniform(uint32_t shaderID, const char* uniformName);

	void SetValue(int32_t value);

	void SetValue(float value);

	void SetValue(bool value);

	void SetValue(mat4 value);

	void SetValue(mat3 value);

	void SetValue(vec4 value);

	void SetValue(vec3 value);

	void SetValue(vec2 value);

	void SetValue(int32_t *value, int32_t length);

	void SetValue(float *value, int32_t length);

private:
	int32_t ID;

};

#endif