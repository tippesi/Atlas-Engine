#ifndef AE_GRAPHICSQUERYPOOL_H
#define AE_GRAPHICSQUERYPOOL_H

#include "Common.h"

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;

        struct QueryPoolDesc {


        };

        class QueryPool {
        public:
            QueryPool(GraphicsDevice* device, QueryPool& desc);

        };

    }

}

#endif
