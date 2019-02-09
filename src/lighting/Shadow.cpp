#include "Shadow.h"
#include "ILight.h"
#include "DirectionalLight.h"
#include "PointLight.h"

namespace Atlas {

	namespace Lighting {

		Shadow::Shadow(float distance, float bias, int32_t resolution, int32_t cascadeCount, float splitCorrection) :
				distance(distance), bias(bias), resolution(resolution) {

			cascadeCount = glm::min(cascadeCount, MAX_SHADOW_CASCADE_COUNT);
			splitCorrection = glm::clamp(splitCorrection, 0.0f, 1.0f);
			componentCount = cascadeCount;
			this->splitCorrection = splitCorrection;

			sampleCount = 16;
			sampleRange = 2.2f;
			useCubemap = false;

			components = new ShadowComponent[cascadeCount];

			maps = new Texture::Texture2DArray(resolution, resolution, cascadeCount, 
				AE_DEPTH24, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

			maps->Bind(GL_TEXTURE0);

			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

			Update();

		}

		Shadow::Shadow(float distance, float bias, int32_t resolution, bool useCubemap) :
				distance(distance), bias(bias), resolution(resolution), useCubemap(useCubemap) {

			sampleCount = 16;
			sampleRange = 2;
			splitCorrection = 0.0f;


			if (useCubemap) {
				componentCount = 6;

				cubemap  = new Texture::Cubemap(AE_UINT, resolution, resolution, AE_DEPTH24,
									   GL_CLAMP_TO_EDGE, GL_LINEAR, false);

				cubemap->Bind(GL_TEXTURE0);

				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

			}
			else {
				componentCount = 1;

				maps = new Texture::Texture2DArray(resolution, resolution, 1,
										  AE_DEPTH24, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

				maps->Bind(GL_TEXTURE0);

				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

			}

			components = new ShadowComponent[componentCount];

			Update();

		}

		void Shadow::Update() {

			update = true;

		}

		Shadow::~Shadow() {

			delete[] components;
			delete maps;
			delete cubemap;

		}

	}

}