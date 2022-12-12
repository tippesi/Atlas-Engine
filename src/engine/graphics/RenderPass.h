#ifndef AE_GRAPHICSRENDERPASS_H
#define AE_GRAPHICSRENDERPASS_H

#include "Common.h"
#include "Image.h"

#include <vector>

namespace Atlas {

    namespace Graphics {

        struct RenderPassDesc {
            std::vector<Image*> textureAttachments;
        };

        class RenderPass {



        };

    }

}

#endif
