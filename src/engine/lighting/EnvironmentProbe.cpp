#include "EnvironmentProbe.h"

namespace Atlas {

    namespace Lighting {

        EnvironmentProbe::EnvironmentProbe(const Texture::Cubemap& cubemap) : resolution(cubemap.width),
            cubemap(cubemap), depth(cubemap.width, cubemap.height, VK_FORMAT_D16_UNORM),
            filteredSpecular(std::min(cubemap.width, 512), std::min(cubemap.height, 512), VK_FORMAT_R16G16B16A16_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear),
            filteredDiffuse(8, 8, VK_FORMAT_R16G16B16A16_SFLOAT) {

            SetPosition(position);

        }

        EnvironmentProbe::EnvironmentProbe(int32_t res, vec3 position) : resolution(res),
            cubemap(res, res, VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::ClampToEdge,
                Texture::Filtering::MipMapLinear), depth(res, res, VK_FORMAT_D16_UNORM),
            filteredSpecular(std::min(res, 512), std::min(res, 512), VK_FORMAT_R16G16B16A16_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear),
            filteredDiffuse(8, 8, VK_FORMAT_R16G16B16A16_SFLOAT) {

            SetPosition(position);

        }

        void EnvironmentProbe::SetPosition(vec3 position) {

            this->position = position;

            const mat4 clip = mat4(1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                0.0f, 0.0f, 0.5f, 1.0f);

            projectionMatrix = clip * glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
            vec3 faces[] = { vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f),
                             vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
                             vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f) };

            vec3 ups[] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
                           vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f),
                           vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) };

            for (uint8_t i = 0; i < 6; i++) {
                viewMatrices.push_back(glm::lookAt(position, position + faces[i], ups[i]));
            }

        }

        vec3 EnvironmentProbe::GetPosition() {

            return position;

        }

    }

}