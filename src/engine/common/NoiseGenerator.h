#ifndef AE_NOISEGENERATOR_H
#define AE_NOISEGENERATOR_H

#include "../System.h"
#include "../texture/Texture2D.h"
#include "Image.h"

#include <perlin noise/PerlinNoise.h>
#include <algorithm>

#include <glm/gtc/noise.hpp>

namespace Atlas {

    namespace Common {

        class NoiseGenerator {

        public:
            template<typename T>
            static void GenerateUniformNoise2D(Image<T>& image) {

                auto& data = image.GetData();

                for (size_t i = 0; i < data.size(); i++) {
                    if constexpr(std::is_same_v<T, uint8_t>) {
                        data[i] = rand() % 255;
                    }
                    else if constexpr (std::is_same_v<T, uint16_t>) {
                        data[i] = rand() % 65535;
                    }
                    else if constexpr (std::is_same_v<T, float>) {
                        data[i] = float(rand()) / float(RAND_MAX);
                    }
                }

            }
            
            template<typename T>
            static void GeneratePerlinNoise2D(Image<T>& image, std::vector<float> amplitudes,
                uint32_t seed, float exp = 1.0f) {

                auto& data = image.GetData();

                auto amplitude = std::accumulate(amplitudes.begin(), amplitudes.end(), 0.0f);

                amplitude = 1.0f / amplitude;

                for (size_t y = 0; y < image.height; y++) {
                    for (size_t x = 0; x < image.width; x++) {
                        auto fx = (float)x / (float)image.width;
                        auto fy = (float)y / (float)image.height;

                        float noise = 0.0f;
                        float oct = 1.0f;
                        for (auto amp : amplitudes) {
                            vec2 coord = vec2(fx, fy) * oct;
                            noise += glm::perlin(coord, vec2(oct)) * amp;
                            oct *= 2.0f;
                        }

                        auto value = noise * amplitude;
                        value = powf(0.5f * value + 0.5f, exp);
                        for (size_t channel = 0; channel < image.channels; channel++) {
                            auto idx = (y * image.width + x) * image.channels + channel;
                            if constexpr (std::is_same_v<T, uint8_t>) {
                                data[idx] = T(255.0f * value);
                            }
                            else if constexpr (std::is_same_v<T, uint16_t>) {
                                data[idx] = T(65535.0f * value);
                            }
                            else if constexpr (std::is_same_v<T, float>) {
                                data[idx] = T(value);
                            }
                        }
                    }
                }

            }

        };        

    }

}

#endif