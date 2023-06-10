#include "Shadow.h"
#include "Light.h"
#include "DirectionalLight.h"
#include "PointLight.h"

namespace Atlas {

    namespace Lighting {

        Shadow::Shadow(float distance, float bias, int32_t resolution, int32_t cascadeCount, float splitCorrection) :
                distance(distance), bias(bias), resolution(resolution) {

            splitCorrection = glm::clamp(splitCorrection, 0.0f, 1.0f);
            componentCount = cascadeCount;
            this->splitCorrection = splitCorrection;

            useCubemap = false;

            maps = Texture::Texture2DArray(resolution, resolution, cascadeCount, 
                VK_FORMAT_D16_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

            components = std::vector<ShadowComponent>(cascadeCount);

            Update();

        }

        Shadow::Shadow(float distance, float bias, int32_t resolution, bool useCubemap) :
                distance(distance), bias(bias), resolution(resolution), useCubemap(useCubemap) {

            splitCorrection = 0.0f;

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

            components = std::vector<ShadowComponent>(componentCount);

            Update();

        }

        void Shadow::Update() {

            update = true;

        }

    }

}