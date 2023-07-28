#include "Sampler.h"
#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        Sampler::Sampler(GraphicsDevice *device, const SamplerDesc &desc) : device(device) {

            VkSamplerCreateInfo samplerInfo = Initializers::InitSamplerCreateInfo(desc.filter,
                desc.mode, desc.mipmapMode, desc.maxLod, desc.mipLodBias);
            if (desc.anisotropicFiltering) {
                samplerInfo.anisotropyEnable = VK_TRUE;
                samplerInfo.maxAnisotropy = device->deviceProperties.properties.limits.maxSamplerAnisotropy;
            }
            if (desc.compareEnabled) {
                samplerInfo.compareEnable = VK_TRUE;
                samplerInfo.compareOp = desc.compareOp;
            }

            VK_CHECK(vkCreateSampler(device->device, &samplerInfo, nullptr, &sampler))

        }

        Sampler::~Sampler() {

            vkDestroySampler(device->device, sampler, nullptr);

        }

    }

}