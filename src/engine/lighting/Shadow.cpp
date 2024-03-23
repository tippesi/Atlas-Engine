#include "Shadow.h"

namespace Atlas {

    namespace Lighting {

        Shadow::Shadow(float distance, float bias, int32_t resolution, float edgeSoftness, int32_t cascadeCount, 
            float splitCorrection) : distance(distance), bias(bias), resolution(resolution), edgeSoftness(edgeSoftness) {

            splitCorrection = glm::clamp(splitCorrection, 0.0f, 1.0f);
            viewCount = cascadeCount;
            this->splitCorrection = splitCorrection;

            isCascaded = true;
            useCubemap = false;

            maps = Texture::Texture2DArray(resolution, resolution, cascadeCount, 
                VK_FORMAT_D16_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

            views = std::vector<ShadowView>(cascadeCount);

            update = true;

        }

        Shadow::Shadow(float distance, float bias, int32_t resolution, float edgeSoftness, bool useCubemap) :
                distance(distance), bias(bias), resolution(resolution), useCubemap(useCubemap), edgeSoftness(edgeSoftness) {

            isCascaded = false;

            if (useCubemap) {
                viewCount = 6;

                cubemap  = Texture::Cubemap(resolution, resolution, VK_FORMAT_D16_UNORM,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
            }
            else {
                viewCount = 1;

                maps = Texture::Texture2DArray(resolution, resolution, 1, VK_FORMAT_D16_UNORM,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
            }

            views = std::vector<ShadowView>(viewCount);

            update = true;

        }

        void Shadow::SetResolution(int32_t resolution) {

            this->resolution = resolution;

            if (useCubemap) {
                cubemap  = Texture::Cubemap(resolution, resolution, VK_FORMAT_D16_UNORM,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
            }
            else {
                maps = Texture::Texture2DArray(resolution, resolution, viewCount, VK_FORMAT_D16_UNORM,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
            }

            update = true;

        }

        void Shadow::Update() {

            update = true;

        }     

    }

}