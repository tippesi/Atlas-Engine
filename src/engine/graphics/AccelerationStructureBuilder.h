#ifndef GRAPHICSASBUILDER_H
#define GRAPHICSASBUILDER_H

#include "Common.h"

#include "BLAS.h"
#include "TLAS.h"

namespace Atlas {

    namespace Graphics {

        class AccelerationStructureBuilder {

        public:
            AccelerationStructureBuilder() = default;

            BLASDesc GetBLASDescForTriangleGeometry(Ref<Buffer> vertexBuffer, Ref<Buffer> indexBuffer);

            void BuildBLAS(std::vector<Ref<BLAS>>& blases);
            
            void BuildTLAS(const std::vector<Ref<BLAS>>& blases);

        private:



        };

    }

}

#endif