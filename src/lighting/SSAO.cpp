#include "SSAO.h"

#include "../common/RandomHelper.h"

namespace Atlas {

	namespace Lighting {

		using namespace Common;

		SSAO::SSAO(uint32_t sampleCount) : sampleCount(sampleCount) {

			noiseTexture = Texture::Texture2D(4, 4, GL_RG16F, GL_REPEAT, GL_NEAREST);

			// Generate vec2 noise (4 * 4 * 2 floats)
			std::vector<float> noise;
			for (uint32_t i = 0; i < noiseTexture.width * noiseTexture.height * 2; i++)
				noise.push_back(Random::CanonicalUniform());

			noiseTexture.SetData(noise);

			SetSampleCount(sampleCount);

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