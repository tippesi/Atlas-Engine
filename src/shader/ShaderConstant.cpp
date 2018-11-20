#include "ShaderConstant.h"

ShaderConstant::ShaderConstant(const char* constantString) {

	string line(constantString);

	size_t constEndPosition = line.find(' ');
	size_t typePosition = line.find_first_not_of(' ', constEndPosition);

	size_t typeEndPosition = line.find(' ', typePosition);

	string typeString = line.substr(typePosition, typeEndPosition - typePosition);

	bool isArray = false;

	// Check if the constant is an array
	size_t openRectangleBracket = line.find('[', typePosition);
	size_t closeRectangleBracket = line.find(']', openRectangleBracket);

	if (openRectangleBracket != string::npos && closeRectangleBracket != string::npos) {
		isArray = true;
	}

	if (typeString == "float" && !isArray) {
		type = FLOAT_CONSTANT;
	}
	else if (typeString == "int" && !isArray) {
		type = INT_CONSTANT;
	}
	else if (typeString == "bool" && !isArray) {
		type = BOOLEAN_CONSTANT;
	}
	else if (typeString == "vec2" && !isArray) {
		type = VECTOR2_CONSTANT;
	}
	else if (typeString == "vec3" && !isArray) {
		type = VECTOR3_CONSTANT;
	}
	else if (typeString == "vec4" && !isArray) {
		type = VECTOR4_CONSTANT;
	}
	else if (typeString == "float" && isArray) {
		type = FLOAT_ARRAY_CONSTANT;
	}
	else if (typeString == "int" && isArray) {
		type = INT_ARRAY_CONSTANT;
	}


	size_t namePosition = line.find_first_not_of(' ', typeEndPosition);
	size_t nameEndPosition = line.find_first_of(" [;=", namePosition);

	name = line.substr(namePosition, nameEndPosition - namePosition);

	valuedString = line;

}

void ShaderConstant::SetValue(float value) {

	valuedString = "const float " + name + " = " + to_string(value) + ";";

}

void ShaderConstant::SetValue(int32_t value) {

	valuedString = "const int " + name + " = " + to_string(value) + ";";

}

void ShaderConstant::SetValue(bool value) {

	valuedString = "const bool " + name + " = " + to_string(value) + ";";

}

void ShaderConstant::SetValue(vec2 value) {

	valuedString = "const vec2 " + name + " = vec2(" + to_string(value.x) + "," + to_string(value.y) + ");";

}

void ShaderConstant::SetValue(vec3 value) {

	valuedString = "const vec3 " + name + " = vec3(" + to_string(value.x) + "," + to_string(value.y) 
		+ "," + to_string(value.z) + ");";

}

void ShaderConstant::SetValue(vec4 value) {

	valuedString = "const vec4 " + name + " = vec4(" + to_string(value.x) + "," + to_string(value.y)
		+ "," + to_string(value.z) + "," + to_string(value.w) + ");";

}

void ShaderConstant::SetValue(float* value, int32_t length) {

	if (length < 1) {
		return;
	}

	int32_t modulo = 0;

	valuedString = "const float " + name + "[" + to_string(length) + "] = float[](" + to_string(value[0]);

	for (int32_t i = 1; i < length; i++) {

		if (modulo != valuedString.length() % 80) {
			valuedString += '\n';
			modulo = valuedString.length() % 80;
		}

		valuedString += ", " + to_string(value[i]);
	}

	valuedString += ");";

}

void ShaderConstant::SetValue(int32_t *value, int32_t length) {


	if (length < 1) {
		return;
	}

	int32_t modulo = 0;

	valuedString = "const int " + name + "[" + to_string(length) + "] = int[](" + to_string(value[0]);

	for (int32_t i = 1; i < length; i++) {

		if (modulo != valuedString.length() % 80) {
			valuedString += '\n';
			modulo = valuedString.length() % 80;
		}

		valuedString += ", " + to_string(value[i]);
	}

	valuedString += ");";

}

string ShaderConstant::GetName() {

	return name;

}

string ShaderConstant::GetValuedString() {

	return valuedString;

}