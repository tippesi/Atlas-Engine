#include "PrimitiveBatchWrapper.h"

#include "graphics/GraphicsDevice.h"

namespace Atlas::Editor {

    PrimitiveBatchWrapper::PrimitiveBatchWrapper() {

        auto device = Graphics::GraphicsDevice::DefaultDevice;

        primitiveBatch = CreateRef<Renderer::PrimitiveBatch>();
        primitiveBatch->SetLineWidth(device->support.wideLines ? 2.0f : 1.0f);

    }

    void PrimitiveBatchWrapper::RenderLineAABB(Volume::AABB aabb, glm::vec3 color) {

        auto corners = aabb.GetCorners();

        primitiveBatch->AddLine(corners[0], corners[1], color, color);
        primitiveBatch->AddLine(corners[1], corners[2], color, color);
        primitiveBatch->AddLine(corners[2], corners[3], color, color);
        primitiveBatch->AddLine(corners[3], corners[0], color, color);
        primitiveBatch->AddLine(corners[4], corners[5], color, color);
        primitiveBatch->AddLine(corners[5], corners[6], color, color);
        primitiveBatch->AddLine(corners[6], corners[7], color, color);
        primitiveBatch->AddLine(corners[7], corners[4], color, color);
        primitiveBatch->AddLine(corners[0], corners[4], color, color);
        primitiveBatch->AddLine(corners[1], corners[5], color, color);
        primitiveBatch->AddLine(corners[2], corners[6], color, color);
        primitiveBatch->AddLine(corners[3], corners[7], color, color);

    }

    void PrimitiveBatchWrapper::RenderLineFrustum(Volume::Frustum frustum, glm::vec3 color) {

        auto corners = frustum.GetCorners();
        if (corners.empty())
            return;

        primitiveBatch->AddLine(corners[0], corners[1], color, color);
        primitiveBatch->AddLine(corners[2], corners[3], color, color);
        primitiveBatch->AddLine(corners[0], corners[2], color, color);
        primitiveBatch->AddLine(corners[1], corners[3], color, color);
        primitiveBatch->AddLine(corners[4], corners[5], color, color);
        primitiveBatch->AddLine(corners[6], corners[7], color, color);
        primitiveBatch->AddLine(corners[4], corners[6], color, color);
        primitiveBatch->AddLine(corners[5], corners[7], color, color);
        primitiveBatch->AddLine(corners[0], corners[4], color, color);
        primitiveBatch->AddLine(corners[1], corners[5], color, color);
        primitiveBatch->AddLine(corners[2], corners[6], color, color);
        primitiveBatch->AddLine(corners[3], corners[7], color, color);

    }

}