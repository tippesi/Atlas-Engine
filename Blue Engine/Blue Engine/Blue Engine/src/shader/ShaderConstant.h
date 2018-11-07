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

class ShaderConstant {

public:
	ShaderConstant(const char* constantString);

	void SetValue(float value);

	void SetValue(int32_t value);

	void SetValue(bool value);

	void SetValue(vec2 value);

	void SetValue(vec3 value);

	void SetValue(vec4 value);

	string GetName();

	string GetValuedString();

private:
	string name;

	int32_t type;

	float fvalue;
	int32_t ivalue;
	bool bvalue;

	vec2 v2value;
	vec3 v3value;
	vec4 v4value;

	string valuedString;

};


#endif