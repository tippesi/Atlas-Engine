#ifndef AE_UNIFORM_H
#define AE_UNIFORM_H

#include "../System.h"
#include <string>

namespace Atlas {

	namespace Shader {

		class ShaderBatch;

		/**
		 * Manages shader uniforms
		 */
		class Uniform {

		public:
			/**
             * Constructs a Uniform object for a shader.
             * @param shaderID The ID of the shader.
             * @param uniformName The name of the uniform
             * @param shaderBatch The shader batch the uniform abstracts, if uniform abstracts access of a shader batch
             * @param ID The ID of the uniform in the shader batch, if uniform abstracts access of a shader batch
             */
			Uniform(uint32_t shaderID, std::string uniformName, ShaderBatch* shaderBatch = nullptr, int32_t ID = 0);

			/**
             * Updates the ID of the uniform.
             */
			void Update();

			void SetValue(int32_t value);

			void SetValue(float value);

			void SetValue(bool value);

			void SetValue(mat4 value);

			void SetValue(mat3 value);

			void SetValue(vec4 value);

			void SetValue(vec3 value);

			void SetValue(vec2 value);

			void SetValue(ivec4 value);

			void SetValue(ivec3 value);

			void SetValue(ivec2 value);

			void SetValue(int32_t *value, int32_t length);

			void SetValue(float *value, int32_t length);

			void SetValue(vec4* value, int32_t length);

			void SetValue(vec3* value, int32_t length);

			void SetValue(vec2* value, int32_t length);

			std::string name;

		private:
			int32_t ID;
			uint32_t shaderID;

			ShaderBatch* shaderBatch;

			inline Uniform* GetBatchUniform();

		};

	}

}

#endif