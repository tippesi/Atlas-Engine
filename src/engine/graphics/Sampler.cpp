#include "Sampler.h"
#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        Sampler::Sampler(GraphicsDevice *device, SamplerDesc &desc) : device(device) {

            VkSamplerCreateInfo samplerInfo = Initializers::InitSamplerCreateInfo(desc.filter,
                desc.mode, desc.mipmapMode, desc.maxLod, desc.mipLodBias);
            if (desc.anisotropicFiltering) {
                samplerInfo.anisotropyEnable = VK_TRUE;
                samplerInfo.maxAnisotropy = device->deviceProperties.limits.maxSamplerAnisotropy;
            }
            VK_CHECK(vkCreateSampler(device->device, &samplerInfo, nullptr, &sampler))

        }

        Sampler::~Sampler() {

            vkDestroySampler(device->device, sampler, nullptr);

        }

    }

}