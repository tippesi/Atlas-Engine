#ifndef SHADERCONSTANT_H
#define SHADERCONSTANT_H

#include "../System.h"

#include <string>

#define FLOAT_CONSTANT 0x0
#define INT_CONSTANT 0x1
#define BOOLEAN_CONSTANT 0x2
#define VECTOR2_CONSTANT 0x3
#define VECTOR3_CONSTANT 0x4
#define VECTOR4_CONSTANT 0x5
#define FLOAT_ARRAY_CONSTANT 0x6
#define INT_ARRAY_CONSTANT 0x7

class ShaderConstant {

public:
	ShaderConstant(string constantString);

	void SetValue(float value);

	void SetValue(int32_t value);

	void SetValue(bool value);

	void SetValue(vec2 value);

	void SetValue(vec3 value);

	void SetValue(vec4 value);

	void SetValue(float* value, int32_t length);

	void SetValue(int32_t* value, int32_t length);

	string GetName();

	string GetValuedString();

private:
	string name;

	int32_t type;

	string valuedString;

};


#endif