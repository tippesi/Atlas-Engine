#include "ShaderConstant.h"

namespace Atlas {

	namespace Shader {

		ShaderConstant::ShaderConstant(std::string constantString) {

			size_t constEndPosition = constantString.find(' ');
			size_t typePosition = constantString.find_first_not_of(' ', constEndPosition);

			size_t typeEndPosition = constantString.find(' ', typePosition);

			std::string typeString = constantString.substr(typePosition, typeEndPosition - typePosition);

			bool isArray = false;

			// Check if the constant is an array
			size_t openRectangleBracket = constantString.find('[', typePosition);
			size_t closeRectangleBracket = constantString.find(']', openRectangleBracket);

			if (openRectangleBracket != std::string::npos && closeRectangleBracket != std::string::npos) {
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


			size_t namePosition = constantString.find_first_not_of(' ', typeEndPosition);
			size_t nameEndPosition = constantString.find_first_of(" [;=", namePosition);

			name = constantString.substr(namePosition, nameEndPosition - namePosition);

			valuedString = constantString;

		}

		void ShaderConstant::SetValue(float value) {

			valuedString = "const float " + name + " = " + std::to_string(value) + ";";

		}

		void ShaderConstant::SetValue(int32_t value) {

			valuedString = "const int " + name + " = " + std::to_string(value) + ";";

		}

		void ShaderConstant::SetValue(bool value) {

			valuedString = "const bool " + name + " = " + std::to_string(value) + ";";

		}

		void ShaderConstant::SetValue(vec2 value) {

			valuedString = "const vec2 " + name + " = vec2(" + std::to_string(value.x) + "," + std::to_string(value.y) + ");";

		}

		void ShaderConstant::SetValue(vec3 value) {

			valuedString = "const vec3 " + name + " = vec3(" + std::to_string(value.x) + "," + std::to_string(value.y)
						   + "," + std::to_string(value.z) + ");";

		}

		void ShaderConstant::SetValue(vec4 value) {

			valuedString = "const vec4 " + name + " = vec4(" + std::to_string(value.x) + "," + std::to_string(value.y)
						   + "," + std::to_string(value.z) + "," + std::to_string(value.w) + ");";

		}

		void ShaderConstant::SetValue(float* value, int32_t length) {

			if (length < 1) {
				return;
			}

			uint32_t modulo = 0;

			valuedString = "const float " + name + "[" + std::to_string(length) + "] = float[](" + std::to_string(value[0]);

			for (int32_t i = 1; i < length; i++) {

				if (modulo != valuedString.length() % 80) {
					valuedString += '\n';
					modulo = valuedString.length() % 80;
				}

				valuedString += ", " + std::to_string(value[i]);
			}

			valuedString += ");";

		}

		void ShaderConstant::SetValue(int32_t *value, int32_t length) {


			if (length < 1) {
				return;
			}

			uint32_t modulo = 0;

			valuedString = "const int " + name + "[" + std::to_string(length) + "] = int[](" + std::to_string(value[0]);

			for (int32_t i = 1; i < length; i++) {

				if (modulo != valuedString.length() % 80) {
					valuedString += '\n';
					modulo = valuedString.length() % 80;
				}

				valuedString += ", " + std::to_string(value[i]);
			}

			valuedString += ");";

		}

		std::string ShaderConstant::GetName() {

			return name;

		}

		std::string ShaderConstant::GetValuedString() {

			return valuedString;

		}

	}

}