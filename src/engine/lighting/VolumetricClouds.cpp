#include "VolumetricClouds.h"

#include "../common/NoiseGenerator.h"
#include "../common/Noise.h"

#include "loader/ImageLoader.h"

namespace Atlas {

    namespace Lighting {

        VolumetricClouds::VolumetricClouds(int32_t coverageResolution, int32_t shapeResolution,
            int32_t detailResolution, int32_t shadowResolution) :
            coverageTexture(coverageResolution, coverageResolution,
                VK_FORMAT_R16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::Linear),
            shapeTexture(shapeResolution, shapeResolution, shapeResolution,
                VK_FORMAT_R16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::MipMapLinear),
            detailTexture(detailResolution, detailResolution, detailResolution,
                VK_FORMAT_R16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::MipMapLinear),
            shadowTexture(shadowResolution, shadowResolution,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear) {

            Common::Image<float> noiseImage(coverageResolution, coverageResolution, 1);
            std::vector<float> amplitudes = {0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.25f, 0.125f};
            Common::NoiseGenerator::GeneratePerlinNoise2DParallel(noiseImage, amplitudes, 0, JobPriority::Medium);

            auto data = noiseImage.ConvertData<float16>();
            coverageTexture.SetData(data);

            auto heightImage = Loader::ImageLoader::LoadImage<uint8_t>("cloudHeight.png", false, 4);
            heightTexture = Texture::Texture2D(heightImage->width, heightImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            heightTexture.SetData(heightImage->GetData());

            GenerateCoverageTexture();

        }

        void VolumetricClouds::GetShadowMatrices(const CameraComponent& camera, glm::vec3 lightDirection,
            glm::mat4 &viewMatrix, glm::mat4 &projectionMatrix) {

            auto cameraLocation = camera.GetLocation();

            auto cascadeCenter = cameraLocation;

            // A near enough up vector. This is because if the light location is
            // (0.0f, 1.0f, 0.0f) the shadows wouldn't render correctly due to the
            // shadows (or lights) view matrix. This is just a hack
            vec3 up = glm::vec3(0.0000000000000001f, 1.0f, 0.0000000000000001f);
            viewMatrix = lookAt(cascadeCenter, cascadeCenter + lightDirection, up);

            auto corners = camera.GetFrustumCorners(-camera.farPlane, camera.farPlane);

            vec3 maxProj = vec3(viewMatrix * vec4(corners.at(0), 1.0f));
            vec3 minProj = maxProj;

            auto maxLength = 0.0f;

            for (auto corner : corners) {
                maxLength = glm::max(maxLength, glm::length(corner - cascadeCenter));

                corner = vec3(viewMatrix * vec4(corner, 1.0f));

                maxProj.x = glm::max(maxProj.x, corner.x);
                maxProj.y = glm::max(maxProj.y, corner.y);
                maxProj.z = glm::max(maxProj.z, corner.z);

                minProj.x = glm::min(minProj.x, corner.x);
                minProj.y = glm::min(minProj.y, corner.y);
                minProj.z = glm::min(minProj.z, corner.z);
            }

            const mat4 clip = mat4(1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, -1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                0.0f, 0.0f, 0.5f, 1.0f);

            projectionMatrix = clip * glm::ortho(minProj.x,
                maxProj.x,
                minProj.y,
                maxProj.y,
                -maxProj.z - 10250.0f, // We need to render stuff behind the camera
                -minProj.z + 10.0f);

        }

        void VolumetricClouds::SetShadowResolution(int32_t shadowResolution) {

            shadowTexture = Texture::Texture2D(shadowResolution, shadowResolution,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        }

        void VolumetricClouds::GenerateCoverageTexture() {

            auto remap = [](float originalValue, float originalMin, float originalMax, float newMin, float newMax) -> float {
                return newMin + ((glm::max(0.0f, originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
                };

            auto perlin = [](vec2 pos, float scale) -> float {
                return (0.5f * glm::perlin(pos, vec2(scale)) + 0.5) * 0.9f;
                };

            auto perlin2 = [&](vec2 pos, float scale, vec2 weights) -> float {
                float octaves = perlin(pos * scale, scale) * weights.x
                    + perlin(2.0f * pos * scale, 2.0f * scale) * weights.y;

                return octaves / (weights.x + weights.y);
                };

            auto worley4 = [](vec3 pos, float scale, float seed, vec4 weights) -> float {
                float octave0 = Common::Worley(pos, 1.0f * scale, seed) * weights.x;
                float octave1 = Common::Worley(pos, 2.0f * scale, seed) * weights.y;
                float octave2 = Common::Worley(pos, 4.0f * scale, seed) * weights.z;
                float octave3 = Common::Worley(pos, 8.0f * scale, seed) * weights.w;

                return (octave0 + octave1 + octave2 + octave3) / 
                    (weights.x + weights.y + weights.z + weights.w);
                };

            Common::Image<float> noiseImage(coverageTexture.width, coverageTexture.height, 1);

            const float scale = 3.0f;
            for (int32_t y = 0; y < coverageTexture.height; y++) {
                for (int32_t x = 0; x < coverageTexture.width; x++) {

                    vec3 pos = vec3(
                        float(x) / float(noiseImage.width),
                        float(y) / float(noiseImage.height),
                        0.0f
                    );

                    float perlinNoise = perlin2(vec2(pos), scale, vec2(1.0f, 0.5f));

                    vec4 weights = vec4(1.0f, 0.5f, 0.25f, 0.125f);
                    float worley = worley4(pos, scale, 4387535.0f, weights);
                    //float worley = glm::min(1.0f, Common::Worley(pos, scale, 4387535.0f));

                    float noise = remap(perlinNoise, worley, 1.0f, 0.0f, 1.0f);
                    noiseImage.SetData(x, y, 0, noise);

                }
            }

            auto ref = CreateRef(noiseImage);
            ref->fileFormat = Common::ImageFormat::HDR;
            Loader::ImageLoader::SaveImage(ref, "noise.hdr");

            auto data = noiseImage.ConvertData<float16>();
            coverageTexture.SetData(data);

        }

    }

}
