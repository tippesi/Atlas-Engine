#include "VolumetricClouds.h"

#include "../common/NoiseGenerator.h"

namespace Atlas {

    namespace Lighting {

        VolumetricClouds::VolumetricClouds(int32_t coverageResolution, int32_t shapeResolution, int32_t detailResolution) :
            coverageTexture(coverageResolution, coverageResolution,
                VK_FORMAT_R16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::Linear),
            shapeTexture(shapeResolution, shapeResolution / 4, shapeResolution,
                VK_FORMAT_R16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::MipMapLinear),
            detailTexture(detailResolution, detailResolution, detailResolution,
                VK_FORMAT_R16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::MipMapLinear)  {

            Common::Image<float> noiseImage(coverageResolution, coverageResolution, 1);
            std::vector<float> amplitudes = {0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.25f, 0.125f};
            Common::NoiseGenerator::GeneratePerlinNoise2D(noiseImage, amplitudes, 0);

            auto data = noiseImage.ConvertData<float16>();
            coverageTexture.SetData(data);

        }

    }

}