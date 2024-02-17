#pragma once

#include "renderer/PrimitiveBatch.h"
#include "volume/AABB.h"
#include "volume/Frustum.h"

namespace Atlas::Editor {

    class PrimitiveBatchWrapper {

    public:
        PrimitiveBatchWrapper();

        void RenderLineAABB(Volume::AABB aabb, vec3 color);

        void RenderLineFrustum(Volume::Frustum frustum, vec3 color);

        void RenderLineSphere(vec3 point, float radius, vec3 color);

        void RenderLineRectangle(vec3 point, vec3 right, vec3 down, vec3 color);

        Ref<Renderer::PrimitiveBatch> primitiveBatch;

    };

}