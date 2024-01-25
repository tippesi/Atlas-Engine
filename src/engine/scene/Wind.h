#pragma once

#include "../System.h"
#include "../texture/Texture2D.h"
#include "../common/NoiseGenerator.h"
#include "../loader/ImageLoader.h"

namespace Atlas {

    namespace Scene {

        class Wind {

        public:
            Wind() {

                auto image = CreateRef<Common::Image<float>>(512, 512, 1);
                std::vector<float> amplitudes = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f / 2.0f, 1.0f / 4.0f, 1.0f / 8.0f};
                Common::NoiseGenerator::GeneratePerlinNoise2D(*image, amplitudes, 51234543, 2.0f);

                noiseMap = Texture::Texture2D(image, Texture::Wrapping::Repeat, Texture::Filtering::MipMapLinear);

            }

            vec2 direction = vec2(1.0f);
            float speed = 10.0f;

            Texture::Texture2D noiseMap;

        };

    }

}