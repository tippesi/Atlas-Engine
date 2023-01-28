#include "Impostor.h"

namespace Atlas {

	namespace Mesh {

		Impostor::Impostor(int32_t views, int32_t resolution) : 
			views(views), resolution(resolution) {

            /*
			baseColorTexture = Atlas::Texture::Texture2DArray(resolution,
				resolution, views * views, AE_RGBA8, GL_CLAMP_TO_EDGE,
				GL_LINEAR, true, true);
			roughnessMetalnessAoTexture = Atlas::Texture::Texture2DArray(resolution,
				resolution, views * views, AE_RGB8, GL_CLAMP_TO_EDGE,
				GL_LINEAR, true, true);
			normalTexture = Atlas::Texture::Texture2DArray(resolution,
				resolution, views * views, AE_RGB8, GL_CLAMP_TO_EDGE,
				GL_LINEAR, true, true);
             */

		}

		void Impostor::FillViewPlaneBuffer(std::vector<vec3> rightVectors, std::vector<vec3> upVectors) {

			std::vector<ViewPlane> viewPlanes;

			for (size_t i = 0; i < rightVectors.size(); i++) {

				ViewPlane viewPlane;

				viewPlane.right = vec4(rightVectors[i], 0.0f);
				viewPlane.up = vec4(upVectors[i], 0.0f);

				viewPlanes.push_back(viewPlane);

			}

			//viewPlaneBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(ViewPlane),
			//	0, viewPlanes.size(), viewPlanes.data());

		}

	}

}