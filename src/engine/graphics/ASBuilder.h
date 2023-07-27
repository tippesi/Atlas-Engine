#ifndef GRAPHICSASBUILDER_H
#define GRAPHICSASBUILDER_H

#include "Common.h"

#include "BLAS.h"
#include "TLAS.h"

namespace Atlas {

    namespace Graphics {

        class ASBuilder {

        public:
            ASBuilder() = default;

            BLASDesc GetBLASDescForTriangleGeometry(Ref<Buffer> vertexBuffer, Ref<Buffer> indexBuffer,
                size_t vertexCount, size_t vertexSize, size_t indexCount, size_t indexSize);

            void BuildBLAS(std::vector<Ref<BLAS>>& blases);
            
            void BuildTLAS(const std::vector<Ref<BLAS>>& blases);

        private:



        };

    }

}

#endif