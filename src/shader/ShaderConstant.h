#ifndef AE_SHADERCONSTANT_H
#define AE_SHADERCONSTANT_H

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

namespace Atlas {

	namespace Shader {

		class ShaderConstant {

		public:
			///
			/// \param constantString
			ShaderConstant(std::string constantString);

			void SetValue(float value);

			void SetValue(int32_t value);

			void SetValue(bool value);

			void SetValue(vec2 value);

			void SetValue(vec3 value);

			void SetValue(vec4 value);

			void SetValue(float* value, int32_t length);

			void SetValue(int32_t* value, int32_t length);

			std::string GetName();

			std::string GetValuedString();

		private:
			std::string name;

			int32_t type;

			std::string valuedString;

		};

	}

}

#endif