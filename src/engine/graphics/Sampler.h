#ifndef AE_GRAPHICSSAMPLER_H
#define AE_GRAPHICSSAMPLER_H

#include "Common.h"

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;

        struct SamplerDesc {
            VkFilter filter = VK_FILTER_NEAREST;
            VkSamplerAddressMode mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

            VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            float maxLod = 0.0f;
            float mipLodBias = 0.0f;

            VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

            bool compareEnabled = false;
            VkCompareOp compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

            bool anisotropicFiltering = false;
        };

        class Sampler {
        public:
            Sampler(GraphicsDevice* device, const SamplerDesc& desc);

            ~Sampler();

            VkSampler sampler;

        private:
            GraphicsDevice* device;

        };

    }

}

#endif