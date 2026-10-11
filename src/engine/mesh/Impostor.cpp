#include "Impostor.h"

namespace Atlas {

    namespace Mesh {

        Impostor::Impostor() {

            impostorInfoBuffer = Buffer::UniformBuffer(sizeof(ImpostorInfo));

        }

        Impostor::Impostor(int32_t views, int32_t resolution) : 
            views(views), resolution(resolution) {

            AllocateTextures();

            impostorInfoBuffer = Buffer::UniformBuffer(sizeof(ImpostorInfo));

        }

        void Impostor::FillViewPlaneBuffer(const std::vector<vec3>& rightVectors, const std::vector<vec3>& upVectors) {

            viewPlanes.clear();

            for (size_t i = 0; i < rightVectors.size(); i++) {

                ImpostorViewPlane viewPlane;

                viewPlane.right = vec4(rightVectors[i], 0.0f);
                viewPlane.up = vec4(upVectors[i], 0.0f);

                viewPlanes.push_back(viewPlane);

            }

            RefreshViewPlaneBuffer();

        }

        void Impostor::RefreshViewPlaneBuffer() {

            viewPlaneBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit,
                sizeof(ImpostorViewPlane), viewPlanes.size(), this->viewPlanes.data());

        }

        void Impostor::AllocateTextures() {

            baseColorTexture = CreateRef<Atlas::Texture::Texture2DArray>(resolution,
                resolution, views * views, VK_FORMAT_R8G8B8A8_UNORM,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);
            roughnessMetalnessAoTexture = CreateRef<Atlas::Texture::Texture2DArray>(resolution,
                resolution, views * views, VK_FORMAT_R8G8B8A8_UNORM,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);
            normalTexture = CreateRef<Atlas::Texture::Texture2DArray>(resolution,
                resolution, views * views, VK_FORMAT_R8G8B8A8_UNORM,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);
            depthTexture = CreateRef<Atlas::Texture::Texture2DArray>(resolution,
                resolution, views * views, VK_FORMAT_R16_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);

        }

    }

}