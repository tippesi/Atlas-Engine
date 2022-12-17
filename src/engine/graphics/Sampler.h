#ifndef AE_GRAPHICSSAMPLER_H
#define AE_GRAPHICSSAMPLER_H

#include "Common.h"
#include "MemoryManager.h"

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;

        struct SamplerDesc {
            VkFilter filter = VK_FILTER_NEAREST;
            VkSamplerAddressMode mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        };

        class Sampler {
        public:
            Sampler(GraphicsDevice* device, SamplerDesc& desc);

            ~Sampler();

            VkSampler sampler;

        private:
            GraphicsDevice* device;

        };

    }

}

#endif