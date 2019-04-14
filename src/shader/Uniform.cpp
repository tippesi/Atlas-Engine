#include "Uniform.h"
#include "ShaderBatch.h"

namespace Atlas {

	namespace Shader {

		Uniform::Uniform(uint32_t shaderID, std::string uniformName, ShaderBatch* shaderBatch, int32_t ID) :  
			shaderID(shaderID), name(uniformName), shaderBatch(shaderBatch), ID(ID) {

			if (shaderBatch == nullptr) {
				this->ID = glGetUniformLocation(shaderID, uniformName.c_str());
			}

		}

		void Uniform::Update() {

			ID = glGetUniformLocation(shaderID, name.c_str());

		}

		void Uniform::SetValue(int32_t value) {

			if (shaderBatch == nullptr) {
				glUniform1i(ID, value);
			}
			else {
				GetBatchUniform()->SetValue(value);
			}

		}

		void Uniform::SetValue(float value) {

			if (shaderBatch == nullptr) {
				glUniform1f(ID, value);
			}
			else {
				GetBatchUniform()->SetValue(value);
			}

		}

		void Uniform::SetValue(bool value) {

			if (shaderBatch == nullptr) {
				glUniform1i(ID, value);
			}
			else {
				GetBatchUniform()->SetValue(value);
			}

		}

		void Uniform::SetValue(mat4 value) {

			if (shaderBatch == nullptr) {
				glUniformMatrix4fv(ID, 1, GL_FALSE, &value[0][0]);
			}
			else {
				GetBatchUniform()->SetValue(value);
			}
		}

		void Uniform::SetValue(mat3 value) {

			if (shaderBatch == nullptr) {
				glUniformMatrix3fv(ID, 1, GL_FALSE, &value[0][0]);
			}
			else {
				GetBatchUniform()->SetValue(value);
			}

		}

		void Uniform::SetValue(vec4 value) {

			if (shaderBatch == nullptr) {
				glUniform4f(ID, value.x, value.y, value.z, value.w);
			}
			else {
				GetBatchUniform()->SetValue(value);
			}

		}

		void Uniform::SetValue(vec3 value) {

			if (shaderBatch == nullptr) {
				glUniform3f(ID, value.x, value.y, value.z);
			}
			else {
				GetBatchUniform()->SetValue(value);
			}

		}

		void Uniform::SetValue(vec2 value) {

			if (shaderBatch == nullptr) {
				glUniform2f(ID, value.x, value.y);
			}
			else {
				GetBatchUniform()->SetValue(value);
			}
		}

		void Uniform::SetValue(int32_t* value, int32_t length) {

			if (shaderBatch == nullptr) {
				glUniform1iv(ID, length, value);
			}
			else {
				GetBatchUniform()->SetValue(value, length);
			}

		}

		void Uniform::SetValue(float* value, int32_t length) {

			if (shaderBatch == nullptr) {
				glUniform1fv(ID, length, value);
			}
			else {
				GetBatchUniform()->SetValue(value, length);
			}

		}

		void Uniform::SetValue(vec4* value, int32_t length) {

			if (shaderBatch == nullptr) {
				glUniform4fv(ID, length, glm::value_ptr(value[0]));
			}
			else {
				GetBatchUniform()->SetValue(value, length);
			}

		}

		void Uniform::SetValue(vec3* value, int32_t length) {

			if (shaderBatch == nullptr) {
				glUniform3fv(ID, length, glm::value_ptr(value[0]));
			}
			else {
				GetBatchUniform()->SetValue(value, length);
			}

		}

		void Uniform::SetValue(vec2* value, int32_t length) {

			if (shaderBatch == nullptr) {
				glUniform2fv(ID, length, glm::value_ptr(value[0]));
			}
			else {
				GetBatchUniform()->SetValue(value, length);
			}

		}

		inline Uniform* Uniform::GetBatchUniform() {

			return shaderBatch->configBatches[shaderBatch->boundShaderID]->GetShader()->uniforms[ID];

		}

	}

}