#ifndef AE_GRAPHICSRENDERPASS_H
#define AE_GRAPHICSRENDERPASS_H

#include "Common.h"
#include "Texture.h"

#include <vector>

namespace Atlas {

    namespace Graphics {

        struct RenderPassDesc {
            std::vector<Texture*> textureAttachments;
        };

        class RenderPass {



        };

    }

}

#endif
