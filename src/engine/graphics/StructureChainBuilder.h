#pragma once

#include "Common.h"

namespace Atlas {

    namespace Graphics {

        class StructureChainBuilder {

        public:
            StructureChainBuilder() = delete;

            template<typename T>
            StructureChainBuilder(T& structure) {

                chainTail = reinterpret_cast<VkBaseInStructure*>(&structure);

            }

            template<typename T>
            void Append(T& structure) {

                auto chainNext = reinterpret_cast<VkBaseInStructure*>(&structure);

                chainTail->pNext = chainNext;
                chainTail = chainNext;

            }

        private:
            VkBaseInStructure* chainTail = nullptr;

        };

    }

}