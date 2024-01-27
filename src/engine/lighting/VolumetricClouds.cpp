#include "VolumetricClouds.h"

#include "../common/NoiseGenerator.h"

namespace Atlas {

    namespace Lighting {

        VolumetricClouds::VolumetricClouds(int32_t coverageResolution, int32_t shapeResolution,
            int32_t detailResolution, int32_t shadowResolution) :
            coverageTexture(coverageResolution, coverageResolution,
                VK_FORMAT_R16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::Linear),
            shapeTexture(shapeResolution, shapeResolution / 4, shapeResolution,
                VK_FORMAT_R16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::MipMapLinear),
            detailTexture(detailResolution, detailResolution, detailResolution,
                VK_FORMAT_R16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::MipMapLinear),
            shadowTexture(shadowResolution, shadowResolution,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear) {

            Common::Image<float> noiseImage(coverageResolution, coverageResolution, 1);
            std::vector<float> amplitudes = {0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.25f, 0.125f};
            Common::NoiseGenerator::GeneratePerlinNoise2D(noiseImage, amplitudes, 0);

            auto data = noiseImage.ConvertData<float16>();
            coverageTexture.SetData(data);

        }

        void VolumetricClouds::GetShadowMatrices(const CameraComponent& camera, glm::vec3 lightDirection,
            glm::mat4 &viewMatrix, glm::mat4 &projectionMatrix) {

            auto cameraLocation = camera.GetLocation();

            auto cascadeCenter = cameraLocation + camera.direction *
                (camera.nearPlane + camera.farPlane * 0.5f);

            // A near enough up vector. This is because if the light location is
            // (0.0f, 1.0f, 0.0f) the shadows wouldn't render correctly due to the
            // shadows (or lights) view matrix. This is just a hack
            vec3 up = glm::vec3(0.0000000000000001f, 1.0f, 0.0000000000000001f);
            viewMatrix = lookAt(cascadeCenter, cascadeCenter + lightDirection, up);

            std::vector<vec3> corners = camera.GetFrustumCorners(camera.nearPlane, camera.farPlane);

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

    }

}