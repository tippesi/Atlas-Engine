#pragma once

#include "renderer/PrimitiveBatch.h"
#include "volume/AABB.h"

namespace Atlas::Editor {

    class PrimitiveBatchWrapper {

    public:
        PrimitiveBatchWrapper();

        void RenderLineAABB(Volume::AABB aabb, vec3 color);

        Ref<Renderer::PrimitiveBatch> primitiveBatch;

    };

}