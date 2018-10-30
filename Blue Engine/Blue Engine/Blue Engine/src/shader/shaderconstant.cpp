#include "shaderconstant.h"

ShaderConstant::ShaderConstant(const char* constantString) {

	string line(constantString);

	int32_t constEndPosition = line.find(' ');
	int32_t typePosition = line.find_first_not_of(' ', constEndPosition);

	int32_t typeEndPosition = line.find(' ', typePosition);

	string typeString = line.substr(typePosition, typeEndPosition - typePosition);

	if (typeString == "float") {
		type = FLOAT_CONSTANT;
	}
	else if (typeString == "int") {
		type = INT_CONSTANT;
	}
	else if (typeString == "bool") {
		type = BOOLEAN_CONSTANT;
	}
	else if (typeString == "vec2") {
		type = VECTOR2_CONSTANT;
	}
	else if (typeString == "vec3") {
		type = VECTOR3_CONSTANT;
	}
	else if (typeString == "vec4") {
		type = VECTOR4_CONSTANT;
	}

	int32_t namePosition = line.find_first_not_of(' ', typeEndPosition);
	int32_t nameEndPosition = line.find_first_of(" ;=", namePosition);

	name = line.substr(namePosition, nameEndPosition - namePosition);

	valuedString = line;

}

void ShaderConstant::SetValue(float value) {

	fvalue = value;
	valuedString = "const float " + name + " = " + to_string(value) + ";";

}

void ShaderConstant::SetValue(int32_t value) {

	ivalue = value;
	valuedString = "const int " + name + " = " + to_string(value) + ";";

}

void ShaderConstant::SetValue(bool value) {

	bvalue = value;
	valuedString = "const bool " + name + " = " + to_string(value) + ";";

}

void ShaderConstant::SetValue(vec2 value) {

	v2value = value;
	valuedString = "const int " + name + " = vec2(" + to_string(value.x) + "," + to_string(value.y) + ");";

}

void ShaderConstant::SetValue(vec3 value) {

	v3value = value;
	valuedString = "const int " + name + " = vec2(" + to_string(value.x) + "," + to_string(value.y) 
		+ "," + to_string(value.z) + ");";

}

void ShaderConstant::SetValue(vec4 value) {

	v4value = value;
	valuedString = "const int " + name + " = vec2(" + to_string(value.x) + "," + to_string(value.y)
		+ "," + to_string(value.z) + "," + to_string(value.w) + ");";

}

string ShaderConstant::GetName() {

	return name;

}

string ShaderConstant::GetValuedString() {

	return valuedString;

}