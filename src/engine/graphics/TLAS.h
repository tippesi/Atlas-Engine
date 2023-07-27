#ifndef AE_GRAPHICSTLAS_H
#define AE_GRAPHICSTLAS_H

#include "Common.h"

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;

        struct TLASDesc {
            
        };

        class TLAS {

        public:
            TLAS(GraphicsDevice* device, TLASDesc desc);

        };

    }

}

#endif