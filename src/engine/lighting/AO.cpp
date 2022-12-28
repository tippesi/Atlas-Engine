#include "AO.h"

#include "../common/RandomHelper.h"

namespace Atlas {

	namespace Lighting {

		using namespace Common;

		AO::AO(int32_t sampleCount) : sampleCount(sampleCount) {

            noiseTexture = Texture::Texture2D(8, 8, VK_FORMAT_R16G16_SFLOAT);

			// Generate vec2 noise (4 * 4 * 2 floats)
			std::vector<float> noise;
			for (int32_t i = 0; i < noiseTexture.width * noiseTexture.height * 2; i++)
				noise.push_back(Random::SampleUniformFloat());

			noiseTexture.SetData(noise);

			SetSampleCount(sampleCount);

		}

		void AO::SetSampleCount(int32_t sampleCount) {			

			this->sampleCount = sampleCount;

			samples.clear();
			for (int32_t i = 0; i < sampleCount; i++) {

				glm::vec3 sample = normalize(glm::vec3(
					2.0f * Random::SampleUniformFloat() - 1.0f, 
					2.0f * Random::SampleUniformFloat() - 1.0f, 
					Random::SampleUniformFloat()));

				sample *= Random::SampleUniformFloat();

				float scale = float(i) / float(sampleCount);

				scale = glm::mix(0.1f, 1.0f, scale * scale);
				sample *= scale;

				samples.push_back(sample);

			}
		}

	}

}