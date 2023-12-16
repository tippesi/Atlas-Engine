#pragma once

#include "Common.h"

#include <vector>
#include <unordered_map>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;

        enum class DescriptorSetBindingType {

        };

        struct DescriptorSetBinding {
            uint32_t bindingIdx;

        };

        class DescriptorSetLayout {

        };

        /**
        * Should allow to be specified by hand (e.g. to allow efficient bindless sets)
        * Should allow to force bind in command list
        * Needs to be able to check for compatibility of a shader
        * Needs to be able to write the associated data
        */
        class DescriptorSet {

        public:
            explicit DescriptorSet(std::vector<DescriptorSetBinding> bindings);

            ~DescriptorSet();

            bool IsCompatible();

            void WriteBuffer();

            void WriteCombindImageSampler();

            void WriteImage();

            void WriteTLAS();

            void PushWrites();

        private:
            VkDescriptorSet set;



        };

    }

}