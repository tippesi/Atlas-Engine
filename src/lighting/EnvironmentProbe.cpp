#include "EnvironmentProbe.h"

namespace Atlas {

	namespace Lighting {

		EnvironmentProbe::EnvironmentProbe(const Texture::Cubemap& cubemap) : 
			cubemap(cubemap), depth(cubemap.width, cubemap.height, AE_DEPTH24) {

			SetPosition(position);

		}

		EnvironmentProbe::EnvironmentProbe(int32_t width, int32_t height, vec3 position) :
			cubemap(width, height, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR, false),
			depth(width, height, AE_DEPTH24) {

			SetPosition(position);

		}

		void EnvironmentProbe::SetPosition(vec3 position) {

			this->position = position;

			mat4 projectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
			vec3 faces[] = { vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f),
							 vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
							 vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f) };

			vec3 ups[] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
						   vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f),
						   vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) };

			for (uint8_t i = 0; i < 6; i++) {
				matrices.push_back(projectionMatrix *
					glm::lookAt(position, position + faces[i], ups[i]));
			}

		}

		vec3 EnvironmentProbe::GetPosition() {

			return position;

		}

	}

}