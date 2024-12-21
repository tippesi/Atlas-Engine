#pragma once

#include "renderer/PrimitiveBatch.h"
#include "volume/AABB.h"
#include "volume/Frustum.h"
#include "volume/Rectangle.h"

namespace Atlas::Editor {

    class PrimitiveBatchWrapper {

    public:
        PrimitiveBatchWrapper();

        void RenderLineAABB(Volume::AABB aabb, vec3 color, bool depthTest);

        void RenderLineFrustum(Volume::Frustum frustum, vec3 color, bool depthTest);

        void RenderLineSphere(vec3 point, float radius, vec3 color, bool depthTest);

        void RenderLineRectangle(const Volume::Rectangle& rect, vec3 color, bool depthTest);

        void Clear();

        Ref<Renderer::PrimitiveBatch> primitiveBatchNoDepthTest;
        Ref<Renderer::PrimitiveBatch> primitiveBatchDepthTest;

    };

}