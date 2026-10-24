#include "VolumetricClouds.h"
#include "common/MathHelper.h"

#include "../common/Noise.h"

namespace Atlas {

    namespace Lighting {

        std::unordered_map<int32_t, std::vector<float16>> VolumetricClouds::coverageDataCache;
        std::mutex VolumetricClouds::coverageDataCacheMutex;

        VolumetricClouds::VolumetricClouds(int32_t coverageResolution, int32_t shapeResolution,
            int32_t detailResolution, int32_t shadowResolution) :
            coverageTexture(coverageResolution, coverageResolution,
                VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::Linear),
            shapeTexture(shapeResolution, shapeResolution, shapeResolution,
                VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::MipMapLinear),
            detailTexture(detailResolution, detailResolution, detailResolution,
                VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::MipMapLinear),
            shadowTexture(shadowResolution, shadowResolution,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear) {

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

            {
                std::scoped_lock lock(coverageDataCacheMutex);
                if (coverageDataCache.contains(coverageTexture.width)) {
                    coverageTexture.SetData(coverageDataCache[coverageTexture.width]);
                    return;
                }
            }

            auto perlin = [](vec2 pos, float scale) -> float {
                return (0.5f * glm::perlin(pos, vec2(scale)) + 0.5) * 0.99f;
                };

            auto perlin2 = [&](vec2 pos, float scale, vec2 weights) -> float {
                float octaves = perlin(pos * scale, scale) * weights.x
                    + perlin(2.0f * pos * scale, 2.0f * scale) * weights.y;

                return octaves / (weights.x + weights.y);
                };

            auto perlin3 = [&](vec2 pos, float scale, vec3 weights) -> float {
                float octaves = perlin(pos * scale, scale) * weights.x
                    + perlin(2.0f * pos * scale, 2.0f * scale) * weights.y
                    + perlin(4.0f * pos * scale, 4.0f * scale) * weights.z;

                return octaves / (weights.x + weights.y + weights.z);
                };

            auto worley4 = [](vec3 pos, float scale, float seed, vec4 weights) -> float {
                float octave0 = Common::Worley(pos, 1.0f * scale, seed) * weights.x;
                float octave1 = Common::Worley(pos, 2.0f * scale, seed) * weights.y;
                float octave2 = Common::Worley(pos, 4.0f * scale, seed) * weights.z;
                float octave3 = Common::Worley(pos, 8.0f * scale, seed) * weights.w;

                return (octave0 + octave1 + octave2 + octave3) / 
                    (weights.x + weights.y + weights.z + weights.w);
                };

            // Weather map with coverage, cloud top type, cloud bottom type and maximum cloud height
            Common::Image<float> noiseImage(coverageTexture.width, coverageTexture.height, 4);

            for (int32_t y = 0; y < coverageTexture.height; y++) {
                for (int32_t x = 0; x < coverageTexture.width; x++) {

                    vec3 pos = vec3(
                        float(x) / float(noiseImage.width),
                        float(y) / float(noiseImage.height),
                        0.0f
                    );

                    vec2 uv = vec2(pos);

                    // Large weather systems, which are split into clusters and broken up by cellular gaps
                    float synoptic = perlin3(uv, 2.0f, vec3(1.0f, 0.48f, 0.20f));
                    float clusters = perlin3(uv + vec2(0.347f, 0.113f),
                        6.0f, vec3(1.0f, 0.52f, 0.22f));

                    vec4 weights = vec4(1.0f, 0.5f, 0.25f, 0.125f);
                    float worley = worley4(pos, 10.0f, 4387535.0f, weights);
                    float cellular = 1.0f - Helper::Math::Saturate(worley);

                    float coverage = 0.46f * synoptic + 0.39f * clusters + 0.15f * cellular;
                    float clusterBreakup = glm::smoothstep(0.22f, 0.78f,
                        0.72f * clusters + 0.28f * cellular);
                    coverage *= glm::mix(0.72f, 1.12f, clusterBreakup);
                    coverage = glm::smoothstep(0.16f, 0.84f,
                        Helper::Math::Saturate(coverage));

                    float topMacro = perlin3(uv + vec2(0.173f, 0.619f),
                        2.5f, vec3(1.0f, 0.45f, 0.16f));
                    float topCells = perlin2(uv + vec2(0.613f, 0.291f),
                        7.0f, vec2(1.0f, 0.35f));
                    float topType = 0.55f * topMacro + 0.35f * topCells + 0.10f * coverage;

                    float bottomType = perlin3(uv + vec2(0.731f, 0.247f),
                        3.0f, vec3(1.0f, 0.35f, 0.12f));

                    float heightMacro = perlin3(uv + vec2(0.419f, 0.837f),
                        3.0f, vec3(1.0f, 0.42f, 0.16f));
                    float heightCells = perlin2(uv + vec2(0.271f, 0.463f),
                        8.0f, vec2(1.0f, 0.30f));
                    float maxHeight = 0.52f * heightMacro + 0.30f * heightCells +
                        0.18f * topType;

                    noiseImage.SetData(x, y, 0, coverage);
                    noiseImage.SetData(x, y, 1, glm::smoothstep(0.08f, 0.92f, topType));
                    noiseImage.SetData(x, y, 2, glm::smoothstep(0.08f, 0.92f, bottomType));
                    noiseImage.SetData(x, y, 3, glm::smoothstep(0.05f, 0.95f, maxHeight));

                }
            }

            auto data = noiseImage.ConvertData<float16>();
            coverageTexture.SetData(data);

            std::scoped_lock lock(coverageDataCacheMutex);
            coverageDataCache[coverageTexture.width] = data;

        }

    }

}
