#include "Shadow.h"

namespace Atlas {

    namespace Lighting {

        Shadow::Shadow(float distance, float bias, int32_t resolution, int32_t cascadeCount, float splitCorrection) :
                distance(distance), bias(bias), resolution(resolution) {

            splitCorrection = glm::clamp(splitCorrection, 0.0f, 1.0f);
            componentCount = cascadeCount;
            this->splitCorrection = splitCorrection;

            isCascaded = true;
            useCubemap = false;

            maps = Texture::Texture2DArray(resolution, resolution, cascadeCount, 
                VK_FORMAT_D16_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

            components = std::vector<ShadowView>(cascadeCount);

            update = true;

        }

        Shadow::Shadow(float distance, float bias, int32_t resolution, bool useCubemap) :
                distance(distance), bias(bias), resolution(resolution), useCubemap(useCubemap) {

            splitCorrection = 0.0f;
            isCascaded = false;

            if (useCubemap) {
                componentCount = 6;

                cubemap  = Texture::Cubemap(resolution, resolution, VK_FORMAT_D16_UNORM,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
            }
            else {
                componentCount = 1;

                maps = Texture::Texture2DArray(resolution, resolution, 1, VK_FORMAT_D16_UNORM,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
            }

            components = std::vector<ShadowView>(componentCount);

            update = true;

        }

        void Shadow::SetResolution(int32_t resolution) {

            this->resolution = resolution;

            if (useCubemap) {
                cubemap  = Texture::Cubemap(resolution, resolution, VK_FORMAT_D16_UNORM,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
            }
            else {
                maps = Texture::Texture2DArray(resolution, resolution, componentCount, VK_FORMAT_D16_UNORM,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
            }

            update = true;

        }

        void Shadow::Update() {

            update = true;

        }     

    }

}