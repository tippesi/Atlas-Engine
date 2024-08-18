#include "EnvironmentProbe.h"

namespace Atlas {

    namespace Lighting {

        EnvironmentProbe::EnvironmentProbe(const ResourceHandle<Texture::Cubemap>& cubemap) : cubemap(cubemap), 
            generatedCubemap(4, 4, VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::ClampToEdge,
                Texture::Filtering::MipMapLinear), depth(4, 4, VK_FORMAT_D16_UNORM),
            filteredDiffuse(8, 8, VK_FORMAT_R16G16B16A16_SFLOAT),
            filteredSpecular(512, 512, VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear) {

            SetPosition(position);

        }

        EnvironmentProbe::EnvironmentProbe(int32_t res, vec3 position) : 
            generatedCubemap(res, res, VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::ClampToEdge,
                Texture::Filtering::MipMapLinear), depth(res, res, VK_FORMAT_D16_UNORM),
            filteredDiffuse(8, 8, VK_FORMAT_R16G16B16A16_SFLOAT),
            filteredSpecular(std::min(res, 512), std::min(res, 512), VK_FORMAT_R16G16B16A16_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear) {

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

        vec3 EnvironmentProbe::GetPosition() const {

            return position;

        }

        const Texture::Cubemap& EnvironmentProbe::GetCubemap() const {

            return cubemap.IsLoaded() ? *(cubemap.Get()) : generatedCubemap; 

        }

    }

}