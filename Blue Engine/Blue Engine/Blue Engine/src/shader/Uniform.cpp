#include "Uniform.h"

Uniform::Uniform(uint32_t shaderID, const char* uniformName) {

	ID = glGetUniformLocation(shaderID, uniformName);

}

void Uniform::SetValue(int32_t value) {

	glUniform1i(ID, value);

}

void Uniform::SetValue(float value) {

	glUniform1f(ID, value);

}

void Uniform::SetValue(bool value) {

	glUniform1i(ID, value);

}

void Uniform::SetValue(mat4 value) {

	glUniformMatrix4fv(ID, 1, GL_FALSE, &value[0][0]);

}

void Uniform::SetValue(mat3 value) {

	glUniformMatrix3fv(ID, 1, GL_FALSE, &value[0][0]);

}

void Uniform::SetValue(vec4 value) {

	glUniform4f(ID, value.x, value.y, value.z, value.w);

}

void Uniform::SetValue(vec3 value) {

	glUniform3f(ID, value.x, value.y, value.z);

}

void Uniform::SetValue(vec2 value) {

	glUniform2f(ID, value.x, value.y);

}

void Uniform::SetValue(int32_t* value, int32_t length) {

	glUniform1iv(ID, length, value);

}

void Uniform::SetValue(float* value, int32_t length) {

	glUniform1fv(ID, length, value);

}