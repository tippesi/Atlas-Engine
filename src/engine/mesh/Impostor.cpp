#include "Impostor.h"

namespace Atlas {

    namespace Mesh {

        Impostor::Impostor(int32_t views, int32_t resolution) : 
            views(views), resolution(resolution) {

            baseColorTexture = Atlas::Texture::Texture2DArray(resolution,
                resolution, views * views, VK_FORMAT_R8G8B8A8_UNORM,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);
            roughnessMetalnessAoTexture = Atlas::Texture::Texture2DArray(resolution,
                resolution, views * views, VK_FORMAT_R8G8B8A8_UNORM,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);
            normalTexture = Atlas::Texture::Texture2DArray(resolution,
                resolution, views * views, VK_FORMAT_R8G8B8A8_UNORM,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);
            depthTexture = Atlas::Texture::Texture2DArray(resolution,
                resolution, views * views, VK_FORMAT_R16_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);

        }

        void Impostor::FillViewPlaneBuffer(std::vector<vec3> rightVectors, std::vector<vec3> upVectors) {

            std::vector<ViewPlane> viewPlanes;

            for (size_t i = 0; i < rightVectors.size(); i++) {

                ViewPlane viewPlane;

                viewPlane.right = vec4(rightVectors[i], 0.0f);
                viewPlane.up = vec4(upVectors[i], 0.0f);

                viewPlanes.push_back(viewPlane);

            }

            viewPlaneBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit,
                sizeof(ViewPlane), viewPlanes.size(), viewPlanes.data());

        }

    }

}