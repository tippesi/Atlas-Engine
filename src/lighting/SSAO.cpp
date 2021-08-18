#include "SSAO.h"

#include "../common/RandomHelper.h"

namespace Atlas {

	namespace Lighting {

		using namespace Common;

		SSAO::SSAO(int32_t width, int32_t height, uint32_t sampleCount) {

			noiseTexture = Texture::Texture2D(4, 4, GL_RG16F, GL_REPEAT, GL_NEAREST);

			// Generate vec2 noise (4 * 4 * 2 floats)
			std::vector<float> noise;
			for (uint32_t i = 0; i < 32; i++)
				noise.push_back(Random::CanonicalUniform());

			noiseTexture.SetData(noise);

			SetSampleCount(sampleCount);

			map = Texture::Texture2D(width, height, GL_R16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
			blurMap = Texture::Texture2D(width, height, GL_R16F, GL_CLAMP_TO_EDGE, GL_LINEAR);

		}

		void SSAO::SetSampleCount(uint32_t sampleCount) {			

			this->sampleCount = sampleCount;

			samples.clear();
			for (uint32_t i = 0; i < sampleCount; i++) {

				glm::vec3 sample = normalize(glm::vec3(
					2.0f * Random::CanonicalUniform() - 1.0f, 
					2.0f * Random::CanonicalUniform() - 1.0f, 
					Random::CanonicalUniform()));

				sample *= Random::CanonicalUniform();

				float scale = float(i) / float(sampleCount);

				scale = glm::mix(0.1f, 1.0f, scale * scale);
				sample *= scale;

				samples.push_back(sample);

			}
		}

	}

}